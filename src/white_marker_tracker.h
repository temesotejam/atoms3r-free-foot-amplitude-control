#pragma once

#include <stdint.h>

enum class MarkerDetectionReason : uint8_t {
    Detected, NoFrame, LowContrast, LowWeight
};
const char* markerDetectionReasonName(MarkerDetectionReason reason);

struct WhiteMarkerObservation {
    bool valid = false;
    MarkerDetectionReason reason = MarkerDetectionReason::NoFrame;
    uint8_t templates_tested = 0;
    int id = -1;
    float center_x_px = 0.0f;
    // Center of the selected sparse-row template, not a measured Y centroid.
    float center_y_px = 0.0f;
    float peak_contrast = 0.0f;
    float weight_sum = 0.0f;
    int peak_x_px = 0;
    int bright_width_px = 0;
    uint32_t processing_us = 0;
    uint32_t success_count = 0;
    uint32_t fail_count = 0;
};

class WhiteMarker1DTracker {
public:
    explicit WhiteMarker1DTracker(int marker_id);

    WhiteMarkerObservation process(const uint8_t* gray);

private:
    const int* markerRows() const;
    const int* referenceRows() const;
    int nominalCenterY() const;
    WhiteMarkerObservation evaluate(const uint8_t* gray, int y_offset) const;

    int _id;
    uint32_t _success_count = 0;
    uint32_t _fail_count = 0;
};
