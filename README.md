# Current integrated build: Free-foot Runtime V2 / 0.47.19 interrupt-driven IMU

[Web flasher](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/) ·
[0.47.19 IMU waiting and control latency](docs/CONTROL_LATENCY_04719.md) ·
[0.47.17 five-run baseline and direct-Q inverse](docs/DIRECT_Q_INVERSE_04718.md) ·
[0.47.16 hardware results and exact-input calculation reuse](docs/CONTROL_CACHE_PREVIEW_04717.md) ·
[0.47.15 hardware result, targeted IRAM and weak candidate confirmation](docs/IRAM_AND_MARKER_GUARD_04716.md) ·
[0.47.14 recovered run and deferred stack diagnostics](docs/RECOVERED_RUN_STACK_TIMING_04715.md) ·
[0.47.13 startup ESTOP and compact encoder correction](docs/LOG_ENCODER_REGRESSION_04714.md) ·
[0.47.12 hardware result and log encoder follow-up](docs/LOG_ENCODER_TIMING_04713.md) ·
[0.47.11 hardware result and filter timing follow-up](docs/FILTER_TIMING_04712.md) ·
[0.47.10 hardware result and deferred attitude display](docs/DEFERRED_ATTITUDE_TIMING_04711.md) ·
[Successful free-foot run and timing follow-up](docs/FREEFOOT_SUCCESS_TIMING_04710.md) ·
[Follow-up ESTOP and MEKF work reduction](docs/ESTOP_WORK_REDUCTION_0479.md) ·
[ESTOP analysis and control work profiling](docs/ESTOP_BACKLOG_0478.md) ·
[USB diagnostic procedure](docs/USB_DIAGNOSTICS.md) ·
[Fixed-pose foot calibration](docs/FOOT_CALIBRATION_0477.md) ·
[Static-pose comparison and yaw diagnostics](docs/POSE_COMPARISON_0476.md) ·
[Observed range update and all-axis MEKF diagnostics](docs/RANGE_MEKF_DIAGNOSTICS_0475.md) ·
[Marker identity, zero quality and image diagnosis](docs/MARKER_IDENTITY_0474.md) ·
[Coordinate audit](docs/ROBOT_COORDINATE_AUDIT_20260924_JA.md) ·
[Earlier marker-loss analysis](docs/MARKER_TRACKING_0473.md) ·
[Instability review](docs/INSTABILITY_REVIEW.md) ·
[Runtime architecture](docs/FREEFOOT_RUNTIME_V2.md)

The current main branch integrates observation-only right/left foot angles with
the V46al-R2 controller. It uses permanent control ownership, PSRAM event storage,
and a resumable, CRC-verified RWLOG export. **Version 0.47.9 completed a 30-second
free-foot hardware run with MEKF, camera foot observation and actuation together.**
All 12,143 acquired IMU samples were delivered, with no queue drops or sequence
gaps. Both feet were observed during the same run (288/292 right, 291/292 left).
The user reports fore/aft movement with unfixed feet and increased foot-switching
loss; the observed amplitude asymmetry is an improvement topic, not a failure of
this integration milestone. Strict 2.5 ms processing deadlines remain unmet in
947/12,144 completions (maximum 6,884 us).

**Version 0.47.10 also finished 30 seconds with MEKF, camera and actuation.**
All detected feet were within the configured support (right 282/286 valid,
left 285/286 valid during measurement). However, the 2.5 ms overruns were
1,007/12,145 (8.29%, maximum 7,589 us), so this hardware run did not show timing
improvement. No queue drops or delivery sequence gaps were recorded; acquired
and delivered audit counts differ by one at the measurement boundary.

**Version 0.47.11 again finished the 30-second integrated run.** During measurement,
right/left detections were 291/293 and 293/293, with no detected observation outside
support. The expanded right support accepted 107 valid positions above the former
173-pixel upper bound. The weighted filter mean fell from 779.529 to 730.169 us,
and attitude extraction from 156.681 to 105.037 us. However, total 2.5 ms overruns
increased to 1,127/12,145 (9.28%), despite the maximum falling to 6,235 us.
This is partial work reduction, not completion of the real-time target.

