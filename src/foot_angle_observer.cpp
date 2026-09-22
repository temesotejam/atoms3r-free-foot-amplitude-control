#include "foot_angle_observer.h"

#include <WiFi.h>
#include <esp_camera.h>
#include <math.h>

namespace {

constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 240;

constexpr int kRightMarkerRows[] = {58, 62, 66, 70, 74};
constexpr int kRightReferenceRows[] = {38, 42, 46};
constexpr int kLeftMarkerRows[] = {152, 156, 160, 164, 168};
constexpr int kLeftReferenceRows[] = {182, 186, 190};

constexpr float kPeakMinContrast = 55.0f;
constexpr float kCentroidBaseline = 35.0f;
constexpr float kMinWeightSum = 150.0f;
constexpr int kCentroidHalfWindowPx = 35;
constexpr int kSmoothRadius = 2;
constexpr int kSmoothWindow = 2 * kSmoothRadius + 1;
constexpr int kContrastScale = 15;
constexpr int kPeakThresholdScaled =
    static_cast<int>(kPeakMinContrast * kContrastScale);
constexpr int kCentroidBaselineScaled =
    static_cast<int>(kCentroidBaseline * kContrastScale);
constexpr int64_t kMinWeightScaled =
    static_cast<int64_t>(kMinWeightSum * kContrastScale);

constexpr float kRightMinCalXPx = 42.0f;
constexpr float kRightMaxCalXPx = 173.0f;
constexpr float kLeftMinCalXPx = 43.5f;
constexpr float kLeftMaxCalXPx = 177.5f;

// AtomS3R-CAM / GC0308 pin map inherited unchanged from
// temesotejam/atoms3r-foot-angle-tracker.
constexpr int kCamPowerN = 18;
constexpr int kCamSda = 12;
constexpr int kCamScl = 9;
constexpr int kCamVsync = 10;
constexpr int kCamHref = 14;
constexpr int kCamXclk = 21;
constexpr int kCamPclk = 40;
constexpr int kCamD0 = 3;
constexpr int kCamD1 = 42;
constexpr int kCamD2 = 46;
constexpr int kCamD3 = 48;
constexpr int kCamD4 = 4;
constexpr int kCamD5 = 17;
constexpr int kCamD6 = 11;
constexpr int kCamD7 = 13;

constexpr uint8_t kFlagRightValid = 1u << 0;
constexpr uint8_t kFlagLeftValid = 1u << 1;
constexpr uint8_t kFlagRightInRange = 1u << 2;
constexpr uint8_t kFlagLeftInRange = 1u << 3;
constexpr uint8_t kFlagZeroReady = 1u << 4;

bool xInRange(float x, bool right) {
  return right ? (x >= kRightMinCalXPx && x <= kRightMaxCalXPx)
               : (x >= kLeftMinCalXPx && x <= kLeftMaxCalXPx);
}

}  // namespace

bool FootAngleObserver::begin() {
  if (!psramFound()) {
    last_error_ = "foot_psram_not_found";
    return false;
  }

  run_log_ = static_cast<FootLogRow*>(
      ps_malloc(sizeof(FootLogRow) * kRunLogCapacity));
  if (!run_log_) {
    last_error_ = "foot_log_psram_allocation_failed";
    return false;
  }

  if (!initCamera()) return false;

  BaseType_t ok = xTaskCreatePinnedToCore(
      taskEntry, "foot_angle", kTaskStackBytes, this, kTaskPriority,
      &task_handle_, kTaskCore);
  if (ok != pdPASS || !task_handle_) {
    last_error_ = "foot_task_create_failed";
    esp_camera_deinit();
    return false;
  }

  portENTER_CRITICAL(&mux_);
  snapshot_.camera_ok = true;
  snapshot_.task_running = true;
  snapshot_.right_zero_x_px = right_zero_x_px_;
  snapshot_.left_zero_x_px = left_zero_x_px_;
  portEXIT_CRITICAL(&mux_);
  last_error_ = "";
  return true;
}

