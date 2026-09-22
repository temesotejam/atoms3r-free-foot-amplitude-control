# V46x / 0.46.23 — IMU 0.5 msポーリング測定版

## 目的

V46wの2.5 msポーリングでは30秒で11,544 gyro sample（約384.8 Hz）となり、400 Hz相当の取得数が減った。
そこで、BMI270 gyro 400 Hzの1周期2.5 msに対してホスト側を0.5 ms（2 kHz）で確認し、data-readyの発見遅延と取りこぼしの可能性を減らした場合の実機挙動を測定する。

## 変更

V46wからの機能変更は `IMU_POLL_PERIOD_US` のみ。

- V46w: 2500 us
- V46x: 500 us

## 維持するもの

- BMI270 Gyro ODR: 400 Hz
- BMI270 Accel ODR: 200 Hz
- BMI270 I2C: 1 MHz
- STATUSベースのgyro-only 6 byte / accel+gyro 12 byte選択読出し
- MEKF / Madgwick / 角度座標
- Fast solver / 制御ゲイン / パルス幅選択
- 電流監査
- 300 mA / 最大100 ms
- IMU delivery-age 10,000 us ESTOP
- Core / priority / queue構成

## 注意

0.5 ms pollは400 Hz gyro 1 sample期間あたり最大約5回STATUSを確認する。
そのため2.5 ms版よりI2Cアクセス・reader wake回数は増える。

現在の取得タスクは、reader処理時間がpoll周期以上になった場合に既存のoverrun yieldを行う。
V46xではその境界も0.5 msになるため、実機ログでは
`forced_yields`、reader処理時間、coalesced wake、queue depthも確認する。

## 実機判定

V46v 1 ms、V46w 2.5 msと比較する。

- 30秒のcaptured / delivered
- 実効gyro取得レート
- queue drop / sequence gap / fault
- host gyro interval分布
- poll回数 / no-data回数
- forced yield / coalesced wake
- reader処理時間
- sample取得からrunner終了まで
- runner.update時間

ビルド成功だけでは0.5 ms pollingの妥当性を判断しない。実機Runで判断する。