**Version 0.47.12 again completed the 30-second integrated run.** Acquisition and
delivery audits both counted 12,145 samples, with no queue drops, sequence gaps,
or fault. Right/left feet were valid in 287/293 and 293/293 measurement frames;
no detected position was outside support. Deadline overruns fell to 806/12,145
(6.64%), and the completion mean to 1,417.280 us, but the maximum rose to 7,170 us.
The pulse-active/fresh-accel group had 312/1,293 overruns (24.13%). Partial timing
improvement is observed; the strict deadline and rare long delays remain unresolved.

**Version 0.47.13 regressed: measurement stopped after 87,904 us with ESTOP.**
The delivery age reached 10,682 us while sensor polling remained regular. The
first pulse log took 935 us versus 108 us in the previous run; pulse-on log
encoding averaged 836.552 us. All 47 foot frames belong to START_SYNC, so this
run does not validate simultaneous foot observation during measurement.

Version 0.47.14 removed forced per-field inlining and uses one shared quantizer
with the size-oriented compiler policy. The exact numerical formula, all 258
row bytes, logging rates and synchronous storage remain unchanged. A linked
code-size gate prevents recurrence of the expanded encoder, but is not timing
proof. The 10 ms stale-data stop and 2.5 ms completion deadline are unchanged.
MEKF, Madgwick, camera, calibration and control are retained.

**Version 0.47.14 recovered: the new hardware run finished 30 seconds.** All
12,144 acquired samples were delivered, with no queue drops, sequence gaps or
fault. MEKF was adopted throughout measurement, with right/left foot detections
in 283/288 and 287/288 frames and no valid observation outside support. The
2.5 ms deadline remains unmet in 1,045/12,145 completions (8.60%, mean 1,513.601 us,
maximum 6,905 us). The overrun rate is worse than 0.47.12 (6.64%); recovery from
the startup ESTOP is not a timing-goal success. Acquisition and completion
audits use different transition boundaries, explaining the one-count difference.

Version 0.47.15 defers the control and IMU tasks' periodic stack-watermark scans
through START_SYNC, RUNNING and END_SYNC. Each task resumes its own due scan when
idle; heartbeats continue and USB diagnostics expose the cache age, scan count,
allowed flag and last/maximum scan wall time. This removes known diagnostic
memory walks from real-time owners. The previous profile does not isolate their
contribution, so it does not establish them as the cause of all deadline overruns.
The RTC journal layout, overflow protection, estimator/control math, foot
calibration, logging rates and deadline definitions are unchanged. Callback
exclusion, idle resumption, clock wrap and the existing runtime suite are checked.

**Version 0.47.15 also finished 30 seconds.** Acquisition, delivery and completion
audits all counted 12,144 samples, with no queue loss, sequence gaps or fault.
Overruns improved to 942/12,144 (7.76%) and mean completion to 1,449.287 us,
but the maximum increased to 7,720 us. The pulse-plus-acceleration group still
averaged 2,628.506 us. Without USB stack-scan measurements or a controlled repeated
comparison, the improvement cannot be attributed entirely to scan deferral.
Right/left feet were valid in 294/295 and 295/295 measurement frames. One right
observation at X=204.94246 was outside support; it also jumped in X/Y while
contrast and weight collapsed, making feature misidentification a concern.

**Version 0.47.16 finished three 30-second runs.** Across 36,423 measured
completions, 311 exceeded 2.5 ms (0.854%, maximum 4,365 us), with no queue drops
or delivery sequence gaps. During measurement, right/left feet were valid in
872/879 and 877/879 frames, with no detected observation outside support.
Normal pulse starts account for 112 of the 132 runner-only overruns.

**Five new 0.47.17 runs completed 30 seconds each.** The comparison baseline is
512/60,706 deadline overruns (0.843%, maximum 5,742 us), with no IMU queue drops
or delivery sequence gaps. Foot support exceedances were zero on both sides.
Right/left measurement detections were 1,444/1,462 and 1,462/1,462.