bool FootAngleObserver::initCamera() {
  pinMode(kCamPowerN, OUTPUT);
  digitalWrite(kCamPowerN, LOW);
  delay(500);

  camera_config_t c = {};
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.pin_xclk = kCamXclk;
  c.pin_sccb_sda = kCamSda;
  c.pin_sccb_scl = kCamScl;
  c.pin_d7 = kCamD7;
  c.pin_d6 = kCamD6;
  c.pin_d5 = kCamD5;
  c.pin_d4 = kCamD4;
  c.pin_d3 = kCamD3;
  c.pin_d2 = kCamD2;
  c.pin_d1 = kCamD1;
  c.pin_d0 = kCamD0;
  c.pin_vsync = kCamVsync;
  c.pin_href = kCamHref;
  c.pin_pclk = kCamPclk;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_GRAYSCALE;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  c.sccb_i2c_port = -1;

  const esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    last_error_ = "esp_camera_init_failed";
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) {
    last_error_ = "camera_sensor_missing";
    esp_camera_deinit();
    return false;
  }
  sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  sensor->set_vflip(sensor, 1);
  sensor->set_hmirror(sensor, 0);
  last_error_ = "";
  return true;
}

void FootAngleObserver::taskEntry(void* arg) {
  static_cast<FootAngleObserver*>(arg)->taskLoop();
}

void FootAngleObserver::taskLoop() {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    processFrame();
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kFramePeriodMs));
  }
}

FootAngleObserver::MarkerObservation FootAngleObserver::detectMarker(
    const uint8_t* gray, bool right) const {
  MarkerObservation out;
  if (!gray) return out;

  const int* marker_rows = right ? kRightMarkerRows : kLeftMarkerRows;
  const int* reference_rows = right ? kRightReferenceRows : kLeftReferenceRows;

  int contrast_scaled[kFrameWidth];
  for (int x = 0; x < kFrameWidth; ++x) {
    int marker_sum = 0;
    int reference_sum = 0;
    for (int i = 0; i < 5; ++i) {
      marker_sum += gray[marker_rows[i] * kFrameWidth + x];
    }
    for (int i = 0; i < 3; ++i) {
      reference_sum += gray[reference_rows[i] * kFrameWidth + x];
    }
    // 3*five-row sum and 5*three-row sum put both groups on the same scale.
    contrast_scaled[x] = 3 * marker_sum - 5 * reference_sum;
  }

  int smoothed_scaled[kFrameWidth] = {};
  int peak_x = kSmoothRadius;
  int peak_value = -32768;
  int rolling = 0;
  for (int x = 0; x < kSmoothWindow; ++x) rolling += contrast_scaled[x];

  for (int x = kSmoothRadius; x < kFrameWidth - kSmoothRadius; ++x) {
    if (x > kSmoothRadius) {
      rolling -= contrast_scaled[x - kSmoothRadius - 1];
      rolling += contrast_scaled[x + kSmoothRadius];
    }
    const int value = rolling / kSmoothWindow;
    smoothed_scaled[x] = value;
    if (value > peak_value) {
      peak_value = value;
      peak_x = x;
    }
  }

  const int lo = max(0, peak_x - kCentroidHalfWindowPx);
  const int hi = min(kFrameWidth - 1, peak_x + kCentroidHalfWindowPx);
  int64_t weight_sum = 0;
  int64_t weighted_x_sum = 0;
  int bright_left = -1;
  int bright_right = -1;

  for (int x = lo; x <= hi; ++x) {
    const int weight = smoothed_scaled[x] - kCentroidBaselineScaled;
    if (weight <= 0) continue;
    weight_sum += weight;
    weighted_x_sum += static_cast<int64_t>(x) * weight;
    if (bright_left < 0) bright_left = x;
    bright_right = x;
  }

  out.peak_contrast = static_cast<float>(peak_value) / kContrastScale;
  out.bright_width_px = bright_left >= 0
      ? static_cast<uint16_t>(bright_right - bright_left + 1)
      : 0;

  out.valid =
      peak_value >= kPeakThresholdScaled && weight_sum >= kMinWeightScaled;
  if (out.valid) {
    out.center_x_px =
        static_cast<float>(weighted_x_sum) / static_cast<float>(weight_sum);
  }
  return out;
}

