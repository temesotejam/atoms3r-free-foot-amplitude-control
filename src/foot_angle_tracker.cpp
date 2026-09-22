#include "foot_angle_tracker.h"

#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include "esp_camera.h"

namespace {

// Physical mapping frozen for the free-foot phase:
//   upper image lane (Marker A) = RIGHT foot
//   lower image lane (Marker B) = LEFT foot
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 240;

constexpr int kMarkerRowCount = 5;
constexpr int kReferenceRowCount = 3;
constexpr int kRightRows[kMarkerRowCount] = {58, 62, 66, 70, 74};
constexpr int kRightReferenceRows[kReferenceRowCount] = {38, 42, 46};
constexpr int kLeftRows[kMarkerRowCount] = {152, 156, 160, 164, 168};
constexpr int kLeftReferenceRows[kReferenceRowCount] = {182, 186, 190};

constexpr float kPeakMinContrast = 55.0f;
constexpr float kCentroidBaseline = 35.0f;
constexpr float kMinWeightSum = 150.0f;
constexpr int kCentroidHalfWindowPx = 35;
constexpr int kSmoothRadius = 2;
constexpr int kSmoothWindow = 2 * kSmoothRadius + 1;
constexpr int kContrastScale = 15;
constexpr int kPeakThresholdScaled = static_cast<int>(kPeakMinContrast * kContrastScale);
constexpr int kCentroidBaselineScaled = static_cast<int>(kCentroidBaseline * kContrastScale);
constexpr int64_t kMinWeightScaled = static_cast<int64_t>(kMinWeightSum * kContrastScale);

// Calibration v1 slopes from atoms3r-foot-angle-tracker.
// Only the boot-specific zero offset is recalibrated in this firmware.
constexpr float kRightDegPerPx = 0.167779119f;  // Marker A / upper / right foot
constexpr float kLeftDegPerPx = 0.162645305f;   // Marker B / lower / left foot
constexpr float kRightMinCalXPx = 42.0f;
constexpr float kRightMaxCalXPx = 173.0f;
constexpr float kLeftMinCalXPx = 43.5f;
constexpr float kLeftMaxCalXPx = 177.5f;
constexpr uint32_t kMinZeroVisionSamples = 4;

// AtomS3R-CAM / GC0308 pins, copied from the independently validated tracker.
constexpr int PIN_CAM_POWER_N = 18;
constexpr int PIN_CAM_SDA = 12;
constexpr int PIN_CAM_SCL = 9;
constexpr int PIN_CAM_VSYNC = 10;
constexpr int PIN_CAM_HREF = 14;
constexpr int PIN_CAM_XCLK = 21;
constexpr int PIN_CAM_PCLK = 40;
constexpr int PIN_CAM_D0 = 3;
constexpr int PIN_CAM_D1 = 42;
constexpr int PIN_CAM_D2 = 46;
constexpr int PIN_CAM_D3 = 48;
constexpr int PIN_CAM_D4 = 4;
constexpr int PIN_CAM_D5 = 17;
constexpr int PIN_CAM_D6 = 11;
constexpr int PIN_CAM_D7 = 13;

constexpr uint32_t kFramePeriodMs = 66;  // about 15 Hz; observation only.
constexpr uint32_t kTaskStackBytes = 6144;
constexpr UBaseType_t kTaskPriority = 1;
constexpr BaseType_t kTaskCore = 0;

constexpr uint8_t kFlagRightDetected = 1u << 0;
constexpr uint8_t kFlagLeftDetected = 1u << 1;
constexpr uint8_t kFlagRightAngleValid = 1u << 2;
constexpr uint8_t kFlagLeftAngleValid = 1u << 3;
constexpr uint8_t kFlagRightInRange = 1u << 4;
constexpr uint8_t kFlagLeftInRange = 1u << 5;
constexpr uint8_t kFlagZeroReady = 1u << 6;

}  // namespace

bool FootAngleTracker::begin() {
  records_ = static_cast<LogRecord*>(ps_malloc(sizeof(LogRecord) * kMaxLogRecords));
  if (!records_) {
    last_error_ = "foot_log_psram_alloc_failed";
    return false;
  }
  memset(records_, 0, sizeof(LogRecord) * kMaxLogRecords);

  if (!initCamera()) {
    return false;
  }

  snapshot_.camera_ok = true;
  camera_ok_ = true;
  last_error_ = "ok";

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "foot_angle_cam", kTaskStackBytes, this, kTaskPriority, &task_, kTaskCore);
  if (created != pdPASS) {
    camera_ok_ = false;
    snapshot_.camera_ok = false;
    last_error_ = "foot_camera_task_create_failed";
    return false;
  }
  return true;
}

