# V46z / 0.46.25 — Event-relative MEKF comparison zero

## Purpose

Video/MEKF comparison should not depend on the camera horizontal zero or on the IMU gravity-frame absolute zero.

V46z keeps the adopted MEKF state continuous and adds **comparison-only relative angles** whose zero is captured at meaningful experiment events.

No new zero operation resets or modifies:

- MEKF quaternion
- MEKF gyro-bias state
- MEKF covariance
- gyro/accel data
- controller state
- motor command
- existing `pitch_mekf_control_deg`

The new values are observational/logging coordinates.

## Reference events

### START_SYNC start

Captured in `beginStartSync()`, when the start/synchronization sequence begins.

Fields:

- `mekf_start_sync_zero_abs_deg`
- `mekf_start_sync_zero_sample_us`
- `pitch_mekf_start_sync_relative_deg`

Definition:

`pitch_mekf_start_sync_relative_deg = pitch_mekf_abs_deg - mekf_start_sync_zero_abs_deg`

Use this only when inspecting the START synchronization period. In Autonomous Energy Control, the existing MEKF upright reinitialization occurs during the first START_SYNC OFF segment; therefore this coordinate can contain that intentional estimator-frame jump. It is **not** the default dynamic video-comparison coordinate.

### Measurement start

Captured in `beginMeasurementRun()`, immediately before the measurement enters `RUNNING_BATCH_SWEEP` and emits the t=0 row.

Fields:

- `mekf_measurement_zero_abs_deg`
- `mekf_measurement_zero_sample_us`
- `pitch_mekf_measurement_relative_deg`

Definition:

`pitch_mekf_measurement_relative_deg = pitch_mekf_abs_deg - mekf_measurement_zero_abs_deg`

This is the **default video-comparison coordinate**.

For video:

`video_measurement_relative = video_angle - video_angle_at_measurement_start`

Compare that with `pitch_mekf_measurement_relative_deg`.

### Trial start

Captured in `beginTrial()` for multi-trial modes.

Fields:

- `mekf_trial_zero_abs_deg`
- `mekf_trial_zero_sample_us`
- `pitch_mekf_trial_relative_deg`

Definition:

`pitch_mekf_trial_relative_deg = pitch_mekf_abs_deg - mekf_trial_zero_abs_deg`

For dedicated single-trial modes (including autonomous energy control and passive capture), trial zero is deliberately identical to measurement zero.

## Absolute angle remains available

`pitch_mekf_abs_deg` remains the continuous posterior MEKF pitch in the IMU gravity/body coordinate.

This field is never zero-subtracted and is retained for:

- estimator diagnostics
- absolute-frame inspection
- reproducing any relative coordinate offline

## Existing control angle is unchanged

The physical controller continues to use the pre-existing:

`pitch_mekf_control_deg = predicted_MEKF_angle - existing_control_offset`

Its prediction horizon and zero-cross detector semantics are unchanged.

The new comparison-relative fields are not read by the controller, solver, pulse selector, motor driver or ESTOP logic.

## Sensor specification remains frozen

V46z keeps the V46y measurement specification:

- BMI270 gyro 400 Hz
- BMI270 accel 200 Hz
- host poll 1.0 ms
- BMI270 I2C 1 MHz
- STATUS-based selective 6/12-byte reads
- existing reader task/core/priority/queue
- 10 ms delivery-age ESTOP

## RWLOG

RWLOG format is incremented from v46 to **v47** by append-only extension.

Added timeseries columns:

- `pitch_mekf_start_sync_relative_deg`
- `pitch_mekf_measurement_relative_deg`
- `pitch_mekf_trial_relative_deg`
- `mekf_start_sync_zero_abs_deg`
- `mekf_measurement_zero_abs_deg`
- `mekf_trial_zero_abs_deg`
- `mekf_start_sync_zero_sample_us`
- `mekf_measurement_zero_sample_us`
- `mekf_trial_zero_sample_us`

The converter remains backward compatible with v46 and earlier supported formats.
