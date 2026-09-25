#pragma once

// Placement only: numerical expressions and optimization policies stay intact.
// The target link audit checks these routines actually land in internal RAM.
// Calls into libm / other code and constant data can still access flash; this
// is not an assertion that the complete path is safe with caches disabled.
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_attr.h>
#define RW_HOT_CODE IRAM_ATTR
#else
#define RW_HOT_CODE
#endif