bool FootAngleTracker::initCamera() {
  pinMode(PIN_CAM_POWER_N, OUTPUT);
  digitalWrite(PIN_CAM_POWER_N, LOW);
  delay(500);

  camera_config_t c = {};
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.pin_xclk = PIN_CAM_XCLK;
  c.pin_sccb_sda = PIN_CAM_SDA;
  c.pin_sccb_scl = PIN_CAM_SCL;
  c.pin_d7 = PIN_CAM_D7;
  c.pin_d6 = PIN_CAM_D6;
  c.pin_d5 = PIN_CAM_D5;
  c.pin_d4 = PIN_CAM_D4;
  c.pin_d3 = PIN_CAM_D3;
  c.pin_d2 = PIN_CAM_D2;
  c.pin_d1 = PIN_CAM_D1;
  c.pin_d0 = PIN_CAM_D0;
  c.pin_vsync = PIN_CAM_VSYNC;
  c.pin_href = PIN_CAM_HREF;
  c.pin_pclk = PIN_CAM_PCLK;
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
  sensor_t* s = esp_camera_sensor_get();
  if (!s) {
    last_error_ = "camera_sensor_missing";
    return false;
  }
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
  return true;
}

void FootAngleTracker::taskEntry(void* arg) {
  static_cast<FootAngleTracker*>(arg)->taskLoop();
}

void FootAngleTracker::taskLoop() {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      portENTER_CRITICAL(&mux_);
      ++snapshot_.camera_failures;
      portEXIT_CRITICAL(&mux_);
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kFramePeriodMs));
      continue;
    }

    const uint32_t sample_time_us = micros();
    if (fb->width == kFrameWidth && fb->height == kFrameHeight &&
        fb->format == PIXFORMAT_GRAYSCALE && fb->buf) {
      processFrame(fb->buf, sample_time_us);
    } else {
      portENTER_CRITICAL(&mux_);
      ++snapshot_.camera_failures;
      portEXIT_CRITICAL(&mux_);
    }
    esp_camera_fb_return(fb);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kFramePeriodMs));
  }
}

