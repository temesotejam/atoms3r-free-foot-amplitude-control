# RWLOG v30: Vbat蜍慕噪ﾎｲ繝ｻ蜈･蜉帷ｵゆｺ・ｾ・3 ms菫晄戟繝ｻﾏ・3 ms繝ｻ蝗ｺ螳墅ｲ豈碑ｼ・

譛ｬ繝励Ο繧ｸ繧ｧ繧ｯ繝医・RWLOG蠖｢蠑竣30繧貞・蜉帙＠縺ｾ縺吶ＡRwLogFileHeader` 縺ｯ110 bytes縲｝acked `LogSample` 縺ｯ121 bytes縺ｧ縺吶・

v29縺九ｉ縲∵ｬ｡縺ｮ7 bytes繧定ｩｦ陦湖ｲ險ｭ螳壹・逶ｴ蠕後∈霑ｽ蜉縺励∪縺励◆縲・

| field | type | meaning |
|---|---|---|
| `beta_model_vbat_mV` | `uint16_t` | 繝輔か繝ｼ繝ｫ繝舌ャ繧ｯ繝ｻ繧ｯ繝ｩ繝ｳ繝怜ｾ後↓髮ｻ豬√Δ繝・Ν縺ｸ菴ｿ縺｣縺滄崕蝨ｧ |
| `predicted_i_goal_mA` | `int16_t` | Vbat陬懈ｭ｣莉倥″鬟ｽ蜥悟ｾ後・莠域ｸｬ蛻ｰ驕秘崕豬・|
| `predicted_peak_current_mA` | `int16_t` | 繝代Ν繧ｹ邨らｫｯ縺ｮ莠域ｸｬ螳滄崕豬・|
| `beta_model_vbat_status` | `uint8_t` | 0=螳滓ｸｬ遽・峇蜀・・=遽・峇遶ｯ縺ｸ繧ｯ繝ｩ繝ｳ繝励・=7.50 V繝輔か繝ｼ繝ｫ繝舌ャ繧ｯ |

譌｢蟄倅ｽ咲ｽｮ縺ｮ `trial_recommended_beta_low` 縺ｯ `trial_predicted_beta_min`縲～trial_beta_tau_up_s` 縺ｯ `beta_recovery_tau_s` 縺ｸ諢丞袖縺ｨCSV蜷阪ｒ螟画峩縺励※縺・∪縺吶・

4譛ｬ縺ｮ譌ｧ蛟呵｣懷・縺ｯ縲∝崋螳墅ｲ繝ｻ謗｡逕ｨ蛟呵｣懊・ﾎｲmin荳贋ｸ矩剞縺ｮ豈碑ｼ・・縺ｨ縺励※谺｡縺ｮ鬆・↓菴ｿ縺・∪縺吶・

```text
fixed_b100
adopted_vbat_hold73_soft300_linear0075_max020
floor_b005_hold73_soft300_linear0075_max020
floor_b010_hold73_soft300_linear0075_max020
```

菫晄戟譎る俣縺ｯ繝舌う繝翫Μ蛻励ｒ霑ｽ蜉縺帙★縲ヽWLOG繝｡繧ｿ繝・・繧ｿ縺ｮ `beta_hold_after_input_ms`・・3・峨→ `beta_recovery_rule` 縺ｸ險倬鹸縺吶ｋ縲ゅし繝ｳ繝励Ν縺斐→縺ｮ `beta_target_*` 縺ｨ `beta_applied_*` 縺ｧ菫晄戟繝ｻ蠕ｩ蟶ｰ繧呈､懆ｨｼ縺吶ｋ縲ＡLogSample`縺ｯ121 bytes縺ｮ縺ｾ縺ｾ縺ｪ縺ｮ縺ｧ縲∵里蟄倥・v30螟画鋤蝎ｨ縺ｨ莠呈鋤縺ｧ縺ゅｋ縲・

荳ｻ縺ｪCSV蛻・

- 蜷梧悄: `time_s`, `log_time_s`, `t_test_ms`, `led_state`, `sync_event_id`
- 蜈･蜉帙・螳滓ｸｬ: `motor_cmd_mA`, `roller_actual_current_mA`, `roller_battery_mV`
- 繝｢繝・Ν: `beta_model_vbat_mV`, `predicted_i_goal_mA`, `predicted_peak_current_mA`, `trial_predicted_beta_min`, `beta_model_vbat_status`
- 隗貞ｺｦ: `pitch_fixed_b100_deg`, `pitch_dynamic_adopted_deg`, `pitch_dynamic_floor_b005_deg`, `pitch_dynamic_floor_b010_deg`
- ﾎｲ逶ｮ讓・ `beta_target_fixed`, `beta_target_adopted`, `beta_target_floor_b005`, `beta_target_floor_b010`
- 驕ｩ逕ｨﾎｲ: `beta_applied_fixed`, `beta_applied_adopted`, `beta_applied_floor_b005`, `beta_applied_floor_b010`

`time_s` 縺ｯ髢句ｧ記ED鄂ｲ蜷榊ｾ後ｒ0縺ｨ縺吶ｋ貂ｬ螳壽凾險医〒縺吶ょ虚逕ｻ蜷梧悄縺ｫ縺ｯRWLOG蜈磯ｭ縺九ｉ騾｣邯壹☆繧・`log_time_s` 繧剃ｽｿ逕ｨ縺励※縺上□縺輔＞縲・

CRC32縺ｯ蠕捺擂縺ｩ縺翫ｊ縲√・繝・ム縲゛SON繝｡繧ｿ繝・・繧ｿ縲∝・繧ｵ繝ｳ繝励Ν縺ｫ蟇ｾ縺励※險育ｮ励＠縺ｾ縺吶Ａtools/convert_rwlog_to_csv.py` 縺ｯv30縺ｨ蠕捺擂縺ｮv23・柧27繝ｻv29繧定ｪｭ縺ｿ蜿悶ｊ縺ｾ縺吶・

