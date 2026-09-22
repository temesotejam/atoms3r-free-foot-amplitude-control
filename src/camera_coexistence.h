#pragma once

#include <Arduino.h>

struct CameraCoexistenceSnapshot {
  bool camera_ok = false;
  bool first_frame_seen = false;
  uint32_t frame_count = 0;
  uint32_t frame_failures = 0;
  uint32_t last_frame_bytes = 0;
  uint16_t last_width = 0;
  uint16_t last_height = 0;

  uint32_t xclk_hz = 16000000UL;
  uint8_t target_capture_hz = 5;
  int8_t consumer_core = 0;
  uint8_t consumer_priority = 1;

  bool cam_task_priority_patch_observed = false;
  uint8_t cam_task_original_priority = 0;
  uint8_t cam_task_effective_priority = 0;
  int8_t cam_task_core = -1;

  uint32_t internal_free_before = 0;
  uint32_t internal_free_after = 0;
  uint32_t internal_largest_before = 0;
  uint32_t internal_largest_after = 0;
  uint32_t dma_free_before = 0;
  uint32_t dma_free_after = 0;
  uint32_t psram_free_before = 0;
  uint32_t psram_free_after = 0;
};

class CameraCoexistenceProbe {
 public:
  bool begin();
  CameraCoexistenceSnapshot snapshot() const;
  const char* lastError() const { return last_error_; }

 private:
  static void taskEntry(void* arg);
  void taskLoop();
  bool initCameraOnTemporaryI2c0();
  void captureMemoryBefore();
  void captureMemoryAfter();
  void setError(const char* error);

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t task_ = nullptr;
  CameraCoexistenceSnapshot snapshot_;
  char last_error_[64] = "not_initialized";
};
