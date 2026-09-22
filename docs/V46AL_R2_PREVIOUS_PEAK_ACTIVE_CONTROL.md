# V46al-R2 / 0.46.42 — A_prev実制御 + stable RWLOG

V46al-R1ではA_prev制御そのものに加えてZEROクロスevent metadataも拡張した。
実機ではRWLOGダウンロードが途中で停止したため、R2ではその追加ログを撤回する。

## 基準

- stable: `atoms3r-amplitude-control-v46ak-stable@bb9c5ed07c5ca8b3c6c6b5813b6c2f1b1f57a6ec`
- 姿勢推定: V46aj
- 観測: V46ak
- RWLOG logger/converter: stableと完全同一

## R2で残す変更

ZEROクロスでV46ak rate-only予測を計算した後、A_prev残差補正だけを適用する。

```text
A_free = rate_only(|omega0|, side)
correction = c_side + k_side * (A_prev - 8 deg)
A_free_new = max(0, A_free + clamp(correction, -0.70, +0.70))
```

適用条件:

- target = 8.0°
- t >= 10.0 s
- A_prev finite
- A_prevがside別5 Run support内

条件外はV46akの`A_free`をそのまま使う。

## R2で撤回する変更

A_prev制御専用のZEROクロスevent fieldは追加しない。
補正量・適用理由・clampは既存ログからオフラインで完全再計算する。

必要な既存項目:

- `zero_cross_time_ms`
- `previous_peak_amplitude_deg`
- `physical_next_peak_side`
- `target_peak_deg`
- `rate_baseline_peak_deg`
- `free_next_peak_amplitude_deg`

## ダウンロード固定

以下はstableと同じGit blobに固定する。

- `src/psram_logger.cpp`: `e61167fba2869ad948df37d999a7bcb6e5346817`
- `src/psram_logger.h`: `63a16781660142a5e3a82721f90cadd9cc2ce9b7`
- `tools/convert_rwlog_to_csv.py`: `7a2c1229376e1ec204a3d9305cedf0b67af7a231`

また、`handleRwLog()`、`streamRwLog()`、`writeBytes()`もstableの内容を固定テストする。

## 変更しないもの

MEKF、3 ms補償、ZEROクロス/ピーク/side判定、I0/電流モデル、Ki、
bounded solver、300 mA/100 ms、ESTOP、V46ak観測、RWLOG v51 / 258 bytes。
