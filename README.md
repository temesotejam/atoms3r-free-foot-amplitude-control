# 0.47.30 — yawを停止し、振幅制御のみへ

[Web書込みページ](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/)

yaw自動補正、±0.2°の左右目標応答試験、独立ジャイロ方位の計算・表示を停止。
左右とも選択した8°・10°・12°を目標に、従来どおり30秒測定する。
8°用の前回ピーク補正、左右別の振幅積分補正、MEKFの横揺れ推定、
固定3ms補償、300mA・最大100ms、足角度観測、LED同期、ログ保存を維持する。

RWLOGはv53 / 112 bytesを維持。互換用yaw欄は未測定値、valid=false、
目標差・周期数は0、reason=8（disabled）。方位0°の測定として扱わない。
メタデータにフィードバック・応答試験・方位計算の停止を明記した。
以前のログも従来の変換器で読み込める。

書込み後は本体WebUIを再読み込みする。0.47.30の実機処理時間は未確認。
以下は過去版の記録。0.47.28の操舵と0.47.29の応答試験は現在の測定では動作しない。

---

# 0.47.29 — 左右目標と旋回の応答確認

[Web書込みページ](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/)

8°用の前回ピーク補正が、操舵で片側目標を変えたときに切れる干渉を修正した。
適用判定には選択した平均目標を使い、従来の10秒条件・支持域・係数は維持する。

今回はyaw自動補正を休止し、同じ30秒の中で目標差に対する応答を確認する。
0–10秒は同じ目標、10–18秒と18–26秒は逆向きに片側±0.2°、26秒からゼロへ戻す。
δの変化は有効な1往復につき0.08°以内。正負の順序は連続するRunで逆になる。
平均目標、3ms、300mA、最大100ms、30秒、LED・Wi-Fi・ログ保存の流れを維持する。

**最初は10°で続けて2回。** 各回のRWLOGと足CSVを保存し、電源を切らずに
「ログを消去・次の測定へ」で2回目へ進む。その後8°でも同様に確認できる。
Webに今回の応答確認と順序を表示し、追加のモード選択は設けない。
RWLOGはv53 / 112 bytesのまま、停止後メタデータに条件を記録する。

0.47.28実機3回では制御完了2.5ms超過0 / 36,423、最大2.029ms、
ログ0.62–0.65MBを確認した。旋回方向は実機と一致、角度は未測定。
0.47.29の実機応答と処理時間は次の測定で確認する。

仕様・操作・評価方法：[STEERING_RESPONSE_04729.md](docs/STEERING_RESPONSE_04729.md)。
以下は過去版の記録で、0.47.28のyawフィードバックは今回の通常測定では休止する。

---

# 0.47.28 — ジャイロ旋回フィードバックとRWLOG軽量化

Web書込み: https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/

通常の起動・目標角選択（8° / 10° / 12°）・30秒測定・ログ保存の中で動作する。
開始10秒後から、独立したジャイロ方位の1往復あたりの変化と実際の正負ピーク差を使い、
平均目標角を保って正側を `A0 + δ`、負側を `A0 - δ` に調整する。
δは最大±1°、1往復で最大0.08°。固定の左右オフセットを引く方法ではない。
旋回を抑える目的であり、開始時の方位へ戻す制御や左右ピーク差ゼロの強制ではない。

ジャイロ方位は起動時の固定バイアスと開始前の加速度から初期化し、400 Hzで3次元積分する。
MEKFの推定バイアス・yaw・地磁気を使わない。横揺れのMEKF、既存の振幅制御、3ms補償、
300mA・最大100msパルス、LED同期、Wi-Fi/Webの動作は従来の構成を使用する。
ピーク評価には、実際にそのピークへ指令した目標角を保持して用いる。

RWLOG v53は **1試料274→112バイト（59.1%削減）**。
MEKFのyaw復元用クォータニオン、地磁気、未使用のMadgwick比較・β系列・重複ゼロ点などを除去。
加速度・ジャイロ・横揺れのMEKF・電流・LED・操舵状態・足角度・処理時間の検証値は残す。
足観測とイベントは `fields` / `rows` 形式で項目名を共有し、値・精度・件数を保持する。
旧形式もPython変換器で読め、v53のCSVと変換後metadataは従来どおり行ごとの項目名を持つ。
WebUIの足CSV、CRC、中断・再開にも対応する。地磁気の追加トリム取得・記録・表示は削除した。

前回の約1.61MBの2ログを同じ件数・新形式で保存した容量見積りは約0.61MB（約62%削減）。
これは形式の見積りで、新版の実機測定値ではない。
最大件数（足768、ピーク256、ゼロクロス256、監査128）の実シリアライズでも欠落なし。

ホストで、独立ジャイロの3次元運動・欠測・時刻wrap、補正の符号・上限・出力飽和・解除、
実際の制御処理による目標保持、新旧ログ変換とCRC・ダウンロード再開を検証する。
補正ゼロの制御は旧実装との20,000状態＋32,000連続サンプルの比較で一致する。
実機の直進改善量と2.5ms期限は未確認。最初は前回と同じ10°の通常30秒測定で、
実測旋回角、RWLOG、足CSVから補正方向・効果・処理時間を確認する。

仕様: [STEERING_04728.md](docs/STEERING_04728.md)。以下は過去版の記録。

---

# Current integrated build: Free-foot Runtime V2 / 0.47.26 Target selection

## 目標角の選択（0.47.26）

WebUIの開始ボタンの上に **8°・10°・12°** の選択欄を追加した。
目標角は、測定開始時の胴体ゼロ点から左右それぞれのピークまでの片側振幅。
初期値は8°。開始前に選択し、「30秒測定を開始」を押すとそのRunに適用する。
測定終了後は画面の「今回の測定目標」とRWLOGの目標角を確認して保存し、
「ログを消去・次の測定へ」で次の角度を選ぶ。同じ画面では選択を維持する。
再読み込み時は本体の現在値に合わせ、本体を再起動すると8°に戻る。

`POST /start-energy-control-autonomous?target_deg=10` のように開始要求へ目標角を同梱する。
HTTP側で候補を検証し、HTTP休止を経て制御担当タスクへ値と開始要求を一緒に渡す。
目標角の設定・開始・ログ生成は従来の単一所有者が行い、受付済みの目標を別の要求で上書きしない。
欠落・不正な目標角は400で拒否し、HTTP休止も測定開始もしない。
古いページからの引数なしSTARTは拒否されるため、書き換え後はWebUIを再読み込みする。

制御モデル・ゲイン・300 mA・最大100 ms・3 ms補償・30秒測定は0.47.25から維持。
**既存の直前ピーク補正は目標8°専用で、10°・12°では適用されない。**
これは今回追加した補正ではなく、従来からの対象角度の条件による。
0.47.25の5回分の期限実測を下に残す。0.47.26と10°・12°の実機結果は別に取得する。

検証：目標角の解析・候補外拒否、8/10/12の制御タスクへの引渡し、受付後の上書き拒否、
STOPによる未実行STARTの取消、状態読戻し、ブラウザの選択保持・実行中ロック・HTTP復帰をホスト試験で確認。
GitHub Actionsでは既存の全runtime試験、ESP32-S3ビルド、IRAM配置・サイズ検査、公開バイナリのSHA-256照合を継続する。

[書き込みページ](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/)

<a id="timing-overrun-summary"></a>

## 時間超過対策の総まとめ（0.47.25まで）

2026-09-27更新。対象は本リポジトリで行った **0.47.8〜0.47.25の時間超過対策**。
その前提となる、旧版から引き継いだ取得・実行基盤も以下に記す。

**変更前の0.47.25は、目標8°・30秒・予測3 msの5回をすべて完走し、
60,708回の制御完了で2.5 ms超過0回、平均280.135 µs、最大1,939 µsを確認した。**
最長の処理でも期限まで561 µsの余裕がある。Wi-Fi APを維持し、運転中のHTTPを休止する構成での結果である。
これは今回の条件・5回の実測結果であり、任意の運動・運転時間に対する最悪実行時間の保証ではない。