void FootAngleObserver::processFrame() {
  const uint32_t t0 = micros();
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    portENTER_CRITICAL(&mux_);
    ++snapshot_.camera_failures;
    snapshot_.camera_ok = false;
    portEXIT_CRITICAL(&mux_);
    last_error_ = "esp_camera_fb_get_failed";
    return;
  }

  const bool frame_ok =
      fb->buf && fb->width == kFrameWidth && fb->height == kFrameHeight &&
      fb->len >= static_cast<size_t>(kFrameWidth * kFrameHeight);
  if (!frame_ok) {
    esp_camera_fb_return(fb);
    portENTER_CRITICAL(&mux_);
    ++snapshot_.camera_failures;
    snapshot_.camera_ok = false;
    portEXIT_CRITICAL(&mux_);
    last_error_ = "unexpected_camera_frame";
    return;
  }

  const uint32_t frame_time_us = micros();
  const MarkerObservation right = detectMarker(fb->buf, true);
  const MarkerObservation left = detectMarker(fb->buf, false);
  const uint32_t vision_us = static_cast<uint32_t>(micros() - t0);
  esp_camera_fb_return(fb);

  float right_zero = kRightNominalZeroXPx;
  float left_zero = kLeftNominalZeroXPx;
  bool zero_ready = false;
  bool run_logging = false;
  uint32_t run_start_us = 0;

  portENTER_CRITICAL(&mux_);
  if (zero_collecting_ && !zero_ready_ && right.valid && left.valid) {
    zero_sum_right_ += right.center_x_px;
    zero_sum_left_ += left.center_x_px;
    ++zero_sample_count_;
  }
  right_zero = right_zero_x_px_;
  left_zero = left_zero_x_px_;
  zero_ready = zero_ready_;
  run_logging = run_logging_;
  run_start_us = rwlog_run_start_us_;
  portEXIT_CRITICAL(&mux_);

  const bool right_range = right.valid && xInRange(right.center_x_px, true);
  const bool left_range = left.valid && xInRange(left.center_x_px, false);
  const float right_angle = right.valid
      ? kRightDegPerPx * (right_zero - right.center_x_px) : NAN;
  const float left_angle = left.valid
      ? kLeftDegPerPx * (left_zero - left.center_x_px) : NAN;

  portENTER_CRITICAL(&mux_);
  snapshot_.camera_ok = true;
  snapshot_.frame_time_us = frame_time_us;
  ++snapshot_.frame_count;
  snapshot_.vision_us = vision_us;
  if (vision_us > snapshot_.max_vision_us) snapshot_.max_vision_us = vision_us;
  snapshot_.right_valid = right.valid;
  snapshot_.left_valid = left.valid;
  snapshot_.right_in_range = right_range;
  snapshot_.left_in_range = left_range;
  snapshot_.right_cx_px = right.center_x_px;
  snapshot_.left_cx_px = left.center_x_px;
  snapshot_.right_angle_deg = zero_ready ? right_angle : NAN;
  snapshot_.left_angle_deg = zero_ready ? left_angle : NAN;
  snapshot_.right_zero_x_px = right_zero_x_px_;
  snapshot_.left_zero_x_px = left_zero_x_px_;
  snapshot_.right_peak_contrast = right.peak_contrast;
  snapshot_.left_peak_contrast = left.peak_contrast;
  snapshot_.right_bright_width_px = right.bright_width_px;
  snapshot_.left_bright_width_px = left.bright_width_px;
  snapshot_.zero_collecting = zero_collecting_;
  snapshot_.zero_ready = zero_ready_;
  snapshot_.zero_samples = zero_sample_count_;
  if (!right.valid) ++snapshot_.right_detect_failures;
  if (!left.valid) ++snapshot_.left_detect_failures;

  if (run_logging && run_log_) {
    if (run_log_count_ < kRunLogCapacity) {
      FootLogRow& row = run_log_[run_log_count_++];
      row.time_us = static_cast<uint32_t>(frame_time_us - run_start_us);
      row.frame_index = snapshot_.frame_count;
      row.right_angle_deg = zero_ready ? right_angle : NAN;
      row.left_angle_deg = zero_ready ? left_angle : NAN;
      row.right_cx_px = right.center_x_px;
      row.left_cx_px = left.center_x_px;
      row.right_peak_contrast = right.peak_contrast;
      row.left_peak_contrast = left.peak_contrast;
      row.right_bright_width_px = right.bright_width_px;
      row.left_bright_width_px = left.bright_width_px;
      row.vision_us = vision_us;
      row.flags = (right.valid ? kFlagRightValid : 0) |
                  (left.valid ? kFlagLeftValid : 0) |
                  (right_range ? kFlagRightInRange : 0) |
                  (left_range ? kFlagLeftInRange : 0) |
                  (zero_ready ? kFlagZeroReady : 0);
    } else {
      run_log_overflow_ = true;
    }
  }
  snapshot_.run_logging = run_logging_;
  snapshot_.run_log_count = run_log_count_;
  snapshot_.run_log_overflow = run_log_overflow_;
  snapshot_.run_id = run_id_;
  portEXIT_CRITICAL(&mux_);
  last_error_ = "";
}