## Soft-start linear recovery metadata

New captures use `beta_recovery_rule` =
`hold_beta_min_after_input_then_softstart_linear_recovery`.  The metadata
records `beta_hold_after_input_ms` = 73, `beta_soft_start_ms` = 300, and
`beta_linear_recovery_per_ms` = 0.000075, and `beta_dynamic_max` = 0.020.  The fixed-reference filter remains at 0.100.  The legacy
`beta_recovery_tau_s` field is `0.0` because no exponential recovery is used.
The unchanged per-sample `beta_target_*` and `beta_applied_*` fields provide
the authoritative applied profile.

## RWLOG v31: gyro-observed peak phase-exponential beta test

This test changes only the `adopted` dynamic-beta strategy.  The fixed
`beta=0.100` reference and the two constant-floor dynamic strategies remain
unchanged comparison series.

For each pulse, the adopted strategy integrates the bias-corrected pitch gyro
rate from pulse start.  After the input/hold protection has ended, it records
the outgoing direction.  When the rate has reversed past the configured
confirmation threshold, that observed gyro-integral angle is used as the
cycle's peak magnitude `theta_peak`.  No uncalibrated mass, geometry, or
energy-model constant is used in this first firmware test.

Until a turn is confirmed, the adopted beta is held at the existing
current/Vbat-derived floor.  After confirmation, the existing time-based beta
profile is retained as the primary value, but it is capped by the following
increasing, convex exponential phase ceiling:

```text
x = clamp((abs(theta_peak) - abs(theta_gyro_from_pulse)) / abs(theta_peak), 0, 1)
beta_phase = beta_min + (beta_max - beta_min) * (exp(k*x) - 1) / (exp(k) - 1)
beta_apply = min(beta_base, beta_phase)
```

The trial constants are `k=1.50`, outgoing and turn confirmation thresholds
of `0.25 deg/s`, a minimum usable observed peak of `0.25 deg`, and
`beta_max=0.020`.  Thus beta stays low near the observed maximum angle and is
allowed to rise toward the existing profile only as the motion returns to the
next zero crossing.

`LogSample` is 130 packed bytes in v31.  New CSV columns are
`beta_phase_state` (0 idle, 1 outgoing/waiting for turn, 2 return to zero),
`beta_phase_progress`, `beta_phase_peak_angle_deg`,
`beta_phase_angle_deg`, and `beta_phase_ceiling`.  The converter supports
v23-v27 and v29-v31.

## RWLOG v32: manual zero-cross input and five dynamic-beta ceilings

This firmware version records one selected 30 s condition per capture. The Web UI exposes only:

- current: 100, 150, or 200 mA;
- input width: 30, 50, or 70 ms;
- start, stop, clear log, and RWLOG download.

The selected condition is used for the start kick and for every accepted zero-cross pulse. The motor direction alternates at each accepted crossing. The 9 allowed combinations are recorded in metadata as a reproducibility grid; they are not automatically batch-run.

Each capture runs six Madgwick series simultaneously: fixed `beta=0.100`, then dynamic ceilings `0.025`, `0.050`, `0.075`, `0.100`, and `0.125`. All dynamic series share the same Vbat/current-based beta minimum, 73 ms hold, 50 ms soft start, and time-matched recovery. Therefore each ceiling is reached at the same post-input time. The `0.050` series remains the zero-cross timing reference.

`LogSample` is 146 packed bytes in v32. New CSV names encode the ceiling explicitly: `beta_ceiling_dynamic_max025` through `max125`, `pitch_dynamic_max025_deg` through `max125_deg`, and matching `beta_target_dynamic_*` / `beta_applied_dynamic_*` columns. Use `tools/convert_rwlog_to_csv.py`, which supports v32 as well as earlier logged versions.

## RWLOG v33: dynamic beta hold-time comparison

v33 uses a 130-byte packed `LogSample` (the same binary size as v31, but with a different series layout). It records four simultaneous Madgwick series:

1. `fixed_b100` 窶・fixed beta 0.100;
2. `dynamic_vbat_hold073_soft50_timematch_max025`;
3. `dynamic_vbat_hold120_soft50_timematch_max025`;
4. `dynamic_vbat_hold170_soft50_timematch_max025`.

The three dynamic series share the Vbat/current-derived beta floor and ceiling 0.025. Only `beta_hold_after_input_ms` differs (73, 120, 170 ms). After that hold, each series follows the existing 50 ms soft start and time-matched linear ramp. The 73 ms series remains the adopted zero-cross timing reference, so the other series do not change the physical motor command.

The v33 converter exports the explicit columns `pitch_dynamic_hold073_deg`, `pitch_dynamic_hold120_deg`, `pitch_dynamic_hold170_deg`, with matching `beta_ceiling_dynamic_hold*`, `beta_target_dynamic_hold*`, and `beta_applied_dynamic_hold*` fields. Gyro-only and accelerometer-only diagnostics remain `pitch_gyro_raw_deg`, `pitch_gyro_bias_corrected_deg`, and `pitch_accel_only_deg`. Metadata includes `beta_hold_series_ms: "73,120,170"` and `beta_dynamic_ceiling: 0.025`.

