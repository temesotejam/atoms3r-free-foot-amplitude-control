# 統合版の不安定動作に関する全体調査

対象：公開版0.47.0、commit `1521a5952fb1b08c9642f1ea81fad205a30efa60`。
調査日：2026-09-23。対処・観測の追加は0.47.1。

現時点では原因は未確定です。Wi-Fiの設定だけ、USBの給電だけ、あるいは
カメラの初期化だけに絞る根拠はありません。最初に足角度まで表示できたという報告から、
全機能の組合せが原理的に不可能とも判断できません。一方、その事実は継続稼働や
制御の締切時間を保証せず、ハードウェアの電圧・実行時間・エラーの観測が必要です。

## 報告された事実と、その限界

| 報告 | 分かること | まだ分からないこと |
|---|---|---|
| 最初はWebUIと足角度が更新した | 一度はHTTP応答とフレーム処理が成立 | その後のメモリ・CPU・給電の余裕 |
| PCはWi-Fi接続済み、状態URLも開けない | Wi-Fiの接続表示だけではHTTPの生存確認にならない | HTTPだけ停止したか、本体全体が停止したか |
| 通常起動でCOMが現れたり消えたりする | アプリ実行または実行時の負荷との関係を疑う | 毎回本体が再起動したか。USBだけの再列挙もあり得る |
| Download ModeではCOMが消えない | ケーブル／PCが全く通信できない状態ではない | アプリ時の電力負荷やUSB設定・割込みに問題がないか |
| バッテリーでも不安定 | USB給電だけの問題とは考えにくい | 基板側の共通3.3V系、接地、配線・負荷の問題 |
| カメラ側LEDは点く | カメラ電源の有効化の手掛かり | カメラ初期化、画像取得、Webサーバー起動の成功 |

メーカー資料ではGPIO18をLOWにするとカメラ電源と表示LEDが有効になります。
M5Unified 0.2.18の機種判定処理にも、カメラ検出のためGPIO18をLOWにする処理があります。
従ってLEDはアプリ側`camera_probe.begin()`が成功した証拠にはできません。

## 原因候補の評価

| 原因候補 | コード／資料から確認した内容 | 症状との関係・次の証拠 |
|---|---|---|
| 例外・スタック不足・ヒープ破損 | SDKはpanic後の再起動、brownout、stack canary、watchdogを有効化 | WebとUSBの両方が失われる現象を説明し得る。reset理由とバックトレースが最優先 |
| カメラと取得・制御のCPU競合 | カメラ処理タスクはCore 0だが、カメラ初期化はCore 1。外部割込みは割当元コアに結び付く | タスクをCore 0へ移しただけでCore 1のカメラ負荷がなくなるとは言えない。IMU/controlの最終更新とWDTを確認 |
| 内部RAM／DMA用連続領域不足 | PSRAMへ大配列を移したが、RTOSスタック・DMA・Wi-Fi・TCP・Stringには内部ヒープが必要 | 静的RAMの減少だけで動的メモリ問題を除外できない。空き総量と最大連続領域の両方を見る |
| HTTP／lwIPの待ち | `pacedWrite`はsendループを1.2秒で打ち切るが、`WiFiClient.stop()`と全体の`handleClient()`はその範囲外 | HTTPが長時間応答しない候補。COM消失単独までは説明しない。送信・close・pollの到達点を記録 |
| タスク／ロック停止 | アプリのスナップショットは短いロック中心で、ロック内にI2C・HTTP・ソルバ呼出しは見つからない | ライブラリ内部や破損・CPU占有まで除外はできない。観測処理が同じロックを取らないことが必要 |
| 基板・電源・接地・負荷 | 基板は5Vから3.3Vへ変換。USB利用とバッテリー利用でも共通部分が残る | バッテリーで再現しても電源系を除外しない。brownout理由や3.3V/EN波形で確認 |
| GPIO／I2C競合 | カメラ12/9、IMU45/0、Roller2/1。IMUはI2C1、カメラは起動時だけI2C0、以後Rollerが所有 | 主要な端子の直接重複は見つからない。USB19/20をアプリが別用途に設定するコードも見つからない |
| ブラウザ側の更新停止 | 不完全な状態JSONを受けるとcatch内の`latest.command.pending`でも例外が起き、次回pollが予約されない | ホストで再現できた不具合。修正対象。ただし装置URLが開けないことやCOM消失の全体原因とは断定しない |
| キャッシュ・版の混在 | 書き込みページと装置内WebUIは別物。古いページや別バイナリとの取り違えの余地 | manifest、公開SHA、バイナリハッシュ、実機USB出力のversionを照合 |

## 実行時構成の再点検