### 判定区間と維持した条件

- 判定区間は、RUNNING中に受け取った新しいジャイロについて、**ホストのIMU読取完了時刻から
  `runner.update()`の復帰まで**。`> 2500 µs`を超過とし、2500 µsちょうどは超過に数えない。
  MEKF、制御判断、同じサンプルの比較Madgwick、通常ログの同期保存を含む。
- この区間にはタスクの中断・待ちが含まれる。センサ内部の計測から物理的なモータ出力までの時間とは異なる。
  runner復帰後のスナップショット作成・公開は、従来から別の`owner_iteration`等で計測する。
  対策のために期限の起点・終点を動かしたり、比較計算やログ保存を期限外へ逃がしたりしていない。
- 期限判定には全fresh gyroのカウンターを使う。間引かれた通常ログ行数を分母にしない。
  **30秒完走、2.5 ms以内の完了、IMUの取得間隔、電流の取得間隔は別々に評価する。**
- 今回の対策期間を通して、ジャイロ400 Hz・加速度200 Hz、IMUの1 msポーリング、32件キュー、
  運転中の順次受信、独立した10 msのIMU滞留・鮮度異常停止、3 ms予測、300 mA・最大100 msを維持した。
- MEKFの全軸・全共分散、比較Madgwick、カメラ足観測、RWLOGと足CSVを継続する。
  足位置による損失や振幅非対称のモデル改善は、この時間超過対策とは別の課題とする。

### 引き継いだ取得・実行基盤

以下は0.47.8以降に新しく導入した変更ではなく、今回の対策の前提である。

| 対策 | 内容・現在の扱い | 根拠 |
|---|---|---|
| 取得・制御・モータI/Oの分離 | IMU取得はCore 1／優先度6、制御はCore 1／優先度4、Roller I/OはCore 0／優先度4。制御を常設の単一所有者にし、HTTPは要求とスナップショットだけを扱う。運転開始時には待機中の古いキューだけを境界付きで除き、その後のサンプルは順番に処理する。 | [取得分離](docs/V46N_ACQUISITION_RELEASE.md)、[開始境界](docs/V46O_STARTUP_FIX.md)、[統合基盤](docs/FREEFOOT_RUNTIME_V2.md) |
| 高優先度側の演算削減 | 加速度のノルム・角度は制御側の新規加速度ごとに計算し、ジャイロだけの更新では再利用する。 | [V46q](docs/V46Q_LIGHTWEIGHT_ACQUISITION.md) |
| IMU通信量と周期の整理 | STATUSで未読を確認し、更新された加速度／ジャイロだけを6／12 byteで読む。I2Cは1 MHz。0.5 ms・1 ms・2.5 msのpollを比較し、取得数と制御負荷を両立した1 msを採用した。 | [選択読取](docs/V46U_TIMING.md)、[1 MHz化](docs/V46V_DEADLINE_TIGHTENING.md)、[周期比較](docs/V46Y_IMU_SPEC_FROZEN.md) |
| パルス幅候補の高速探索 | V46rで全幅探索を高速候補探索に置き換え、V46sで判断入力・計算時間・選択幅を保存する監査を追加した。この二段探索は、後述の0.47.18で直接Q逆算へ置き換えた。 | [ソルバ監査](docs/V46S_SOLVER_AUDIT.md)、[直接逆算](docs/DIRECT_Q_INVERSE_04718.md) |
| 電流読取の重複削減 | パルス中の読取機会を1 msごとにし、同じ周回で取得済みなら20 msテレメトリ側の再読取を省く。電流の2 ms期限監査は継続する。 | [V46v](docs/V46V_DEADLINE_TIGHTENING.md) |
| メモリと書き出しの分離 | 大きなイベント・通常行・足フレームはPSRAMに置き、内部RAMを取得・スタック・DMA等に残す。停止後のメタデータ生成・全体CRCは書き出し担当で行い、確定済みログを再開可能なチャンク転送で回収する。 | [統合基盤](docs/FREEFOOT_RUNTIME_V2.md) |

旧周期比較では、0.5 ms pollは1 msより取得数を増やさず制御遅延が増え、2.5 ms pollは取得数が減った。
今回の改善は、センサ更新やログを間引いて期限を満たす方式を採っていない。

### 0.47.8〜0.47.25で実施した変更

各版の資料は変更時点の記録である。後から判明した実測結果と、現在も採用するかどうかを併記する。

