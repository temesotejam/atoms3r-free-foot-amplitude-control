#pragma once
#include <math.h>
#include <stdint.h>
#include "mekf6.hpp"

namespace tilt_stop {
// The mechanical upright axis, expressed in MEKF body coordinates. Project
// estimated world-up onto it. A nonpositive projection means >= 90 degrees
// from upright (forward/backward, also sideways overturning). No Euler angles,
// inverse trig, display zero or extra estimator is needed.
inline const char* reason(const mekf6::Quaternion& q, bool valid, uint32_t age_us,
                          const mekf6::Vec3& upright) {
  const float n = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
  if (!valid || !isfinite(n) || n < 0.9f || n > 1.1f) return "tilt_invalid_mekf";
  if (age_us > 10000) return "tilt_stale_mekf";
  const float gx = 2.0f * (q.x*q.z - q.w*q.y);
  const float gy = 2.0f * (q.y*q.z + q.w*q.x);
  const float gz = q.w*q.w - q.x*q.x - q.y*q.y + q.z*q.z;
  const float up = gx*upright.x + gy*upright.y + gz*upright.z;
  // Floating-point boundary tolerance is < 0.0001 degree at 90 degrees.
  return up <= 1.0e-6f*n ? "body_tilt_90deg" : nullptr;
}
}