## RWLOG v40: log-only Q-model calibration settings

The v40 metadata records the calibration-only initial-amplitude policy as
`cal_initial_target_min_deg`, `cal_initial_target_max_deg`,
`cal_initial_abort_deg`, `cal_initial_max_kicks`, and
`calibration_timeout_ms`. The nominal band stops the bounded initial build-up;
a final peak above the nominal upper edge but below the abort boundary is kept
as the measured free-decay initial condition. At or above the abort boundary,
calibration is invalidated and the unchanged normal Q controller resumes.
These settings and every calibration peak are log-only and never alter live Q
selection.

## RWLOG v41: physical-peak Q-probe calibration diagnostics

v41 keeps the 146-byte binary `LogSample` unchanged and extends only the JSON
metadata used by the calibration-shadow analysis. Every calibration peak now
has two explicit times: `candidate_peak_ms` is the sample time at which the
physical extremum was observed, and `confirmed_ms` is the later time at which
gyro-rate reversal confirmed it. `candidate_peak_ms` is authoritative for
free-decay ordering and Q-probe freshness.

The firmware freezes a candidate as soon as reversal begins. It therefore
cannot be overwritten by the following half-cycle during the confirmation
window. Free-decay fitting accepts only consecutive, alternating candidate
peaks whose gap does not exceed `cal_max_consecutive_peak_gap_ms`; a missing or
non-alternating turn invalidates calibration instead of bridging across it.

`calibration_probe_events` include the pulse start, physical pre- and
post-pulse candidate times, confirmation time, requested/observed side,
probe direction, support decision, individual gain, positive-gain decision,
and whether that sample entered the per-side mean. `post_pulse` is evaluated
from `candidate_peak_ms > pulse_start_ms`, never from confirmation time. A
probe is accepted only when its pre-pulse amplitude lies inside the selected
free-decay support with `cal_q_probe_support_margin_deg`, and when its gain is
positive. `cal_q_probe_samples_per_side` repeated accepted samples are required
for both sides; metadata reports their means (`cal_g_pos`, `cal_g_neg`) and
population standard deviations (`cal_g_pos_stddev`, `cal_g_neg_stddev`).

These changes remain log-only. The normal continuous inverse-Q controller and
its energy limits are unchanged.
## Calibration algorithm revision v43: input-domain Q-probe rebuild

The on-disk RWLOG binary layout remains v41 (the 146-byte `LogSample` is
unchanged), but v43 writes
`calibration_algorithm_revision: "v43_input_domain_q_rebuild"` in metadata so
that a capture can be attributed to this calibration behavior without relying
on a camera filename timestamp.

Free-decay metadata now distinguishes the prior union support
`cal_free_support_*` from the actual predecessor/input domains
`cal_free_input_*`. A Q probe is allowed only when its predecessor amplitude
lies in the intersection of the two selected arrival-side input domains after
subtracting `cal_q_probe_support_margin_deg` at both ends. This prevents using
a free-decay arrival amplitude as if it were an input to the next-side model.

After both five-transition free-decay fits are selected, v43 sends exactly one
bounded `phase=1` calibration build-up event toward
`cal_q_rebuild_target_peak_deg` (10 deg). It uses the unchanged 3.80 mA*s cap,
18 deg prediction guard, and 19 deg abort boundary. The following physical
peak is logged as calibration peak `phase=6`; only then can low-Q probes begin.
Build-up metadata now includes `phase` (`0=initial`, `1=post-fit rebuild`) and
`target_peak_deg`. Normal continuous inverse-Q control is unchanged, and every
calibration output remains shadow-only.

## Calibration algorithm revision v44: bounded multi-rebuild

The RWLOG binary layout remains v41. Metadata writes
`calibration_algorithm_revision: "v44_bounded_multi_rebuild"`.

After the five free-decay transitions on each arrival side, the Q probe still
uses only the common `A_prev` input-domain intersection reduced by
`cal_q_probe_support_margin_deg`. If the confirmed predecessor is outside this
interval, firmware issues a rebuild toward `cal_q_rebuild_target_peak_deg` and
re-evaluates the next confirmed arrival. It repeats only while the predecessor
remains outside, up to `cal_q_rebuild_max_attempts` (6). Each rebuild uses the
existing inverse-model Q cap, predicted-peak guard, and pulse-width bound;
v44 does not relax them. Once the predecessor enters the valid interval, the
first Q probe is issued at that same zero crossing. Exceeding the bound ends
calibration with failure code 8 (`q_rebuild_attempt_limit`); an empty common
interval remains failure code 7.

## Calibration algorithm revision v45: probe recovery

The RWLOG binary layout remains v41. Metadata writes
`calibration_algorithm_revision: "v45_probe_rebuild_recovery"`.

v45 retains v44's bounded multi-rebuild gate, including its existing Q cap,
prediction guard, pulse-width bound, and six-attempt limit. It fixes the
post-probe path: after any incomplete or rejected Q probe, the next crossing
returns to `Q_REBUILD`. If the confirmed predecessor has fallen outside the
margin-reduced common input domain, a bounded rebuild is performed before the
next probe; it no longer exits immediately with code 7 solely because the
previous probe reduced amplitude.