| 版 | 実施した対策 | 狙い・現在の扱い |
|---|---|---|
| [0.47.8](docs/ESTOP_BACKLOG_0478.md) | 同じIMU連番の直立診断の幾何計算、取得済み加速度角を再利用。処理段階別の時間、終了状態、実際のファームウェア版を記録。 | 重複する三角関数・平方根を減らし、遅延箇所を測定可能にした。この段階では滞留ESTOPが残った。 |
| [0.47.9](docs/ESTOP_WORK_REDUCTION_0479.md) | MEKFの既知のゼロ・単位行列との積を省略。予測の行列積432→144積、リセット変換432→108積。状態文字列を長さ制限付きコピーへ変更。 | 全36共分散・交差項・Joseph更新・非ゼロ項の加算順序を保って演算を削減。30秒の同時動作を達成したが、期限超過は残った。 |
| [0.47.10](docs/FREEFOOT_SUCCESS_TIMING_04710.md) | 3要素内積を展開し、Joseph更新の共通積と加速度ノルムを再利用。int16ログ量子化を同じ丸め・飽和・欠測規則の軽量実装へ変更。フィルタ内部の時間を分離記録。 | 推定とログの反復演算を削減。単発実測では総時間は改善せず、部分処理だけで成功判定しなかった。 |
| [0.47.11](docs/DEFERRED_ATTITUDE_TIMING_04711.md) | 毎更新で全クォータニオンを保存し、制御用pitchだけを角度へ変換。診断roll/yawは保存姿勢から診断生成時に算出。 | 制御経路から毎更新2回の`atan2`等を除去。全軸推定と画像に対応する姿勢情報は保持。 |
| [0.47.12](docs/FILTER_TIMING_04712.md) | 比較Madgwickのpitchを上流と同じ式で直接取得し、未使用roll/yaw・重力計算を省く。MEKF演算を`O2 / no-fast-math`へ。パルス有無×新規加速度有無の4群で期限を集計。 | 不要な角度変換と演算実行時間を削減。負荷構成の違いを区別して評価できるようにした。 |
| [0.47.13](docs/LOG_ENCODER_TIMING_04713.md) | ログ変換器を分離し、O2と各フィールドの量子化の強制インライン化を試行。`log_encode`と`log_store`を分離計測。 | **強制インライン化は不採用**。コードが肥大化し、実機は開始約88 msで滞留ESTOP。変換器の分離と内訳計測は残した。 |
| [0.47.14](docs/LOG_ENCODER_REGRESSION_04714.md) | 量子化を共通関数に戻し、ログ変換器は`Os / no-fast-math`で小さく生成。実ELFで対象コード4 KiB以下を検査。 | 0.47.13の回帰を修正し30秒完走を回復。全258 byte、通常20 ms／パルス中2 msの記録周期、同期保存を維持。 |
| [0.47.15](docs/RECOVERED_RUN_STACK_TIMING_04715.md) | IMU取得・制御タスクの約1秒ごとのスタック残量走査を運転中は延期し、待機後に各タスク自身で再開。 | 高優先度側や制御側のメモリ走査を外す。軽量な生存・処理位置記録は継続。後の0.47.21で診断全体へ拡張した。 |
| [0.47.16](docs/IRAM_AND_MARKER_GUARD_04716.md) | MEKFの主要演算・補助関数と小型ログ変換器をIRAMへ配置。量子化を単一の.cpp定義にし、関数とXtensaリテラルの配置を揃える。 | 頻繁な命令取得のフラッシュ依存を減らす。実ELFの配置・対象関数16 KiB上限を検査。キャッシュミスだけが原因だったとは断定しない。 |
| [0.47.17](docs/CONTROL_CACHE_PREVIEW_04717.md) | 同じ判断内の候補幅計算を再利用。電流モデルは入力が完全一致する場合だけ再利用。運転中に使われない160×120プレビュー転記を省略。 | 同一入力の選択幅を維持。撮影・足検出・足記録は継続。二段探索内の候補キャッシュは0.47.18で置換されたが、入力一致の電流モデル再利用とプレビュー抑制は継続。 |
| [0.47.18](docs/DIRECT_Q_INVERSE_04718.md) | 目標振幅から必要Qを直接逆算し、残留電流を含む電流積分から整数ms幅を選択。候補ごとの角度・エネルギー再計算と途中の幅丸めを除去。 | 現在の方式。符号分岐ごとの有界な二分・隣接候補評価でQ誤差を最小化する。**選択基準の意図的変更を含み、旧版と全幅一致ではない。** |
| [0.47.19](docs/CONTROL_LATENCY_04719.md) | 内蔵I2C1を、取得タスクが通信完了待ちでブロックするESP-IDFドライバへ移管。比較Madgwickは入力・betaを固定して同じサンプルの判断直後に実行。直立診断も加速度ノルムを再利用。 | 高優先度IMUタスクがReadyのまま待つ時間を減らし、その間に制御を進める。判断を先行させても、比較計算・ログ保存は完了期限に含める。 |
| [0.47.20](docs/NORMAL_UPDATE_04720.md) | 派生加速度、MEKF呼出し・姿勢コピー、表示値、比較Madgwick等の通常更新にも限定的にO2・IRAMを適用。姿勢コピーを単一定義にしてリテラル配置を揃える。 | ゼロクロス以外の通常更新の負荷を削減。Madgwick本体の数値式は保持し、対象関数12 KiB上限と依存ソースのハッシュを検査。 |
| [0.47.21](docs/OFFLINE_RUN_04721.md) | 運転中のWi-Fi・HTTPを停止する方式を導入。USB診断を初期値OFF／明示ONにし、OFF時の周期出力・RTC詳細記録・メモリ走査を停止。ONでも運転中の重い走査を延期。 | Web処理と診断負荷を減らす。ログ確定後の通信復帰・回収を実装。**Wi-Fi停止は0.47.23で置換**。HTTP休止とUSBの明示ON方式は継続。 |
| [0.47.22](docs/FORE_AFT_STOP_04722.md) | Web STOPを使わない運転に合わせた姿勢STOPを、機械直立からの連続した前後±90°だけに限定。横倒しを除外。 | 通信休止に伴う停止操作の変更。新しい推定器や逆三角関数を追加せず、既存MEKFから判定する。単独の高速化ではない。 |
| [0.47.23](docs/HTTP_PAUSE_04723.md) | **Wi-Fi APを継続し、HTTPクライアントと待ち受けだけを休止**。ブラウザの定期GETも休止。終了後に確定ログを保持したままHTTPを再開。 | PCのSSID再接続作業を減らすための現在の運用方式。無線バックグラウンド処理は残るため再測定し、5回で残った4件の超過を次の対策対象にした。 |
| [0.47.24](docs/CORE_ISOLATION_04724.md) | IMUの1 ms通知をCore 0のESP_TIMER_TASKからCore 1のハードウェアタイマーISRへ移動。カメラ初期化・解放とArduino Wi-FiイベントをCore 0へ。キュー前後の時刻と超過詳細を追加。 | 通知遅延とコア間の干渉を減らす。ISRは時刻保存と通知のみ。実測の通知遅延は改善したが、制御完了には3件の超過が残った。 |
| [0.47.25](docs/MOTION_ACQUISITION_04725.md) | IMU取得・検査・梱包・キュー送受信・集計、ピーク／ゼロクロス、Q逆算、パルス開始終了、電流モデルへ限定O2・IRAMを拡張。runnerとログ行処理は配置のみ変更。 | 通常・イベント両経路の残る負荷とフラッシュ依存を削減。生成ラムダを含む実ELFを検査し、追加対象24 KiB上限に対して14,836 byte。**5回すべて期限内を確認。** |

0.47.18の旧5回・346判断の同一入力再計算では、272件が同じ幅、41件が−1 ms、33件が＋1 msだった。
上下限の飽和フラグは全件一致したが、この変更を「計算結果がすべて同じ高速化」とは扱わない。
それ以外の数値を維持する変更では、凍結した旧実装との比較を実施した。
ホストで値が一致しても、実機では指令時刻が変わり運動も変わり得るため、制御性能と処理時間は別に評価する。

### 診断と検証も段階的に追加

- **負荷の場所を記録：** パルス中／非パルス中の処理段階、MEKF予測・補正、比較更新、ログ変換・保存を分離。
  パルス有無×新規加速度有無の4群で、件数・超過・平均・最大を照合する。
- **同じサンプルを追跡：** 取得連番と時刻で、キュー投入・受信・派生値・MEKF・判断準備・判断・完了を対応付ける。
  取得処理／I2C呼出しとの重なりは実経過時間であり、制御を中断したCPU時間とは扱わない。
- **まれな超過を残す：** 1秒ごとの最悪例に加え、0.47.24から先頭64件の超過詳細を発生順に保存。
  全超過数は上限後も数え、`total = stored + overflow`を保つ。USB診断OFFでも期限監査は有効。
- **数値・配置・実機をそれぞれ確認：** MEKF全状態・全共分散、比較フィルタ、Q逆算、ログ全258 byteを比較し、
  ESP32の実ELFでIRAM配置とコードサイズを検査。その後に実機ログで期限を判定する。
  0.47.25の制御再生では20,000判断条件＋32,000連続サンプルを比較した。
- **保存機能を確認：** 停止・HTTP復帰・転送中断からの再開、RWLOGのCRC、添付足CSVの全フィールドを照合する。
  ログ生成・ダウンロードの成功だけで時間条件の達成とはしない。

### 実機結果の推移

平均は対象件数による加重平均、単位はµs。各行はその版で採用した比較集団であり、
すべての行が同じ試行数ではない。初期の単発比較や電池・床・姿勢の異なる実験から、
差分すべてを一つの変更の効果と断定しない。出典・集計方法・各ファイルのSHA-256は
[集計記録](docs/data/timing_results_through_04725.json)に保存する。

| 版 | 比較対象 | 2.5 ms超過 / 対象回数 | 平均 µs | 最大 µs |
|---|---|---:|---:|---:|
| 0.47.8 | 1回、約0.52秒でESTOP | 97 / 207 | 4,180.203 | 11,186 |
| 0.47.9 | 30秒×1回 | 947 / 12,144 | 1,514.268 | 6,884 |
| 0.47.10 | 30秒×1回 | 1,007 / 12,145 | 1,535.954 | 7,589 |
| 0.47.11 | 30秒×1回 | 1,127 / 12,145 | 1,574.942 | 6,235 |
| 0.47.12 | 30秒×1回 | 806 / 12,145 | 1,417.280 | 7,170 |
| 0.47.13 | 1回、約88 msでESTOP | 30 / 31 | 6,088.968 | 11,276 |
| 0.47.14 | 30秒×1回 | 1,045 / 12,145 | 1,513.601 | 6,905 |
| 0.47.15 | 30秒×1回 | 942 / 12,144 | 1,449.287 | 7,720 |
| 0.47.16 | 30秒×3回 | 311 / 36,423 | 923.805 | 4,365 |
| 0.47.17 | 30秒×5回 | 512 / 60,706 | 865.538 | 5,742 |
| 0.47.18 | 30秒×5回 | 533 / 60,713 | 970.689 | 5,582 |
| 0.47.19 | 30秒×5回 | 124 / 60,710 | 870.396 | 3,623 |
| 0.47.20 | 追加の30秒×5回 | 9 / 60,711 | 568.369 | 3,475 |
| 0.47.22 | 30秒×5回、Wi-Fi停止 | 0 / 60,709 | 483.678 | 2,325 |
| 0.47.23 | 30秒×5回、Wi-Fi維持 | 4 / 60,708 | 516.100 | 2,969 |
| 0.47.24 | 30秒×5回、Wi-Fi維持 | 3 / 60,709 | 529.674 | 3,192 |
| **0.47.25** | **30秒×5回、Wi-Fi維持** | **0 / 60,708** | **280.135** | **1,939** |

