#include "white_marker_tracker.h"

#include <Arduino.h>
#include "foot_tracking_config.h"

namespace {

constexpr int kWidth = appcfg::kFrameWidth;
constexpr int kSmoothRadius = 2;
constexpr int kSmoothWindow = 2 * kSmoothRadius + 1;

constexpr int kContrastScale = 15;
static_assert(appcfg::kWhiteMarkerRowCount == 5 && appcfg::kWhiteReferenceRowCount == 3,
              "Integer contrast scale assumes five marker and three reference rows");
constexpr int kPeakThresholdScaled =
    static_cast<int>(appcfg::kWhitePeakMinContrast * kContrastScale);
constexpr int kCentroidBaselineScaled =
    static_cast<int>(appcfg::kWhiteCentroidBaseline * kContrastScale);
constexpr int64_t kMinWeightScaled =
    static_cast<int64_t>(appcfg::kWhiteMinWeightSum * kContrastScale);

} // namespace

const char* markerDetectionReasonName(MarkerDetectionReason reason) {
    switch (reason) {
        case MarkerDetectionReason::Detected: return "detected";
        case MarkerDetectionReason::NoFrame: return "no_frame";
        case MarkerDetectionReason::LowContrast: return "low_contrast";
        case MarkerDetectionReason::LowWeight: return "low_weight";
    }
    return "unknown";
}

WhiteMarker1DTracker::WhiteMarker1DTracker(int marker_id)
    : _id(marker_id) {}

const int* WhiteMarker1DTracker::markerRows() const {
    return _id == appcfg::kMarkerAId
        ? appcfg::kWhiteMarkerARows
        : appcfg::kWhiteMarkerBRows;
}

const int* WhiteMarker1DTracker::referenceRows() const {
    return _id == appcfg::kMarkerAId
        ? appcfg::kWhiteReferenceARows
        : appcfg::kWhiteReferenceBRows;
}

int WhiteMarker1DTracker::nominalCenterY() const {
    return _id == appcfg::kMarkerAId
        ? appcfg::kWhiteMarkerACenterY
        : appcfg::kWhiteMarkerBCenterY;
}

WhiteMarkerObservation WhiteMarker1DTracker::process(const uint8_t* gray) {
    const uint32_t t0 = micros();
    WhiteMarkerObservation out;
    out.id = _id;
    out.center_y_px = static_cast<float>(nominalCenterY());
    uint8_t tested = 0;
    if (gray) {
        // Preserve the calibrated nominal-row result whenever it is valid.
        // No previous-frame coordinates are used to manufacture a detection.
        out = evaluate(gray, 0);
        ++tested;
        if (!out.valid) {
            // Evaluate every recovery template, preferring valid, high-contrast
            // evidence. Equal scores keep the template closest to nominal.
            // evaluate() reuses its fixed workspace; no heap or recursion.
            for (int distance = appcfg::kWhiteSearchStepYPx;
                 distance <= appcfg::kWhiteSearchRadiusYPx;
                 distance += appcfg::kWhiteSearchStepYPx) {
                for (int sign = -1; sign <= 1; sign += 2) {
                    const auto candidate = evaluate(gray, sign * distance);
                    ++tested;
                    if ((candidate.valid && !out.valid) ||
                        (candidate.valid == out.valid &&
                         (candidate.peak_contrast > out.peak_contrast ||
                          (candidate.peak_contrast == out.peak_contrast &&
                           candidate.weight_sum > out.weight_sum)))) out = candidate;
                }
            }
        }
    }
    if (out.valid) ++_success_count;
    else ++_fail_count;
    out.templates_tested = tested;
    out.success_count = _success_count;
    out.fail_count = _fail_count;
    out.processing_us = micros() - t0;
    return out;
}

WhiteMarkerObservation WhiteMarker1DTracker::evaluate(const uint8_t* gray, int y_offset) const {
    WhiteMarkerObservation out;
    out.id = _id;
    out.center_y_px = static_cast<float>(nominalCenterY() + y_offset);
    const int* marker_rows = markerRows();
    const int* reference_rows = referenceRows();

    int contrast_scaled[kWidth];
    for (int x = 0; x < kWidth; ++x) {
        int marker_sum = 0;
        int reference_sum = 0;

        for (int i = 0; i < appcfg::kWhiteMarkerRowCount; ++i) {
            marker_sum += gray[(marker_rows[i] + y_offset) * kWidth + x];
        }
        for (int i = 0; i < appcfg::kWhiteReferenceRowCount; ++i) {
            reference_sum += gray[(reference_rows[i] + y_offset) * kWidth + x];
        }

        contrast_scaled[x] = 3 * marker_sum - 5 * reference_sum;
    }

    int smoothed_scaled[kWidth] = {};
    int peak_x = kSmoothRadius;
    int peak_value = -32768;

    int rolling = 0;
    for (int x = 0; x < kSmoothWindow; ++x) {
        rolling += contrast_scaled[x];
    }

    for (int x = kSmoothRadius; x < kWidth - kSmoothRadius; ++x) {
        if (x > kSmoothRadius) {
            rolling -= contrast_scaled[x - kSmoothRadius - 1];
            rolling += contrast_scaled[x + kSmoothRadius];
        }

        const int value = rolling / kSmoothWindow;
        smoothed_scaled[x] = value;
        if (value > peak_value) {
            peak_value = value;
            peak_x = x;
        }
    }

    const int lo = max(0, peak_x - appcfg::kWhiteCentroidHalfWindowPx);
    const int hi = min(kWidth - 1, peak_x + appcfg::kWhiteCentroidHalfWindowPx);

    int64_t weight_sum = 0;
    int64_t weighted_x_sum = 0;
    int bright_left = -1;
    int bright_right = -1;

    for (int x = lo; x <= hi; ++x) {
        const int weight = smoothed_scaled[x] - kCentroidBaselineScaled;
        if (weight <= 0) continue;

        weight_sum += weight;
        weighted_x_sum += static_cast<int64_t>(x) * weight;

        if (bright_left < 0) bright_left = x;
        bright_right = x;
    }

    out.peak_x_px = peak_x;
    out.peak_contrast = static_cast<float>(peak_value) / kContrastScale;
    out.weight_sum = static_cast<float>(weight_sum) / kContrastScale;
    out.bright_width_px =
        bright_left >= 0 ? (bright_right - bright_left + 1) : 0;

    const bool valid =
        peak_value >= kPeakThresholdScaled &&
        weight_sum >= kMinWeightScaled;

    out.valid = valid;
    out.reason = peak_value < kPeakThresholdScaled ? MarkerDetectionReason::LowContrast :
        weight_sum < kMinWeightScaled ? MarkerDetectionReason::LowWeight : MarkerDetectionReason::Detected;
    if (valid) {
        out.center_x_px =
            static_cast<float>(weighted_x_sum) /
            static_cast<float>(weight_sum);
    }
    return out;
}
