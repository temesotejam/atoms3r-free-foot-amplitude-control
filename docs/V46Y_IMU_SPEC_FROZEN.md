# V46y / 0.46.24 — IMU測定系 1.0 ms確定仕様

## 採用仕様

- BMI270 Gyro ODR: 400 Hz
- BMI270 Accel ODR: 200 Hz
- Host IMU poll: 1.0 ms
- BMI270 internal I2C: 1 MHz
- STATUSベースの未読判定
- Gyroのみfresh: 6 byte
- Accelのみfresh: 6 byte
- Accel+Gyro fresh: 12 byte
- FIFOは使用しない
- IMU delivery-age 10,000 us ESTOPは維持

## 実機比較

同じ取得系でhost pollだけを比較した。

| host poll | 30秒Gyro captured | 実効レート | sample→control完了 >2.5 ms |
|---|---:|---:|---:|
| 0.5 ms | 12,143 | 約404.8 Hz | 1,718 |
| 1.0 ms | 12,143 | 約404.8 Hz | 199 |
| 2.5 ms | 11,544 | 約384.8 Hz | 2 |

0.5 msは1.0 msよりfresh gyro取得数を増やさず、reader wake・coalescing・forced yieldが増えて制御側遅延を悪化させた。
2.5 msは制御側負荷は軽いがfresh gyro取得数が減った。

したがってV46y以降の測定系は1.0 ms host pollingを採用する。

## 凍結範囲

今後、姿勢推定や角度系を検討する際に、原因分離なしで以下を変更しない。

- Gyro/Accel ODR
- Host polling 1.0 ms
- BMI270 I2C 1 MHz
- STATUS-based selective reader
- IMU reader task core/priority
- queue / delivery-age ESTOP

変更が必要になった場合は、測定系変更として別版・別比較を作る。
