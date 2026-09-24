#pragma once

#include <stdint.h>

namespace appcfg {

static constexpr int kFrameWidth = 320;
static constexpr int kFrameHeight = 240;
static constexpr int kCameraFpsTarget = 15;

static constexpr int kMarkerAId = 0;
static constexpr int kMarkerBId = 1;

// Sparse-line candidates across the complete bounded vertical search.
// Upper lane = A, lower lane = B.
static constexpr int kWhiteMarkerRowCount = 5;
static constexpr int kWhiteReferenceRowCount = 3;

static constexpr int kWhiteMarkerARows[kWhiteMarkerRowCount] =
    {58, 62, 66, 70, 74};
static constexpr int kWhiteReferenceARows[kWhiteReferenceRowCount] =
    {38, 42, 46};
static constexpr int kWhiteMarkerACenterY = 66;

static constexpr int kWhiteMarkerBRows[kWhiteMarkerRowCount] =
    {152, 156, 160, 164, 168};
static constexpr int kWhiteReferenceBRows[kWhiteReferenceRowCount] =
    {182, 186, 190};
static constexpr int kWhiteMarkerBCenterY = 160;

static constexpr char kWhiteDetectorRevision[] = "sparse_rows_identity_v2";
static constexpr int kWhiteSearchRadiusYPx = 32;
static constexpr int kWhiteSearchStepYPx = 4;
static constexpr int kWhiteMaxTemplates =
    1 + 2 * kWhiteSearchRadiusYPx / kWhiteSearchStepYPx;
static_assert(kWhiteSearchRadiusYPx % kWhiteSearchStepYPx == 0, "Complete vertical search");
static_assert(kWhiteReferenceARows[0] - kWhiteSearchRadiusYPx >= 0, "Upper reference in frame");
static_assert(kWhiteReferenceBRows[kWhiteReferenceRowCount - 1] + kWhiteSearchRadiusYPx < kFrameHeight,
              "Lower reference in frame");
static_assert(kWhiteMarkerARows[kWhiteMarkerRowCount - 1] + kWhiteSearchRadiusYPx <
                  kWhiteMarkerBRows[0] - kWhiteSearchRadiusYPx,
              "Right and left marker search rows must not overlap");

static constexpr float kWhitePeakMinContrast = 55.0f;
static constexpr float kWhiteCentroidBaseline = 35.0f;
static constexpr float kWhiteMinWeightSum = 150.0f;
static constexpr int kWhiteCentroidHalfWindowPx = 35;
static constexpr int kWhiteMinWidthPx = 4;
static constexpr int kWhiteMaxWidthPx = 72;
static constexpr int kWhiteMaxCandidates = 24;
static constexpr float kWhiteAmbiguousRatio = 0.82f;
static constexpr float kWhiteMaxTrackStepXPx = 80.0f;
static constexpr float kWhiteMaxTrackStepYPx = 32.0f;
static constexpr uint32_t kWhiteTrackMemoryUs = 500000;
static constexpr uint8_t kWhiteReacquireFrames = 3;

// Foot-angle calibration v2, measured 2026-09-24 (0.47.7).
// Definition:
//   foot_angle_deg = angle of the rigid foot/leg link relative to the body.
//   Upright initial posture = 0 deg.
//   Positive direction = the direction observed when marker X moves left.
//
// Two noncontact held poses: body roll change -20.276416529 deg,
// A X: 171.64306 -> 40.95084; B X: 169.55292 -> 39.15524 px.
// Slopes = -delta_body_roll / (upright_x - tilted_x), each side separately.
// Provisional MEKF-referenced scale; hand-supported poses were held out.
// These older nominal zeros are only the neutral-image plausibility centers;
// actual zero is still measured independently for each foot on every boot.
static constexpr float kFootAngleAZeroXPx = 169.615317f;
static constexpr float kFootAngleBZeroXPx = 174.843512f;
static constexpr float kFootAngleADegPerPx = 0.155146317f;
static constexpr float kFootAngleBDegPerPx = 0.155496758f;
static constexpr float kFootCalibrationBodySpanDeg = 20.276416529f;
static constexpr float kFootCalibrationAUprightX = 171.64306f;
static constexpr float kFootCalibrationATiltedX = 40.95084f;
static constexpr float kFootCalibrationBUprightX = 169.55292f;
static constexpr float kFootCalibrationBTiltedX = 39.15524f;

// v1 support extended by the 0.47.4 hardware observations, sequences 694/781:
// right X=41.7336, left X=40.3504. Floor the observed minimum minus 1 px.
// Accepted pixel bounds are distinct from the v2 fit anchors above.
// Keep the original support explicit; extrapolation accuracy is not validated.
static constexpr float kFootAngleAOriginalMinCalXPx = 42.0f;
static constexpr float kFootAngleBOriginalMinCalXPx = 43.5f;
static constexpr float kFootAngleAMinCalXPx = 40.0f;
static constexpr float kFootAngleAMaxCalXPx = 173.0f;
static constexpr float kFootAngleBMinCalXPx = 39.0f;
static constexpr float kFootAngleBMaxCalXPx = 177.5f;

// IMU diagnostics retained to validate body motion independently.
static constexpr float kBodyTiltComplementaryTauS = 0.50f;
static constexpr float kTiltStaticMaxGyroDps = 3.0f;
static constexpr float kTiltStaticAccelNormToleranceG = 0.05f;

// Automatic upright zeroing.
//
// On the current AtomS3R-CAM mount, gravity points approximately along IMU -Z
// when the body is upright. Upright recognition is intentionally based only
// on IMU gravity direction + stability; marker X is never used to decide
// whether the body is upright.
//
// IMU upright does not establish foot neutrality or marker identity. The
// measured neutral-image neighborhood is a plausibility gate, not the zero
// value: accepted positions are still averaged independently on every boot.
static constexpr float kAutoZeroMaxNominalOffsetXPx = 35.0f;
static constexpr float kAutoZeroMaxSpreadXPx = 4.0f;
// All conditions must remain continuously true before the reference locks.
static constexpr float kAutoZeroMaxUprightErrorDeg = 5.0f;
static constexpr float kAutoZeroMaxGyroDps = 1.5f;
static constexpr float kAutoZeroAccelNormToleranceG = 0.03f;
static constexpr uint32_t kAutoZeroStableMs = 2000;
static constexpr uint32_t kAutoZeroMinVisionSamples = 15;

static constexpr uint32_t kImuControlPeriodUs = 5000; // 200 Hz
static constexpr uint32_t kTelemetryPeriodMs = 100;   // 10 Hz JSON

} // namespace appcfg