Version 0.47.19 gives the exclusive internal I2C1 bus to the ESP-IDF interrupt
completion driver after M5Unified boot configuration and sensor validation.
The high-priority reader blocks during transfer completion, allowing control
on the same core to run. The existing 1 ms polling and 400/200 Hz ODR remain.
Autonomous comparison Madgwick uses captured pre-decision data/beta and runs
in the same sample after the control decision, before logs and publication.
The complete runner deadline still includes comparison and logging. Latency
metadata links sample sequences to receive, MEKF, decision and completion,
with I2C/poll overlap explicitly labeled as wall time rather than CPU time.
The 10 ms stale-data stop remains independent of the IDF driver's potentially
longer bus-error watchdog. Hardware operation/timing needs measurement.

Version 0.47.18 directly inverts the amplitude model into a continuous requested
Q, clips feedforward before the existing side integral, then solves the current
integral for one integer-ms pulse. Both signs of charge are handled when residual
current opposes the command. Final quantization minimizes absolute Q error;
equal errors choose the shorter pulse including zero. The prior intermediate
feedforward width rounding and repeated angle/energy candidate calculations are
removed. Same-state replay of the five runs changes 74/346 widths by exactly
1 ms and keeps 272 unchanged; upper/lower saturation flags match throughout.
The active production decision block is tested against a full 101-width Q-error
oracle, including invalid inputs, reverse-current branches and domain guards.
RWLOG v51 layout is retained; solver audit schema 2 explicitly identifies the
new field meanings and unavailable feedforward width. This is an intentional
arithmetic change, not a claim of identical physical control or achieved timing.
MEKF, foot calibration, 3 ms compensation, 300 mA/100 ms limits, acquisition and
logging rates, task priorities and the 10 ms stale-data stop are retained.

Version 0.47.17 reuses charge/energy predictions only within one zero-cross
selection and caches current-model results only for exactly matching inputs.
Both searches, target corrections, tie-breaking and the zero-output baseline
are retained. All 208 recorded decisions and 20,000 additional cases match the
frozen controller; candidate physics evaluations fall from 9,152 to 4,253 in
that recorded replay. This is a work-count reduction, not a hardware timing
claim. Camera foot tracking and recording continue while unused run-time
thumbnail copies are suppressed. Idle preview images retain paired timestamps
and attitude. The full 0.47.16 build's actual M5GFX dependency is pinned to
0.2.30. Filter/control/log rates, calibration and timing guards are retained;
changed command latency and real motion still require hardware verification.

Version 0.47.16 places selected MEKF routines and the compact log encoder in IRAM,
with a real-ELF placement and 16 KiB function-code gate. Numerical formulas and
compiler policies stay unchanged; external calls and constants can still use
flash, so this is not cache-disabled safety or a deadline guarantee. The marker
tracker now requires the existing three-frame confirmation if a recent candidate
jumps at least 20 px in X and 16 px in Y while contrast drops below 50% and weight
below 10% of its last accepted value. Strong motion and dimming at the same
location remain immediate. The suspect single frame is not used to expand
calibration; persistent confirmed observations can still be reported outside
support. These criteria select only that suspect frame among the seven supplied
run CSVs, but source images are absent and new hardware results are still needed.

Version 0.47.12 computes comparison Madgwick pitch directly from its public
quaternion using the exact upstream 2.4.0 expression, avoiding unused roll/yaw
and gravity calculations. The MEKF numerical file uses GCC O2 without fast-math;
the full estimator and filter rates remain unchanged. Input cohorts now partition
the existing completion counters by pulse state and fresh acceleration, since
workload composition also changed between runs. Pinned upstream getter comparisons,
full MEKF differential tests under both host optimization levels, and the existing
runtime regression suite pass. Installed Madgwick sources are checked during the
target build. The subsequent hardware result and remaining work are described above.

