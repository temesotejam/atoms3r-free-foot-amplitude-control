# V46ac / 0.46.28 — Lightweight actuator-delay compensation

## Goal

Keep the video-comparison angle physically interpretable while compensating only
the measured control/actuator delay in Autonomous timing.

## Coordinates

Video comparison remains:

`pitch_mekf_measurement_relative_deg = posterior_MEKF(t) - posterior_MEKF(measurement_start)`

Autonomous timing uses:

`pitch_mekf_detector_relative_deg = pitch_mekf_measurement_relative_deg + omega_mekf * 0.003`

During Autonomous:

`pitch_mekf_control_deg = pitch_mekf_detector_relative_deg`

where

`omega_mekf = (gy_dps - mekf_bias_y_dps) * MEKF_GYRO_Y_SCALE`

The initial compensation horizon is fixed at **3000 us**.

## Why 3 ms

The V46ab c489 run showed approximately:

- zero-cross processing start -> current command start: median ~0.96 ms
- current command -> first observed >=3 mA current change: median ~3.36 ms
- detector processing start -> observed current rise: median ~4.45 ms

The previous full MEKF forward prediction shifted physical current onset earlier,
but the benefit did not justify the extra quaternion prediction in the high-rate
400 Hz / 1 ms acquisition configuration.

3 ms is intentionally a conservative initial compensation, shorter than the full
observed ~4.45 ms path.

## Heavy MEKF prediction

The old quaternion forward prediction is **not executed in Autonomous mode**.

It remains available for legacy non-Autonomous paths only.

This reduces Autonomous estimator/control computation while retaining the adopted
MEKF posterior itself unchanged.

## Unchanged systems

V46ac does not change:

- BMI270 gyro 400 Hz
- BMI270 accel 200 Hz
- 1.0 ms host poll
- BMI270 I2C 1 MHz
- MEKF posterior update
- MEKF bias/covariance/accel rejection
- gyro-integral amplitude/energy coordinate
- fast solver and Q model
- 300 mA current limit
- 100 ms maximum pulse
- 10 ms IMU delivery-age ESTOP
- queue/acquisition ESTOPs

## RWLOG

RWLOG semantic version is v50; binary sample layout remains compatible with v49.

For Autonomous runs:

- `pitch_mekf_measurement_relative_deg` = uncompensated posterior angle for video comparison
- `pitch_mekf_detector_relative_deg` = 3 ms delay-compensated timing angle
- `pitch_mekf_control_deg` = same compensated timing angle

The difference between detector-relative and measurement-relative angles therefore
shows the applied compensation directly in the log.
