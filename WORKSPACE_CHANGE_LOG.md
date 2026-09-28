# 2026-09-28 / 0.47.29

- Qualify the 8-degree previous-peak model by the selected mean target, preserving time/support/coefficients.
- Pause automatic yaw feedback for a bounded +/-0.2-degree response sequence inside the usual 30 seconds.
- Reverse sequence order on consecutive runs; keep 0.08 degree/cycle slew, gyro health and latched command targets.
- Record the profile and schedule in stopped-export metadata; keep RWLOG v53/112 bytes and timing/sensor/transport paths.
- Add production-path regression coverage for side targets, mean-target qualification, latching, reversed order, faults and UI.

## 2026-09-28 — 0.47.28 gyro steering / compact RWLOG

- User authorized cycle-yaw plus measured sway-asymmetry feedback and removal of unused MEKF yaw/magnetic logs.
- Added independent fixed-startup-bias quaternion gyro heading. Retained MEKF sway estimation and actuator limits.
- Symmetric target correction starts after 10s; bounded ±1°, at most 0.08° per cycle, target latched until peak response.
- RWLOG v53 is 112 bytes/sample; lossless column tables for foot/events; removed inactive metadata and magnetic observation additions.
- Preserved historical converters, actual-current/timing diagnostics, foot precision and normal offline HTTP workflow.
- Frozen stable repositories were not changed. Physical steering and deadline validation require the next normal run.

﻿
## 2026-09-03 14:15:00 +09:00 | started

- Purpose: User approved `タイトルなし.md` ("Energy Control autonomous-start / amplitude-formation mode") as the next implementation direction. Replace the manual-release dependency with an autonomous Energy Control state machine: IDLE -> one START_KICK -> WAIT_FIRST_PEAK -> BUILD_UP -> HOLD, while retaining the existing output safety limits.
- User-authorized scope interpreted from the attached document:
  - Correct the V0.2 signed `WAIT_OPPOSITE_EXCURSION` bug and add exact event 14 -> 15 regression coverage.
  - Add peak-based state and logging, free-decay next-peak prediction, feed-forward energy Q calculation, side-specific integral trim with anti-windup, target selection (first physical run at 2.0 degrees), START_KICK, BUILD_UP/HOLD states, and offline tests.
  - Keep ESTOP, battery guard, max 300 mA, 5--25 ms pulse guard, pulse solver, and current direction mapping. Do not return to Q_IDENT or add a new calibration experiment.
- Pre-change evidence: V0.2 Run 1 showed magnitude rearm reached 0.81965 degrees but was rejected due to a signed rearm logic defect; it also showed one saturated output with observed next peak 0.91631 degrees vs 1.49322-degree prediction.
- Planned workflow: inspect existing source/UI/logging contracts; implement new autonomous mode as a separately identified revision; add deterministic unit and offline-replay tests; build, flash, and verify only after all tests pass. No device write has occurred in this work item.

## 2026-09-12 | RWLOG v46 MEKF dynamic-validation build

- Adopted attitude estimator changed from the historical Madgwick detector to a 6-state MEKF (quaternion nominal state + 3-axis gyro-bias state/error covariance).
- Autonomous V7 zero-cross, peak, Q/event state, and angle-dependent control references now use `pitch_mekf_deg`.
- The adopted dynamic-beta Madgwick hold-073 estimator remains online during Autonomous V7 only as a comparison signal; other historical Madgwick comparison series are NaN during that capture.
- Preserved raw IMU, gyro integration, accel atan2, current audit, motor safety logic, and GPIO38 LED START/MID/END video synchronization.
- RWLOG sample format advanced to v46 (226-byte packed sample), append-only after the complete v45 prefix. Converter remains backward compatible with v44/v45.
- Added continuous MEKF/Madgwick absolute angles, quaternion, MEKF gyro bias, adaptive-accel trust/rejection diagnostics, and IMU timing fields.
- V45 detector sign compatibility was explicitly tested and corrected: accel uses raw axes, while all gyro components are negated for MEKF quaternion propagation; MEKF pitch therefore matches both static `atan2(-ax,...)` and legacy `-gy` detector direction.
- Frozen Autonomous V7 firmware/model revision string remains unchanged; the attitude change is separately identified as `v46_mekf_adopted_dynamic_beta_compare_20260912`.