FootAngleTracker::MarkerObservation FootAngleTracker::detectMarker(
    const uint8_t* gray, bool right_marker) const {
  MarkerObservation out;
  if (!gray) return out;

  const int* marker_rows = right_marker ? kRightRows : kLeftRows;
  const int* reference_rows = right_marker ? kRightReferenceRows : kLeftReferenceRows;

  int contrast_scaled[kFrameWidth];
  for (int x = 0; x < kFrameWidth; ++x) {
    int marker_sum = 0;
    int reference_sum = 0;
    for (int i = 0; i < kMarkerRowCount; ++i) {
      marker_sum += gray[marker_rows[i] * kFrameWidth + x];
    }
    for (int i = 0; i < kReferenceRowCount; ++i) {
      reference_sum += gray[reference_rows[i] * kFrameWidth + x];
    }
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
  out.weight_sum = static_cast<float>(weight_sum) / kContrastScale;
  out.bright_width_px = bright_left >= 0 ? bright_right - bright_left + 1 : 0;
  out.valid = peak_value >= kPeakThresholdScaled && weight_sum >= kMinWeightScaled;
  if (out.valid) {
    out.cx_px = static_cast<float>(weighted_x_sum) / static_cast<float>(weight_sum);
  }
  return out;
}

void FootAngleTracker::processFrame(const uint8_t* gray, uint32_t sample_time_us) {
  const uint32_t t0 = micros();
  const MarkerObservation right = detectMarker(gray, true);
  const MarkerObservation left = detectMarker(gray, false);
  const uint32_t vision_us_raw = static_cast<uint32_t>(micros() - t0);
  const uint16_t vision_us = vision_us_raw > 65535u ? 65535u : static_cast<uint16_t>(vision_us_raw);

  portENTER_CRITICAL(&mux_);

  if (!zero_ready_) {
    if (zero_gate_active_) {
      if (right.valid && left.valid) {
        zero_right_sum_ += right.cx_px;
        zero_left_sum_ += left.cx_px;
        ++zero_samples_;
      }
    } else {
      zero_right_sum_ = 0.0;
      zero_left_sum_ = 0.0;
      zero_samples_ = 0;
    }
  }

  const bool right_angle_valid = zero_ready_ && right.valid;
  const bool left_angle_valid = zero_ready_ && left.valid;
  const float right_angle = right_angle_valid
      ? kRightDegPerPx * (zero_right_x_px_ - right.cx_px) : NAN;
  const float left_angle = left_angle_valid
      ? kLeftDegPerPx * (zero_left_x_px_ - left.cx_px) : NAN;
  const bool right_range = right.valid && right.cx_px >= kRightMinCalXPx && right.cx_px <= kRightMaxCalXPx;
  const bool left_range = left.valid && left.cx_px >= kLeftMinCalXPx && left.cx_px <= kLeftMaxCalXPx;

  snapshot_.camera_ok = camera_ok_;
  snapshot_.zero_ready = zero_ready_;
  snapshot_.collecting_zero = !zero_ready_ && zero_gate_active_;
  snapshot_.right_detected = right.valid;
  snapshot_.left_detected = left.valid;
  snapshot_.right_angle_valid = right_angle_valid;
  snapshot_.left_angle_valid = left_angle_valid;
  snapshot_.right_in_range = right_range;
  snapshot_.left_in_range = left_range;
  snapshot_.right_angle_deg = right_angle;
  snapshot_.left_angle_deg = left_angle;
  snapshot_.right_cx_px = right.cx_px;
  snapshot_.left_cx_px = left.cx_px;
  snapshot_.zero_right_x_px = zero_right_x_px_;
  snapshot_.zero_left_x_px = zero_left_x_px_;
  snapshot_.zero_samples = zero_samples_;
  ++snapshot_.frame_count;
  snapshot_.sample_time_us = sample_time_us;
  snapshot_.vision_us = vision_us;

  if (run_active_ && records_ && record_count_ < kMaxLogRecords) {
    LogRecord& row = records_[record_count_++];
    row.t_run_us = static_cast<uint32_t>(sample_time_us - run_start_us_);
    row.sample_time_us = sample_time_us;
    row.right_angle_cdeg = centi(right_angle);
    row.left_angle_cdeg = centi(left_angle);
    row.right_cx_cpx = centi(right.cx_px);
    row.left_cx_cpx = centi(left.cx_px);
    row.vision_us = vision_us;
    row.flags = 0;
    if (right.valid) row.flags |= kFlagRightDetected;
    if (left.valid) row.flags |= kFlagLeftDetected;
    if (right_angle_valid) row.flags |= kFlagRightAngleValid;
    if (left_angle_valid) row.flags |= kFlagLeftAngleValid;
    if (right_range) row.flags |= kFlagRightInRange;
    if (left_range) row.flags |= kFlagLeftInRange;
    if (zero_ready_) row.flags |= kFlagZeroReady;
    row.frame_count = snapshot_.frame_count;
  }

  portEXIT_CRITICAL(&mux_);
}

void FootAngleTracker::setStartupUprightGate(bool active) {
  portENTER_CRITICAL(&mux_);
  if (!zero_ready_ && !active && zero_gate_active_) {
    zero_right_sum_ = 0.0;
    zero_left_sum_ = 0.0;
    zero_samples_ = 0;
  }
  zero_gate_active_ = active;
  portEXIT_CRITICAL(&mux_);
}

bool FootAngleTracker::lockStartupZero() {
  bool ok = false;
  portENTER_CRITICAL(&mux_);
  if (zero_ready_) {
    ok = true;
  } else if (zero_gate_active_ && zero_samples_ >= kMinZeroVisionSamples) {
    const double n = static_cast<double>(zero_samples_);
    zero_right_x_px_ = static_cast<float>(zero_right_sum_ / n);
    zero_left_x_px_ = static_cast<float>(zero_left_sum_ / n);
    zero_ready_ = true;
    snapshot_.zero_ready = true;
    snapshot_.zero_right_x_px = zero_right_x_px_;
    snapshot_.zero_left_x_px = zero_left_x_px_;
    ok = true;
  }
  portEXIT_CRITICAL(&mux_);
  return ok;
}

bool FootAngleTracker::zeroReady() const {
  portENTER_CRITICAL(&mux_);
  const bool v = zero_ready_;
  portEXIT_CRITICAL(&mux_);
  return v;
}

bool FootAngleTracker::cameraOk() const {
  portENTER_CRITICAL(&mux_);
  const bool v = camera_ok_;
  portEXIT_CRITICAL(&mux_);
  return v;
}

FootAngleSnapshot FootAngleTracker::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const FootAngleSnapshot s = snapshot_;
  portEXIT_CRITICAL(&mux_);
  return s;
}

void FootAngleTracker::beginRun(uint16_t run_id, uint32_t run_start_us) {
  portENTER_CRITICAL(&mux_);
  record_count_ = 0;
  run_id_ = run_id;
  run_start_us_ = run_start_us;
  log_complete_ = false;
  run_active_ = true;
  portEXIT_CRITICAL(&mux_);
}

void FootAngleTracker::endRun() {
  portENTER_CRITICAL(&mux_);
  if (run_active_) {
    run_active_ = false;
    log_complete_ = record_count_ > 0;
  }
  portEXIT_CRITICAL(&mux_);
}

bool FootAngleTracker::runActive() const {
  portENTER_CRITICAL(&mux_);
  const bool v = run_active_;
  portEXIT_CRITICAL(&mux_);
  return v;
}