At a valid crossing the requested probe side is selected from the physical
next-arrival side determined by the gyro rate, not inferred from motor-command
polarity. The metadata still preserves requested and observed sides, making an
unexpected mismatch diagnosable. Gain acceptance and the required two positive
samples remain indexed by the observed arrival side.
## Calibration algorithm revision v46: deterministic signed-Q plan

The binary RWLOG layout remains v41. Metadata writes
`calibration_algorithm_revision: "v46_deterministic_signed_q_plan"`.

The calibration-shadow protocol runs one explicit BASE-polarity plan:
`(-, 0.5), (+, 0.5), (-, 1.0), (+, 1.0), (-, 1.5), (+, 1.5)`, where each pair
is `(desired arrival side, Q target in mA*s)` and `pulse_direction =
-desired_arrival_side`. A non-positive gain is recorded as an observation; it
never triggers an automatic direction reversal.

If the immediate physical arrival at a valid zero crossing is not the planned
side, firmware sends no Q pulse for that half-cycle and rechecks the next
crossing. If the predecessor then leaves the margin-reduced common input
support, it returns through the existing bounded rebuild path without relaxing
the 3.80 mA*s cap, 18 deg prediction guard, or 19 deg abort boundary.

`calibration_protocol_complete` is distinct from `calibration_valid`: completion
means all six planned probes were issued without a terminal scheduler failure;
valid additionally requires three positive in-support gains for both sides.
Neither value changes normal control. Each `calibration_probe_event` records its
plan/Q index, predecessor/requested/observed sides, BASE-polarity verification,
target and predicted-effective Q, rebuild and half-cycle-wait counts, and result
code.
## Calibration algorithm revision v47: 30 ms signed-Q probe ceiling

The binary RWLOG layout remains v41. Metadata writes
`calibration_algorithm_revision: "v47_signed_q_30ms_piecewise_sync"` and
`cal_q_probe_solver_max_pulse_ms: 30`.

Only the calibration Q-probe inverse solver is widened from 25 ms to 30 ms so
the existing deterministic six-condition plan can reach its requested 1.5
mA*s level at the measured 300 mA current response. This setting does not
change the normal controller pulse-width bound. The 3.80 mA*s Q cap, 18 deg
prediction guard, 19 deg abort, BASE polarity rule, support/rebuild/wait
logic, and shadow-only policy are unchanged.

Video/RWLOG analysis for v47 uses accepted start, periodic-mid, and end LED
anchors as a monotone piecewise-linear time map. The recorded middle-anchor
schedule is 2.5 s after t=0 and then every 5.0 s; all accepted anchors are
reported in the comparison summary. A run may still be processed with only
boundary anchors for backward compatibility, but v47 protocol acceptance
requires all expected LED anchors.
## Calibration algorithm revision v48: full-cycle decay and rebuild episodes

The binary RWLOG layout remains v41. Metadata writes
`calibration_algorithm_revision: "v48_fullcycle_decay_rebuild_episode"`.

A mechanically asymmetric foot can have a larger immediate arrival amplitude on
one side even during passive decay. Therefore v48 no longer requires each
half-cycle map `A_next(A_prev)` to satisfy `A_next < A_prev`. Each selected
arrival-side map must instead be finite, positive, and monotone increasing on
its measured `A_prev` input domain. The firmware then composes the two maps for
both same-side full cycles and accepts free decay only when each composition is
positive and strictly contractive (`0 < F(A) < A`) throughout its valid
composition domain. Metadata records the selected half-cycle models, the two
half-cycle monotonicity flags, full-cycle coefficients/domains, and both
full-cycle validity flags. This retains the no-extrapolation rule.

`cal_q_rebuild_max_attempts_per_episode` remains 6 and does not relax the
existing 3.80 mA*s cap, 18 deg predicted-peak guard, 19 deg abort boundary, or
30 ms calibration-Q-probe ceiling. The count now applies independently to one
support-entry/rebuild episode. A probe event records both run-wide
`rebuild_count` and its `rebuild_episode_id` plus
`rebuild_attempt_in_episode`. After a post-pulse Q-probe peak, the episode
attempt count is reset; a later loss of support starts a new bounded episode.
The run-wide total remains in metadata for audit. The deterministic signed-Q
plan, BASE-polarity rule, and shadow-only policy are unchanged.
## Calibration algorithm revision v49: center/half-range shadow comparison

v49 keeps the v48 motor schedule and all safety limits unchanged: 300 mA,
the deterministic 0.5 / 1.0 / 1.5 mA*s signed-Q plan, the 30 ms probe-width
ceiling, the 3.80 mA*s Q cap, the 18 deg prediction guard, the 19 deg abort,
and the per-support-entry rebuild bound. The binary RWLOG layout remains v41.
Metadata writes `calibration_algorithm_revision:
"v49_half_range_shadow_compare"`.

Peak time and side are still detected solely by the adopted
`dynamic_hold073` estimator. At that same candidate physical peak time, v49
also snapshots the `fixed_b100` estimator; it does not independently retime a
fixed-beta maximum. For every consecutive alternating peak pair,

`C_k = (theta_(k-1) + theta_k) / 2`

and

`H_k = abs(theta_k - theta_(k-1)) / 2`

are logged for both estimator coordinates. `C` is a local midpoint, not an
assertion that its change is mechanical rather than estimator-related. `H` is
the half-range evaluated without folding that midpoint shift into an absolute
peak magnitude.

