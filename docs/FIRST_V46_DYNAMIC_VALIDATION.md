# First V46 motor-driven dynamic validation

This is the intended first performance measurement for the V46 MEKF build: run the mechanism with **Autonomous Energy Control V7**, record the same motion with a fixed-horizon video, and compare MEKF against the online dynamic-beta Madgwick estimator in RWLOG v46.

A passive capture remains available as an optional diagnostic, but it is **not required before this motor-driven validation**.

## What the firmware does

During the 30 s Autonomous V7 capture:

- MEKF is the adopted control/detector attitude.
- Dynamic-beta Madgwick `hold073` runs online at the same time as a comparison-only estimator.
- The run begins with one startup-only `300 mA / 100 ms` kick in the configured start direction.
- After the first confirmed physical peak, P1/Q1 Energy Control can command later pulses at accepted zero-cross events.
- Normal autonomous current is 300 mA; pulse width is selected within the V7 output guard (`0..100 ms`, where zero means no output).
- Raw IMU, gyro-only integration, accel-only angle, actual current, MEKF diagnostics, LED video anchors, and both estimator angles are logged.
- Emergency stop and the existing motor/current safety path remain active.

The expected attitude revision is:

`v46_mekf_adopted_dynamic_beta_compare_20260912`

## Firmware artifact

Use a successful GitHub Actions artifact from branch:

`feature/v46-mekf-dynamic-validation`

The artifact contains:

- `bootloader.bin` at `0x0000`
- `partitions.bin` at `0x8000`
- `boot_app0.bin` at `0xe000`
- `firmware.bin` at `0x10000`
- `merged-firmware.bin` containing the same four images, written from `0x0000`
- `SHA256SUMS.txt`
- `FLASH_LAYOUT.txt`

## After flashing

1. Power the mechanism normally.
2. Wait through startup gyro calibration and estimator settling until the UI allows a run to start.
3. Connect the PC/phone to the firmware AP defined in `src/config.h`.
4. Open `http://192.168.4.1/`.
5. Confirm that the UI reports the roller and IMU as ready and that no previous ESTOP remains latched.
6. Start a fixed-horizon video with the synchronization LED visible.

## Motor-driven Run 1

For the first run use the default **8.0 deg** target.

1. Put the mechanism upright in its normal initial condition.
2. In the web UI select `8.0 deg` as the Autonomous Energy Control target.
3. Press **Start autonomous energy control**.
4. Do not manually push the mechanism after starting; the firmware supplies the startup kick itself.
5. Keep the complete mechanism and synchronization LED visible in the video.
6. Let the 30 s capture complete unless an abnormal motion requires Emergency stop.
7. Keep the video running through the END synchronization signature.
8. Download the RWLOG from the web UI after the run finishes.

Do not change the control target during a run.

## Convert the RWLOG

```powershell
python tools\convert_rwlog_to_csv.py <run>.rwlog --out converted_run
```

Use `converted_run/timeseries.csv` for the estimator comparison.

## Signals for attitude-performance evaluation

Use these continuous absolute-angle signals against video ground truth:

- `pitch_mekf_abs_deg` (V46g: physical/video body-frame MEKF pitch; no post-output scaling)
- `pitch_madgwick_dynamic_abs_deg`

Do not use `pitch_mekf_control_deg` as the video ground-truth coordinate; it is the run-relative angle used by control.

Also inspect:

- `mekf_accel_confidence`
- `mekf_accel_residual_deg`
- `mekf_accel_mag_error_g`
- `mekf_accel_used`
- `mekf_bias_x_dps`, `mekf_bias_y_dps`, `mekf_bias_z_dps`
- `imu_update_dt_us`
- `imu_sample_age_us`
- `motor_cmd_mA`
- `roller_actual_current_mA`
- `pulse_active`
- `led_state`
- `sync_event_id`

## Video synchronization

Use the logged LED anchors to map video time to RWLOG time before computing error or lag:

- START signature before the measurement interval,
- first MID anchor at 2.5 s,
- later MID anchors every 5.0 s,
- END signature after the measurement interval.

Do not align the two streams only by pressing the start button; use the LED anchors.

## What to calculate

For both MEKF and dynamic-beta Madgwick, calculate against video angle:

- bias / mean signed error,
- MAE,
- RMSE,
- maximum absolute error,
- error during motor-pulse windows,
- error during non-pulse windows,
- best-fit time lag relative to video,
- error grouped by MEKF accel update used/rejected,
- error grouped by acceleration-confidence range.

The main question is not simply which trace looks smoother. The useful result is whether MEKF gives smaller dynamic angle error and/or smaller lag during the actual motor-driven motion, especially around pulse transients where accelerometer contamination is expected.

## Run 2 / Run 3

After the first 8 deg run is healthy, repeat with the same firmware and camera geometry. Prefer at least two additional runs before changing estimator parameters. The 10 deg and 12 deg target options can then be used to increase dynamic severity, but estimator tuning should not be changed between comparison runs.

## Stop conditions

Use Emergency stop if the mechanism leaves its intended mechanical envelope, the roller behaves unexpectedly, or the UI reports a hardware error. A stopped/invalid run is still useful diagnostically if its RWLOG can be downloaded; do not treat it as an estimator-performance run.
