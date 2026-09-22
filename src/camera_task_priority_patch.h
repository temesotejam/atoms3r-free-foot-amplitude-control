#pragma once

#include <Arduino.h>

struct CameraTaskPriorityPatchSnapshot {
  bool observed = false;
  uint8_t original_priority = 0;
  uint8_t effective_priority = 0;
  int8_t core = -1;
};

CameraTaskPriorityPatchSnapshot cameraTaskPriorityPatchSnapshot();