During the initial unforced free-decay sequence, v49 fits separate affine
shadow maps `H_next = r*H_prev + c` for `dynamic_hold073` and `fixed_b100`.
Metadata stores fit range, RMSE, and leave-one-out RMSE. After a Q probe, the
firmware writes `H_prev`, `H_observed`, `H_free_pred`, `Delta_H`, and
`Delta_H / Q_effective` only when `H_prev` is inside that estimator's measured
input range. An outside-range residual is null; it is never extrapolated.

All v49 center/half-range values, fits, and residuals are diagnostics only.
They do not change free-decay acceptance, rebuild choice, pulse width,
direction, Q scheduling, normal control, `calibration_valid`, or any safety
interlock. Acceptance of a future H-based gain must be made offline by
comparing each `Delta_H` with the recorded free-decay LOOCV uncertainty and
with synchronized video.
## Calibration algorithm revision v50: dynamic-H Q-rebuild entry

The binary RWLOG layout remains v41. Metadata writes
`calibration_algorithm_revision: "v50_dynamic_h_q_rebuild_entry"`.

v50 addresses the repeated v49 terminal path in which the legacy
A-coordinate predecessor-domain condition remained outside support despite a
valid dynamic-H predecessor lying inside the measured H free-decay input
range. A next deterministic Q probe is now admitted when either (1) the
legacy common A-prev input domain contains the predecessor with its existing
margin, or (2) the valid dynamic-H predecessor input range contains the
confirmed H value. Neither map is extrapolated.

`calibration_probe_events` add `rebuild_entry_source` (`0=legacy A support`,
`1=dynamic-H support`), `dynamic_h_in_support_at_command`, and
`rebuild_target_reached`. The last field records whether the predecessor
magnitude at command was at least the existing 10-degree rebuild target. That
10-degree value remains the bounded rebuild-pulse setpoint, rather than an
H-support prerequisite.

The legacy A-coordinate fields `in_support` and `gain_deg_per_mA_s` retain
their original meanings and remain null/out-of-support when the A model cannot
be used. Dynamic-H residual fields are separately available for those valid
H-domain probes. v50 does not change 300 mA operation, the signed Q plan, the
30 ms Q-probe ceiling, 3.80 mA*s cap, 18-degree predicted-peak guard,
19-degree abort, normal control, or `calibration_valid`.
## Calibration algorithm revision v51: balanced Q-probe order

The binary RWLOG layout remains v41. Metadata writes
`calibration_algorithm_revision: "v51_balanced_q_schedule_audit"`.

v51 retains v50's dynamic-H Q-rebuild admission rule and all motor/safety
constants: 300 mA operation, 0.5 / 1.0 / 1.5 mA*s Q levels, 30 ms Q-probe
ceiling, 3.80 mA*s cap, 18-degree predicted-peak guard, 19-degree abort, and
normal control are unchanged. It changes only the ordering of the six Q probes
within an explicitly selected schedule:

- A: 0.5-, 0.5+, 1.0-, 1.0+, 1.5-, 1.5+
- B: 1.0-, 1.5+, 1.5-, 0.5+, 0.5-, 1.0+
- C: 1.5-, 1.0+, 0.5-, 1.5+, 1.0-, 0.5+

The alternating arrival-side pattern and BASE direction rule remain unchanged.
Across one A/B/C triplet, every negative-side Q level occupies positions 1, 3,
and 5 once, and every positive-side Q level occupies positions 2, 4, and 6
once. Metadata stores the selected `q_probe_schedule_id`,
`q_probe_schedule_name`, and selected `cal_q_probe_plan`; every
`calibration_probe_event` repeats `q_probe_schedule_id`.

Each probe event also records the adopted dynamic angle and both raw and
gyro-bias-corrected pitch rates sampled immediately before the Q pulse:
`command_dynamic_angle_deg`, `command_rate_raw_dps`, and
`command_rate_bias_corrected_dps`. The existing H fields form the matching
one-event audit tuple: `half_range_previous_dynamic_hold073_deg` (H_prev),
`center_dynamic_hold073_deg` (C), `q_effective_pred_mA_s`,
`half_range_free_pred_dynamic_hold073_deg` (H_free_pred),
`half_range_observed_dynamic_hold073_deg` (H_post), and
`delta_half_range_dynamic_hold073_deg` (Delta_H). `q_effective_pred_mA_s` is
the residual-current-model prediction based on I0 and selected integer width;
it is not a sensor-integrated charge measurement.

These additions are audit-only. They do not alter pulse selection, free-decay
fitting, H support, safety interlocks, `calibration_valid`, or the normal
controller.
## Calibration algorithm revision v52: online-only Delta-H shadow predictor

The binary sample layout and v51 Q-probe schedule remain unchanged. Metadata
writes `calibration_algorithm_revision:
"v52_online_shadow_v51_balanced_q_schedule"` and
`shadow_model_version: "v52_online_54pt_20260821"`.

The historical v51 field `center_dynamic_hold073_deg` is calculated from the
predecessor peak and the post-pulse observed peak. It is therefore an outcome,
not a valid input for an online pulse-time predictor. v52 never uses it as a
model input.

At pulse time v52 captures the following pre-pulse state in each
`calibration_probe_event`:

- `shadow_Hprev_imu_deg`: prior dynamic-H half range.
- `shadow_C_prev_imu_deg`: center of the two already confirmed peaks; audit
  only, not an adopted prediction term.