0.47.17は先行単発`43655722`を含めない正式5回。0.47.20は先行単発`53253541`を含めない追加5回で、
先行分も含む6回全体では11 / 72,855件、平均557.921 µs、最大3,475 µsだった。
0.47.21の通信休止方式は前後STOP限定後の0.47.22で評価し、意図的な転倒試験`315957546`は完走5回に含めない。

### 最新0.47.25の5回と機能継続

| Run | RWLOG末尾ID | 対象回数 | 超過 | 平均 µs | 最大 µs |
|---|---|---:|---:|---:|---:|
| 1 | 45023275 | 12,145 | 0 | 280.525 | 1,304 |
| 2 | 208179084 | 12,141 | 0 | 279.509 | 1,251 |
| 3 | 296389308 | 12,141 | 0 | 281.126 | 1,338 |
| 4 | 396189453 | 12,141 | 0 | 280.535 | 1,239 |
| 5 | 489609171 | 12,140 | 0 | 278.982 | 1,939 |

0.47.24の5回に対して平均は約47.1%短縮した。最大1,939 µsはRun 5の約14.535秒、
ゼロクロス判断・パルス開始を含む更新であり、探索だけの時間ではない。
パルス有無×新規加速度有無の4群すべてで超過0件だった。

- 全5回がFINISHED、終端エラーなし、終端の電流指令0 mA。
- IMUのキュー欠落・受信連番飛び・読取失敗は0。取得間隔4 ms超過も0、最大3,431 µs。
  取得・受け渡し・制御完了は開始終了境界が異なるため、各総数の小差を直ちにキュー欠落とみなさない。
- 記録された運転中の姿勢値はすべて有限で、MEKF採用が継続。通常ソルバ判断351件に失敗なし。
- 運転中の足1,499フレーム中、右1,470件・左1,495件が有効。**有効観測の校正範囲超過は左右とも0**。
  右の無効29件は形状不良7・候補曖昧22、左の無効4件は形状不良。約200 msの記録間隔は2か所。
- 全RWLOGのCRCが正常で、足CSVは開始・終了同期を含む計1,996行・全37列がRWLOGと一致。
  メタデータの切り詰め、足バッファのoverflow、ソルバ監査の上書きはない。

### 現在の運用と残る範囲

**Wi-Fi APは出し続け、START_SYNC・RUNNING・END_SYNCではHTTPを休止する。**
ブラウザはPC側の時計で待ち時間の目安を表示し、状態GETを休止する。
機体はMEKF・足観測・制御・ログ保存を継続し、通常終了／ESTOPで記録を確定してからHTTPを復帰させる。
復帰時にロガー・足ゼロ・転送tokenを初期化しないため、その後RWLOGと足CSVを回収できる。
AP維持はすべてのPC・電波条件で無切断を保証するものではない。

USB診断は起動ごとにOFFで、Web診断欄か`DIAG ON`で有効化する。USB接続だけでは有効にならない。
ONでも運転中の重いヒープ・スタック走査は延期する。通常の期限監査とログ保存にUSB診断ONは不要。
運転中のWeb STOPは提供せず、姿勢STOPは前後±90°で動作し、横倒しを対象にしない。
前後投影が消える真横姿勢を含む任意の3D転倒を識別する保証はない。
電源断・再起動で本体RAMのログは消えるため、HTTPの復帰処理は本体再起動を使わない。

今回合格したのは上記の**読取完了→制御完了2.5 ms**である。
ホスト取得間隔には1 msポーリング由来のゆらぎがあり、センサ内部時刻の検証済みフラグはfalse。
電流監査も別で、Run 5書き出し時の起動後累積では、電流読取処理の2 ms超過は0だが、
有効電流の取得間隔の2 ms超過は2,662 / 13,488、最大3,528 µsだった。
この起動後累積値を5ファイル分加算しない。すべての時間条件が解決したという意味にはしない。
I2C異常を実機で注入した停止応答、長時間・別振幅での期限、外部基準に対する姿勢／足角度精度、
足位置を取り入れた損失・非対称性の改善は、この5回で新たに検証済みとはしていない。

入力1組のCRC・全足CSVフィールド・期限集計は次の既存ツールで再確認できる。

```sh
python3 tools/summarize_integrated_run.py INPUT.rwlog INPUT_foot.csv --out summary.json
python3 tools/convert_rwlog_to_csv.py INPUT.rwlog --out decoded
```

複数回の平均は`control_latency.done_age`の`sum_us`合計を`count`合計で割る。
同カウンターをまだ持たない旧版は、記録済みの各回平均を件数で加重した値として集計記録に区別する。
最大値は各回最大の最大、超過数は各回の`v46u_deadline.over_budget`の合計を使う。

## 版別資料と過去の記録

