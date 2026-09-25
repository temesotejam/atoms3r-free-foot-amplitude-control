#pragma once
#include <cmath>
#include <cstdint>

namespace log_quantization {
constexpr int16_t kMissing = -32768;

// RWLOG's existing saturated int16 encoding, including ties away from zero.
// The bounded conversion uses the FPU's truncation/conversion instructions
// instead of a libm lroundf call for every logged field. Adding +/-0.5 first
// is NOT equivalent immediately below a half-integer, so compare the exact
// fractional remainder after truncation instead.
#if defined(__GNUC__)
__attribute__((always_inline))
#endif
inline int16_t scaledI16(float value, float scale) {
  if (!std::isfinite(value)) return kMissing;
  value *= scale;
  if (value > 32767.0f) return 32767;
  if (value < -32767.0f) return -32767;
  int32_t integer = static_cast<int32_t>(value);
  const float fraction = value - static_cast<float>(integer);
  if (fraction >= 0.5f) ++integer;
  else if (fraction <= -0.5f) --integer;
  return static_cast<int16_t>(integer);
}
}  // namespace log_quantization
