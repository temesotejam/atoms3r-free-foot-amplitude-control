#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include "../src/control_work_profile.h"
#include "../src/upright_pose_guide.h"
#include "../src/config.h"

int main(int argc, char** argv) {
  assert(argc == 2);
  using namespace control_work;
  profile.reset();
  host_us = UINT32_MAX - 100;
  {
    Scope work(Stage::LogRow, true, true);
    { Scope encode(Stage::LogEncode, true, true); host_us += 100; }
    { Scope store(Stage::LogStore, true, true); host_us = 150; }
  }
  { Scope work(Stage::Filter, true, false); host_us += 700; }
  { Scope idle(Stage::Filter, false, true); host_us += 900; }
  assert(profile.stages[1][static_cast<uint8_t>(Stage::LogRow)].sum_us == 251);
  assert(profile.stages[1][static_cast<uint8_t>(Stage::LogEncode)].sum_us == 100);
  assert(profile.stages[1][static_cast<uint8_t>(Stage::LogStore)].sum_us == 151);
  assert(profile.stages[0][static_cast<uint8_t>(Stage::Filter)].max_us == 700);
  assert(profile.stages[1][static_cast<uint8_t>(Stage::Filter)].count == 0);
  std::ofstream(argv[1]) << profile.json().c_str();
  profile.reset();
  assert(profile.stages[1][static_cast<uint8_t>(Stage::LogRow)].count == 0);

  UprightPoseGuide::CachedMetrics cached;
  ImuReading r{};
  cached.update(r); // no sample yet; match the old invalid-gravity diagnostic
  assert(cached.direction_error_deg == UprightPoseGuide::directionErrorDeg(r));
  uint32_t seed = 937;
  auto sample = [&]() { seed = seed * 1664525U + 1013904223U; return (static_cast<int32_t>(seed >> 8) - 8388608) / 8388608.0f; };
  for (unsigned i = 0; i < 10000; ++i) {
    r.gyro_sequence = UINT32_MAX - 6000 + i; // include a sequence wrap
    r.gx_dps = sample() * 100; r.gy_dps = sample() * 100; r.gz_dps = sample() * 100;
    if (!(i % 2)) {
      r.accel_sequence = UINT32_MAX - 3000 + i / 2;
      r.ax_g = sample(); r.ay_g = sample(); r.az_g = sample();
      r.acc_norm_g = UprightPoseGuide::accelNormG(r);
      r.pitch_accel_only_deg = Config::PITCH_SIGN * atan2f(-r.ax_g,
          sqrtf(r.ay_g*r.ay_g + r.az_g*r.az_g)) * 57.2957795f;
    }
    cached.update(r);
    assert(cached.accel_norm_g == UprightPoseGuide::accelNormG(r));
    assert(cached.direction_error_deg == UprightPoseGuide::directionErrorDeg(r));
    assert(cached.gyro_norm_dps == UprightPoseGuide::gyroNormDps(r));
    // The runner's physical-roll coordinate is the sign-reversed cached accel
    // angle. Compare with its previous full expression over both hemispheres.
    const float before = atan2f(r.ax_g, sqrtf(r.ay_g*r.ay_g + r.az_g*r.az_g)) * 57.2957795f;
    assert(before == -Config::PITCH_SIGN * r.pitch_accel_only_deg);
    // Repeated queue-empty control steps must retain identical diagnostics.
    cached.update(r);
    assert(cached.direction_error_deg == UprightPoseGuide::directionErrorDeg(r));
  }
  ++r.accel_sequence; r.ax_g = NAN; r.acc_norm_g = NAN; cached.update(r);
  assert(std::isnan(cached.accel_norm_g) && cached.direction_error_deg == 180);
  std::cout << "Control profile cohorts, idle exclusion, clock wrap, reset and cached geometry equivalence PASS\n";
}
