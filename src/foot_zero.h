#pragma once
#include <stdint.h>
#include <math.h>
#include "foot_tracking_config.h"

// No automatic re-zero after lock. Both continuous IMU stability and continuous
// valid images are required; IMU epoch detects movement between two frames.
class FootZero {
 public:
  void observe(uint32_t now_ms, uint32_t epoch, bool stable,
               bool valid, float a, float b) {
    if (ready) return;
    if (!stable || !valid || !isfinite(a) || !isfinite(b)) { reset(); return; }
    if (count && (epoch != epoch_ || static_cast<uint32_t>(now_ms - last_ms_) > 300)) reset();
    if (!count) { first_ms_ = now_ms; epoch_ = epoch; }
    last_ms_ = now_ms; sum_a_ += a; sum_b_ += b; ++count;
    if (count >= appcfg::kAutoZeroMinVisionSamples &&
        static_cast<uint32_t>(now_ms - first_ms_) >= appcfg::kAutoZeroStableMs) {
      a_zero = sum_a_ / count; b_zero = sum_b_ / count; ready = true;
    }
  }
  bool ready = false;
  float a_zero = appcfg::kFootAngleAZeroXPx, b_zero = appcfg::kFootAngleBZeroXPx;
  uint32_t count = 0;
 private:
  void reset() { count = 0; sum_a_ = sum_b_ = 0; }
  uint32_t first_ms_ = 0, last_ms_ = 0, epoch_ = 0;
  double sum_a_ = 0, sum_b_ = 0;
};
