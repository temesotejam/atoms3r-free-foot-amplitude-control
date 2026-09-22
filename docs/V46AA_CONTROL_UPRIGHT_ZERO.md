# V46aa / 0.46.26 — Initial-upright control timing coordinate

## Purpose

The autonomous controller should interpret the mechanism motion relative to the actual upright pose at the start of the measurement, not relative to the IMU's absolute gravity-frame zero.

V46aa therefore makes the timing-detector coordinate explicit:

`pitch_mekf_detector_relative_deg = predicted_MEKF(t) - predicted_MEKF(measurement_start)`

The measurement-start predicted MEKF angle is captured before the start kick, while the mechanism is at the initial upright pose.

## Why predicted MEKF is used

The existing controller already compensates for acquisition/control latency by predicting the posterior MEKF state forward to the control time.

The timing detector therefore keeps that prediction:

- video/estimator comparison: posterior measurement-relative MEKF angle
- control timing: predicted measurement-relative MEKF angle

This avoids discarding the existing short-horizon timing compensation.

## Relationship to the previous implementation

Before V46aa the autonomous detector computed:

`(predicted_MEKF(t) - posterior_MEKF(measurement_start))
 - (predicted_MEKF(measurement_start) - posterior_MEKF(measurement_start))`

Algebraically this is:

`predicted_MEKF(t) - predicted_MEKF(measurement_start)`

V46aa stores and uses this final coordinate directly.

A native regression test checks the previous float32 expression against the new expression over 1,000,000 deterministic cases. The only possible difference is float32 reassociation rounding and is bounded to 4e-6 deg.

## New status/log fields

- `pitch_mekf_detector_relative_deg`
- `mekf_detector_zero_predicted_abs_deg`
- `mekf_detector_zero_sample_us`

RWLOG is append-only bumped to v48.

## What uses this angle

Autonomous timing detection uses `pitch_mekf_detector_relative_deg` directly for:

- peak-side / return-to-centre tracking
- zero-cross detection
- zero-cross interpolation

The angle is a timing/detector coordinate only.

## What does not use this angle

The energy/amplitude coordinate remains the existing scaled gyro integral:

`energy_control_autonomous_gyro_relative_deg_`

The new detector coordinate does not change:

- MEKF quaternion, bias or covariance
- posterior MEKF update
- prediction horizon
- energy model
- Q selector / fast solver
- motor current
- pulse duration limits
- anti-windup
- ESTOP conditions

## Sensor specification remains frozen

- BMI270 gyro: 400 Hz
- BMI270 accel: 200 Hz
- host poll: 1.0 ms
- BMI270 I2C: 1 MHz
- STATUS-based selective reads
- existing reader task/core/priority/queue
- 10 ms delivery-age ESTOP

## Recommended coordinates

For video comparison:

`pitch_mekf_measurement_relative_deg`

For autonomous timing control:

`pitch_mekf_detector_relative_deg`

Both use the same physical event — measurement start / initial upright pose — as their zero. The difference is that the video-comparison coordinate uses posterior MEKF while the control coordinate uses the existing predicted MEKF state.