void FootAngleObserver::startZeroCollection() {
  portENTER_CRITICAL(&mux_);
  if (!zero_ready_) {
    zero_collecting_ = true;
    zero_sum_right_ = 0.0f;
    zero_sum_left_ = 0.0f;
    zero_sample_count_ = 0;
    snapshot_.zero_collecting = true;
    snapshot_.zero_samples = 0;
  }
  portEXIT_CRITICAL(&mux_);
}

void FootAngleObserver::cancelZeroCollection() {
  portENTER_CRITICAL(&mux_);
  if (!zero_ready_) {
    zero_collecting_ = false;
    zero_sum_right_ = 0.0f;
    zero_sum_left_ = 0.0f;
    zero_sample_count_ = 0;
    snapshot_.zero_collecting = false;
    snapshot_.zero_samples = 0;
  }
  portEXIT_CRITICAL(&mux_);
}

bool FootAngleObserver::lockZero() {
  bool ready = false;
  portENTER_CRITICAL(&mux_);
  if (zero_ready_) {
    ready = true;
  } else if (zero_collecting_ && zero_sample_count_ >= kZeroMinimumSamples) {
    const float n = static_cast<float>(zero_sample_count_);
    right_zero_x_px_ = zero_sum_right_ / n;
    left_zero_x_px_ = zero_sum_left_ / n;
    zero_ready_ = true;
    zero_collecting_ = false;
    snapshot_.zero_ready = true;
    snapshot_.zero_collecting = false;
    snapshot_.right_zero_x_px = right_zero_x_px_;
    snapshot_.left_zero_x_px = left_zero_x_px_;
    ready = true;
  }
  portEXIT_CRITICAL(&mux_);
  return ready;
}

bool FootAngleObserver::zeroReady() const {
  portENTER_CRITICAL(&mux_);
  const bool ready = zero_ready_;
  portEXIT_CRITICAL(&mux_);
  return ready;
}

void FootAngleObserver::beginRunLog(
    uint16_t run_id, uint64_t rwlog_run_start_us) {
  portENTER_CRITICAL(&mux_);
  run_id_ = run_id;
  rwlog_run_start_us_ = static_cast<uint32_t>(rwlog_run_start_us);
  run_log_count_ = 0;
  run_log_overflow_ = false;
  run_logging_ = zero_ready_ && run_log_ != nullptr;
  snapshot_.run_id = run_id_;
  snapshot_.run_log_count = 0;
  snapshot_.run_log_overflow = false;
  snapshot_.run_logging = run_logging_;
  portEXIT_CRITICAL(&mux_);
}

void FootAngleObserver::endRunLog() {
  portENTER_CRITICAL(&mux_);
  run_logging_ = false;
  snapshot_.run_logging = false;
  snapshot_.run_log_count = run_log_count_;
  snapshot_.run_log_overflow = run_log_overflow_;
  portEXIT_CRITICAL(&mux_);
}

void FootAngleObserver::clearRunLog() {
  portENTER_CRITICAL(&mux_);
  if (!run_logging_) {
    run_id_ = 0;
    rwlog_run_start_us_ = 0;
    run_log_count_ = 0;
    run_log_overflow_ = false;
    snapshot_.run_id = 0;
    snapshot_.run_log_count = 0;
    snapshot_.run_log_overflow = false;
  }
  portEXIT_CRITICAL(&mux_);
}