Version 0.47.11 retained the full posterior quaternion on every filter update,
extracts only control pitch in the control path, and derives all three display
angles from the copied quaternion when serializing diagnostics. Two `atan2`
calls per control update are removed without lowering the MEKF update rate or
dropping roll/yaw diagnostics. Frozen camera snapshots keep their own attitude
and timestamp. Control pitch and display axes match the frozen reference,
including normalization and pitch near +/-90 degrees. The full covariance,
other filter work, control limits, logging and deadline endpoints are unchanged.
The subsequent hardware result and remaining work are described above.

Version 0.47.10 extended accepted marker support to right [39,182] and left
[37,177.5] pixels, covering all supplied identity-v2 observations with at least
one pixel of margin. Scale coefficients and independent per-boot zeroing remain
in use. MEKF fixed-size inner products are unrolled and identical Joseph factors
are reused; bounded log quantization preserves the previous encoded integers.
MEKF prediction, correction, attitude extraction and Madgwick comparison times
are separately recorded inside the filter profile. The subsequent hardware
result and next timing change are described above; host tests are not deadline proof.

Version 0.47.9 followed another delivery-backlog ESTOP on 0.47.8 at 0.521 seconds.
The added profile shows pulse-on filter updates averaging 1,007 us and control
iterations averaging 2,615 us. The MEKF now omits known-zero and identity products
while retaining all covariance terms, Joseph update and summation order. Snapshot
strings use bounded copies. A frozen 0.47.8 differential reference verifies the
posterior, diagnostics and all 36 covariance entries. The subsequent hardware
success and remaining timing work are described above. Foot observation, filter
rates and control limits are retained.

Version 0.47.8 follows a hardware ESTOP about 0.77 seconds into measurement:
IMU delivery age reached 10,284 us and exceeded the unchanged 10,000 us guard.
It reuses duplicate pose geometry calculations and records per-stage wall times
separately for pulse-on and pulse-off work, including control snapshot publication.
RWLOG metadata now includes the terminal state/current command and actual runtime
version. The supplied log shows age rising during pulses, but does not isolate
one bottleneck. This is a diagnostic build; resolution on hardware is unverified.
The foot calibration, controller, acquisition sequence and protection limits
remain unchanged. See the linked analysis for evidence and one-run retry steps.

Version 0.47.7 updates the independent foot-angle scales using the two supplied
poses captured without hand contact: a 20.2764165-degree body roll change and
right/left marker displacements of 130.69222 / 130.39768 pixels. New scales are
0.155146317 / 0.155496758 deg/px. Applied to the two hand-supported poses held out
from the fit, both sides have residuals of about 0.26--0.32 degrees. This is a
provisional MEKF-referenced calibration, not independently established absolute
accuracy; one held-out pose has a 7.36-degree yaw change. Per-boot independent
zeros and pixel support bounds remain in use. Normal status, frozen images and
RWLOG share the new calibration metadata and source-file hash.

Version 0.47.6 adds a pre-measurement static-pose comparison in the device WebUI.
Record an upright baseline and several held fore/aft tilts and return poses. Each
pose checks five frozen frame/IMU snapshots over about three seconds and saves the
last CRC-verified image. The display compares changes in relative foot angles
and body roll; other-axis changes are flagged. All-axis MEKF inputs and estimated
gyro bias are available in regular and image diagnostics. A bounded browser-side
status history accompanies the comparison JSON. Reboots, changed zeros, movement,
stale inputs and failed transfers cannot silently replace the baseline.

The latest 0.47.5 hardware pair changes body roll by -20.434 degrees, foot angles
by +21.617 / +21.066 degrees, and yaw by +8.446 degrees. These two points do not
establish new slope coefficients. The added comparison is for collecting repeated
poses and identifying the remaining error; it does not apply a calibration fit.
A synthetic pure fore/aft out/hold/return replay with accelerometer corrections
keeps yaw below 0.02 degrees; actual motion and estimator drift still need hardware
evidence. See the procedure above. Control math and RWLOG v51 are unchanged. The later 0.47.7 fixed-pose fit is described above.

