#include "foot_observer.h"
#include "foot_angle_estimator.h"
#include "esp_timer.h"
#include <new>

bool FootObserver::begin(OneShotCamera& camera, RunControlWorker& control) {
  camera_ = &camera; control_ = &control;
  frames_ = static_cast<FootFrame*>(ps_malloc(sizeof(FootFrame) * kCapacity));
  if (!frames_ || !camera.snapshot().camera_ok || !control.ready()) return false;
  portENTER_CRITICAL(&mux_); status_.available = true; portEXIT_CRITICAL(&mux_);
  if (xTaskCreatePinnedToCore(entry, "foot_observer", 10240, this, 1, &task_, 0) != pdPASS) {
    portENTER_CRITICAL(&mux_); status_.available = false; portEXIT_CRITICAL(&mux_); return false;
  }
  return true;
}
FootSnapshot FootObserver::snapshot() const {
  portENTER_CRITICAL(&mux_); const auto copy = status_; portEXIT_CRITICAL(&mux_);
  return copy;
}
bool FootObserver::readyToStart() const {
  const auto s = snapshot();
  return s.available && s.zero_ready && s.latest.right_valid && s.latest.left_valid &&
      esp_timer_get_time() - s.latest.delivered_us < 500000;
}
void FootObserver::beginRun(uint16_t id, uint64_t epoch) {
  portENTER_CRITICAL(&mux_);
  recording_run_id_ = id; log_epoch_us_ = epoch;
  status_.count = 0; status_.overflow = false; status_.recording = true;
  portEXIT_CRITICAL(&mux_);
}
void FootObserver::finishRun() {
  portENTER_CRITICAL(&mux_); status_.recording = false; portEXIT_CRITICAL(&mux_);
}
void FootObserver::clearRun() {
  portENTER_CRITICAL(&mux_);
  status_.recording = false; status_.count = 0; status_.overflow = false;
  portEXIT_CRITICAL(&mux_);
}
void FootObserver::loop() {
  uint32_t seq = 0;
  uint64_t last_valid_us = 0;
  for (;;) {
    const auto run_before = control_->snapshot();
    // Capture for boot zero/idle alignment and throughout START/MEASURE/END.
    // Once a run is complete the camera is idle while logs are prepared/sent.
    if (run_before.state_id == 4 || run_before.state_id == 5) {
      camera_->stopContinuous(); vTaskDelay(pdMS_TO_TICKS(20)); continue;
    }
    const uint64_t begin = esp_timer_get_time();
    camera_fb_t* fb = camera_->startContinuous() ? camera_->acquireContinuous(200) : nullptr;
    const auto run = control_->snapshot();
    FootFrame f{};
    f.sequence = ++seq; f.delivered_us = esp_timer_get_time();
    f.imu_sample_us = run.imu_sample_us; f.state_id = run.state_id;
    f.led_state = run.led_state; f.sync_event_id = run.sync_event_id;
    f.run_id = run.run_id;
    f.frame_valid = fb && fb->buf && fb->len == 320U * 240U &&
        fb->width == 320 && fb->height == 240 && fb->format == PIXFORMAT_GRAYSCALE;
    if (fb) {
      f.frame_us = static_cast<uint64_t>(fb->timestamp.tv_sec) * 1000000ULL + fb->timestamp.tv_usec;
      f.timestamp_valid = f.frame_us > 0 && f.frame_us <= f.delivered_us &&
          f.delivered_us - f.frame_us < 500000;
    }
    const uint32_t processing_start = micros();
    const auto a = right_.process(f.frame_valid ? fb->buf : nullptr);
    const auto b = left_.process(f.frame_valid ? fb->buf : nullptr);
    // Check the IMU at BOTH sides of capture; any instability in between changes
    // the epoch. A stale queued image must not contribute to upright zeroing.
    zero_.observe(millis(), run.upright_epoch,
        run.upright_stable && run_before.upright_stable &&
        run.upright_epoch == run_before.upright_epoch && !run.running &&
        f.timestamp_valid && f.frame_us >= run.upright_since_us,
        a.valid && b.valid, a.center_x_px, b.center_x_px);
    const auto ar = estimateFootAngle(a, zero_.a_zero, zero_.ready);
    const auto bl = estimateFootAngle(b, zero_.b_zero, zero_.ready);
    f.zero_ready = zero_.ready; f.right_valid = ar.valid; f.left_valid = bl.valid;
    f.right_in_range = a.valid && ar.in_calibration_range;
    f.left_in_range = b.valid && bl.in_calibration_range;
    if (a.valid) { f.right_x = a.center_x_px; f.right_deg = ar.angle_deg; }
    if (b.valid) { f.left_x = b.center_x_px; f.left_deg = bl.angle_deg; }
    f.right_contrast = a.peak_contrast; f.left_contrast = b.peak_contrast;
    f.processing_us = micros() - processing_start;
    camera_->releaseContinuous(fb);
    float fps = 0;
    if (f.frame_valid) {
      if (last_valid_us && f.delivered_us > last_valid_us) fps = 1000000.0f / (f.delivered_us - last_valid_us);
      last_valid_us = f.delivered_us;
    }
    portENTER_CRITICAL(&mux_);
    if (!f.frame_valid) ++status_.frame_failures;
    if (status_.recording && f.run_id == recording_run_id_) {
      // Signed offsets retain frame-boundary cases; never label them as zero.
      f.log_time_us = f.timestamp_valid ? static_cast<int64_t>(f.frame_us) - log_epoch_us_ : -1;
      f.measurement_time_us = f.timestamp_valid && run.measurement_epoch_us
          ? static_cast<int64_t>(f.frame_us) - run.measurement_epoch_us : -1;
      if (status_.count < kCapacity) frames_[status_.count++] = f;
      else status_.overflow = true;
    }
    status_.latest = f; status_.zero_ready = zero_.ready;
    status_.right_zero = zero_.a_zero; status_.left_zero = zero_.b_zero;
    status_.zero_samples = zero_.count;
    status_.fps = fps;
    portEXIT_CRITICAL(&mux_);
    const int64_t remaining = 66667 - (esp_timer_get_time() - begin);
    if (remaining > 0) vTaskDelay(pdMS_TO_TICKS((remaining + 999) / 1000));
    else vTaskDelay(1);
  }
}

