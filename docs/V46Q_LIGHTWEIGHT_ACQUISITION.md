# V46q / 0.46.16 — 取得軽量化と残遅延の内訳

基準はV46p `a1f8a1b7b81d3465620e66e67b9242d9df9520e2`。
Run 68642601がモータ駆動で30秒完走した構成を維持する。

## 変更

取得タスクから加速度norm、norm誤差、加速度角のsqrtf/atan2fを除く。
式は変えず、起動時の実サンプル検証と、consumerの新しいaccel連番の時だけ計算する。
gyroだけの配送では前回値を再使用し、200Hzの処理を400Hzに倍増させない。
生の値、単位、軸、センサ取得時刻と連番は変更しない。

reader=Core1 priority6、control=Core1 priority4、HTTP=priority2、Roller=Core0 priority4。
1ms通知、32件queue、10ms配送異常、overflow異常、1tick overrun waitは変更しない。
起動初期化・立位確認・開始境界・非常停止・V7±300mA/100ms上限・2ms電流監査も維持する。
高速solverは今回もshadowのみ。ピーク座標修正やFIFO追加は行わない。

## 診断

`v46n_imu_acquisition.v46q_poll_profile`に、測定本体中の以下を自動保存する。

- `latest_notify_age`: 最新callbackの時刻スナップショットからreader実行まで。通知合算時の最古通知待ちではない。
- `observed_callback_gap`: 観測できた最新callback間隔。理想alarm時刻からの遅延やISR遅延ではない。
- `poll_start_interval`: readerの開始間隔。
- `update_api`: M5.Imu.update全体。純粋なI2Cバス占有時間とは区別する。
- `convert_api`: M5.Imu.getImuData全体。
- `validate_pack`: finite確認・時刻/連番・値の整理。
- `publish_queue`: 既存監査とキュー投入。
- `poll_total`: poll開始から取得処理終了まで。profile集計とoverrun待機を含まない。
- `overrun_yield`: vTaskDelay(1)を実際に待った時間。

各項目に個別のcount/mean/max。convert/packはデータあり、publishは新gyroのpollのみなので
異なる母数の平均を足し合わせない。すべて壁時計時間で、高優先度の割込み等を含む。
既存の全取得数・配送数・ヒストグラムと併用する。

1秒ごとの集計を32区間分保存。4ms超のgapの**100ms区間ごとの最悪例**を320区間分保存し、
前回の「先頭128例だけで後半を観測できない」問題を補う。ただし全gapの逐次トレースではない。
全件数は別に保存。gap例には現在と前回のpoll時間、前回のoverrun待機も含める。
ホスト時刻でありBMI270内部の欠落数やセンサ内部フィルタ遅延は分からない。

profileメモリは20KB未満。readerだけが計測中に更新し、停止中だけJSONを生成する。
大きな構造体の初期化をspinlock内で行わず、大きな一時構造体をreaderの4KBスタックに置かない。
`record_overhead_max_us`に集計コストを含める。計測そのもののわずかな影響は実Runで評価する。

## 試験・手順

`python tools/run_v46q_tests.py`で実ImuManagerと決定的なセンサ/RTOS代役を使い、
5000組の加速度の計算一致・gyro-only再使用・時刻不変、NaN停止、時刻wrap、後半のgap保存、
再Runのreset、計測中export拒否、停止時JSONを検証する。V46oの269ケースとV46p所有権試験も継続。
代役は実ESP32のスケジューリング・電源投入・I2C電気条件を再現するものではない。
ビルドとPages両方で同じ試験・PlatformIOコンパイルを行い、コミット済みソースを変更しない。

書込後は従来の起動・立位手順→8°Autonomousを1回→RWLOG保存。
解析は `python tools/analyze_v46q_acquisition.py RUN.rwlog --output result`。
新しいstage平均/最大、overrun回数、1秒毎の取得数、配送age、モータの出力と停止理由を見る。
完走/取得安定/制御指令までの低遅延を同一視せず、実機の結果が出るまで改善量を断定しない。
