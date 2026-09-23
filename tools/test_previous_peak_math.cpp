
#include <cassert>
#include <cmath>
#include "previous_peak_control_correction.h"
int main() {
  using namespace previous_peak_control;
  auto early = evaluate(7.5f, 8.0f, 1, 8.0f, 9999);
  assert(!early.applied && early.reason == PREV_REASON_BEFORE_ENABLE_TIME && early.corrected_free_peak_deg == 7.5f);
  auto other = evaluate(7.5f, 8.0f, 1, 10.0f, 15000);
  assert(!other.applied && other.reason == PREV_REASON_TARGET_UNSUPPORTED);
  auto outside = evaluate(7.5f, 6.0f, 1, 8.0f, 15000);
  assert(!outside.applied && outside.reason == PREV_REASON_OUTSIDE_SUPPORT);
  auto plus = evaluate(7.5f, 8.0f, 1, 8.0f, 15000);
  assert(plus.applied && std::fabs(plus.applied_correction_deg - 0.591392151f) < 1e-5f);
  auto minus = evaluate(8.5f, 8.0f, -1, 8.0f, 15000);
  assert(minus.applied && std::fabs(minus.applied_correction_deg + 0.157912422f) < 1e-5f);
  auto cap = evaluate(7.0f, 7.19424f, 1, 8.0f, 15000);
  assert(cap.applied && cap.clamped && std::fabs(cap.applied_correction_deg - 0.70f) < 1e-6f);
}
