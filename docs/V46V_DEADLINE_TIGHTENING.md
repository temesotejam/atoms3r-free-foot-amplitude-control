# V46v / 0.46.21 — 時間超過の削減

## 目的

角度推定・角度基準・MEKF/Madgwick・パルス幅決定・モータゲインは変更しない。
V46u実機Run a864で残った時間超過だけを減らす。

V46u実測:
- IMU reader 1 ms超過: 948 / 29,108
- fresh gyro取得からrunner終了まで2.5 ms超過: 327 / 12,144
- runner.update 2.5 ms超過: 104 / 12,144
- 電流読出し処理2 ms超過: 1 / 764
- 有効電流の連続取得間隔2 ms超過: 539 / 702

## 変更

1. BMI270の内部I2Cを400 kHz相当の従来設定からFast-mode Plus 1 MHzへ変更。
   Bosch BMI270はI2C Fast-mode Plus 1 MHzを仕様上サポートする。
   M5Unifiedの `M5.Imu.setClock()` でBMI270デバイスの転送クロックだけを変更する。
   ODR 400/200 Hz、選択読出し、軸、変換、フィルタ、推定器は変更しない。

2. パルス中の電流監査の読出し機会を2 msごとから1 msごとへ前倒しする。
   Q積分やパルス終了条件は変更しない。観測サンプルが増えるだけである。

3. 高速電流監査で同じループ内にCURRENT_READBACKを取得済みなら、
   20 msテレメトリ更新側の同一CURRENT_READBACK再読出しを省略する。
   高速読出しに失敗した場合は従来どおり低頻度側で再試行する。

## 維持するもの

- 300 mA / 最大100 ms
- 10,000 usのIMU配信遅延ESTOP
- Core/priority/queue構成
- 1 ms IMU poll
- BMI270 gyro 400 Hz / accel 200 Hz
- V46u STATUSベースの6/12 byte選択読出し
- 全ての角度・推定・制御計算

## 判定

V46vでも既存の期限カウンタをそのまま使う。
平均や完走だけではPASSにしない。
新しい実機Runで各超過件数・最大値・欠落・faultを確認する。

ビルド成功は実機期限達成の証明ではない。