以下は各版の公開・解析時点の記録を保持したもの。「未確認」「次の測定」は当時の状態を表す。
最新の評価と現在採用している対策は、上の[時間超過対策の総まとめ](#timing-overrun-summary)を参照する。

[0.47.25: acquisition and motion/pulse hot-code placement](docs/MOTION_ACQUISITION_04725.md) ·
[0.47.24: Core1 IMU timer, Core0 camera lifecycle/events and overrun detail](docs/CORE_ISOLATION_04724.md) ·
[0.47.23: keep Wi-Fi connected, pause HTTP, resume sealed-log downloads](docs/HTTP_PAUSE_04723.md)

[0.47.22: fore/aft-only STOP, sideways overturning excluded](docs/FORE_AFT_STOP_04722.md)

[0.47.21: offline run, tilt STOP, USB opt-in and log recovery](docs/OFFLINE_RUN_04721.md)

[Web flasher](https://temesotejam.github.io/atoms3r-free-foot-amplitude-control/) ·
[0.47.19 five-run results and 0.47.20 normal-update execution](docs/NORMAL_UPDATE_04720.md) ·
[0.47.19 IMU waiting and control latency](docs/CONTROL_LATENCY_04719.md) ·
[0.47.17 five-run baseline and direct-Q inverse](docs/DIRECT_Q_INVERSE_04718.md) ·
[0.47.16 hardware results and exact-input calculation reuse](docs/CONTROL_CACHE_PREVIEW_04717.md) ·
[0.47.15 hardware result, targeted IRAM and weak candidate confirmation](docs/IRAM_AND_MARKER_GUARD_04716.md) ·
[0.47.14 recovered run and deferred stack diagnostics](docs/RECOVERED_RUN_STACK_TIMING_04715.md) ·
[0.47.13 startup ESTOP and compact encoder correction](docs/LOG_ENCODER_REGRESSION_04714.md) ·
[0.47.12 hardware result and log encoder follow-up](docs/LOG_ENCODER_TIMING_04713.md) ·
[0.47.11 hardware result and filter timing follow-up](docs/FILTER_TIMING_04712.md) ·
[0.47.10 hardware result and deferred attitude display](docs/DEFERRED_ATTITUDE_TIMING_04711.md) ·
[Successful free-foot run and timing follow-up](docs/FREEFOOT_SUCCESS_TIMING_04710.md) ·
[Follow-up ESTOP and MEKF work reduction](docs/ESTOP_WORK_REDUCTION_0479.md) ·
[ESTOP analysis and control work profiling](docs/ESTOP_BACKLOG_0478.md) ·
[USB diagnostic procedure](docs/USB_DIAGNOSTICS.md) ·
[Fixed-pose foot calibration](docs/FOOT_CALIBRATION_0477.md) ·
[Static-pose comparison and yaw diagnostics](docs/POSE_COMPARISON_0476.md) ·
[Observed range update and all-axis MEKF diagnostics](docs/RANGE_MEKF_DIAGNOSTICS_0475.md) ·
[Marker identity, zero quality and image diagnosis](docs/MARKER_IDENTITY_0474.md) ·
[Coordinate audit](docs/ROBOT_COORDINATE_AUDIT_20260924_JA.md) ·
[Earlier marker-loss analysis](docs/MARKER_TRACKING_0473.md) ·
[Instability review](docs/INSTABILITY_REVIEW.md) ·
[Runtime architecture](docs/FREEFOOT_RUNTIME_V2.md)

The current main branch integrates observation-only right/left foot angles with
the V46al-R2 controller. It uses permanent control ownership, PSRAM event storage,
and a resumable, CRC-verified RWLOG export. **Version 0.47.9 completed a 30-second
free-foot hardware run with MEKF, camera foot observation and actuation together.**
All 12,143 acquired IMU samples were delivered, with no queue drops or sequence
gaps. Both feet were observed during the same run (288/292 right, 291/292 left).
The user reports fore/aft movement with unfixed feet and increased foot-switching
loss; the observed amplitude asymmetry is an improvement topic, not a failure of
this integration milestone. Strict 2.5 ms processing deadlines remain unmet in
947/12,144 completions (maximum 6,884 us).

**Version 0.47.10 also finished 30 seconds with MEKF, camera and actuation.**
All detected feet were within the configured support (right 282/286 valid,
left 285/286 valid during measurement). However, the 2.5 ms overruns were
1,007/12,145 (8.29%, maximum 7,589 us), so this hardware run did not show timing
improvement. No queue drops or delivery sequence gaps were recorded; acquired
and delivered audit counts differ by one at the measurement boundary.

**Version 0.47.11 again finished the 30-second integrated run.** During measurement,
right/left detections were 291/293 and 293/293, with no detected observation outside
support. The expanded right support accepted 107 valid positions above the former
173-pixel upper bound. The weighted filter mean fell from 779.529 to 730.169 us,
and attitude extraction from 156.681 to 105.037 us. However, total 2.5 ms overruns
increased to 1,127/12,145 (9.28%), despite the maximum falling to 6,235 us.
This is partial work reduction, not completion of the real-time target.

**Version 0.47.12 again completed the 30-second integrated run.** Acquisition and
delivery audits both counted 12,145 samples, with no queue drops, sequence gaps,
or fault. Right/left feet were valid in 287/293 and 293/293 measurement frames;
no detected position was outside support. Deadline overruns fell to 806/12,145
(6.64%), and the completion mean to 1,417.280 us, but the maximum rose to 7,170 us.
The pulse-active/fresh-accel group had 312/1,293 overruns (24.13%). Partial timing
improvement is observed; the strict deadline and rare long delays remain unresolved.

**Version 0.47.13 regressed: measurement stopped after 87,904 us with ESTOP.**
The delivery age reached 10,682 us while sensor polling remained regular. The
first pulse log took 935 us versus 108 us in the previous run; pulse-on log
encoding averaged 836.552 us. All 47 foot frames belong to START_SYNC, so this
run does not validate simultaneous foot observation during measurement.

Version 0.47.14 removed forced per-field inlining and uses one shared quantizer
with the size-oriented compiler policy. The exact numerical formula, all 258
row bytes, logging rates and synchronous storage remain unchanged. A linked
code-size gate prevents recurrence of the expanded encoder, but is not timing
proof. The 10 ms stale-data stop and 2.5 ms completion deadline are unchanged.
MEKF, Madgwick, camera, calibration and control are retained.

**Version 0.47.14 recovered: the new hardware run finished 30 seconds.** All
12,144 acquired samples were delivered, with no queue drops, sequence gaps or
fault. MEKF was adopted throughout measurement, with right/left foot detections
in 283/288 and 287/288 frames and no valid observation outside support. The
2.5 ms deadline remains unmet in 1,045/12,145 completions (8.60%, mean 1,513.601 us,
maximum 6,905 us). The overrun rate is worse than 0.47.12 (6.64%); recovery from
the startup ESTOP is not a timing-goal success. Acquisition and completion
audits use different transition boundaries, explaining the one-count difference.

Version 0.47.15 defers the control and IMU tasks' periodic stack-watermark scans
through START_SYNC, RUNNING and END_SYNC. Each task resumes its own due scan when
idle; heartbeats continue and USB diagnostics expose the cache age, scan count,
allowed flag and last/maximum scan wall time. This removes known diagnostic
memory walks from real-time owners. The previous profile does not isolate their
contribution, so it does not establish them as the cause of all deadline overruns.
The RTC journal layout, overflow protection, estimator/control math, foot
calibration, logging rates and deadline definitions are unchanged. Callback
exclusion, idle resumption, clock wrap and the existing runtime suite are checked.

**Version 0.47.15 also finished 30 seconds.** Acquisition, delivery and completion
audits all counted 12,144 samples, with no queue loss, sequence gaps or fault.
Overruns improved to 942/12,144 (7.76%) and mean completion to 1,449.287 us,
but the maximum increased to 7,720 us. The pulse-plus-acceleration group still
averaged 2,628.506 us. Without USB stack-scan measurements or a controlled repeated
comparison, the improvement cannot be attributed entirely to scan deferral.
Right/left feet were valid in 294/295 and 295/295 measurement frames. One right
observation at X=204.94246 was outside support; it also jumped in X/Y while
contrast and weight collapsed, making feature misidentification a concern.

**Version 0.47.16 finished three 30-second runs.** Across 36,423 measured
completions, 311 exceeded 2.5 ms (0.854%, maximum 4,365 us), with no queue drops
or delivery sequence gaps. During measurement, right/left feet were valid in
872/879 and 877/879 frames, with no detected observation outside support.
Normal pulse starts account for 112 of the 132 runner-only overruns.

**Five new 0.47.17 runs completed 30 seconds each.** The comparison baseline is
512/60,706 deadline overruns (0.843%, maximum 5,742 us), with no IMU queue drops
or delivery sequence gaps. Foot support exceedances were zero on both sides.
Right/left measurement detections were 1,444/1,462 and 1,462/1,462.

**Version 0.47.19 completed all five 30-second runs.** Deadline overruns fell
from 533/60,713 (0.878%, 5,582 us maximum) in 0.47.18 to 124/60,710
(0.204%, 3,623 us maximum). No acquisition error, queue drop, delivery gap or
foot calibration exceedance occurred. One run had significant amplitude
asymmetry; timing improvement is not evidence of improved amplitude accuracy.

Version 0.47.20 targets normal-update execution: selected estimator wrappers,
derived acceleration, display calculations and comparison Madgwick routines
use O2/no-fast-math and internal instruction RAM. Expressions, update ordering,
sampling, control rules and the deadline measurement scope stay unchanged.
Host differential tests and ESP32 link gates check equivalence and placement;
hardware timing still needs the next five-run comparison.

Version 0.47.19 gives the exclusive internal I2C1 bus to the ESP-IDF interrupt
completion driver after M5Unified boot configuration and sensor validation.
The high-priority reader blocks during transfer completion, allowing control
on the same core to run. The existing 1 ms polling and 400/200 Hz ODR remain.
Autonomous comparison Madgwick uses captured pre-decision data/beta and runs
in the same sample after the control decision, before logs and publication.
The complete runner deadline still includes comparison and logging. Latency
metadata links sample sequences to receive, MEKF, decision and completion,
with I2C/poll overlap explicitly labeled as wall time rather than CPU time.
The 10 ms stale-data stop remains independent of the IDF driver's potentially
longer bus-error watchdog. Fault-injection hardware testing is still pending.

Version 0.47.18 directly inverts the amplitude model into a continuous requested
Q, clips feedforward before the existing side integral, then solves the current
integral for one integer-ms pulse. Both signs of charge are handled when residual
current opposes the command. Final quantization minimizes absolute Q error;
equal errors choose the shorter pulse including zero. The prior intermediate
feedforward width rounding and repeated angle/energy candidate calculations are
removed. Same-state replay of the five runs changes 74/346 widths by exactly
1 ms and keeps 272 unchanged; upper/lower saturation flags match throughout.
The active production decision block is tested against a full 101-width Q-error
oracle, including invalid inputs, reverse-current branches and domain guards.
RWLOG v51 layout is retained; solver audit schema 2 explicitly identifies the
new field meanings and unavailable feedforward width. This is an intentional
arithmetic change, not a claim of identical physical control or achieved timing.
MEKF, foot calibration, 3 ms compensation, 300 mA/100 ms limits, acquisition and
logging rates, task priorities and the 10 ms stale-data stop are retained.

Version 0.47.17 reuses charge/energy predictions only within one zero-cross
selection and caches current-model results only for exactly matching inputs.
Both searches, target corrections, tie-breaking and the zero-output baseline
are retained. All 208 recorded decisions and 20,000 additional cases match the
frozen controller; candidate physics evaluations fall from 9,152 to 4,253 in
that recorded replay. This is a work-count reduction, not a hardware timing
claim. Camera foot tracking and recording continue while unused run-time
thumbnail copies are suppressed. Idle preview images retain paired timestamps
and attitude. The full 0.47.16 build's actual M5GFX dependency is pinned to
0.2.30. Filter/control/log rates, calibration and timing guards are retained;
changed command latency and real motion still require hardware verification.

Version 0.47.16 places selected MEKF routines and the compact log encoder in IRAM,
with a real-ELF placement and 16 KiB function-code gate. Numerical formulas and
compiler policies stay unchanged; external calls and constants can still use
flash, so this is not cache-disabled safety or a deadline guarantee. The marker
tracker now requires the existing three-frame confirmation if a recent candidate
jumps at least 20 px in X and 16 px in Y while contrast drops below 50% and weight
below 10% of its last accepted value. Strong motion and dimming at the same
location remain immediate. The suspect single frame is not used to expand
calibration; persistent confirmed observations can still be reported outside
support. These criteria select only that suspect frame among the seven supplied
run CSVs, but source images are absent and new hardware results are still needed.

Version 0.47.12 computes comparison Madgwick pitch directly from its public
quaternion using the exact upstream 2.4.0 expression, avoiding unused roll/yaw
and gravity calculations. The MEKF numerical file uses GCC O2 without fast-math;
the full estimator and filter rates remain unchanged. Input cohorts now partition
the existing completion counters by pulse state and fresh acceleration, since
workload composition also changed between runs. Pinned upstream getter comparisons,
full MEKF differential tests under both host optimization levels, and the existing
runtime regression suite pass. Installed Madgwick sources are checked during the
target build. The subsequent hardware result and remaining work are described above.

Version 0.47.11 retained the full posterior quaternion on every filter update,
extracts only control pitch in the control path, and derives all three display
angles from the copied quaternion when serializing diagnostics. Two `atan2`
calls per control update are removed without lowering the MEKF update rate or
dropping roll/yaw diagnostics. Frozen camera snapshots keep their own attitude
and timestamp. Control pitch and display axes match the frozen reference,
including normalization and pitch near +/-90 degrees. The full covariance,
other filter work, control limits, logging and deadline endpoints are unchanged.
The subsequent hardware result and remaining work are described above.

Version 0.47.10 extended accepted marker support to right [39,182] and left
[37,177.5] pixels, covering all supplied identity-v2 observations with at least
one pixel of margin. Scale coefficients and independent per-boot zeroing remain
in use. MEKF fixed-size inner products are unrolled and identical Joseph factors
are reused; bounded log quantization preserves the previous encoded integers.
MEKF prediction, correction, attitude extraction and Madgwick comparison times
are separately recorded inside the filter profile. The subsequent hardware
result and next timing change are described above; host tests are not deadline proof.

Version 0.47.9 followed another delivery-backlog ESTOP on 0.47.8 at 0.521 seconds.
The added profile shows pulse-on filter updates averaging 1,007 us and control
iterations averaging 2,615 us. The MEKF now omits known-zero and identity products
while retaining all covariance terms, Joseph update and summation order. Snapshot
strings use bounded copies. A frozen 0.47.8 differential reference verifies the
posterior, diagnostics and all 36 covariance entries. The subsequent hardware
success and remaining timing work are described above. Foot observation, filter
rates and control limits are retained.

Version 0.47.8 follows a hardware ESTOP about 0.77 seconds into measurement:
IMU delivery age reached 10,284 us and exceeded the unchanged 10,000 us guard.
It reuses duplicate pose geometry calculations and records per-stage wall times
separately for pulse-on and pulse-off work, including control snapshot publication.
RWLOG metadata now includes the terminal state/current command and actual runtime
version. The supplied log shows age rising during pulses, but does not isolate
one bottleneck. This is a diagnostic build; resolution on hardware is unverified.
The foot calibration, controller, acquisition sequence and protection limits
remain unchanged. See the linked analysis for evidence and one-run retry steps.

Version 0.47.7 updates the independent foot-angle scales using the two supplied
poses captured without hand contact: a 20.2764165-degree body roll change and
right/left marker displacements of 130.69222 / 130.39768 pixels. New scales are
0.155146317 / 0.155496758 deg/px. Applied to the two hand-supported poses held out
from the fit, both sides have residuals of about 0.26--0.32 degrees. This is a
provisional MEKF-referenced calibration, not independently established absolute
accuracy; one held-out pose has a 7.36-degree yaw change. Per-boot independent
zeros and pixel support bounds remain in use. Normal status, frozen images and
RWLOG share the new calibration metadata and source-file hash.

Version 0.47.6 adds a pre-measurement static-pose comparison in the device WebUI.
Record an upright baseline and several held fore/aft tilts and return poses. Each
pose checks five frozen frame/IMU snapshots over about three seconds and saves the
last CRC-verified image. The display compares changes in relative foot angles
and body roll; other-axis changes are flagged. All-axis MEKF inputs and estimated
gyro bias are available in regular and image diagnostics. A bounded browser-side
status history accompanies the comparison JSON. Reboots, changed zeros, movement,
stale inputs and failed transfers cannot silently replace the baseline.

The latest 0.47.5 hardware pair changes body roll by -20.434 degrees, foot angles
by +21.617 / +21.066 degrees, and yaw by +8.446 degrees. These two points do not
establish new slope coefficients. The added comparison is for collecting repeated
poses and identifying the remaining error; it does not apply a calibration fit.
A synthetic pure fore/aft out/hold/return replay with accelerometer corrections
keeps yaw below 0.02 degrees; actual motion and estimator drift still need hardware
evidence. See the procedure above. Control math and RWLOG v51 are unchanged. The later 0.47.7 fixed-pose fit is described above.

Version 0.47.5 incorporates the new 0.47.4 hardware observations. Both feet remain
detected through 781 captured frames, and the tilted status reports right 21.7630°
and left 21.0887°. The observed X positions slightly exceed the old support, so
the accepted ranges extend to right [40,173] and left [39,177.5] pixels, with a
one-pixel minimum margin around the new low endpoints. Original support and the
unvalidated extension are explicit in diagnostics; angle slopes are not refitted.
The WebUI's shaded ranges are read from the same firmware metadata.

Diagnostic JSON now includes MEKF roll, pitch, yaw and quaternion from a coherent
posterior snapshot, with validity and sample age. These are available both in
ordinary diagnostics and alongside an image's saved delivery-time control state.
No filter update or sensor access is performed by HTTP. The main side-to-side
control angle keeps its existing reference and 3 ms compensation. The MEKF frame
is preserved; yaw is relative to filter initialization, not compass heading.

Version 0.47.4 addresses a plausible source of the remaining left/right angle
mismatch: a weaker nominal-row white region can be accepted as the left zero
before a stronger displaced marker is examined. All 17 scan heights now compete,
separate white islands stay separate, and ambiguous candidates or implausible
track jumps are reported as invalid. A neutral-position envelope and stable
marker positions gate automatic zero acquisition; the two zero values are still
measured independently. A synthetic distractor case improves from right 22.99° /
left 10.90° to right 22.99° / left 22.77°. This is a reproduced software failure
pattern, not proof of the hardware diagnosis or calibrated angular accuracy.

The device WebUI can capture one diagnostic image while measurement is stopped.
Its selected positions, candidates, reasons and zeros belong to the same frame,
transferred as a frozen 160×120 grayscale image in CRC-checked chunks. Save the
image and metadata together with “画像付き診断を保存”. Capture an upright frame
and a frame with both feet fixed while the body is tilted fore/aft to inspect the
actual marker choice. The UI identifies MEKF as body side-to-side rocking and
foot angles as body-relative; MEKF axes, gyro calibration and control math are unchanged. Foot
angle slopes use the 0.47.7 fixed-pose fit described above. The user's fore/aft trial does not make the small
side-to-side MEKF value a failure. Real-image identity and angle accuracy still
need hardware confirmation; see the linked procedure and limitations.

The 0.47.2 camera stack fix remains active. Subsequent USB logs extend to 237.5 s
without another reset, and the user reports stable WebUI updates. Frame-size
mismatches and sporadic camera event overflows still occurred; this is not proof
of indefinite stability. See [the matched backtrace and fix](docs/CAMERA_STACK_PANIC_0472.md).
The eight-second USB startup window, automatic monitor reconnection, RTC reset
evidence, and browser recovery from malformed status data remain available.

The earlier implementation notes below are retained as history.

---

# AtomS3R Amplitude Control Development

AtomS3Rを用いたリアクションホイール系の**振幅制御改善**を進めるための開発リポジトリです。

このリポジトリは、姿勢推定・計測系の検証を行った
[`atoms3r-mekf-dynamic-validation`](https://github.com/temesotejam/atoms3r-mekf-dynamic-validation)
の最終状態 **V46aj / 0.46.35** を、そのまま初期ベースラインとして引き継いで開始しました。

## 開発の出発点

- 初期ファームウェア: **V46aj / 0.46.35**
- 姿勢推定: **6-state MEKF**
- ZEROクロス遅延補償: **3 ms固定**
- 次ピーク基準振幅: **ZEROクロス角速度と進行方向による rate-only model**
- RWLOG: **v51**
- 元リポジトリのV46aj実装コミット: `1e9fb1bfdd8261bedc6657158c27142921848939`
- 元リポジトリの凍結時点: `1af3031d068a483361f0b519c35a826999040719`
- インポートしたTree: `b90c4314bb296b66dc2f44cbf80228605f0ee6a9`

初回インポート時点では、ソース、ドキュメント、テスト、GitHub Actions、Web flasherを含むTreeが元リポジトリと完全一致しています。

## このリポジトリで進めること

姿勢推定そのものを主題に戻すのではなく、現在のMEKF計測を土台として、主に次を検討します。

- ZEROクロス状態からの**次ピーク振幅予測の改善**
- 予測誤差と実測ピークを使った**入力Qの決定方法の改善**
- 正負半周期の差や状態依存性の整理
- 目標振幅へ収束させる**振幅制御則の改善**
- 実機RWLOG・動画を用いたモデル／制御則の検証

新しいモデルや制御則は、V46ajベースラインとの差が追える形で追加します。
元の `atoms3r-mekf-dynamic-validation` は保存版として維持し、今後の開発変更はこのリポジトリ側で行います。

## ベースラインで確認済みのこと

2026-09-19のV46ai実機測定（目標8°、遅延補償3 ms、30秒）では、

- 動画とファームウェアのピーク **69個がすべて一対一で対応**
- 10〜30秒の46ピークで、MEKFと動画のピーク差 **RMSE 約0.103°**
- 同区間の目標8°に対するMEKFピークのRMSE **約0.643°**

でした。

この結果から、**実際の揺れを捉える姿勢推定誤差と、目標振幅へ揃える制御誤差を分けて扱う**ことを、この開発の出発点とします。

詳細:
- [現在の角度推定](docs/ATTITUDE_ESTIMATION_V46AI_JA.md)
- [V46aj: 3 ms固定化](docs/V46AJ_FIXED_3MS.md)
- [V46ai: rate-only次ピーク予測](docs/V46AI_RATE_ONLY_BASELINE.md)
- [元リポジトリ最終スナップショット](docs/FINAL_SNAPSHOT_20260920_JA.md)

## 現在の開発版: V46al-R2 / 0.46.42

実機確認済みの `atoms3r-amplitude-control-v46ak-stable` を戻り基準とし、
V46akのZEROクロス rate-only予測に **直前ピーク `A_prev` の残差補正だけ**を追加して、
その補正後の `A_free` を既存のQ決定へ渡す実制御検証版です。

補正は **target=8°、t>=10 s、side別の5 Run測定support内**でのみ有効です。
それ以外ではV46akのrate-only予測へそのままフォールバックします。

変更しないもの:
- 6-state MEKF
- 3 ms固定ZEROクロス補償
- ZEROクロス/ピーク/side判定
- 電流・I0モデル
- 既存Q solverとKi
- 300 mA / 最大100 ms
- ESTOPと安全条件
- V46akの入力直前実電流・ホイール速度観測
- RWLOG logger/metadata生成・v51時系列レイアウト
- 現在のRWLOGダウンロード経路（logger.cpp/h と converter はstableと完全一致）

基準stable: `atoms3r-amplitude-control-v46ak-stable@bb9c5ed07c5ca8b3c6c6b5813b6c2f1b1f57a6ec`

- [V46al-R2の変更内容](docs/V46AL_R2_PREVIOUS_PEAK_ACTIVE_CONTROL.md)
- [V46akの観測追加内容](docs/V46AK_PRE_INPUT_STATE_OBSERVATION.md)
- [V46ajの確定済み角度推定](docs/ATTITUDE_ESTIMATION_V46AI_JA.md)

## 現在のWeb flasher

[AtomS3R Web flasher](https://temesotejam.github.io/atoms3r-amplitude-control-development/)

現在は **V46al-R2 / 0.46.42** を書き込みます。

---

## Frozen control baseline: V46aj / 0.46.35

Autonomousの遅延補償は **3 ms固定**です。0・6・9 msへの切り替え、選択画面、設定APIはありません。
ZEROクロス判定には、MEKF角度をバイアス補正済み角速度で3 ms先へ進めた角度を使います。
次ピーク予測は最初の通常判断から角速度式を使用し、直前ピークは予測式の入力にしません。
MEKF、ピーク追跡、Qゲイン、Ki、300 mA・最大100 msの出力上限はV46ajベースラインのままです。

以下は過去の構成・検証の記録です。

---

# V46 MEKF + Dynamic-beta Madgwick synchronized validation build

This working firmware is derived from the V45 current-audit / Autonomous Energy Control V7 build.
The **adopted control/detector attitude is now a 6-state MEKF**. During the Autonomous V7 capture, only the adopted **dynamic-beta Madgwick hold-073** filter is kept online as a comparison estimator. It never commands the motor.

Key validation behavior:

- MEKF drives zero-cross, peak detection, and Autonomous V7 angle-dependent decisions.
- Dynamic-beta Madgwick is logged online for later comparison only.
- Raw accelerometer/gyro, gyro-only integration, and accel-only atan2 remain logged for offline analysis.
- The existing GPIO38 `LED_SYNC_PATTERN_V2_LOGGED_ANCHORS` START/MID/END video synchronization is unchanged.
- RWLOG binary format is **v46**, append-only over the complete v45 sample prefix.
- For video comparison use the continuous columns `pitch_mekf_abs_deg` and `pitch_madgwick_dynamic_abs_deg`; `pitch_mekf_control_deg` is the run-relative angle used by control.
- MEKF acceleration trust diagnostics (`mekf_accel_confidence`, residual, magnitude error, `mekf_accel_used`) are logged on every sample.

See `docs/MEKF_DYNAMIC_COMPARE_V46.md` for the validation-specific log map and checks. Historical documentation below is retained because the control/current-audit logic is inherited.

---

# Q1 direct next-peak shadow passive logger

This derived firmware implements the current online-shadow candidate, **Q1**, and is strictly motor OFF. It records manual motion, detects zero-cross states, predicts the next video-coordinate absolute peak at Q=0, and records the inverse request that would be required on the canonical `q_target_mA_s` axis. It never applies that request.

The source project `2026_08_24_v62_passive_free_decay_logger` remains unchanged. E2/gyro half-range work remains an offline diagnostic result and is not in the Q1 validity or calculation path.

## Fixed Q1 model

```text
A_next_abs = 0.01304 + 0.08812 * abs(physical_roll_rate_dps)
             + 0.11858 * physical_next_peak_side + g_side * q_target_mA_s

g_plus  = 0.29032 deg/(mA s)
g_minus = 0.25455 deg/(mA s)
```

The physical rate is official `+gy` after startup y-bias subtraction. The adopted-filter detector angle is anti-correlated with `+gy`, so the fixed acceptance rule is: detector `- -> +` requires `+gy < 0`; detector `+ -> -` requires `+gy > 0`. At an accepted zero-cross, the current-sample `+gy` sign determines `physical_next_peak_side`: positive rate means physical `+1` next peak, and negative rate means physical `-1` next peak. The detector coordinate, its measurement-start reference, 0.08 deg rearm, and 200 ms event interval are unchanged.

The Q=0 baseline is calculated directly from Q1; E2 `H_next` and an IMU absolute-peak reconstruction are not used.

## Safety boundary

- `Q1_SHADOW_MOTOR_OFF_ONLY=true` is independent of the UI. `serviceFast()`, passive start, and the retained legacy `beginPulse()` all force motor command, current setting, pulse width, and pulse-active state to zero.
- `Q_req_shadow` is metadata only. No Q1 function calls motor/current/pulse generation.
- The target is locked while a run is active.
- Negative or zero required augmentation is `INVALID_BRAKING_NOT_IDENTIFIED`; no negative Q is calculated.
- Q is not clipped. The nonzero canonical support is `0.454 <= q_target_mA_s <= 1.197`. Outside it the event is `INVALID_Q_BELOW_SUPPORT` or `INVALID_Q_ABOVE_SUPPORT`.
- The rate support recorded from the canonical dataset is `1.68--27.80 deg/s`; an accepted detector crossing outside it is retained with a rate-support INVALID reason rather than extrapolated.

## Run procedure

1. Wait for `READY`. ZERO and Current Roll target remain display-only.
2. Before starting, set **Target next peak |A|**. It is an absolute peak target for Q1 shadow, separate from Current Roll. `0.0` intentionally produces braking INVALID records.
3. Start the fixed-horizon video, then press **Start passive capture**. Settings lock for the run.
4. After the LED start signature, move and release manually once. No strict hand-release velocity condition and no free-decay lock are required. Q1 accepts a crossing only after the detector angle leaves 0.08 deg.
5. Download the RWLOG. Verify all time-series samples have `motor_cmd_mA=0`, `current_mA_setting=0`, `pulse_width_ms=0`, and `pulse_active=0`.

## RWLOG and offline evaluation

Metadata fixes `q1_model_name=direct_video_q1_20260829`, `q_model_axis_type=q_target`, and the five Q1 coefficients. Each `q1_shadow_events` item records:

```text
q1_shadow_event_index, zero_cross_time_ms,
zero_cross_rate_dps, zero_cross_abs_rate_dps,
physical_next_peak_side,
detector_crossing_direction,
detector_angle_before_deg, detector_angle_after_deg,
crossing_interpolation_alpha, interpolated_zero_cross_time_ms,
physical_roll_rate_before_dps, physical_roll_rate_after_dps,
interpolated_physical_roll_rate_dps, sign_gate_passed,
q1_intercept_deg, q1_rate_term_deg, q1_side_term_deg,
q1_baseline_next_peak_abs_deg,
target_next_peak_abs_deg, delta_peak_required_deg,
q1_gain_deg_per_mAs, q_model_axis_mA_s, q_req_shadow_mA_s,
q1_shadow_valid, q1_shadow_invalid_reason
```

`zero_cross_time_ms` and `zero_cross_rate_dps` remain the formal Q1 inputs. The interpolation columns are diagnostics only and are recorded in parallel for video comparison.

Convert a log:

```powershell
python tools\convert_rwlog_to_csv.py passive_absolute_roll_run_*.rwlog --out converted_run
```

The converter writes `q1_shadow_events.csv`. Match each event to the next fixed-horizon video peak and first evaluate `A_next_video - q1_baseline_next_peak_abs_deg`. Since this build sends no Q, it must not be used to claim that `baseline + g*Q_req` was realized.

## Build

```powershell
C:\Users\arika\.platformio\penv\Scripts\platformio.exe run
python tools\test_q1_shadow_logic.py
```

This project documents build verification only; it does not instruct or perform a firmware upload.
## Q_IDENT fixed-Q actual-output protocol

`q1_current_hw_fixed_q_ident_v3_qhigh0900_vbat8100_20260902` keeps the isolated actual-output mode, retains Qhigh=`0.900 mA*s`, and extends only the automatic Q_IDENT upper battery guard from `8.020` to `8.100 V`. It is not a Q1, E2, inverse-Q, target controller, calibration, start-kick, rebuild, or continuous-control mode.

- Qhigh=`0.900 mA*s` is selected at the configured minimum battery `6.180 V` and `I0=0 mA`: its 23 ms integer pulse leaves 2 ms below the unchanged 25 ms guard. 300 mA, 5--25 ms, four fixed schedules, ARM, direction and support limits are unchanged.
- Battery eligibility is checked automatically by firmware at `6.180--8.100 V`; no operator voltage confirmation is required. A value outside that inclusive range remains an invalid event with no pulse, replacement, clipping, or carryover.
- Start only with **Start Q_IDENT Run 1**. The selected schedule is Run 1 and cannot be changed while recording.
- The first two accepted, alternating physical-side zero-crosses with `|+gy| >= 30 deg/s` arm the mode; both are logged and cannot output a pulse.
- After arming, output is eligible only at `1.68 <= |+gy| <= 27.80 deg/s` (both endpoints included). Each side follows its fixed schedule independently. `Q=0` is a valid, consumed baseline event with no pulse.
- Every nonzero command uses the existing 300 mA current/pulse-width solver. `q_ident_events` now also preserve event Vbat, I0, continuous required width, selected integer width and the immutable guard, including a pulse-width-guard failure. Battery, roller, solver, state, and ESTOP failures record an invalid event and never substitute, clip, or carry a Q value.
- The command-direction rule remains `-physical_next_peak_side`; Q1 remains motor-off-only and cannot command the Q_IDENT path.
- Run 1 must pass all scheduled Q levels and LED-anchor video synchronization before the same firmware is frozen for Runs 2--4.

The RWLOG remains binary format v43 because the sample layout is unchanged; it contains expanded JSON metadata `q_ident_events`. Use `tools/convert_rwlog_to_csv.py` to create `q_ident_events.csv`.