- `shadow_rate_abs_dps`: absolute gyro-bias-corrected pitch rate.
- `shadow_next_peak_side`: expected next physical peak side (`-1` or `+1`).
- `shadow_q_effective_pred_mA_s`: residual-current-model Q at the chosen
  integer width, not sensor-integrated charge.

The adopted log-only model is:

```text
DeltaH_video_pred = -1.1393659 - 0.4714266*Hprev
                    + 0.0823895*abs(rate) - 0.0961605*next_peak_side
                    + 0.1919561*Q_effective_pred
```

Its identified Q range is 0.503--1.582 mA*s; v52 marks values outside the
implemented 0.50--1.60 mA*s support as invalid for this model. RWLOG contains
`shadow_delta_h_video_pred_deg`, `shadow_H_free_imu_pred_deg`, and
`shadow_H_post_pred_deg`, each with an explicit validity flag. The first is
the comparison target for synchronized video `Delta_H`; it is not an
absolute-next-peak prediction.

`shadow_H_ref_deg`, `shadow_q_req_raw_mA_s`,
`shadow_q_req_clamped_mA_s`, `shadow_q_req_valid`, and
`shadow_q_req_reason` form an inverse-Q audit interface. No half-range
reference has been configured in v52, so the reason is
`reference_unconfigured` and all Q-request values are null. v52 never
substitutes the existing absolute-peak UI target, and none of these fields can
alter Q selection, width, safety guards, calibration, or a motor command.
## Calibration algorithm revision v53: inverse-shadow audit

v53 preserves the binary sample layout, the physical v51 Q-probe schedule, and
the complete Q selector / width / motor-command path. Metadata writes
`calibration_algorithm_revision: "v53_inverse_shadow_v51_video_hfree"` and
`shadow_model_version: "v53_inverse_shadow_v51_video_hfree_20260821"`.
All v53 values are log-only.

The forward Delta-H model is unchanged from v52. v53 additionally uses the
fixed external coordinate map identified from v51 data:

```text
H_free_video_pred = -0.1960 + 0.94286 * H_free_imu_pred
H_post_pred(Q) = H_free_video_pred + DeltaH_video_pred(Q)
```

`shadow_H_post_pred_deg` is the video-half-range post-peak diagnostic at the
actual selected Q when the actual-Q comparison is valid. The separate
`shadow_H_post_pred_q0_deg`, `_q05_deg`, `_q10_deg`, and `_q15_deg` are
candidate diagnostics. Q=0 is marked by `shadow_q0_extrapolated=true`; it is
not model-supported and is never a command.

v53 stores `shadow_H_free_video_pred_deg`, `shadow_direction`,
`shadow_state_in_support`, `shadow_H_post_pred_q*`,
`shadow_H_ref_deg`, `shadow_q_req_raw_mA_s`, `shadow_q_req_region`,
`shadow_q_req_valid`, `shadow_future_control_eligible`, and
`shadow_control_candidate`. The fixed `H_ref` was 6.0 deg through v58; V59 records 6.10 deg in the video
half-range coordinate.

`shadow_state_in_support` uses the v51 forward-model limits
`5.98--8.67 deg` for Hprev and `51.97--73.15 deg/s` for abs(rate). A raw
inverse-Q is still logged outside these ranges when its inputs are available,
but it cannot be valid or future-control-eligible. The raw inverse-Q regions
are `0=unavailable`, `1=below_zero`, `2=within_zero_to_qmax1p5`, and
`3=above_qmax1p5`. Validity also requires the dynamic free-H prediction and a
raw Q in 0.50--1.50 mA*s. `shadow_control_candidate` is always false in v53.

The extra metadata and calibration-probe JSON fields do not change the binary
RWLOG sample format; the existing converter preserves them in `metadata.json`.
## Calibration algorithm revision v54: target-reachability audit

v54 preserves the v53 binary sample layout and the entire physical Q selection,
pulse-width, zero-cross timing, safety, calibration, and motor-command paths.
It changes only metadata and the per-`calibration_probe_event` JSON record.
Metadata identifies
`calibration_algorithm_revision: "v54_reachability_log_only_v53_inverse_shadow"`
and `shadow_reachability_audit_version:
"v54_reachability_log_only_20260821"`. All v54 values are log-only.

For every event, v54 records:

- `shadow_H_supported_min_pred_deg = shadow_H_post_pred_q05_deg`
- `shadow_H_supported_max_pred_deg = shadow_H_post_pred_q15_deg`
- `shadow_href_in_current_control_candidate_reachable_range`
- `shadow_reachability_valid`, `shadow_reachability_reason`, and
  `shadow_recommended_action`
- `shadow_recommended_q_mA_s`
- `shadow_q_support_reason`

The reachability range is the current discrete control-candidate interval
`Q=0.50--1.50 mA*s`, distinct from the forward-model measurement support
`Q=0.50--1.60 mA*s`. When state support, free-H, and candidate predictions are
valid, v54 records the following policy without applying it:

| Target relative to candidate range | `shadow_recommended_action` | Recommended Q |
| --- | --- | --- |
| `H_ref < H_supported_min` | `no_pulse` | `0.0 mA*s` |
| `H_supported_min <= H_ref <= H_supported_max` | `inverse_q` | raw inverse Q |
| `H_ref > H_supported_max` | `saturate_qmax` | `1.5 mA*s` |

