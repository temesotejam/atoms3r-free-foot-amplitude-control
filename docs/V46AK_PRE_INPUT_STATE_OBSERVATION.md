# V46ak / 0.46.36 — 次ピーク再現性のための入力直前状態観測

## 目的

V46ajで確定した姿勢推定・ZEROクロス判定・3 ms遅延補償・次ピーク予測・Q選択・
パルス出力を変更せず、**制御入力を与える直前のRoller状態**を記録します。

今回の目的は制御改善ではなく、次ピークの再現性に

- 入力直前の実測電流
- 入力直前のホイール速度

が関係しているかを、後から実機ログで判別できるようにすることです。

## 凍結した部分

V46akでも次の部分はV46ajのままです。

- 6-state MEKFとその設定
- BMI270取得系
- MEKF角度の基準
- ZEROクロスの3 ms固定補償
- ピーク判定
- rate-only次ピーク基準振幅
- QゲインとKi
- fast pulse solver
- 300 mA・最大100 msの出力範囲
- ESTOPと既存安全条件
- RWLOG v51の時系列バイナリ配置

姿勢推定の識別子も
`v46aj_fixed_3ms_compensation_20260920`
のまま維持します。

V46ak固有の観測識別子は
`v46ak_pre_input_state_observation_20260920`
です。

## Roller485ホイール速度

M5Stack Unit Roller485のI2Cプロトコルに定義されている
**Speed Readback (0x60, X100 Int)** を使用します。
取得した32-bit符号付き値を100で割った値をrpmとして記録します。

速度読出しは観測専用です。

- 電流指令が0
- pendingの電流指令も0

のときだけ、既存20 msの通常Rollerテレメトリ周期内で読みます。
アクティブな1 ms電流監査中には速度読出しを追加しません。
速度読出し失敗だけを理由に既存の `roller_ok` / 制御許可を変更しません。

各速度サンプルには取得時刻・age・validも持たせます。

## Autonomous ZEROクロスイベントへ追加する項目

通常ZEROクロス判断で、solverやモータ指令を実行する**前**に最新Rollerテレメトリを
1回snapshotし、以下を保存します。

```text
pre_input_capture_time_us

pre_input_measured_current_mA
pre_input_current_sample_time_us
pre_input_current_age_us
pre_input_current_valid

pre_input_wheel_speed_rpm
pre_input_wheel_speed_sample_time_us
pre_input_wheel_speed_age_us
pre_input_wheel_speed_valid
```

既存の `i0_estimated_mA` もそのまま残るため、

```text
推定I0
実測直前電流
ホイール速度
ZEROクロス角速度
side
Q
次ピーク
```

を同じ実験から比較できます。

これらの `pre_input_*` 値はログ出力専用であり、制御計算から読みません。

## RWLOG

時系列の `LogSample` は変更せず、RWLOG formatは **v51のまま**です。
追加値は `energy_control_autonomous_zero_cross_events` のJSONメタデータへ追加します。

`tools/convert_rwlog_to_csv.py` で変換すると、
`energy_control_autonomous_zero_cross_events.csv` に同じ列が追加されます。

## 最初の測定

まずV46ajと同じ条件を維持します。

- target: 8°
- Autonomous: 30秒
- ZEROクロス補償: 3 ms固定
- 機体条件・撮影条件をできるだけ統一

最低3 Run、可能なら5 Run取得し、各Run終了後にRWLOGと動画を保存します。

この段階ではホイール速度や実測直前電流を使って制御を変更しません。
別Runでも次ピーク誤差との関係が再現するかを確認してから、次のモデル変更を判断します。
