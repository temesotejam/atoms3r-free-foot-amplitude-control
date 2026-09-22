# V46aj / 0.46.35 — 遅延補償を3 ms固定にする

## 変更の目的

測定で採用した3 msだけを使い、0・6・9 msを選べることによる混乱をなくします。
Autonomousの計算、画面、ログ、説明を3 ms固定にそろえました。

## 現在の処理

```text
omega = (gy_dps - mekf_bias_y_dps) * 0.908911
theta_detector = theta_posterior_measurement_relative + omega * 0.003
```

MEKFが求めた開始姿勢基準の角度を、補正済み角速度で3 ms先へ進めてZEROクロスを判定します。
ピーク振幅の測定には予測前のMEKF角度を使います。
3 msを選んでいたV46aiと同じ演算です。

| 項目 | V46ajの扱い |
|---|---|
| 遅延補償時間 | コンパイル時定数3000 µs |
| 操作画面 | 「遅延補償：3 ms固定」と表示。選択欄なし |
| 次のRun用・実行中Run用の選択状態 | 削除 |
| 補償時間設定API | 削除 |
| 開始API | `/start-energy-control-autonomous`。補償時間の引数なし |
| 古い画面からの `timing_ms` 付き開始要求 | 400で拒否。画面を再読み込みしてから開始 |
| ステータスJSON | `autonomous_timing_compensation_ms: 3`、`autonomous_timing_compensation_selectable: false` |
| AutonomousのRWLOGメタデータ | `autonomous_timing_compensation_us: 3000`、`autonomous_timing_compensation_selectable: false` |

メタデータの時間はRun開始時に記録します。非Autonomousのログでは0を記録しますが、
Autonomousで0 msを選べる意味ではありません。RWLOG v51のバイナリ配置は変更していません。
識別文字列は `v46aj_fixed_3ms_compensation_20260920` です。

角度推定の詳細は[現在の角度推定](ATTITUDE_ESTIMATION_V46AI_JA.md)、
角速度による次ピーク予測の式は[V46aiの説明](V46AI_RATE_ONLY_BASELINE.md)を参照してください。
MEKF、ピーク判定、角速度式の係数、Qゲイン、Ki、出力上限、緊急停止の変更はありません。

## 書き込み後の操作

1. [書き込みページ](https://temesotejam.github.io/atoms3r-amplitude-control-development/)でV46aj / 0.46.35を確認して書き込みます。
2. 機体の操作画面を再読み込みし、「遅延補償：3 ms固定」を確認します。
3. 比較測定は目標8°・30秒で行います。補償時間の設定操作は不要です。
4. 各Run終了後、次の開始前にRWLOGと動画を保存します。

## 検証

`tools/test_v46aj_fixed_timing.py`は実装中の予測処理・開始API・メタデータ生成をホストで実行し、
固定3 msの演算、古い時間指定の拒否、開始条件の保護、保存値を確認します。
実際の組み込みJavaScriptも実行し、二重開始、古いステータス応答との競合、緊急停止を確認します。
既存のIMU取得、MEKF、ピーク検出、角速度式、出力制御のテストも継続します。
今回の変更による新たな実機測定結果はまだありません。