| 処理 | アプリ設定のcore / priority | スタックbytes | 見直し上の注意 |
|---|---|---|---|
| Roller I/O | 0 / 4 | 6144 | I2Cタイムアウト20ms、再初期化あり。通常は1tick待機 |
| カメラドライバtask | 0 / 3 | SDK設定2048 | タスクの配置とISRの配置は別。小さいスタックもpanic時の確認対象 |
| 足画像処理 | 0 / 1 | 10240 | 15fpsは消費側の目標周期。XCLK20MHzのセンサ受信負荷と同義ではない |
| ログexport | 0 / 1 | 12288 | メタデータ作成は長い同期処理。CRC部は4KiBごとにyield |
| IMU reader | 1 / 6 | 4096 | 1ms通知、gyro400/accel200Hz。過負荷時はyieldするが実測の余裕は未確認 |
| controller | 1 / 4 | 16384 | 常設所有者。IMUキュー受信待ちまたはidle時のyield |
| Arduino / HTTP | 1 / 2 | SDK設定8192 | 一つのHTTPサービス経路。上位タスクの負荷やソケット待ちの影響 |
| 追加USB observer | 0 / 2 | 6144 | 0.47.1のみ。20ms周期、ログ約1秒周期。アプリのロックを読まない |

SDKのtask watchdogはCore 0のidleを監視し、Core 1のidleは監視しない設定です。
割込みwatchdogは両コア・300msです。したがって「再起動しない」場合でも、
Core 1上の低優先度HTTPだけが実行できない故障を除外できません。

### メモリ

大きな固定PSRAM確保はサンプル約5MiB、イベント190,144 bytes、足フレーム768件、
任意のsolver auditです。export時に最大2MiBのメタデータ領域を追加します。
ホストの最大イベント＋768足フレームの実シリアライズでは説明JSONは1,237,910 bytesでした。
このテストは容量と形式の検証であり、実機のヒープ断片化・DMA競合・電圧や処理時間の検証ではありません。

測定終了だけで大きなexport JSONを作るコードはありません。ユーザーが
`/export/prepare`を要求してから作ります。従って、起動後、測定・ダウンロード前にも
止まる故障の第一原因をexportの大量JSONに求めるのは適切ではありません。
ただしexport中にのみ再現する故障なら、この同期処理を再び優先して調べます。

### カメラドライバ

連続取得は`cam_start/cam_take/cam_give/cam_stop`という低レベルAPIを利用しています。
リンクできることと、全状況での開始・停止・DMA状態の整合性が保証されることは別です。
ESP32-S3のesp32-camera v2.0.9ソースにはXCLK等でPSRAM DMA方式が変わる条件があり、
フレームをPSRAMへ置く設定だけでは内部DMAバッファ不要とは言えません。
ただし、この参照タグとArduinoに同梱されたバイナリの完全な同一性は確定していないため、
この条件だけを根拠にクロックやcoreを変更して原因確定とはしません。

## 今回修正したもの、次に判断するもの

0.47.1では再現できたブラウザ更新停止を修正し、USB受信の接続ハンドル解放と
ログ保持・再接続を整えました。旧ページに残っていた未接続のカメラ操作コマンドと
port81/net-probeの案内を除去しました。診断版は全機能を搭載したままです。

次の実機確認はまず測定を始めず、USBログを取って通常起動します。
Wi-Fi接続前、接続後、WebUIを開いた後のどこで発生するかを確認します。
これはモーター作動やログexportを必要としない故障かを先に判断するためです。

取得結果に応じ、panicなら例外アドレスとELFを照合、WDTなら停止したコアと処理を確認、
HTTPだけ停止ならソケットとサービス経路、brownoutなら給電の実測へ進みます。
全機能を載せたログでも絞れない場合は、カメラの有無、無線の有無、Roller通信の有無を
一つずつ変える比較ビルドを使います。その比較版は最終構成の機能削除を意味しません。

## 一次資料

- [M5Stack AtomS3R-CAM：LED、ピン表、8MB PSRAM、5V/3.3V構成](https://docs.m5stack.com/en/core/AtomS3R%20Cam)
- [M5Unified 0.2.18：機種判定と内部I2C割当](https://github.com/m5stack/M5Unified/blob/0.2.18/src/M5Unified.cpp)
- [ESP-IDF 4.4.7：USB Serial/JTAG](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/usb-serial-jtag-console.html)
- [ESP-IDF 4.4.7：割込み割当とコア](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/intr_alloc.html)
- [ESP-IDF 4.4.7：fatal errors](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/fatal-errors.html)
- [Arduino ESP32 2.0.16：ESP32-S3 sdkconfig](https://github.com/espressif/arduino-esp32/blob/2.0.16/tools/sdk/esp32s3/sdkconfig)
- [Arduino ESP32 2.0.16：WebServer](https://github.com/espressif/arduino-esp32/blob/2.0.16/libraries/WebServer/src/WebServer.cpp)
- [Arduino ESP32 2.0.16：WiFiClient](https://github.com/espressif/arduino-esp32/blob/2.0.16/libraries/WiFi/src/WiFiClient.cpp)
- [Arduino ESP32 2.0.16：HWCDC](https://github.com/espressif/arduino-esp32/blob/2.0.16/cores/esp32/HWCDC.cpp)
- [esp32-camera v2.0.9：cam_hal.c](https://github.com/espressif/esp32-camera/blob/v2.0.9/driver/cam_hal.c)
