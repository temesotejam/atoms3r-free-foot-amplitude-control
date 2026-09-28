# 地磁気観測 0.47.27

目的は、通常測定の処理時間・制御・保存の流れを維持しながら、内蔵BMM150が
モータ動作中に利用できるかを実測すること。専用テストも新しい操作も設けない。
地磁気によるMEKF補正・方位フィルタ・操舵はこの版には含めない。

## 取得と処理時間

M5Unified 0.2.18と既存のBMI270 selective readerは、従来からAUX ready時に
0x04から8バイトを取得していた。この同じ転送からRHALLを含む8バイトを保持する。
BMM150側DRDYが立つデータだけ時刻・連番を更新し、通常のジャイロ配送に同梱。
新しい周期タスク、割込み、運転中I2C要求、レジスタ書込み、待ち処理を追加しない。
ODR、制御周期、ログ間引き、2.5 msの厳密な制御完了期限と超過記録を維持する。

高優先度経路は整数判定と固定長コピーだけ。RWLOGサンプルには時刻・連番各4バイトと
AUX 8バイトの計16バイトを末尾追加。従来258バイトの接頭部分は不変、v52は274バイト。
5 MiBの既存ログ領域を維持。コピー量とレコードサイズは増えるため、時間への影響が
ゼロとは主張しない。追加版の実機期限確認は、次の通常測定で行う。

工場トリムはタスク開始前にBMI270 AUX手動モードで読む。AUX設定・アドレス・
電源レジスタを復元して読戻し確認する。トリム取得失敗時は磁気値のみ無効。
復元失敗はセンサ初期化失敗として既存の起動時再試行へ戻す。
トリムはタスク開始後不変。工場補正とnorm計算はHTTP表示または
PCの`tools/convert_rwlog_to_csv.py`で実行。運転中HTTP休止は従来どおり。

## 値の意味

- AUX時刻はホスト読出し時刻であり、BMM150の物理変換時刻ではない。
- dieのraw x/y/zとRHALLを保存し、Boschの工場補正式でµTに変換する。
- AtomS3R-CAMのM5磁気軸変換(-x,+y,-z)とロボット変換(-x,+y,-z)の合成により、
  die xyzとロボット/MEKF body xyzが一致する。ジャイロYの倍率は磁気に適用しない。
- 工場補正は感度・温度補正。組付け後のhard/soft-iron校正は未実施。
- 値が得られても地磁気だけを測っている保証はない。強い一定磁場やモータ電流に
  同期した変動も記録する。磁場の大小で観測データを捨てない。
- UIのfreshは最新AUX時刻から250 ms以内。連番で更新停止と同じ値の継続を区別する。
- overflow、RHALL=0、工場トリム無効時は補正値を無効とし、rawは残す。

## 通常の測定

同じ書込みページから0.47.27を書込み、WebUIを再読み込みする。
従来どおり起動・直立確認・目標角選択・30秒測定・RWLOG保存を行う。
待機画面で3軸と強さを表示する。測定中は表示を休止し、同じログへ記録する。

次のログで最初に見るのは制御完了期限、最大値、配送欠落・バックログ、
IMU取得周期、電流取得周期、カメラ・ログの欠落。磁気の連番・欠測と、
電流・パルス・車輪状態に同期した磁場変動も、その同じログで調べる。

比較用0.47.26 run_1_76049506（目標8°、手で保持）のcontrol_latency.done_ageは
12,142回、2.5 ms超過0回、最大1,364 µs。追加前の1回の結果であり、追加版の証明や
全運転条件の上限ではない。同データのゼロ点差はユーザー申告の手で保持した影響と
して扱い、起動時初期化の不具合や変更理由に用いない。

## 検証と出典

既存制御算術と従来258バイトのログ接頭部分の等価性、selective readerの全8 mask、
転送失敗・ready競合・追加転送なし、AUX復元・工場係数失敗・磁気なし、時刻wrapとstale、
Bosch公式float計算64ベクトル、符号・trim復号・overflow、RWLOG v44〜v52・CRC・
既存転送・オフライン運転・WebUIをホスト上で検証。ESP32ビルドと既存IRAM/link検証はCI。
ホスト時間を実機時間と見なさない。

[BMM150 SensorAPI](https://github.com/boschsensortec/BMM150_SensorAPI/tree/0dce0617873cda1f6d51f6b7b961fdc2641e0c7c)
のfloat補正式に基づく。BSD-3-Clauseライセンスを`THIRD_PARTY_BMM150_LICENSE.txt`に同梱。
[M5Unified 0.2.18](https://github.com/m5stack/M5Unified/tree/0.2.18)のBMI270_Class/IMU_Classで
既存AUX取得とAtomS3R-CAM軸配置を確認。
[BMM150 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmm150-ds001.pdf) /
[BMI270 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf)。
