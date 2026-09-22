# V46al-R1 / 0.46.41 — V46ak stableから再構築した直前ピーク実制御

## 目的

実機確認済みの `atoms3r-amplitude-control-v46ak-stable` を戻り基準にし、
V46akの次ピーク予測へ **直前ピーク `A_prev` の残差補正だけ**を追加して、
補正後の予測を実際のQ決定へ使用する。

基準:

`atoms3r-amplitude-control-v46ak-stable@bb9c5ed07c5ca8b3c6c6b5813b6c2f1b1f57a6ec`

## 制御変更

V46ak:

```text
A_free = rate_only(|omega0|, side)
```

V46al-R1:

```text
correction = c_side + k_side * (A_prev - 8 deg)
A_free_new = max(0, A_free + clamp(correction, -0.70, +0.70))
```

係数はV46akの5 Run、8°、10–30 sで得た残差モデルを固定して使用する。

```text
next side +:
  c = +0.591392151 deg
  k = -0.442636343
  A_prev support = 7.19424 .. 9.34474 deg

next side -:
  c = -0.157912422 deg
  k = +0.585367534
  A_prev support = 6.95706 .. 8.75588 deg
```

補正の適用条件:

- target = 8.0°
- t >= 10.0 s
- A_prevがfinite
- A_prevがnext-side別support内

条件外ではV46akのrate-only `A_free` をそのまま使用する。

## 変更しないもの

- 6-state MEKF
- 3 ms固定ZEROクロス補償
- ZEROクロス判定
- ピーク検出
- side判定
- 電流/I0モデル
- 既存bounded solver
- Ki
- 300 mA / 最大100 ms
- ESTOP・安全条件
- V46akの入力直前電流・ホイール速度観測
- RWLOG v51 / LogSample 258 bytes
- 現在の `/download/rwlog` と直接stream処理

ホイール速度と入力直前実電流は引き続き観測のみで、今回の補正には使用しない。

## RWLOG

時系列バイナリは変更しない。ZEROクロスevent metadataへ以下を追加する。

- `free_next_peak_before_previous_peak_correction_deg`
- `previous_peak_control_raw_correction_deg`
- `previous_peak_control_correction_deg`
- `previous_peak_control_reason`
- `previous_peak_control_applied`
- `previous_peak_control_clamped`
- `previous_peak_control_model_revision`

これにより、各ZEROクロスでV46ak予測から何度補正し、最終的なQがどう変わったかを後解析できる。

## 最初の実機評価

target 8°、3 ms固定、30秒でまず3 Run取得する。

比較対象はstableのV46ak測定とし、特に10–30 sについて

- 次ピークの8°に対するRMSE / MAE / bias
- side別誤差
- `A_prev`補正量
- Q、pulse width、Ki
- 補正support外/clip回数
- 交互振動や発散の有無

を確認する。