Version 0.47.5 incorporates the new 0.47.4 hardware observations. Both feet remain
detected through 781 captured frames, and the tilted status reports right 21.7630°
and left 21.0887°. The observed X positions slightly exceed the old support, so
the accepted ranges extend to right [40,173] and left [39,177.5] pixels, with a
one-pixel minimum margin around the new low endpoints. Original support and the
unvalidated extension are explicit in diagnostics; angle slopes are not refitted.
The WebUI's shaded ranges are read from the same firmware metadata.

Diagnostic JSON now includes MEKF roll, pitch, yaw and quaternion from a coherent
posterior snapshot, with validity and sample age. These are available both in
ordinary diagnostics and alongside an image's saved delivery-time control state.
No filter update or sensor access is performed by HTTP. The main side-to-side
control angle keeps its existing reference and 3 ms compensation. The MEKF frame
is preserved; yaw is relative to filter initialization, not compass heading.

Version 0.47.4 addresses a plausible source of the remaining left/right angle
mismatch: a weaker nominal-row white region can be accepted as the left zero
before a stronger displaced marker is examined. All 17 scan heights now compete,
separate white islands stay separate, and ambiguous candidates or implausible
track jumps are reported as invalid. A neutral-position envelope and stable
marker positions gate automatic zero acquisition; the two zero values are still
measured independently. A synthetic distractor case improves from right 22.99° /
left 10.90° to right 22.99° / left 22.77°. This is a reproduced software failure
pattern, not proof of the hardware diagnosis or calibrated angular accuracy.

The device WebUI can capture one diagnostic image while measurement is stopped.
Its selected positions, candidates, reasons and zeros belong to the same frame,
transferred as a frozen 160×120 grayscale image in CRC-checked chunks. Save the
image and metadata together with “画像付き診断を保存”. Capture an upright frame
and a frame with both feet fixed while the body is tilted fore/aft to inspect the
actual marker choice. The UI identifies MEKF as body side-to-side rocking and
foot angles as body-relative; MEKF axes, gyro calibration and control math are unchanged. Foot
angle slopes use the 0.47.7 fixed-pose fit described above. The user's fore/aft trial does not make the small
side-to-side MEKF value a failure. Real-image identity and angle accuracy still
need hardware confirmation; see the linked procedure and limitations.

The 0.47.2 camera stack fix remains active. Subsequent USB logs extend to 237.5 s
without another reset, and the user reports stable WebUI updates. Frame-size
mismatches and sporadic camera event overflows still occurred; this is not proof
of indefinite stability. See [the matched backtrace and fix](docs/CAMERA_STACK_PANIC_0472.md).
The eight-second USB startup window, automatic monitor reconnection, RTC reset
evidence, and browser recovery from malformed status data remain available.

The earlier implementation notes below are retained as history.

---

# AtomS3R Amplitude Control Development

AtomS3Rを用いたリアクションホイール系の**振幅制御改善**を進めるための開発リポジトリです。

