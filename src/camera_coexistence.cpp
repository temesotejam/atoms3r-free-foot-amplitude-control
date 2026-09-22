#include "camera_coexistence.h"
#include "camera_task_priority_patch.h"

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "soc/gpio_sig_map.h"

// Private esp32-camera primitives intentionally wrapped by OneShotCamera.
// The rest of the application never calls these directly.
extern "C" void cam_stop(void);
extern "C" void cam_start(void);
extern "C" camera_fb_t* cam_take(TickType_t timeout);
extern "C" void cam_give(camera_fb_t* fb);

namespace {

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

constexpr uint32_t kCameraXclkHz = 16000000UL;
constexpr uint32_t kCaptureTimeoutMs = 500;
constexpr uint32_t kXclkWarmupMs = 20;

// The first proof deliberately guarantees a long quiet window. This is not a
// 5 Hz scheduler: the 300 ms starts only AFTER one-shot capture has completed.
constexpr uint32_t kMinimumIdleMs = 300;

constexpr uint32_t kConsumerStackBytes = 4096;
constexpr UBaseType_t kConsumerPriority = 1;
constexpr BaseType_t kConsumerCore = 0;

static_assert(CAM_CLK_IDX == 149, "Unexpected ESP32-S3 CAM clock matrix signal");
static_assert(SIG_GPIO_OUT_IDX == 256, "Unexpected ESP32-S3 GPIO output matrix signal");

}  // namespace

void OneShotCamera::setError(const char* error) {
  if (!error) error = "unknown";
  snprintf(last_error_, sizeof(last_error_), "%s", error);
}

void OneShotCamera::captureMemoryBefore() {
  snapshot_.internal_free_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  snapshot_.internal_largest_before = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  snapshot_.dma_free_before = heap_caps_get_free_size(MALLOC_CAP_DMA);
  snapshot_.psram_free_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void OneShotCamera::captureMemoryAfter() {
  snapshot_.internal_free_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  snapshot_.internal_largest_after = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  snapshot_.dma_free_after = heap_caps_get_free_size(MALLOC_CAP_DMA);
  snapshot_.psram_free_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void OneShotCamera::setXclkEnabled(bool enabled) {
  const gpio_num_t pin = static_cast<gpio_num_t>(PIN_CAM_XCLK);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);

  if (enabled) {
    // ESP32-S3 esp32-camera generates XCLK in LCD_CAM and routes CAM_CLK_IDX
    // through the GPIO matrix. Reconnect that signal only for a capture.
    gpio_matrix_out(PIN_CAM_XCLK, CAM_CLK_IDX, false, false);
  } else {
    // Disconnect CAM_CLK_IDX and drive GPIO21 low. GC0308 remains powered, so
    // its register configuration is retained while PCLK/VSYNC/HREF stop.
    gpio_matrix_out(PIN_CAM_XCLK, SIG_GPIO_OUT_IDX, false, false);
    gpio_set_level(pin, 0);
  }

  portENTER_CRITICAL(&mux_);
  snapshot_.xclk_active = enabled;
  portEXIT_CRITICAL(&mux_);
}

void OneShotCamera::flushQueuedFrames() {
  for (;;) {
    camera_fb_t* stale = cam_take(0);
    if (!stale) break;
    cam_give(stale);
  }
}

bool OneShotCamera::begin() {
  snapshot_ = CameraOneShotSnapshot{};
  snapshot_.xclk_hz = kCameraXclkHz;
  snapshot_.xclk_warmup_ms = kXclkWarmupMs;
  snapshot_.minimum_idle_ms = kMinimumIdleMs;
  snapshot_.consumer_core = kConsumerCore;
  snapshot_.consumer_priority = kConsumerPriority;
  captureMemoryBefore();

  capture_mutex_ = xSemaphoreCreateMutex();
  if (!capture_mutex_) {
    setError("camera_capture_mutex_create_failed");
    captureMemoryAfter();
    return false;
  }

  if (!initCameraOnTemporaryI2c0()) {
    captureMemoryAfter();
    return false;
  }

  snapshot_.camera_driver_active = true;
  snapshot_.sensor_powered = true;
  snapshot_.camera_deinitialized = false;
  snapshot_.one_shot_mode = true;

  const CameraTaskPriorityPatchSnapshot task_patch = cameraTaskPriorityPatchSnapshot();
  snapshot_.cam_task_priority_patch_observed = task_patch.observed;
  snapshot_.cam_task_original_priority = task_patch.original_priority;
  snapshot_.cam_task_effective_priority = task_patch.effective_priority;
  snapshot_.cam_task_core = task_patch.core;

  // esp_camera_init starts continuous capture by default. Convert it to our
  // idle one-shot state immediately: receiver OFF, XCLK OFF, queue empty.
  cam_stop();
  setXclkEnabled(false);
  flushQueuedFrames();

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "camera_oneshot", kConsumerStackBytes, this,
      kConsumerPriority, &task_, kConsumerCore);
  if (created != pdPASS) {
    esp_camera_deinit();
    digitalWrite(PIN_CAM_POWER_N, HIGH);
    snapshot_.camera_driver_active = false;
    snapshot_.sensor_powered = false;
    snapshot_.camera_deinitialized = true;
    setError("camera_oneshot_task_create_failed");
    captureMemoryAfter();
    return false;
  }

  snapshot_.camera_ok = true;
  setError("ok_true_oneshot_idle");
  captureMemoryAfter();
  return true;
}

