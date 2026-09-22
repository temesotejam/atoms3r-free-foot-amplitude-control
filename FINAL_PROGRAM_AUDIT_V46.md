# Final program audit — V46 MEKF + Dynamic-beta Madgwick

Date: 2026-09-12

## Audit target

Derived from `ENERGY_CONTROL_AUTONOMOUS_V7_V45_CURRENT_AUDIT_FIRMWARE_20260904`.

The intended V46 behavior is:

1. MEKF is the adopted angle for Autonomous Energy Control V7.
2. The adopted dynamic-beta Madgwick `hold073` estimator runs online for comparison only.
3. Raw IMU, gyro integration, accel atan2, actual-current audit, motor safety, and LED video synchronization remain available.
4. RWLOG v46 contains all signals needed for post-run video/log analysis.

## Final findings

### Fixed during audit: detector sign consistency

An initial wrapper produced the correct static angle sign but the wrong dynamic gyro sign. It was corrected before release.

Final adapter:

- MEKF accel input = raw M5Unified accel XYZ.
- MEKF gyro propagation input = negative raw gyro XYZ.
- MEKF reported pitch = MEKF Euler pitch.

This preserves the historical V45 detector convention:

- static: `atan2(-ax, hypot(ay,az))`
- dynamic positive direction: `-gy`

Host tests:

- static +10 deg -> +10 deg
- raw `gy=-90 deg/s` for 100 ms -> +9.000 deg
- synthetic 90 deg/s sweep -> 45.000 deg at 0.5 s
- quaternion norm -> 1.0000000

### Adaptive accel rejection

A strong contaminated acceleration vector (`[0.8, 0, 1] g`, norm 1.281 g, direction residual 38.66 deg) was rejected in the host test:

- `mekf_accel_used = 0`
- confidence = 0

### Control adoption

Static source guards verify that the Autonomous/zero-cross/peak-related adopted angle references use `pitch_mekf_deg`.

The only remaining direct legacy adopted Madgwick angle-array access is the comparison/legacy status assignment; it does not command the motor.

During Autonomous V7 capture:

- MEKF runs on each fresh IMU sample.
- only the adopted dynamic-beta `hold073` bias-corrected Madgwick is updated for online comparison.
- non-adopted historical comparison series are written as NaN.

### Motor/current path

The following V45 functions/source were verified unchanged:

- `ExperimentRunner::serviceFast`
- `ExperimentRunner::beginEnergyControlAutonomousPulse`
- `ExperimentRunner::beginEnergyControlAutonomousStartKickPulse`
- `ExperimentRunner::updateEnergyControlAutonomousPulse`
- `roller485_manager.cpp`

Frozen Autonomous V7 model/direction regression tests pass. The original frozen firmware/model revision string is retained. The attitude change is identified separately as:

`v46_mekf_adopted_dynamic_beta_compare_20260912`

### LED video synchronization

Verified unchanged versus V45:

- GPIO 38
- `LED_SYNC_PATTERN_V2_LOGGED_ANCHORS`
- START sync implementation
- first MID anchor: 2.5 s
- subsequent MID interval: 5.0 s
- END sync implementation
- `led_state` and `sync_event_id` remain logged

The START/MID/END function bodies are byte-equivalent after newline normalization.

### RWLOG v46

Packed sample size:

- v45: 190 bytes
- v46: 226 bytes

V46 is append-only over the complete v45 prefix. Added fields include:

- MEKF control and continuous angles
- online dynamic-beta Madgwick continuous angle
- MEKF quaternion
- MEKF gyro-bias estimate
- accel confidence / direction residual / magnitude error / used flag
- IMU sample dt and log-sample age
- adopted filter identifier

Converter tests confirm v44/v45 backward compatibility and v46 CRC/sample-size/CSV conversion.

For video comparison use:

- `pitch_mekf_abs_deg`
- `pitch_madgwick_dynamic_abs_deg`

Do not use `pitch_mekf_control_deg` as the video absolute teacher coordinate; it is measurement-start zero-subtracted for control.

## Tests passed

- MEKF host sign/dynamics/rejection test
- RWLOG v41/v42 conversion regression
- RWLOG v44/v45 conversion regression
- RWLOG v44/v45/v46 conversion + CRC regression
- V46 source guard (MEKF adoption, current audit, LED sync)
- Autonomous V7 frozen direction/model regression
- Autonomous metadata/JSON/RWLOG offset/CRC/converter regression
- Energy Control V0 replay/safety regression
- Energy Control V0.2 rearm/alternation regression
- Q1 shadow regression
- Q_IDENT frozen-provenance guard
- E2/Q1 shadow regression
- Python syntax compilation for changed log/converter tests
- C++ host syntax check for `experiment_runner.cpp`, `imu_manager.cpp`, and `mekf6.cpp` using Arduino/AHRS interface stubs
- packed C++ `sizeof(RwLogFileHeader)=110`, `sizeof(LogSample)=226`

## Test not runnable from the supplied archive

`tools/test_energy_control_autonomous_v7_correction.py` requires:

`analysis/v7_side_response_correction_20260904/v7_side_response_summary.json`

That external analysis fixture is not present in the uploaded archive, so this one test cannot run here. The frozen V7 direction/model regression and metadata tests do run and pass.

## Full PlatformIO compile status

A real PlatformIO/ESP32-S3 compile could not be run in this execution environment because the PlatformIO/Espressif/M5 packages are not installed locally and external package download is unavailable. The changed C++ translation units were syntax-checked with host stubs, and the standalone MEKF was compiled and executed as a native test.

The stale V45 `firmware/firmware.bin` was intentionally removed from this package so it cannot be mistaken for the new MEKF firmware. Build `env:atoms3cam` with PlatformIO before flashing hardware.

## Release judgement

**Program-level audit result: ready for a first controlled dynamic validation build, subject to a real PlatformIO build before flashing.**

For the first run, start with a conservative bench/manual-motion test and verify the converted v46 log contains synchronized MEKF/Madgwick absolute angles, `mekf_accel_used`, and LED anchors before progressing to the full autonomous motion experiment.
