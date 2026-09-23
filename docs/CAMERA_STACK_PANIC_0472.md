# 0.47.2：実機で確認したカメラ内部タスクのスタック不足

2026-09-23の0.47.1 USBログには、次の例外が3回あり、すべて同じバックトレースでした。

```text
Guru Meditation Error: Core  0 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (cam_task)
```

直後に`Rebooting...`、次の起動で`reset=PANIC`が記録され、boot IDは2→3→4→5と増加しています。
今回記録されたUSBポート消失は、本体のpanic再起動と対応しています。
「ソースを見た限りプログラムに問題がない」という結論ではありません。
通常のフレーム取得だけでなく、ライブラリ内部のエラー処理まで含めたスタック割当が不足していました。

## 実機と完全一致するELFでの照合

| 対象 | 値 |
|---|---|
| 実機バージョン | `0.47.1-usb-diagnostics` |
| ソースcommit | `daec6eabc181cc641dde5f45a7a043c63b3fe9e1` |
| GitHub Actions build run | `35845050835` |
| ビルド成果物ZIP SHA-256 | `85ff9669c7b3e4e49432abe766d1fcddd4121e52ed97d3c779834d5583adcf3e` |
| 公開firmware.bin SHA-256 | `3d1be56544a6bd8666ef5bed4282297e9c4c89abca33f70c0810fb328d8f0eb1` |
| 実機出力のELF SHA-256先頭 | `b852670c1d0fc275` |
| firmware.elf SHA-256とbin内app descriptorのELF hash | `b852670c1d0fc275750a7092444ab7f7e17417f28769090b330e58fb6057f118` |

ZIPのSHA-256はGitHubのartifact digestと一致し、ELFファイルのSHA-256、bin内のELF hash、
実機が報告したhash先頭が一致しました。別のソースを再ビルドしたELFからの推測ではありません。

`xtensa-esp32s3-elf-addr2line -pfiaC`の結果を、呼び出し元から障害箇所の順に並べると次の通りです。

| PC | 復号結果 |
|---|---|
| `0x420847da` | `cam_task`、`esp32-camera/driver/cam_hal.c:196` |
| `0x42083c1f` / `0x42083b8a` | `__wrap_esp_log_write` / `__wrap_esp_log_writev` |
| `0x420e28e1` | `esp_log_writev` |
| `0x420cf2f5` / `0x420cf17f` | `vprintf` / `_vfprintf_r` |
| `0x420d5fa9` / `0x420c72b5` | `__sprint_r` / `__sfvwrite_r` |
| `0x420c685a` / `0x420c67d2` / `0x420c8165` | `_fflush_r` / `__sflush_r` / `__swrite` |
| `0x42062d5d` / `0x42063501` / `0x420644ba` | `esp_vfs_write` / `console_write` / `uart_write` |
| `0x40379bb9` / `0x40379ae1` | `_lock_acquire_recursive` / `lock_acquire_generic` |
| `0x4037e964` / `0x4037e8c3` | `xQueueTakeMutexRecursive` / `xQueueSemaphoreTake` |
| `0x403802b4` | `pvTaskIncrementMutexHeldCount` |
| `0x40380bdc`（例外PC `0x40380bdf`） | `xPortEnterCriticalTimeout`内の`spinlock_acquire` / `compare_and_set_native` |

さらにELFの逆アセンブルで、`0x420847da`の呼び出しに渡す文字列リテラルが
`E (%u) %s: FB-SIZE: %u != %u\n`であることを確認しました。
同じELFの`cam_config`ではスタック引数が2048で、既存のRTOSラッパーを呼んでいました。
0.47.1のラッパーは優先度とcoreだけを変更し、スタック引数は2048のままでした。

したがって、カメラが画像サイズ不一致を検出した後、そのエラーを通常コンソールへ出す
深い呼び出し経路でcam_taskのスタック境界違反が発生しています。追加したUSB observer自体や
足画像解析タスクのスタックではありません。エラー文字列がUSBに届く前に落ちる場合もあります。

