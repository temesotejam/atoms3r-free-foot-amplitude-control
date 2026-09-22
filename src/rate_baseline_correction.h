#pragma once
#include <cmath>
#include <cstdint>

// V46ai: a single rate-only baseline for every accepted zero-cross decision.
// The signed side selects a fixed formula; no previous peak or run setting is
// an input. Q gains are separate and retain their existing values.
namespace rate_baseline {
constexpr float PLUS_AT_65_DPS = 7.217460941f;
constexpr float PLUS_PER_DPS = 0.286814471f;
constexpr float MINUS_AT_65_DPS = 8.399746959f;
constexpr float MINUS_PER_DPS = 0.130807354f;
enum Reason : uint8_t { APPLIED=0, INVALID_INPUT=4, ZERO_FLOOR=5,
                        NOT_EVALUATED=255 };
struct Result {
  float rate_deg;
  float adjusted_deg;
  Reason reason;
};
inline Result evaluate(float abs_rate_dps, int8_t side) {
  Result r{NAN, NAN, INVALID_INPUT};
  if (!std::isfinite(abs_rate_dps) || abs_rate_dps < 0 ||
      (side != 1 && side != -1)) {
    return r;
  }
  r.rate_deg = side > 0 ? PLUS_AT_65_DPS + PLUS_PER_DPS*(abs_rate_dps-65.0f)
                       : MINUS_AT_65_DPS + MINUS_PER_DPS*(abs_rate_dps-65.0f);
  if (!std::isfinite(r.rate_deg)) return r;
  // A peak amplitude cannot be negative. Keep this floor continuous, expose
  // the raw formula in the log, and never substitute a different predictor.
  r.adjusted_deg=std::fmax(0.0f,r.rate_deg);
  r.reason=r.rate_deg < 0.0f ? ZERO_FLOOR : APPLIED;
  return r;
}
} // namespace rate_baseline
