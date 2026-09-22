# V46n / 0.46.13 — 独立IMU取得・モータ駆動検証

## 変更の目的

V46lではCore 0へ分離したのはRollerのI/Oだけで、Core 1のArduino loopがIMU取得、MEKF、V7 solver、ログ、Webを逐次実行していた。したがってsolverの処理中は次のIMU取得へ進めなかった。

V46nではBMI270取得をCore 1の専用優先度6タスクへ移す。ESP timerは1 ms周期でタスクに通知するだけで、I2Cや演算をコールバック内で実行しない。通常のArduino consumerは優先度1。Rollerは従来どおりCore 0、優先度4。

内蔵IMUのM5.In_I2CはI2C controller 1、SDA45/SCL0。RollerのArduino Wireはcontroller 0、SDA2/SCL1。ライブラリM5Unified 0.2.18の割当を確認し、ファームウェアでも内蔵バスが1でなければIMU初期化を失敗として扱う。実測のcore、priority、I2Cポート・端子は診断へ出力する。

## データの所有権と時刻

- 取得タスクだけがM5.Imuとcapture_を書き換える。
- Arduino consumerだけがreading_、MEKF、ExperimentRunnerを更新する。
- 32件の静的FreeRTOSキューが取得済みスナップショットをコピーする。
- Run中は順序を保持し、1 loopにつき1件を処理する。消費側に1 msの取得待ちゲートはない。
- 非Run時のみキューから最新値へ進める。ダウンロード中の古い姿勢が残り続けないため。
- 時刻はM5Unifiedのホスト取得時刻であり、BMI270ハードウェアsensor timeではない。キューから取り出した時刻に置き換えない。
- ソフトウェアキューはBMI270ハードウェアFIFOではない。

## モータと保護

実モータは従来のAutonomous V7で動作する。START_KICK -300 mA / 100 ms、通常±300 mA・最大100 ms、30秒Run、立位判定、START同期のMEKF再初期化、非常停止、Roller自動復旧、2 ms電流監査は維持する。

`src/config.h`、Roller実装、MEKF本体、立位判定、RWLOGの226-byte sample形式、PlatformIO設定をV46l baselineと全バイト比較する。ExperimentRunner本体への変更は、queued sampleがパルス開始より前に取得されていた場合にタイミング診断へ誤採用しないための時刻条件1か所だけ。

キューあふれと、キューから取り出したサンプルの取得時刻からの遅れが10 msを超えた場合はfaultをラッチする。Run中は既存requestEmergencyStop経路へ伝える。取得の停止は既存500 ms stale条件でも検出する。取得系faultは自動再ARMせず再起動を必要とする。

## 変えていないもの

旧solverは実制御を継続し、高速solverはV46lと同じpost-pulse shadowのまま。ピーク振幅のジャイロ積分座標、P1/Q1、左右補正、MEKF予測設定は変更していない。今回の比較は取得タスク分離の効果に限定する。

高優先度取得が低優先度solverを中断するため、solverの壁時計時間や制御への受け渡し遅れまでゼロになるとは限らない。取得周期と受け渡し遅れは必ず別々に判定する。実機での改善量・無欠落はRun前には未確認。

## ログ

RWLOG metadataの `v46n_imu_acquisition` に次を格納する。

- `captured`、`delivered`、`duration_us`、`samples_per_second`
- 全取得サンプルの `dt_mean_us`、`dt_max_us`、`over_4ms`、`over_5ms`、`over_10ms`
- 250 us刻みの `acquisition_dt_histogram`、`delivery_age_histogram`（最終binは10 ms以上）
- `delivery_age_mean_us`、`delivery_age_max_us`、`max_delivery_age_per_second_us`
- `queue_drops`、`queue_high_water`、`delivery_sequence_gaps`
- 4 ms超の取得間隔の時系列 `long_gaps`（最大128件、超過件数も表示）
- `coalesced_wakes`、`poll_max_us`、実行core/priority、内蔵I2C情報

記録はRUNNING_BATCH_SWEEP区間だけ。END_SYNCへ移った時点で固定し、次Runで初期化する。従来のtimeseriesは20 ms/パルス中2 msの間引きログなので、その行数を全IMU取得数・欠落数として扱わない。

V46k由来の `imu_update_call_us` はこの版では取得I/O時間ではなくconsumer処理時間。取得側処理時間は新診断の `poll_max_us` 等で区別する。

Run後、機体の `/imu-acquisition.json` からも同じ診断を取得できる。Run中のこの追加エンドポイントは409を返し、不要なJSON生成で制御を妨げない。

## 実験手順

ブラウザ書き込み画面のV46n / 0.46.13を確認して書き込む。従来どおり倒した状態で起動し、起動LEDの合図で立てる。8°設定でAutonomous Runを1回行い、終了後RWLOGを保存する。動画同期LEDも維持している。

判定は「モータ出力あり」のRunで行う。取得が約400件/秒か、パルス開始に毎回出ていた約6 msの取得間隔が減ったか、queue_dropsが0か、受け渡し遅れが増え続けていないかを確認する。単に収集周期だけ改善しても、古いデータを制御に渡していれば合格とはしない。

## ビルド

`pio run -e atoms3cam`。通常buildとPagesはソースを書き換えず、コミットされたコードをそのままテスト・ビルドする。`build-sha.txt`、各binのSHA256、Actionsのfirmware.elfを保存する。
