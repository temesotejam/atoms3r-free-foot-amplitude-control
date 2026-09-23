#include "camera_task_priority_patch.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "config.h"

namespace {

constexpr UBaseType_t kCameraInternalTaskPriority = 3;
static_assert(kCameraInternalTaskPriority < Config::ROLLER_IO_TASK_PRIORITY,
              "Camera internal task must stay below Roller485");

portMUX_TYPE g_patch_mux = portMUX_INITIALIZER_UNLOCKED;
CameraTaskPriorityPatchSnapshot g_patch_snapshot;

}  // namespace

extern "C" BaseType_t __real_xTaskCreatePinnedToCore(
    TaskFunction_t pvTaskCode,
    const char* const pcName,
    const uint32_t usStackDepth,
    void* const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* const pvCreatedTask,
    const BaseType_t xCoreID);

extern "C" BaseType_t __wrap_xTaskCreatePinnedToCore(
    TaskFunction_t pvTaskCode,
    const char* const pcName,
    const uint32_t usStackDepth,
    void* const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t* const pvCreatedTask,
    const BaseType_t xCoreID) {
  UBaseType_t effective_priority = uxPriority;
  BaseType_t effective_core = xCoreID;

  if (pcName && strcmp(pcName, "cam_task") == 0) {
    effective_priority = kCameraInternalTaskPriority;
    effective_core = 0;
    portENTER_CRITICAL(&g_patch_mux);
    g_patch_snapshot.observed = true;
    g_patch_snapshot.original_priority =
        static_cast<uint8_t>(uxPriority > 255 ? 255 : uxPriority);
    g_patch_snapshot.effective_priority =
        static_cast<uint8_t>(effective_priority);
    g_patch_snapshot.core = static_cast<int8_t>(effective_core);
    portEXIT_CRITICAL(&g_patch_mux);
  }

  return __real_xTaskCreatePinnedToCore(
      pvTaskCode, pcName, usStackDepth, pvParameters,
      effective_priority, pvCreatedTask, effective_core);
}

CameraTaskPriorityPatchSnapshot cameraTaskPriorityPatchSnapshot() {
  portENTER_CRITICAL(&g_patch_mux);
  const CameraTaskPriorityPatchSnapshot copy = g_patch_snapshot;
  portEXIT_CRITICAL(&g_patch_mux);
  return copy;
}