State-invalid, state-out-of-support, free-H-invalid, and candidate-invalid
conditions are separate reachability reasons and record `no_pulse`; they are
not treated as Q-support failures. `shadow_q_support_reason` reports Q alone:
`negative`, `below_forward_model_support`,
`within_current_candidates`,
`above_candidates_within_forward_model_support`, or
`above_forward_model_support`.

`shadow_control_candidate` remains false for every event. The extra fields are
metadata-only and do not change the binary RWLOG sample format; the converter
continues to preserve them in `metadata.json`.
## Calibration algorithm revision v57: support-aware inverse-shadow audit

v57 keeps the binary sample format and all physical control paths unchanged. It
sets `calibration_algorithm_revision` to
`v57_support_aware_inverse_shadow_v51_video_hfree` and writes the v51 forward
model identifier plus the v57 audit identifier in metadata. No v57 value is
used for Q selection, pulse width, timing, calibration, safety, or a motor
command.

Each `calibration_probe_event` adds:

- `shadow_h_in_support`, `shadow_rate_in_support`, and
  `shadow_state_in_support` for the fixed v51 state support.
- `shadow_q_req_in_model_support` (0.50--1.60 mA*s),
  `shadow_q_req_in_control_candidate_range` (0.50--1.50 mA*s), and
  `shadow_q_req_candidate_valid`.
- `shadow_v57_q_req_reason`: `0=unavailable`, `1=valid`,
  `2=reference_unconfigured`, `3=state_invalid`,
  `4=state_out_of_support`, `5=h_free_invalid`,
  `6=q_below_candidate_range`, `7=q_above_candidate_range`, or
  `8=reference_unreachable`.
- `shadow_delta_h_imu_actual_deg`,
  `shadow_delta_h_pred_minus_imu_dynamic_deg`, and
  `shadow_imu_proxy_residual_valid`.

The last three are an on-device comparison against dynamic-H IMU data. They
are explicitly not a synchronized video accuracy result; analysis joins video
Delta-H offline using the event `probe_index`.
## Calibration algorithm revision v58: inverse-Q repeatability state gate

v58 keeps the v57 support-aware inverse-shadow audit, binary RWLOG sample
format, Q-probe schedule, pulse timing, Q selector, and all motor-command
paths unchanged. It only labels an analysis subset in every
`calibration_probe_event`; no v58 field can select Q, alter pulse width, or
command the motor.

The configured state gate is `6.40 <= shadow_Hprev_imu_deg <= 6.80` deg,
`57.50 <= shadow_rate_abs_dps <= 60.50` deg/s, and
`shadow_next_peak_side = +1`. Each event adds:

- `shadow_v58_repeatability_gate_passed`.
- `shadow_v58_repeatability_gate_reason`: `0=unavailable`, `1=passed`,
  `2=disabled`, `3=state_invalid`, `4=hprev_below`, `5=hprev_above`,
  `6=rate_below`, `7=rate_above`, or `8=direction_mismatch`.

The existing v57 fields remain recorded for both passed and non-passed events.
The gate is for collecting repeated inverse-Q candidates around the two
observed valid-inverse states, not an indication that an inverse-Q value is
being applied.
## Calibration algorithm revision v59: state-wait fixed-Q response measurement

The binary sample layout remains RWLOG v41. V59 changes the physical
calibration-Q measurement protocol only: it waits at each zero crossing for a
measured pre-pulse state, then issues the already scheduled fixed Q. It is not
an inverse-Q controller. The planned Q index advances only for an accepted
fixed-Q command; unforced waits and rebuild pulses do not advance it.

The metadata revision is `v59_state_wait_fixed_q_v51_video_hfree`. The V59
pre-pulse gate requires the measured per-run dynamic-H support plus
`Hprev=6.50--6.75 deg`, `Cprev=0.70--1.30 deg`, `abs(rate)=58.00--60.50 deg/s`,
and next-peak side `+1`. Cprev's first window is based on the usable low-H V58
neighbourhood and is an explicit quantity for subsequent re-identification.

`calibration_state_gate_events` records every candidate crossing, including:
`planned_probe_index`, `q_level_index`, `reason`, `action`, cumulative wait and
rebuild count, crossing time, `Hprev_imu_deg`, `Cprev_imu_deg`, rate, direction,
state validity, and dynamic-H support. `action` is `0=wait_no_fixed_q`,
`1=accepted_fixed_q`, `2=low_state_or_v61_controlled_side_rebuild`, or
`3=v60_cooldown_free_decay`. Reasons are `0=unavailable`, `1=passed`,
`2=disabled`, `3=state_invalid`, `4=dynamic_h_out_of_support`,
`5=hprev_below`, `6=hprev_above`, `7=cprev_below`, `8=cprev_above`,
`9=rate_below`, `10=rate_above`, `11=direction_mismatch`, or
`12=v61_state_feedback_wait_or_rebuild`.

Each `calibration_probe_event` additionally records `v59_block_index`,
`v59_block_order`, `v59_state_gate_reason`, and `v59_state_gate_passed`.
Run-level counters are `v59_gate_event_count`, `v59_gate_pass_count`,
`v59_gate_skip_count`, and `v59_rebuild_from_low_state_count`. The 12-event
V59 schedule is deliberately positive-arrival only, so the historical two-side
`calibration_valid` summary can remain false even when the V59 protocol itself
is complete; use `calibration_protocol_complete` and the V59 counters for this
measurement.
## Calibration algorithm revision v61: state-feedback rebuild phase

