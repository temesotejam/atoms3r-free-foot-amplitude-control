# V46ab / 0.46.27 — No MEKF forward prediction in Autonomous timing control

## Decision

Autonomous peak / return-to-centre / zero-cross timing uses the same posterior,
measurement-start-relative MEKF angle used for video comparison:

`pitch_mekf_measurement_relative_deg = pitch_mekf_abs_deg - mekf_measurement_zero_abs_deg`

During Autonomous control:

`pitch_mekf_control_deg = pitch_mekf_measurement_relative_deg`

and

`pitch_mekf_detector_relative_deg = pitch_mekf_measurement_relative_deg`

The measurement start is the initial upright pose before the start kick, so the
physical run starts from 0° in both video comparison and timing control.

## Why the prediction was removed

With the frozen sensor path:

- BMI270 gyro 400 Hz
- BMI270 accel 200 Hz
- host poll 1.0 ms
- BMI270 I2C 1 MHz

the posterior MEKF is already current enough that the previous forward prediction
showed only a small timing benefit in the V46z 2ad2 hardware/video run, while the
posterior angle matched the same-time video slightly better.

Observed on that run:

- posterior vs video RMSE: about 0.221°
- predicted vs video RMSE: about 0.248°
- posterior zero-cross MAE: about 4.12 ms
- predicted zero-cross MAE: about 3.89 ms

The prediction therefore remains available as a diagnostic, but it is no longer
used by Autonomous timing decisions.

## Prediction remains logged only

The firmware still computes and logs:

- `pitch_mekf_predicted_abs_deg`
- `mekf_prediction_horizon_us`
- `mekf_detector_zero_predicted_abs_deg`
- `mekf_detector_zero_sample_us`

These fields are diagnostic-only in Autonomous mode and do not feed:

- peak tracking
- return-to-centre detection
- zero-cross detection or interpolation
- fast solver input selection
- motor pulse start / stop decisions

## Energy coordinate is unchanged

The peak amplitude / energy coordinate remains the existing scaled gyro integral:

`energy_control_autonomous_gyro_relative_deg_`

V46ab does not replace that coordinate with MEKF angle.

## Safety and output envelope are unchanged

V46ab does not change:

- MEKF state update, bias or covariance
- accel rejection / confidence logic
- fast solver
- Q model
- side-response correction
- integral anti-windup
- 300 mA Autonomous current limit
- 100 ms maximum pulse width
- 10 ms IMU delivery-age ESTOP
- queue overflow / acquisition-stop ESTOP behavior

## RWLOG

The RWLOG format version is **v49**.

The binary sample layout is unchanged from v48. The version bump identifies the
semantic change that `pitch_mekf_detector_relative_deg` and Autonomous
`pitch_mekf_control_deg` now use posterior, measurement-start-relative MEKF
instead of the short-horizon predicted MEKF coordinate.
