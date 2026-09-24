#pragma once
#include <Arduino.h>
#include "foot_tracking_config.h"

// One description shared by status, frozen images and the RWLOG export.
inline String footCalibrationDiagnosticsJson() {
  String json; json.reserve(850);
  json = "{\"revision\":\"fixed_two_poses_20260924\",\"provisional\":true";
  json += ",\"method\":\"two_noncontact_pose_means\",\"reference\":\"negative_mekf_roll_change\"";
  json += ",\"assumption\":\"feet_ground_fixed_fore_aft_planar_motion\"";
  json += ",\"right_deg_per_px\":" + String(appcfg::kFootAngleADegPerPx, 9);
  json += ",\"left_deg_per_px\":" + String(appcfg::kFootAngleBDegPerPx, 9);
  json += ",\"body_span_deg\":" + String(appcfg::kFootCalibrationBodySpanDeg, 6);
  json += ",\"fit_right_x\":[" + String(appcfg::kFootCalibrationATiltedX, 5) + "," + String(appcfg::kFootCalibrationAUprightX, 5) + "]";
  json += ",\"fit_left_x\":[" + String(appcfg::kFootCalibrationBTiltedX, 5) + "," + String(appcfg::kFootCalibrationBUprightX, 5) + "]";
  json += ",\"zero\":\"independent_per_boot\",\"fit_poses\":2,\"heldout_hand_supported_poses\":2";
  json += ",\"independent_angular_accuracy_validated\":false";
  json += ",\"source_file\":\"freefoot-pose-comparison.json\"";
  json += ",\"source_sha256\":\"f731b0ac1019d3ec2b054df9ccc9cbf91c7c013c39f565851d124ad3409f3676\"}";
  return json;
}