bool FootAngleObserver::runLogDownloadable() const {
  portENTER_CRITICAL(&mux_);
  const bool ok = !run_logging_ && run_log_ && run_log_count_ > 0;
  portEXIT_CRITICAL(&mux_);
  return ok;
}

void FootAngleObserver::downloadFilename(char* out, size_t out_len) const {
  if (!out || out_len == 0) return;
  portENTER_CRITICAL(&mux_);
  const uint16_t run_id = run_id_;
  portEXIT_CRITICAL(&mux_);
  snprintf(out, out_len, "footlog_run%04u.csv", static_cast<unsigned>(run_id));
}

bool FootAngleObserver::streamCsv(WebServer& server) {
  uint16_t count = 0;
  uint16_t run_id = 0;
  float right_zero = NAN;
  float left_zero = NAN;
  bool overflow = false;

  portENTER_CRITICAL(&mux_);
  const bool available = !run_logging_ && run_log_ && run_log_count_ > 0;
  if (available) {
    count = run_log_count_;
    run_id = run_id_;
    right_zero = right_zero_x_px_;
    left_zero = left_zero_x_px_;
    overflow = run_log_overflow_;
  }
  portEXIT_CRITICAL(&mux_);

  if (!available) {
    server.send(409, "text/plain", "footlog_not_ready");
    return false;
  }

  char filename[48];
  downloadFilename(filename, sizeof(filename));
  server.sendHeader(
      "Content-Disposition",
      String("attachment; filename=\"") + filename + "\"");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");

  String chunk;
  chunk.reserve(4096);
  chunk +=
      "run_id,time_us,frame_index,right_foot_angle_deg,left_foot_angle_deg,"
      "right_valid,left_valid,right_in_range,left_in_range,zero_ready,"
      "right_cx_px,left_cx_px,right_zero_x_px,left_zero_x_px,"
      "right_peak_contrast,left_peak_contrast,right_bright_width_px,"
      "left_bright_width_px,vision_us,log_overflow\n";

  char line[384];
  for (uint16_t i = 0; i < count; ++i) {
    FootLogRow row;
    portENTER_CRITICAL(&mux_);
    row = run_log_[i];
    portEXIT_CRITICAL(&mux_);

    const bool rv = row.flags & kFlagRightValid;
    const bool lv = row.flags & kFlagLeftValid;
    const bool rr = row.flags & kFlagRightInRange;
    const bool lr = row.flags & kFlagLeftInRange;
    const bool zr = row.flags & kFlagZeroReady;

    snprintf(
        line, sizeof(line),
        "%u,%lu,%lu,%.6f,%.6f,%u,%u,%u,%u,%u,%.6f,%.6f,%.6f,%.6f,"
        "%.3f,%.3f,%u,%u,%lu,%u\n",
        static_cast<unsigned>(run_id),
        static_cast<unsigned long>(row.time_us),
        static_cast<unsigned long>(row.frame_index),
        row.right_angle_deg, row.left_angle_deg,
        rv ? 1u : 0u, lv ? 1u : 0u, rr ? 1u : 0u, lr ? 1u : 0u,
        zr ? 1u : 0u,
        row.right_cx_px, row.left_cx_px, right_zero, left_zero,
        row.right_peak_contrast, row.left_peak_contrast,
        static_cast<unsigned>(row.right_bright_width_px),
        static_cast<unsigned>(row.left_bright_width_px),
        static_cast<unsigned long>(row.vision_us),
        overflow ? 1u : 0u);

    if (chunk.length() + strlen(line) > 3500) {
      server.sendContent(chunk);
      chunk = "";
    }
    chunk += line;
  }
  if (chunk.length()) server.sendContent(chunk);
  server.sendContent("");
  return true;
}

FootAngleSnapshot FootAngleObserver::snapshot() const {
  FootAngleSnapshot out;
  portENTER_CRITICAL(&mux_);
  out = snapshot_;
  portEXIT_CRITICAL(&mux_);
  return out;
}

const char* FootAngleObserver::lastError() const {
  return last_error_;
}
