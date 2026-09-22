# V46o / 0.46.14 — 起動・開始直後ESTOPの修正

基準: V46n `d6206986fd4bc5f4b11700907e1ad2494e1ecbfa`。
実機ログ `energy_control_autonomous_run_1_55676303.rwlog` はSTART_SYNC 1行、
指令/実電流0、取得間隔2569 us、ログ時のage 8005 us。
`imu_delivery_backlog_over_10ms` がラッチされ、30秒の測定本体には到達していない。
このログは冷間電源投入の履歴や、異常になった項目そのもののageを含まない。

## 修正

1. mainが開始HTTP応答から戻って順次処理へ切り替わる瞬間の最新gyro連番を記録する。
   その連番以前の待機中キューだけを、最大32項目の有界処理で除く。
   満杯の待機キューで取得タスクが先に起きた場合も、境界以前の1項目だけを除き、
   新しい取得値を保存する。両タスクがCore1固定でreader優先度が高い前提。
   境界は1回だけ設定し、START_SYNCから測定本体への遷移では設定し直さない。
   それ以降の項目は全て順序を保って受け渡す。10 ms判定・容量32は変更しない。
2. 開始前の最新値確認を追加。START_SYNCを含むRun中のstatus.jsonは固定長の小さい応答にする。
   大きなidle状態JSONやroot HTMLをRun中に生成・送信しない。停止POSTはそのまま。
   UIは1秒ごとの軽量状態確認で終了・ESTOPを知る。41秒間の盲目的な表示凍結を除去。
3. M5.beginによる先行IMU初期化を止め、ImuManagerからboard/I2C指定で初期化する。
   電源安定待ち100 ms、最大5回の起動時再試行、BMI270 internal status/init_ok、
   accel/gyro power enable、200/400 Hzレジスタ読戻しと実サンプル8 accel/16 gyroを確認する。
   試行間隔200 ms、各stream確認は400 ms以内。取得タスクは確認成功後に1つだけ生成。
   Run中にセンサ再初期化したり、ラッチ済みfaultを解除・自動再ARMしたりしない。
4. 立位案内は実際に更新された10 ms以内のサンプルで従来の400 ms安定判定を行う。
   UIに初期化試行数、失敗理由、重力norm/方向ずれ/gyro norm、サンプルage、案内理由を出す。
5. START_SYNCでも最初のfaultの時刻・取得時刻・age・queue深さ・状態・連番を保持。
   起動統計/境界破棄数/START_SYNCの配送ageと、本測定中の取得統計は別に保存。
   RWLOGの`v46n_imu_acquisition`キーと226-byte sample互換を維持。

## 変更しないもの

V46nからExperimentRunner、config、Roller、MEKF、立位閾値・同期LEDパターン、
±300 mA/最大100 ms・開始キック・30秒のV7制御、2 ms電流監査は全て保持する。
優先度は取得Core1=6、consumer Core1=2、Roller Core0=4。
高速solverの実制御採用やピーク座標の変更は含めない。

## 検証と制約

ネイティブ試験は実際の`src/imu_manager.cpp`を決定的なセンサ/FreeRTOSテストダブルと
コンパイルして実行する。起動失敗→再試行、レジスタ不正、無更新、旧データ
8.005/11.001/30/79 ms、キュー深さ1〜32、連番/時刻wrap、境界後の本当の遅延・overflow、
12,000件の連続配送、faultラッチ保持、初回fault診断を確認する。
これは実ESP32の並行スケジューリング・電気的な電源投入を再現する試験ではない。
実機の冷間起動改善・LEDの消灯・モータ駆動中の400 Hz安定性は書込後に要確認。

倒した状態で電源投入→10秒後のLEDで立てる→400 ms静止で消灯→8°のAutonomous開始。
開始同期には最初の1秒消灯とMEKF再初期化があり、その後点滅して開始キックへ進む。
異常時は停止し、RWLOGと停止中の`/imu-acquisition.json`を保存する。

## ビルドと公開

`pio run -e atoms3cam`。通常buildとPagesはソースを書き換えず、コミットされたコードをそのままテスト・ビルドする。
mainへの更新時にPagesを自動実行する。公開の完了条件はビルド成功だけでなく、Pages deployment成功、
公開されたmanifestの版番号・build-sha.txt・書き込み用4binのSHA256の一致まで確認すること。
ソースZIPのみの提供やブランチ上のテスト成功を、Web Flasher公開完了として扱わない。
実機動作の改善量はモータ駆動Runで別途確認する。
