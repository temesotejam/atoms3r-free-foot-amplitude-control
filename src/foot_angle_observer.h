#pragma once

#include <Arduino.h>
#include <WebServer.h>

struct FootAngleSnapshot {
  bool task_running = false;
  bool camera_ok = false;
  bool zero_collecting = false;
  bool zero_ready = false;
  uint32_t zero_samples = 0;

  bool right_valid = false;
  bool left_valid = false;
  bool right_in_range = false;
  bool left_in_range = false;

  float right_angle_deg = NAN;
  float left_angle_deg = NAN;
  float right_cx_px = NAN;
  float left_cx_px = NAN;
  float right_zero_x_px = NAN;
  float left_zero_x_px = NAN;
  float right_peak_contrast = NAN;
  float left_peak_contrast = NAN;
  uint16_t right_bright_width_px = 0;
  uint16_t left_bright_width_px = 0;

  uint32_t frame_time_us = 0;
  uint32_t frame_count = 0;
  uint32_t camera_failures = 0;
  uint32_t right_detect_failures = 0;
  uint32_t left_detect_failures = 0;
  uint32_t vision_us = 0;
  uint32_t max_vision_us = 0;

  bool run_logging = false;
  bool run_log_overflow = false;
  uint16_t run_id = 0;
  uint16_t run_log_count = 0;
};

class FootAngleObserver {
 public:
  static constexpr uint8_t kTaskCore = 0;
  static constexpr UBaseType_t kTaskPriority = 1;
  static constexpr uint32_t kTaskStackBytes = 12288;
  static constexpr uint32_t kFramePeriodMs = 67;  // about 15 fps
  static constexpr uint16_t kRunLogCapacity = 1024;
  static constexpr uint16_t kZeroMinimumSamples = 4;

  // Physical mapping fixed for the current mechanism:
  // upper image lane = RIGHT foot, lower image lane = LEFT foot.
  static constexpr float kRightDegPerPx = 0.167779119f;
  static constexpr float kLeftDegPerPx = 0.162645305f;
  static constexpr float kRightNominalZeroXPx = 169.615317f;
  static constexpr float kLeftNominalZeroXPx = 174.843512f;

  bool begin();

  // These are driven only by the existing IMU-based startup upright detector.
  // Marker position never decides whether the body is upright.
  void startZeroCollection();
  void cancelZeroCollection();
  bool lockZero();
  bool zeroReady() const;

  // Separate observation log. RWLOG layout/transport is intentionally untouched.
  void beginRunLog(uint16_t run_id, uint64_t rwlog_run_start_us);
  void endRunLog();
  void clearRunLog();
  bool runLogDownloadable() const;
  bool streamCsv(WebServer& server);
  void downloadFilename(char* out, size_t out_len) const;

  FootAngleSnapshot snapshot() const;
  const char* lastError() const;

 private:
  struct MarkerObservation {
    bool valid = false;
    float center_x_px = NAN;
    float peak_contrast = NAN;
    uint16_t bright_width_px = 0;
  };

  struct FootLogRow {
    uint32_t time_us = 0;  // same origin as RWLOG time_us
    uint32_t frame_index = 0;
    float right_angle_deg = NAN;
    float left_angle_deg = NAN;
    float right_cx_px = NAN;
    float left_cx_px = NAN;
    float right_peak_contrast = NAN;
    float left_peak_contrast = NAN;
    uint16_t right_bright_width_px = 0;
    uint16_t left_bright_width_px = 0;
    uint32_t vision_us = 0;
    uint8_t flags = 0;
  };

  static void taskEntry(void* arg);
  void taskLoop();
  bool initCamera();
  MarkerObservation detectMarker(const uint8_t* gray, bool right) const;
  void processFrame();

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t task_handle_ = nullptr;
  FootAngleSnapshot snapshot_;

  bool zero_collecting_ = false;
  bool zero_ready_ = false;
  float zero_sum_right_ = 0.0f;
  float zero_sum_left_ = 0.0f;
  uint32_t zero_sample_count_ = 0;
  float right_zero_x_px_ = kRightNominalZeroXPx;
  float left_zero_x_px_ = kLeftNominalZeroXPx;

  bool run_logging_ = false;
  uint16_t run_id_ = 0;
  uint32_t rwlog_run_start_us_ = 0;
  uint16_t run_log_count_ = 0;
  bool run_log_overflow_ = false;
  FootLogRow run_log_[kRunLogCapacity];

  const char* last_error_ = "not_started";
};
