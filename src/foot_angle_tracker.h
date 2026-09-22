#pragma once

#include <Arduino.h>
#include <WebServer.h>

struct FootAngleSnapshot {
  bool camera_ok = false;
  bool zero_ready = false;
  bool collecting_zero = false;
  bool right_detected = false;
  bool left_detected = false;
  bool right_angle_valid = false;
  bool left_angle_valid = false;
  bool right_in_range = false;
  bool left_in_range = false;
  float right_angle_deg = NAN;
  float left_angle_deg = NAN;
  float right_cx_px = NAN;
  float left_cx_px = NAN;
  float zero_right_x_px = NAN;
  float zero_left_x_px = NAN;
  uint32_t zero_samples = 0;
  uint32_t frame_count = 0;
  uint32_t camera_failures = 0;
  uint32_t sample_time_us = 0;
  uint32_t vision_us = 0;
};

class FootAngleTracker {
public:
  static constexpr uint16_t kMaxLogRecords = 1024;

  bool begin();
  void setStartupUprightGate(bool active);
  bool lockStartupZero();
  bool zeroReady() const;
  bool cameraOk() const;
  FootAngleSnapshot snapshot() const;

  void beginRun(uint16_t run_id, uint32_t run_start_us);
  void endRun();
  bool runActive() const;
  bool logDownloadable() const;
  void clearFinishedLog();
  void downloadFilename(char* out, size_t out_len) const;
  bool streamCsv(WebServer& server);
  const char* lastError() const { return last_error_; }

private:
  struct MarkerObservation {
    bool valid = false;
    float cx_px = NAN;
    float peak_contrast = 0.0f;
    float weight_sum = 0.0f;
    int bright_width_px = 0;
  };

#pragma pack(push, 1)
  struct LogRecord {
    uint32_t t_run_us = 0;
    uint32_t sample_time_us = 0;
    int16_t right_angle_cdeg = INT16_MIN;
    int16_t left_angle_cdeg = INT16_MIN;
    int16_t right_cx_cpx = INT16_MIN;
    int16_t left_cx_cpx = INT16_MIN;
    uint16_t vision_us = 0;
    uint8_t flags = 0;
    uint8_t reserved = 0;
    uint32_t frame_count = 0;
  };
#pragma pack(pop)

  static void taskEntry(void* arg);
  void taskLoop();
  bool initCamera();
  MarkerObservation detectMarker(const uint8_t* gray, bool right_marker) const;
  void processFrame(const uint8_t* gray, uint32_t sample_time_us);
  static int16_t centi(float value);
  static bool writeAll(WiFiClient& client, const uint8_t* data, size_t len);
  int formatCsvLine(char* out, size_t out_len, const LogRecord& row) const;

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t task_ = nullptr;
  LogRecord* records_ = nullptr;
  uint16_t record_count_ = 0;
  uint16_t run_id_ = 0;
  uint32_t run_start_us_ = 0;
  bool run_active_ = false;
  bool log_complete_ = false;

  bool camera_ok_ = false;
  bool zero_ready_ = false;
  bool zero_gate_active_ = false;
  float zero_right_x_px_ = 169.615317f;
  float zero_left_x_px_ = 174.843512f;
  double zero_right_sum_ = 0.0;
  double zero_left_sum_ = 0.0;
  uint32_t zero_samples_ = 0;

  FootAngleSnapshot snapshot_;
  const char* last_error_ = "not_initialized";
};