bool FootAngleTracker::logDownloadable() const {
  portENTER_CRITICAL(&mux_);
  const bool v = log_complete_ && !run_active_ && record_count_ > 0;
  portEXIT_CRITICAL(&mux_);
  return v;
}

void FootAngleTracker::clearFinishedLog() {
  portENTER_CRITICAL(&mux_);
  if (!run_active_) {
    record_count_ = 0;
    log_complete_ = false;
    run_id_ = 0;
    run_start_us_ = 0;
  }
  portEXIT_CRITICAL(&mux_);
}

void FootAngleTracker::downloadFilename(char* out, size_t out_len) const {
  if (!out || out_len == 0) return;
  portENTER_CRITICAL(&mux_);
  const uint16_t run_id = run_id_;
  portEXIT_CRITICAL(&mux_);
  snprintf(out, out_len, "foot-angle-run-%u.csv", static_cast<unsigned>(run_id));
}

int16_t FootAngleTracker::centi(float value) {
  if (!isfinite(value)) return INT16_MIN;
  value *= 100.0f;
  if (value > 32767.0f) return 32767;
  if (value < -32767.0f) return -32767;
  return static_cast<int16_t>(lroundf(value));
}

int FootAngleTracker::formatCsvLine(char* out, size_t out_len, const LogRecord& row) const {
  const auto value = [](int16_t v, char* dst, size_t n) {
    if (v == INT16_MIN) snprintf(dst, n, "");
    else snprintf(dst, n, "%.2f", static_cast<double>(v) / 100.0);
  };
  char ra[20], la[20], rx[20], lx[20];
  value(row.right_angle_cdeg, ra, sizeof(ra));
  value(row.left_angle_cdeg, la, sizeof(la));
  value(row.right_cx_cpx, rx, sizeof(rx));
  value(row.left_cx_cpx, lx, sizeof(lx));
  return snprintf(out, out_len,
      "%lu,%lu,%lu,%s,%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu\n",
      static_cast<unsigned long>(row.t_run_us),
      static_cast<unsigned long>(row.sample_time_us),
      static_cast<unsigned long>(row.frame_count),
      ra, la, rx, lx,
      (row.flags & kFlagRightDetected) ? 1u : 0u,
      (row.flags & kFlagLeftDetected) ? 1u : 0u,
      (row.flags & kFlagRightAngleValid) ? 1u : 0u,
      (row.flags & kFlagLeftAngleValid) ? 1u : 0u,
      (row.flags & kFlagRightInRange) ? 1u : 0u,
      (row.flags & kFlagLeftInRange) ? 1u : 0u,
      (row.flags & kFlagZeroReady) ? 1u : 0u,
      static_cast<unsigned>(row.vision_us),
      static_cast<unsigned>(run_id_),
      0u,
      static_cast<unsigned long>(run_start_us_));
}

bool FootAngleTracker::writeAll(WiFiClient& client, const uint8_t* data, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    const size_t written = client.write(data + offset, len - offset);
    if (written == 0) return false;
    offset += written;
    delay(0);
  }
  return true;
}

bool FootAngleTracker::streamCsv(WebServer& server) {
  uint16_t count = 0;
  portENTER_CRITICAL(&mux_);
  const bool available = log_complete_ && !run_active_ && record_count_ > 0;
  count = record_count_;
  portEXIT_CRITICAL(&mux_);
  if (!available) {
    server.send(404, "text/plain", "foot_angle_log_not_ready");
    return false;
  }

  static const char header[] =
      "t_run_us,sample_time_us,frame,right_foot_angle_deg,left_foot_angle_deg,"
      "right_cx_px,left_cx_px,right_detected,left_detected,right_angle_valid,"
      "left_angle_valid,right_in_range,left_in_range,zero_ready,vision_us,run_id,"
      "reserved,run_start_us\n";

  size_t content_length = sizeof(header) - 1;
  char line[256];
  for (uint16_t i = 0; i < count; ++i) {
    const int n = formatCsvLine(line, sizeof(line), records_[i]);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(line)) {
      server.send(500, "text/plain", "foot_angle_csv_format_error");
      return false;
    }
    content_length += static_cast<size_t>(n);
  }

  char filename[64];
  downloadFilename(filename, sizeof(filename));
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(content_length);
  server.send(200, "text/csv; charset=utf-8", "");

  WiFiClient client = server.client();
  if (!writeAll(client, reinterpret_cast<const uint8_t*>(header), sizeof(header) - 1)) {
    last_error_ = "foot_angle_download_header_failed";
    return false;
  }
  for (uint16_t i = 0; i < count; ++i) {
    const int n = formatCsvLine(line, sizeof(line), records_[i]);
    if (n <= 0 || !writeAll(client, reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n))) {
      last_error_ = "foot_angle_download_body_failed";
      return false;
    }
  }
  return true;
}
