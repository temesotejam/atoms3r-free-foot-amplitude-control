# MEKF + Dynamic-beta Madgwick validation (RWLOG v46)

## Purpose

This build keeps Autonomous Energy Control V7 and the V45 actual-current audit intact while changing the adopted attitude estimate to a 6-state multiplicative/error-state EKF (MEKF). The only online Madgwick estimator kept during the autonomous capture is the adopted dynamic-beta `hold073` strategy, and it is comparison-only.

## Online estimator roles

| Signal | Role | Motor/control authority |
|---|---|---|
| `pitch_mekf_control_deg` | Run-relative adopted angle | **Yes** |
| `pitch_mekf_abs_deg` | Continuous MEKF physical/video body-frame pitch (Ry180 sensor-to-body transform) | No; video comparison |
| `pitch_madgwick_dynamic_abs_deg` | Continuous bias-corrected dynamic-beta hold073 Madgwick | No; comparison only |
| `pitch_gyro_*` | Integrated gyro diagnostics | No |
| `pitch_accel_only_deg` | `atan2` diagnostic | No |

The legacy detector coordinate is preserved: static angle follows `atan2(-ax, hypot(ay,az))`, and its positive dynamic direction follows `-gy`. The MEKF wrapper keeps raw accelerometer axes and negates all gyro components before quaternion propagation so its reported pitch follows that same detector convention. Raw gyro values in RWLOG remain unmodified.

## Dynamic-beta comparison profile

The online comparison is `Config::FILTER_ADOPTED_INDEX = FILTER_DYNAMIC_HOLD_073_INDEX`. It retains the existing pulse-protected beta profile: input/hold floor, 73 ms post-input hold, then the configured soft-start/recovery toward the adopted ceiling. `beta_target_dynamic_hold073` and `beta_applied_dynamic_hold073` remain the authoritative beta traces in converted CSV. Other historical filter series are NaN during Autonomous V7 v46 capture.

## MEKF state and adaptive accel rejection

Nominal state: unit quaternion plus nominal gyro bias. Error covariance: `[dtheta_x,dtheta_y,dtheta_z,dbx,dby,dbz]`. Acceleration updates use both magnitude consistency and predicted-gravity direction consistency. Lower confidence increases measurement covariance; below the minimum confidence the accel update is skipped.

RWLOG v46 adds:

- `pitch_mekf_control_deg`
- `pitch_mekf_abs_deg`
- `pitch_madgwick_dynamic_abs_deg`
- `mekf_q_w/x/y/z`
- `mekf_bias_x/y/z_dps`
- `mekf_accel_confidence`
- `mekf_accel_residual_deg`
- `mekf_accel_mag_error_g`
- `imu_update_dt_us`
- `imu_sample_age_us`
- `mekf_accel_used`
- `attitude_filter_adopted` (`1 = MEKF`)

## Video synchronization

The existing synchronization is deliberately unchanged:

- LED GPIO: 38
- pattern ID: `LED_SYNC_PATTERN_V2_LOGGED_ANCHORS`
- START signature before measurement
- first MID anchor at 2.5 s
- further MID anchors every 5.0 s
- END signature after measurement
- each sample retains `led_state` and `sync_event_id`

Use the LED anchors to construct the video-to-RWLOG time map before comparing estimator angle or lag. For estimator accuracy, compare video angle to `pitch_mekf_abs_deg` and `pitch_madgwick_dynamic_abs_deg`, not the run-relative control angle.

## Program-level validation performed before release

- MEKF host sign test: static +10 deg -> +10 deg.
- Legacy detector-rate sign: raw `gy=-90 deg/s` for 100 ms -> MEKF +9 deg.
- Synthetic 90 deg/s sweep with matching gravity vector -> 45.0 deg and zero direction residual.
- Strong translational acceleration -> accel update rejected.
- Quaternion normalization maintained.
- RWLOG v44/v45 backward conversion and v46 CRC/size/conversion tests pass.
- Source guards verify MEKF adoption, comparison-only dynamic-beta Madgwick, preserved current audit, and unchanged LED synchronization.
