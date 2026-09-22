#pragma once

#include <Arduino.h>
#include <math.h>

#include "imu_manager.h"

namespace UprightPoseGuide {

// Measured from the stationary START_SYNC portion of
// 20260913_122233_294_501c (V46 hardware run).
// Mean raw acceleration was approximately (+0.0218,+0.0338,-1.0066) g.
// The normalized vector below is used only as an upright-direction reference.
static constexpr float REF_AX = 0.021626f;
static constexpr float REF_AY = 0.033568f;
static constexpr float REF_AZ = -0.999202f;

// User workflow: power up while the mechanism is safely lying down. Ten seconds
// after boot, illuminate the sync LED to say "now stand the mechanism upright".
static constexpr uint32_t GUIDE_LED_ON_AFTER_BOOT_MS = 10000UL;
static constexpr uint32_t UPRIGHT_STABLE_HOLD_MS = 400UL;

// Upright acceptance is intentionally based on gravity direction, not Euler
// angle. Norm and gyro gates keep the LED from turning off while the mechanism
// is being moved through the target direction.
static constexpr float UPRIGHT_MAX_DIRECTION_ERROR_DEG = 8.0f;
static constexpr float UPRIGHT_MIN_ACCEL_NORM_G = 0.85f;
static constexpr float UPRIGHT_MAX_ACCEL_NORM_G = 1.15f;
static constexpr float UPRIGHT_MAX_GYRO_NORM_DPS = 5.0f;

// Autonomous V7 re-initializes MEKF during the first OFF segment of the
// unchanged START synchronization pattern (the first segment is 1000 ms).
static constexpr uint32_t MEKF_REINIT_AVERAGE_MS = 800UL;
static constexpr uint32_t MEKF_REINIT_MIN_SAMPLES = 60UL;

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float accelNormG(const ImuReading& r) {
  return sqrtf(r.ax_g * r.ax_g + r.ay_g * r.ay_g + r.az_g * r.az_g);
}

inline float gyroNormDps(const ImuReading& r) {
  return sqrtf(r.gx_dps * r.gx_dps + r.gy_dps * r.gy_dps + r.gz_dps * r.gz_dps);
}

inline float directionErrorDeg(const ImuReading& r) {
  const float n = accelNormG(r);
  if (!isfinite(n) || n < 0.2f) return 180.0f;
  const float dot = clampf((r.ax_g * REF_AX + r.ay_g * REF_AY + r.az_g * REF_AZ) / n,
                           -1.0f, 1.0f);
  return acosf(dot) * 57.29577951308232f;
}

inline bool isUprightStableSample(const ImuReading& r) {
  const float a_norm = accelNormG(r);
  if (!isfinite(a_norm) || a_norm < UPRIGHT_MIN_ACCEL_NORM_G ||
      a_norm > UPRIGHT_MAX_ACCEL_NORM_G) {
    return false;
  }
  if (directionErrorDeg(r) > UPRIGHT_MAX_DIRECTION_ERROR_DEG) return false;
  return gyroNormDps(r) <= UPRIGHT_MAX_GYRO_NORM_DPS;
}

}  // namespace UprightPoseGuide
