#pragma once
#include "camera_coexistence.h"
#include "run_control_worker.h"
#include "psram_string.h"
#include "foot_zero.h"
#include "white_marker_tracker.h"

struct FootFrame {
  uint64_t frame_us = 0, delivered_us = 0;
  int64_t log_time_us = -1, measurement_time_us = -1;
  uint32_t sequence = 0, imu_sample_us = 0, processing_us = 0;
  uint16_t run_id = 0;
  uint8_t state_id = 0, led_state = 0, sync_event_id = 0;
  bool timestamp_valid = false, frame_valid = false, zero_ready = false;
  bool right_valid = false, left_valid = false, right_in_range = false, left_in_range = false;
  float right_x = NAN, left_x = NAN, right_deg = NAN, left_deg = NAN;
  float right_contrast = 0, left_contrast = 0;
};
struct FootSnapshot {
  FootFrame latest;
  bool available = false, zero_ready = false, recording = false, overflow = false;
  uint32_t count = 0, frame_failures = 0, zero_samples = 0;
  float right_zero = appcfg::kFootAngleAZeroXPx, left_zero = appcfg::kFootAngleBZeroXPx;
  float fps = 0;
};
class FootObserver {
 public:
  static constexpr uint32_t kCapacity = 768; // 40 s * 15 Hz, with margin
  bool begin(OneShotCamera& camera, RunControlWorker& control);
  bool readyToStart() const;
  void beginRun(uint16_t id, uint64_t log_epoch_us);
  void finishRun();
  void clearRun();
  FootSnapshot snapshot() const;
  CameraOneShotSnapshot cameraSnapshot() const { return camera_ ? camera_->snapshot() : CameraOneShotSnapshot{}; }
  void appendMetadata(PsramString& json) const; // sealed, export worker only
 private:
  static void entry(void* ptr) { static_cast<FootObserver*>(ptr)->loop(); }
  void loop();
  OneShotCamera* camera_ = nullptr;
  RunControlWorker* control_ = nullptr;
  TaskHandle_t task_ = nullptr;
  FootFrame* frames_ = nullptr;
  FootZero zero_;
  WhiteMarker1DTracker right_{0}, left_{1};
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  FootSnapshot status_;
  uint16_t recording_run_id_ = 0;
  uint64_t log_epoch_us_ = 0;
};