static String number(float v) { return isfinite(v) ? String(v, 5) : String("null"); }
void FootObserver::appendMetadata(PsramString& json) const {
  const auto s = snapshot();
  json += "\"foot_observation\":{\"revision\":\"freefoot_runtime_v2_20260923\",\"observation_only\":true,";
  json += "\"calibration_source_commit\":\"ac6df8caf59c93956b87cba57521903c25ff9f00\",";
  json += "\"mapping\":\"right=A upper lane;left=B lower lane\",\"positive_direction\":\"marker_x_decreases\",";
  json += "\"frame_timestamp_semantics\":\"camera_driver_frame_timestamp_not_verified_exposure_time\",";
  json += "\"control_context_semantics\":\"latest_control_snapshot_at_frame_delivery_not_exposure\",";
  json += "\"target_fps\":15,\"capacity\":" + String(kCapacity);
  json += ",\"count\":" + String(s.count) + ",\"overflow\":" + String(s.overflow ? "true" : "false");
  json += ",\"available\":" + String(s.available ? "true" : "false");
  json += ",\"zero_ready\":" + String(s.zero_ready ? "true" : "false");
  json += ",\"right_zero_x\":" + number(s.right_zero) + ",\"left_zero_x\":" + number(s.left_zero);
  json += ",\"right_deg_per_px\":0.167779119,\"left_deg_per_px\":0.162645305,";
  json += "\"right_support_x\":[42,173],\"left_support_x\":[43.5,177.5]},\"foot_frames\":[";
  for (uint32_t i = 0; i < s.count; ++i) {
    // Recording is sealed before export begins and clear/start are excluded.
    const auto& f = frames_[i];
    if (i) json += ",";
    char buf[480];
    snprintf(buf, sizeof(buf),
        "{\"sequence\":%lu,\"run_id\":%u,\"frame_us\":%llu,\"delivered_us\":%llu,"
        "\"log_time_us\":%lld,\"measurement_time_us\":%lld,\"imu_sample_us\":%lu,"
        "\"state_id\":%u,\"led_state\":%u,\"sync_event_id\":%u,\"processing_us\":%lu,"
        "\"timestamp_valid\":%s,\"frame_valid\":%s,\"zero_ready\":%s,"
        "\"right_valid\":%s,\"left_valid\":%s,\"right_in_range\":%s,\"left_in_range\":%s",
        static_cast<unsigned long>(f.sequence), f.run_id, f.frame_us, f.delivered_us,
        f.log_time_us, f.measurement_time_us, static_cast<unsigned long>(f.imu_sample_us),
        f.state_id, f.led_state, f.sync_event_id, static_cast<unsigned long>(f.processing_us),
        f.timestamp_valid ? "true" : "false", f.frame_valid ? "true" : "false", f.zero_ready ? "true" : "false",
        f.right_valid ? "true" : "false", f.left_valid ? "true" : "false",
        f.right_in_range ? "true" : "false", f.left_in_range ? "true" : "false");
    json += buf;
    json += ",\"right_x\":" + number(f.right_x) + ",\"left_x\":" + number(f.left_x);
    json += ",\"right_deg\":" + number(f.right_deg) + ",\"left_deg\":" + number(f.left_deg);
    json += ",\"right_contrast\":" + number(f.right_contrast) + ",\"left_contrast\":" + number(f.left_contrast) + "}";
  }
  json += "]";
}
