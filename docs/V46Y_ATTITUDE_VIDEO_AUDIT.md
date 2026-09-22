# V46y attitude / video audit

This audit begins only after the IMU acquisition path was frozen at the V46y specification:

- BMI270 gyro 400 Hz
- BMI270 accel 200 Hz
- host poll 1.0 ms
- BMI270 I2C 1 MHz
- STATUS-based selective reads

No estimator or controller parameter is changed by this document.

## Data

Two independent 1 ms-poll hardware captures with synchronized video-angle CSV were used:

- a864: 2026-09-18 14:52 run
- d95b: 2026-09-18 15:24 run

For the diagnostic comparison, video and RWLOG time were aligned by minimizing the dynamic waveform residual over the 30 s motion interval while allowing one constant time offset. One constant angle offset was also removed when evaluating waveform error. This is a diagnostic fit and does not replace the LED synchronization protocol.

## MEKF versus video

| Run | fitted video = intercept + slope × MEKF | residual RMSE | residual drift over 30 s |
|---|---|---:|---:|
| a864 | -1.4308 + 0.99514 × MEKF | 0.218 deg | -0.089 deg |
| d95b | -0.6211 + 0.99275 × MEKF | 0.207 deg | +0.046 deg |

The fitted dynamic scale is therefore essentially 1.0 in both runs.

Peak-to-peak comparison after time alignment gives 69 matched extrema in each run.

| Run | matched extrema | peak angle-error standard deviation | peak timing standard deviation |
|---|---:|---:|---:|
| a864 | 69 | 0.094 deg | 12.1 ms |
| d95b | 69 | 0.086 deg | 11.8 ms |

For the control coordinate, video was zeroed by its own pre-run static baseline and compared against `pitch_mekf_control_deg`.

| Run | matched zero crossings | mean timing error | timing standard deviation | max absolute error |
|---|---:|---:|---:|---:|
| a864 | 69 | -2.82 ms | 4.33 ms | 11.62 ms |
| d95b | 69 | -2.28 ms | 4.29 ms | 9.62 ms |

## What is actually drifting

The gyro-only bias-corrected integral does drift relative to video:

- a864: about +3.92 deg / 30 s residual trend
- d95b: about +3.66 deg / 30 s residual trend

The adopted MEKF does not show this behavior. Its corresponding residual trend is below 0.1 deg over 30 s in both runs.

Dynamic-beta Madgwick also has little long-term residual trend, but its waveform error is about 1.0 deg RMSE, much larger than MEKF.

## Static absolute-angle offset

Before motion, MEKF agrees with the *raw accelerometer gravity-angle candidate* almost exactly:

- a864: MEKF - raw candidate = +0.015 deg mean
- d95b: MEKF - raw candidate = -0.004 deg mean

The separately calibrated static physical-roll mapping is

`physical = 0.9278941864 × raw_candidate - 0.4983084809 deg`.

Its zero-body-angle point corresponds to a raw candidate of

`0.53703158 deg`.

Therefore the roughly 0.5–0.6 deg static difference between raw MEKF absolute pitch and the calibrated physical-roll coordinate is expected from the existing calibration definitions. It is not evidence of MEKF dynamic drift.

The video absolute intercept must not be used as a global sensor correction: the fitted constant video/MEKF offset differs between the two camera runs, while the MEKF/raw-accelerometer relationship remains stable.

## Do not apply the full static affine mapping to MEKF output

Applying the full 0.927894 slope directly to the MEKF output would double-count dynamic scale correction. The MEKF already receives the calibrated gyro-Y scale (0.908911) before prediction, and its measured dynamic video slope is already ~0.993–0.995.

On d95b, applying the full static affine transform to MEKF increased the bias-removed waveform RMSE from about 0.212 deg to about 0.446 deg.

## Current conclusion

1. The V46y IMU acquisition specification can remain frozen.
2. The adopted MEKF dynamic angle should not be rescaled.
3. The previously observed long-term drift belongs to the gyro-only integral, not the MEKF estimate.
4. `pitch_mekf_control_deg` is behaving as a relative run-zero coordinate and its zero-cross timing agrees with video to a few milliseconds.
5. If an absolute *physical body angle* is needed for display/reporting, treat the static body-frame zero offset as a separate coordinate-calibration problem. Do not alter the control coordinate until that absolute-coordinate mapping is validated independently.
