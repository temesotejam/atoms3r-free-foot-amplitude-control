# V46w / 0.46.22 — IMU 2.5 msポーリング測定版

## 目的

BMI270ジャイロ400 Hzの公称周期2.5 msに対して、ホスト側IMUポーリング周期も2.5 msへ合わせたときの実機挙動を測定する。

V46vから変更するのは `IMU_POLL_PERIOD_US` のみで、1,000 usから2,500 usへ変更する。
角度推定、MEKF/Madgwick、制御則、パルス幅、モータゲイン、電流監査、I2C 1 MHz、STATUSベース選択読出し、安全停止条件は変更しない。

## 維持する設定

- BMI270 Gyro ODR: 400 Hz
- BMI270 Accel ODR: 200 Hz
- BMI270 I2C: 1 MHz
- Gyro-only 6 byte / Accel+Gyro 12 byte選択読出し
- 電流監査の1 ms読出し機会と2 ms期限評価
- IMU delivery-age 10,000 us ESTOP
- 300 mA / 最大100 ms
- 既存Core/priority/queue構成

## 評価

次の実機Runで以下をV46vと比較する。

- 30秒取得の完走
- captured / delivered
- queue drop / sequence gap / fault
- fresh gyroのホスト取得間隔
- IMU reader処理時間
- fresh gyro取得からrunner終了まで
- runner.update時間

2.5 msポーリングはセンサ内部クロックとESP32側タイマーを位相同期するものではない。
センサ更新直前にpollするケースでは、そのサンプルを次のpollまで待つ可能性がある。
したがってビルド成功や平均2.5 msだけで成功とは判定しない。

0.5 msポーリングも比較候補だが、400 Hzの1サンプル当たり約5回pollするためCPU/I2C負荷が増える。
まず2.5 msを測定し、必要なら次に0.5 msを比較する。