V61 keeps V59's binary sample layout, fixed-Q plan, and narrow pre-pulse H/C/rate/direction gate. It changes only the transition into that plan. After at least six free-decay transitions per arrival side (and an unforced wait of at most eight retained transitions for dynamic-H support), it uses the measured side-specific maps. For the fixed-Q positive arrival, it solves one controlled negative peak followed by free positive and negative peaks so that the predicted post-cooldown `H=(Apos+Aneg)/2` and `C=(Apos-Aneg)/2` are inside the unchanged gate. Both model inputs must remain within their measured supports; no model is extrapolated.

In the current user-declared safe test setup, V61 commands only that controlled side with full model-inverted Q and width, without the former Q cap, prediction guard, width upper bound, or rebuild-attempt limit. An observed controlled peak schedules the two unforced half-cycles only if its modeled post-cooldown H/C are both in gate. Otherwise the next same-side command target is corrected by the observed peak error; V61 does not insert an opposite-side rebuild. Run metadata adds `v61_state_target_valid`, `v61_predicted_gate_h_deg`, `v61_predicted_gate_c_deg`, `v61_feedback_command_peak_deg`, and `v61_feedback_correction_count`. `v60_rebuild_target_observed` now means that a controlled observed peak predicted an in-gate H/C state, not merely that its raw magnitude exceeded a target.
## Calibration algorithm revision V62: joint H/C/rate feasible-state selection

V62 retains the V59 fixed-Q gate and V61 controlled-side rebuild sequence, but removes the H/C gate-center assumption. During unforced free decay it records positive-arrival `(Hprev,Cprev,abs(rate))` states and fits `abs(rate)=aH+bC+c` with at least four samples. It scans the side-specific free-decay locus only inside the measured side inputs, dynamic-H input domain, and observed rate-model H/C inputs. The selected target maximizes interior H/C margin while requiring its predicted rate to remain in the unchanged `58.00--60.50 deg/s` gate. Metadata records `v62_rate_*`, `v62_hc_target_feasible`, `v62_state_target_valid`, and the selected H/C/rate/controlled-peak target. Failure reason 11 denotes a valid free-decay model with no supported H/C target; reason 12 denotes an unavailable/weak rate model or no joint H/C/rate target. These are state-generation outcomes, not fixed-Q response outcomes.

## RWLOG v42: Current Roll static-calibrated display

v42 keeps every v41 field in its original order and appends 12 packed bytes to
`LogSample`; the v42 sample size is therefore **158 bytes**.  v41 remains a
146-byte format and must continue to be decoded using the v41 layout.

The appended v42 values are:

- `physical_roll_abs_cdeg` — static calibrated physical roll in the fixed
  gravity frame.  It uses
  `candidate_deg = atan2(ax_g, hypot(ay_g, az_g))*180/pi`, then
  `0.9278941864271074*candidate_deg - 0.49830848087090135`.
- `current_roll_cdeg` — `physical_roll_abs_deg - display_zero_offset_deg`.
  ZERO is display-only and is never an IMU, video, RWLOG absolute-angle, or
  Model B coordinate reset.
- `physical_roll_rate_cdps` — `gy_dps - startup_gyro_bias_y_dps`.  Positive
  is the selected physical y-axis direction.
- `target_roll_cdeg`, `target_error_cdeg` — the requested display target and
  `current_roll_deg - target_roll_deg`.
- `static_confirmed`, `ready` — byte booleans.  STATIC requires the
  bias-corrected physical rate to remain within 0.5 deg/s for 500 ms.  READY
  requires STATIC and `abs(target_error_deg) <= 0.20` deg.

The angle is an accelerometer/gravity estimate with static-only confidence.
`STATIC=NO` does not invalidate the raw IMU record, but it explicitly
suppresses a claim that `physical_roll_abs_deg` is a validated dynamic angle.
The bias-corrected `+gy` rate is the primary STATIC/READY decision variable;
its all-dynamic-range correspondence to fixed-horizon video remains a separate
validation task.

Target, ZERO, STATIC, and READY are UI/logging only.  They are not connected to
motor commands, current settings, pulse width, Q/rebuild/cooldown, or feedback
control.  In passive capture every row must still record `motor_cmd_mA=0`,
`current_mA_setting=0`, and `pulse_active=0`.

`tools/convert_rwlog_to_csv.py` accepts v23-v27, v29-v41, and v42.  The v42
CSV adds `physical_roll_abs_deg`, `current_roll_deg`,
`physical_roll_rate_dps`, `static_confirmed`, `target_roll_deg`,
`target_error_deg`, and `ready`.  Run
`python tools/test_rwlog_v42_converter.py` for a synthetic CRC-verified v41
compatibility and v42 conversion test.

## RWLOG binary format v46: MEKF adopted + dynamic-beta Madgwick comparison

RWLOG v46 appends 36 bytes to the v45 `LogSample`, increasing the packed sample size from 190 to 226 bytes while preserving the complete v45 prefix. It adds the adopted/run-relative MEKF pitch, continuous MEKF pitch, continuous adopted dynamic-beta Madgwick pitch, quaternion, MEKF gyro-bias estimate, adaptive-accel diagnostics, IMU update timing, and the adopted-filter identifier.

`tools/convert_rwlog_to_csv.py` remains backward compatible with v44/v45 and emits the new v46 columns. The LED fields and synchronization event IDs retain their previous binary locations inside the v45 prefix.