## 周辺の観測と、未確定の部分

- 前回RTCサンプルは`stage=ready`、初期化失敗ビット0、AP有効、接続クライアント0。
  HTTPはまだ完了報告を出していません。今回の再起動にWebUIへのアクセスは不要でした。
- 足画像取得側は`camera_capture`に入り、最初の処理完了前に再起動しています。
  直前までIMU・制御・Rollerの完了報告はあります。
- 最後のメモリサンプルには内部空き229748 bytes、最大連続204788 bytesがありました。
  一般ヒープの空きがあっても、タスクごとに固定確保した2048-byteスタックは増えません。
  このサンプルだけで他の時刻のメモリ破損を完全に除外したわけではありません。
- 起動時の`i2c_driver_delete ... i2c driver install error`は、未インストールのI2C0を
  初期化前に削除する既存処理でも出ます。今回の復号済みpanic経路とは区別します。
- FB-SIZEの具体的な受信長と、その発生原因は今回のログだけでは分かりません。
  XCLK・受信再開時の不完全フレーム、イベント処理の遅れなどは候補です。
  受信の停止・再開構成は今回変更せず、エラー処理が生存した状態で回復状況を確認します。
- 同じboot中の診断時刻を先に採取すると、並行更新されたタスク時刻との差が
  `4294967295`になる競合もありました。これは診断表示の別問題で、panic原因ではありません。

## 修正

1. 既存の`__wrap_xTaskCreatePinnedToCore`で、名前が正確に`cam_task`の呼び出しだけ
   RTOSへ渡すスタックを最低8192 bytesにします。追加内部RAMは通常6144 bytesです。
   要求がそれ以上なら縮小しません。Core 0・優先度3を維持します。
2. SDKのカメラはビルド済みライブラリなので、アプリ側マクロだけを変更する方法は使いません。
   ホストテストで実ラッパーの引数を検証し、実機ビルド後もSDKの`cam_config`がそのラッパーを
   呼んでいることをELFから検査します。
3. 作成結果、要求・実割当サイズ、cam_taskの最小残量をUSB/RTCへ追加します。
   スタック走査は有効なカメラのライフサイクル所有者が最大約1秒間隔で実施します。
   deinit前と初期化失敗時にハンドルを無効化し、USB observerはキャッシュだけを読みます。
4. 診断サンプル時刻を各フィールドのコピー後に取得し、時刻競合による経過時間の異常表示を修正します。
   RTCサンプルの識別子を更新し、増えた出力に合わせ固定出力バッファを4096 bytesにします。

エラー出力・スタック保護・watchdogを無効化しません。制御、IMU、左右足角度、LED同期、
RWLOG/CSV、再開可能なダウンロードを継続します。

## 検証と次の実機確認

`bash tools/test_runtime.sh`で実ラッパーの2048→8192、12288の維持、別名・null名の無変更、
タスク作成失敗、ハンドル出力なし、残量走査の間隔、破棄後の参照防止を検証します。
既存の制御・取得・足角度・最大イベント/足フレームログ・転送CRC・ブラウザ復帰検証も実行します。
GitHub ActionsはESP32-S3ビルド、ELF内の呼び出し検査、公開SHAと4バイナリのハッシュ検査を行います。
これらは修正版を実機で稼働させた証拠ではありません。

[Web flasher](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/)で
0.47.2を書き込み、まず測定開始前に1〜2分のUSBログを採取します。
boot IDの継続、`cam_task`の`created=1,allocated_bytes=8192`と残量、WebUIと左右足角度の
更新継続を確認します。FB-SIZE後の画像取得が回復するかが次の判断点です。
安定した場合に30秒測定とログ取得まで確認します。

公式資料：

- [ESP-IDF 4.4.7 ESP32-S3 fatal errors](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/fatal-errors.html)
- [ESP-IDF 4.4.7 FreeRTOS：スタック引数とhigh-water markはbytes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/freertos.html)
