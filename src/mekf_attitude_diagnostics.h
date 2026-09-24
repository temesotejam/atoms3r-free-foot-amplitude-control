#pragma once
#include <Arduino.h>
#include "mekf6.hpp"

// Copied by the exclusive control owner. HTTP never reads a live filter.
// Euler angles and quaternion describe the same unprojected posterior in the
// MEKF coordinate frame; none of the run/display pitch zeros are subtracted.
struct MekfAttitudeSnapshot {
  bool valid = false;
  uint32_t sample_us = 0;
  float roll_deg = NAN, pitch_deg = NAN, yaw_deg = NAN;
  mekf6::Quaternion quaternion{NAN, NAN, NAN, NAN};
};

inline MekfAttitudeSnapshot captureMekfAttitude(const mekf6::Mekf6& filter,
                                              bool initialized, uint32_t sample_us) {
  MekfAttitudeSnapshot out;
  if (!initialized) return out;
  const auto e = filter.eulerDeg();
  out.quaternion = filter.quaternion();
  out.roll_deg = e.roll; out.pitch_deg = e.pitch; out.yaw_deg = e.yaw;
  out.sample_us = sample_us;
  const auto& q = out.quaternion;
  out.valid = isfinite(e.roll) && isfinite(e.pitch) && isfinite(e.yaw) &&
      isfinite(q.w) && isfinite(q.x) && isfinite(q.y) && isfinite(q.z);
  return out;
}

inline String mekfAttitudeJson(const MekfAttitudeSnapshot& s, uint32_t now_us, bool imu_ok) {
  const auto num = [](float x) { return isfinite(x) ? String(x, 6) : String("null"); };
  const uint32_t age_us = now_us - s.sample_us; // wrap-safe host microsecond clock
  String json; json.reserve(520);
  json = "{\"valid\":" + String(s.valid ? "true" : "false");
  json += ",\"fresh\":" + String(s.valid && imu_ok && age_us < 500000 ? "true" : "false");
  json += ",\"sample_us\":" + String(s.sample_us);
  json += ",\"age_us\":" + (s.valid ? String(age_us) : String("null"));
  json += ",\"estimate\":\"posterior\",\"frame\":\"mekf\",\"euler_order\":\"ZYX\"";
  json += ",\"roll_deg\":" + num(s.roll_deg) + ",\"pitch_deg\":" + num(s.pitch_deg);
  json += ",\"yaw_deg\":" + num(s.yaw_deg);
  json += ",\"quaternion\":{\"w\":" + num(s.quaternion.w) + ",\"x\":" + num(s.quaternion.x);
  json += ",\"y\":" + num(s.quaternion.y) + ",\"z\":" + num(s.quaternion.z) + "}";
  json += ",\"yaw_reference\":\"gyro_integrated_since_filter_initialization\"}";
  return json;
}