camera_fb_t* OneShotCamera::acquire(uint32_t timeout_ms) {
  if (!snapshot_.camera_ok || !capture_mutex_) return nullptr;
  if (xSemaphoreTake(capture_mutex_, pdMS_TO_TICKS(timeout_ms + 100)) != pdTRUE) {
    setError("camera_capture_mutex_timeout");
    return nullptr;
  }

  const uint32_t started_us = micros();

  setXclkEnabled(true);
  delay(kXclkWarmupMs);

  portENTER_CRITICAL(&mux_);
  snapshot_.receiver_active = true;
  portEXIT_CRITICAL(&mux_);

  cam_start();
  camera_fb_t* fb = cam_take(pdMS_TO_TICKS(timeout_ms ? timeout_ms : kCaptureTimeoutMs));

  // The one-shot guarantee: receiver and sensor clock are OFF before acquire()
  // returns to the caller, even on failure.
  cam_stop();
  portENTER_CRITICAL(&mux_);
  snapshot_.receiver_active = false;
  portEXIT_CRITICAL(&mux_);
  setXclkEnabled(false);

  const uint32_t elapsed_us = static_cast<uint32_t>(micros() - started_us);

  if (!fb) {
    portENTER_CRITICAL(&mux_);
    ++snapshot_.frame_failures;
    snapshot_.last_capture_us = elapsed_us;
    if (elapsed_us > snapshot_.max_capture_us) snapshot_.max_capture_us = elapsed_us;
    portEXIT_CRITICAL(&mux_);
    setError("camera_oneshot_frame_timeout");
    xSemaphoreGive(capture_mutex_);
    return nullptr;
  }

  // cam_take is the private primitive used by esp_camera_fb_get, so populate
  // the public frame metadata explicitly.
  fb->width = 320;
  fb->height = 240;
  fb->format = PIXFORMAT_GRAYSCALE;

  portENTER_CRITICAL(&mux_);
  snapshot_.first_frame_seen = true;
  ++snapshot_.frame_count;
  snapshot_.last_frame_bytes = fb->len;
  snapshot_.last_width = static_cast<uint16_t>(fb->width);
  snapshot_.last_height = static_cast<uint16_t>(fb->height);
  snapshot_.last_capture_us = elapsed_us;
  if (elapsed_us > snapshot_.max_capture_us) snapshot_.max_capture_us = elapsed_us;
  portEXIT_CRITICAL(&mux_);

  setError("ok_true_oneshot");
  return fb;
}

void OneShotCamera::release(camera_fb_t* fb) {
  if (!capture_mutex_) return;
  if (fb) cam_give(fb);
  xSemaphoreGive(capture_mutex_);
}

bool OneShotCamera::initCameraOnTemporaryI2c0() {
  pinMode(PIN_CAM_POWER_N, OUTPUT);
  digitalWrite(PIN_CAM_POWER_N, LOW);
  delay(500);

  // Camera SCCB borrows I2C0 only before Roller485 starts.
  // BMI270 remains on I2C1 (GPIO45/0) and is never touched.
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

void OneShotCamera::taskEntry(void* arg) {
  static_cast<OneShotCamera*>(arg)->taskLoop();
}

void OneShotCamera::taskLoop() {
  for (;;) {
    camera_fb_t* fb = acquire(kCaptureTimeoutMs);
    if (fb) {
      // Phase 1G only proves true one-shot acquisition. Image analysis
      // will be inserted here after coexistence is confirmed on hardware.
      release(fb);
    }

    // This is a guaranteed quiet interval AFTER capture completion. During the
    // entire delay both receiver_active and xclk_active are false.
    vTaskDelay(pdMS_TO_TICKS(kMinimumIdleMs));
  }
}

CameraOneShotSnapshot OneShotCamera::snapshot() const {
  portENTER_CRITICAL(&mux_);
  const CameraOneShotSnapshot copy = snapshot_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}
