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