このリポジトリは、姿勢推定・計測系の検証を行った
[`atoms3r-mekf-dynamic-validation`](https://github.com/temesotejam/atoms3r-mekf-dynamic-validation)
の最終状態 **V46aj / 0.46.35** を、そのまま初期ベースラインとして引き継いで開始しました。

## 開発の出発点

- 初期ファームウェア: **V46aj / 0.46.35**
- 姿勢推定: **6-state MEKF**
- ZEROクロス遅延補償: **3 ms固定**
- 次ピーク基準振幅: **ZEROクロス角速度と進行方向による rate-only model**
- RWLOG: **v51**
- 元リポジトリのV46aj実装コミット: `1e9fb1bfdd8261bedc6657158c27142921848939`
- 元リポジトリの凍結時点: `1af3031d068a483361f0b519c35a826999040719`
- インポートしたTree: `b90c4314bb296b66dc2f44cbf80228605f0ee6a9`

初回インポート時点では、ソース、ドキュメント、テスト、GitHub Actions、Web flasherを含むTreeが元リポジトリと完全一致しています。

## このリポジトリで進めること

姿勢推定そのものを主題に戻すのではなく、現在のMEKF計測を土台として、主に次を検討します。

- ZEROクロス状態からの**次ピーク振幅予測の改善**
- 予測誤差と実測ピークを使った**入力Qの決定方法の改善**
- 正負半周期の差や状態依存性の整理
- 目標振幅へ収束させる**振幅制御則の改善**
- 実機RWLOG・動画を用いたモデル／制御則の検証

新しいモデルや制御則は、V46ajベースラインとの差が追える形で追加します。
元の `atoms3r-mekf-dynamic-validation` は保存版として維持し、今後の開発変更はこのリポジトリ側で行います。

## ベースラインで確認済みのこと

2026-09-19のV46ai実機測定（目標8°、遅延補償3 ms、30秒）では、

- 動画とファームウェアのピーク **69個がすべて一対一で対応**
- 10〜30秒の46ピークで、MEKFと動画のピーク差 **RMSE 約0.103°**
- 同区間の目標8°に対するMEKFピークのRMSE **約0.643°**

でした。

この結果から、**実際の揺れを捉える姿勢推定誤差と、目標振幅へ揃える制御誤差を分けて扱う**ことを、この開発の出発点とします。

詳細:
- [現在の角度推定](docs/ATTITUDE_ESTIMATION_V46AI_JA.md)
- [V46aj: 3 ms固定化](docs/V46AJ_FIXED_3MS.md)
- [V46ai: rate-only次ピーク予測](docs/V46AI_RATE_ONLY_BASELINE.md)
- [元リポジトリ最終スナップショット](docs/FINAL_SNAPSHOT_20260920_JA.md)

## 現在の開発版: V46al-R2 / 0.46.42

実機確認済みの `atoms3r-amplitude-control-v46ak-stable` を戻り基準とし、
V46akのZEROクロス rate-only予測に **直前ピーク `A_prev` の残差補正だけ**を追加して、
その補正後の `A_free` を既存のQ決定へ渡す実制御検証版です。

補正は **target=8°、t>=10 s、side別の5 Run測定support内**でのみ有効です。
それ以外ではV46akのrate-only予測へそのままフォールバックします。

変更しないもの:
- 6-state MEKF
- 3 ms固定ZEROクロス補償
- ZEROクロス/ピーク/side判定
- 電流・I0モデル
- 既存Q solverとKi
- 300 mA / 最大100 ms
- ESTOPと安全条件
- V46akの入力直前実電流・ホイール速度観測
- RWLOG logger/metadata生成・v51時系列レイアウト
- 現在のRWLOGダウンロード経路（logger.cpp/h と converter はstableと完全一致）

基準stable: `atoms3r-amplitude-control-v46ak-stable@bb9c5ed07c5ca8b3c6c6b5813b6c2f1b1f57a6ec`

- [V46al-R2の変更内容](docs/V46AL_R2_PREVIOUS_PEAK_ACTIVE_CONTROL.md)
- [V46akの観測追加内容](docs/V46AK_PRE_INPUT_STATE_OBSERVATION.md)
- [V46ajの確定済み角度推定](docs/ATTITUDE_ESTIMATION_V46AI_JA.md)

## 現在のWeb flasher

[AtomS3R Web flasher](https://temesotejam.github.io/atoms3r-amplitude-control-development/)

現在は **V46al-R2 / 0.46.42** を書き込みます。

---

## Frozen control baseline: V46aj / 0.46.35

Autonomousの遅延補償は **3 ms固定**です。0・6・9 msへの切り替え、選択画面、設定APIはありません。
ZEROクロス判定には、MEKF角度をバイアス補正済み角速度で3 ms先へ進めた角度を使います。
次ピーク予測は最初の通常判断から角速度式を使用し、直前ピークは予測式の入力にしません。
MEKF、ピーク追跡、Qゲイン、Ki、300 mA・最大100 msの出力上限はV46ajベースラインのままです。

以下は過去の構成・検証の記録です。

---

# V46 MEKF + Dynamic-beta Madgwick synchronized validation build

This working firmware is derived from the V45 current-audit / Autonomous Energy Control V7 build.
The **adopted control/detector attitude is now a 6-state MEKF**. During the Autonomous V7 capture, only the adopted **dynamic-beta Madgwick hold-073** filter is kept online as a comparison estimator. It never commands the motor.

Key validation behavior:

- MEKF drives zero-cross, peak detection, and Autonomous V7 angle-dependent decisions.
- Dynamic-beta Madgwick is logged online for later comparison only.
- Raw accelerometer/gyro, gyro-only integration, and accel-only atan2 remain logged for offline analysis.
- The existing GPIO38 `LED_SYNC_PATTERN_V2_LOGGED_ANCHORS` START/MID/END video synchronization is unchanged.
- RWLOG binary format is **v46**, append-only over the complete v45 sample prefix.
- For video comparison use the continuous columns `pitch_mekf_abs_deg` and `pitch_madgwick_dynamic_abs_deg`; `pitch_mekf_control_deg` is the run-relative angle used by control.
- MEKF acceleration trust diagnostics (`mekf_accel_confidence`, residual, magnitude error, `mekf_accel_used`) are logged on every sample.

See `docs/MEKF_DYNAMIC_COMPARE_V46.md` for the validation-specific log map and checks. Historical documentation below is retained because the control/current-audit logic is inherited.

---

# Q1 direct next-peak shadow passive logger

This derived firmware implements the current online-shadow candidate, **Q1**, and is strictly motor OFF. It records manual motion, detects zero-cross states, predicts the next video-coordinate absolute peak at Q=0, and records the inverse request that would be required on the canonical `q_target_mA_s` axis. It never applies that request.

The source project `2026_08_24_v62_passive_free_decay_logger` remains unchanged. E2/gyro half-range work remains an offline diagnostic result and is not in the Q1 validity or calculation path.

## Fixed Q1 model

```text
A_next_abs = 0.01304 + 0.08812 * abs(physical_roll_rate_dps)
             + 0.11858 * physical_next_peak_side + g_side * q_target_mA_s

g_plus  = 0.29032 deg/(mA s)
g_minus = 0.25455 deg/(mA s)
```

The physical rate is official `+gy` after startup y-bias subtraction. The adopted-filter detector angle is anti-correlated with `+gy`, so the fixed acceptance rule is: detector `- -> +` requires `+gy < 0`; detector `+ -> -` requires `+gy > 0`. At an accepted zero-cross, the current-sample `+gy` sign determines `physical_next_peak_side`: positive rate means physical `+1` next peak, and negative rate means physical `-1` next peak. The detector coordinate, its measurement-start reference, 0.08 deg rearm, and 200 ms event interval are unchanged.

The Q=0 baseline is calculated directly from Q1; E2 `H_next` and an IMU absolute-peak reconstruction are not used.

## Safety boundary

- `Q1_SHADOW_MOTOR_OFF_ONLY=true` is independent of the UI. `serviceFast()`, passive start, and the retained legacy `beginPulse()` all force motor command, current setting, pulse width, and pulse-active state to zero.
- `Q_req_shadow` is metadata only. No Q1 function calls motor/current/pulse generation.
- The target is locked while a run is active.
- Negative or zero required augmentation is `INVALID_BRAKING_NOT_IDENTIFIED`; no negative Q is calculated.
- Q is not clipped. The nonzero canonical support is `0.454 <= q_target_mA_s <= 1.197`. Outside it the event is `INVALID_Q_BELOW_SUPPORT` or `INVALID_Q_ABOVE_SUPPORT`.
- The rate support recorded from the canonical dataset is `1.68--27.80 deg/s`; an accepted detector crossing outside it is retained with a rate-support INVALID reason rather than extrapolated.

## Run procedure

1. Wait for `READY`. ZERO and Current Roll target remain display-only.
2. Before starting, set **Target next peak |A|**. It is an absolute peak target for Q1 shadow, separate from Current Roll. `0.0` intentionally produces braking INVALID records.
3. Start the fixed-horizon video, then press **Start passive capture**. Settings lock for the run.
4. After the LED start signature, move and release manually once. No strict hand-release velocity condition and no free-decay lock are required. Q1 accepts a crossing only after the detector angle leaves 0.08 deg.
5. Download the RWLOG. Verify all time-series samples have `motor_cmd_mA=0`, `current_mA_setting=0`, `pulse_width_ms=0`, and `pulse_active=0`.

## RWLOG and offline evaluation

Metadata fixes `q1_model_name=direct_video_q1_20260829`, `q_model_axis_type=q_target`, and the five Q1 coefficients. Each `q1_shadow_events` item records:

```text
q1_shadow_event_index, zero_cross_time_ms,
zero_cross_rate_dps, zero_cross_abs_rate_dps,
physical_next_peak_side,
detector_crossing_direction,
detector_angle_before_deg, detector_angle_after_deg,
crossing_interpolation_alpha, interpolated_zero_cross_time_ms,
physical_roll_rate_before_dps, physical_roll_rate_after_dps,
interpolated_physical_roll_rate_dps, sign_gate_passed,
q1_intercept_deg, q1_rate_term_deg, q1_side_term_deg,
q1_baseline_next_peak_abs_deg,
target_next_peak_abs_deg, delta_peak_required_deg,
q1_gain_deg_per_mAs, q_model_axis_mA_s, q_req_shadow_mA_s,
q1_shadow_valid, q1_shadow_invalid_reason
```

`zero_cross_time_ms` and `zero_cross_rate_dps` remain the formal Q1 inputs. The interpolation columns are diagnostics only and are recorded in parallel for video comparison.

Convert a log:

```powershell
python tools\convert_rwlog_to_csv.py passive_absolute_roll_run_*.rwlog --out converted_run
```

The converter writes `q1_shadow_events.csv`. Match each event to the next fixed-horizon video peak and first evaluate `A_next_video - q1_baseline_next_peak_abs_deg`. Since this build sends no Q, it must not be used to claim that `baseline + g*Q_req` was realized.

## Build

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
python tools\test_q1_shadow_logic.py
```

This project documents build verification only; it does not instruct or perform a firmware upload.
## Q_IDENT fixed-Q actual-output protocol

`q1_current_hw_fixed_q_ident_v3_qhigh0900_vbat8100_20260902` keeps the isolated actual-output mode, retains Qhigh=`0.900 mA*s`, and extends only the automatic Q_IDENT upper battery guard from `8.020` to `8.100 V`. It is not a Q1, E2, inverse-Q, target controller, calibration, start-kick, rebuild, or continuous-control mode.

- Qhigh=`0.900 mA*s` is selected at the configured minimum battery `6.180 V` and `I0=0 mA`: its 23 ms integer pulse leaves 2 ms below the unchanged 25 ms guard. 300 mA, 5--25 ms, four fixed schedules, ARM, direction and support limits are unchanged.
- Battery eligibility is checked automatically by firmware at `6.180--8.100 V`; no operator voltage confirmation is required. A value outside that inclusive range remains an invalid event with no pulse, replacement, clipping, or carryover.
- Start only with **Start Q_IDENT Run 1**. The selected schedule is Run 1 and cannot be changed while recording.
- The first two accepted, alternating physical-side zero-crosses with `|+gy| >= 30 deg/s` arm the mode; both are logged and cannot output a pulse.
- After arming, output is eligible only at `1.68 <= |+gy| <= 27.80 deg/s` (both endpoints included). Each side follows its fixed schedule independently. `Q=0` is a valid, consumed baseline event with no pulse.
- Every nonzero command uses the existing 300 mA current/pulse-width solver. `q_ident_events` now also preserve event Vbat, I0, continuous required width, selected integer width and the immutable guard, including a pulse-width-guard failure. Battery, roller, solver, state, and ESTOP failures record an invalid event and never substitute, clip, or carry a Q value.
- The command-direction rule remains `-physical_next_peak_side`; Q1 remains motor-off-only and cannot command the Q_IDENT path.
- Run 1 must pass all scheduled Q levels and LED-anchor video synchronization before the same firmware is frozen for Runs 2--4.

The RWLOG remains binary format v43 because the sample layout is unchanged; it contains expanded JSON metadata `q_ident_events`. Use `tools/convert_rwlog_to_csv.py` to create `q_ident_events.csv`.
