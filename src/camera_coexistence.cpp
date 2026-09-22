#include "camera_coexistence.h"
#include "camera_task_priority_patch.h"

#include <Arduino.h>
#include "driver/i2c.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"

namespace {

// AtomS3R-CAM / GC0308 pins from the independently validated camera tracker.
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

// Important for coexistence:
// On the ESP32-S3 camera driver used by Arduino-ESP32 2.x, 16 MHz XCLK selects
// the PSRAM direct-DMA path. This avoids the high-priority cam_task copying an
// entire QVGA grayscale frame from internal DMA RAM into PSRAM.
constexpr uint32_t kCameraXclkHz = 16000000UL;

// Hold the single framebuffer most of the period. With fb_count=1 this leaves
// no free framebuffer for the internal cam_task, intentionally throttling the
// camera while still proving continuous runtime coexistence.
constexpr uint32_t kFrameHoldMs = 180;
constexpr uint32_t kFrameReleaseMs = 20;
constexpr uint8_t kTargetCaptureHz = 5;

constexpr uint32_t kConsumerStackBytes = 4096;
constexpr UBaseType_t kConsumerPriority = 1;
constexpr BaseType_t kConsumerCore = 0;

}  // namespace

void CameraCoexistenceProbe::setError(const char* error) {
  if (!error) error = "unknown";
  snprintf(last_error_, sizeof(last_error_), "%s", error);
}

void CameraCoexistenceProbe::captureMemoryBefore() {
  snapshot_.internal_free_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  snapshot_.internal_largest_before = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  snapshot_.dma_free_before = heap_caps_get_free_size(MALLOC_CAP_DMA);
  snapshot_.psram_free_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void CameraCoexistenceProbe::captureMemoryAfter() {
  snapshot_.internal_free_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  snapshot_.internal_largest_after = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  snapshot_.dma_free_after = heap_caps_get_free_size(MALLOC_CAP_DMA);
  snapshot_.psram_free_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

bool CameraCoexistenceProbe::begin() {
  snapshot_ = CameraCoexistenceSnapshot{};
  snapshot_.xclk_hz = kCameraXclkHz;
  snapshot_.target_capture_hz = kTargetCaptureHz;
  snapshot_.consumer_core = kConsumerCore;
  snapshot_.consumer_priority = kConsumerPriority;
  captureMemoryBefore();

  if (!initCameraOnTemporaryI2c0()) {
    captureMemoryAfter();
    return false;
  }

  const CameraTaskPriorityPatchSnapshot task_patch = cameraTaskPriorityPatchSnapshot();
  snapshot_.cam_task_priority_patch_observed = task_patch.observed;
  snapshot_.cam_task_original_priority = task_patch.original_priority;
  snapshot_.cam_task_effective_priority = task_patch.effective_priority;
  snapshot_.cam_task_core = task_patch.core;

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "camera_probe", kConsumerStackBytes, this,
      kConsumerPriority, &task_, kConsumerCore);
  if (created != pdPASS) {
    esp_camera_deinit();
    setError("camera_probe_task_create_failed");
    captureMemoryAfter();
    return false;
  }

  portENTER_CRITICAL(&mux_);
  snapshot_.camera_ok = true;
  portEXIT_CRITICAL(&mux_);
  setError("ok");
  captureMemoryAfter();
  return true;
}

bool CameraCoexistenceProbe::initCameraOnTemporaryI2c0() {
  pinMode(PIN_CAM_POWER_N, OUTPUT);
  digitalWrite(PIN_CAM_POWER_N, LOW);
  delay(500);

  // Roller485 owns I2C0 during normal runtime. At this point Roller has not
  // started yet, so Camera SCCB may temporarily own I2C0 on GPIO12/9.
  // Never touch I2C1: the already-running BMI270 reader owns it on GPIO45/0.
  (void)i2c_driver_delete(I2C_NUM_0);

  i2c_config_t sccb = {};
  sccb.mode = I2C_MODE_MASTER;
  sccb.sda_io_num = static_cast<gpio_num_t>(PIN_CAM_SDA);
  sccb.sda_pullup_en = GPIO_PULLUP_ENABLE;
  sccb.scl_io_num = static_cast<gpio_num_t>(PIN_CAM_SCL);
  sccb.scl_pullup_en = GPIO_PULLUP_ENABLE;
  sccb.master.clk_speed = 100000;

  esp_err_t err = i2c_param_config(I2C_NUM_0, &sccb);
  if (err != ESP_OK) {
    setError("camera_sccb_i2c0_param_config_failed");
    return false;
  }

  err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK) {
    setError("camera_sccb_i2c0_driver_install_failed");
    return false;
  }

  camera_config_t c = {};
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.pin_xclk = PIN_CAM_XCLK;
  c.pin_sccb_sda = -1;
  c.pin_sccb_scl = -1;
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
  c.xclk_freq_hz = kCameraXclkHz;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_GRAYSCALE;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  c.sccb_i2c_port = I2C_NUM_0;

  err = esp_camera_init(&c);
  if (err != ESP_OK) {
    (void)i2c_driver_delete(I2C_NUM_0);
    setError("esp_camera_init_failed");
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) {
    esp_camera_deinit();
    (void)i2c_driver_delete(I2C_NUM_0);
    setError("camera_sensor_missing");
    return false;
  }

  // These are the only runtime sensor writes in Phase 1. After this point SCCB
  // is intentionally unavailable because Roller485 needs I2C0.
  sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  sensor->set_vflip(sensor, 1);
  sensor->set_hmirror(sensor, 0);

  err = i2c_driver_delete(I2C_NUM_0);
  if (err != ESP_OK) {
    esp_camera_deinit();
    setError("camera_sccb_i2c0_release_failed");
    return false;
  }

  return true;
}

void CameraCoexistenceProbe::taskEntry(void* arg) {
  static_cast<CameraCoexistenceProbe*>(arg)->taskLoop();
}

void CameraCoexistenceProbe::taskLoop() {
  for (;;) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      portENTER_CRITICAL(&mux_);
      ++snapshot_.frame_failures;
      portEXIT_CRITICAL(&mux_);
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    portENTER_CRITICAL(&mux_);
    snapshot_.first_frame_seen = true;
    ++snapshot_.frame_count;
    snapshot_.last_frame_bytes = fb->len;
    snapshot_.last_width = static_cast<uint16_t>(fb->width);
    snapshot_.last_height = static_cast<uint16_t>(fb->height);
    portEXIT_CRITICAL(&mux_);

    // Deliberately keep the only framebuffer unavailable most of the time.
    // This is a coexistence probe, not yet the 15 Hz foot-angle implementation.
    vTaskDelay(pdMS_TO_TICKS(kFrameHoldMs));
    esp_camera_fb_return(fb);
    vTaskDelay(pdMS_TO_TICKS(kFrameReleaseMs));
  }
}

CameraCoexistenceSnapshot CameraCoexistenceProbe::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const CameraCoexistenceSnapshot copy = snapshot_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}
