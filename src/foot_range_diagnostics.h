#pragma once
#include <Arduino.h>
#include "foot_tracking_config.h"

// One source for API, image diagnostics and RWLOG range metadata.
inline String footRangeDiagnosticsJson() {
  const auto pair = [](float lo, float hi) { return "[" + String(lo, 4) + "," + String(hi, 4) + "]"; };
  String json; json.reserve(440);
  json = "{\"revision\":\"observed_extension_20260924\"";
  json += ",\"basis\":\"original_calibration_plus_observed_marker_positions\"";
  json += ",\"right_support_x\":" + pair(appcfg::kFootAngleAMinCalXPx, appcfg::kFootAngleAMaxCalXPx);
  json += ",\"left_support_x\":" + pair(appcfg::kFootAngleBMinCalXPx, appcfg::kFootAngleBMaxCalXPx);
  json += ",\"original_right_support_x\":" + pair(appcfg::kFootAngleAOriginalMinCalXPx, appcfg::kFootAngleAMaxCalXPx);
  json += ",\"original_left_support_x\":" + pair(appcfg::kFootAngleBOriginalMinCalXPx, appcfg::kFootAngleBMaxCalXPx);
  json += ",\"extension_angle_accuracy_validated\":false}";
  return json;
}
