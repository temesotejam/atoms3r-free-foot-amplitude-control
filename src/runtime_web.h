#pragma once
#include <Arduino.h>
static const char RUNTIME_HTML[] PROGMEM = R"FREEFOOT(<!doctype html><html lang="ja"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AtomS3R Free-foot</title>
<style>
:root{font-family:system-ui,sans-serif;color:#1d293d;background:#eef2f5;font-size:16px}*{box-sizing:border-box}body{max-width:950px;margin:auto;padding:20px}h1{font-size:1.65rem;margin-bottom:4px}h2{font-size:1.08rem}p{line-height:1.6}.muted{color:#546477;font-size:.88rem}.card{background:white;border-radius:14px;padding:20px;margin:16px 0;border:1px solid #d9e1e8}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:14px}.value{font-size:2rem;font-variant-numeric:tabular-nums;margin:4px 0}.label{font-size:.85rem;color:#546477}button{padding:13px 18px;border:0;border-radius:8px;background:#174b8e;color:white;font:inherit;cursor:pointer;margin:4px 4px 4px 0}button:disabled{opacity:.4;cursor:default}#stop{background:#b62032}#clear,#cancel{background:#58677a}code,pre{font-family:ui-monospace,monospace}pre{white-space:pre-wrap;font-size:.78rem;overflow-wrap:anywhere}#connection{font-weight:600}progress{width:100%;height:24px}table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:9px 4px;border-bottom:1px solid #e1e6eb}canvas{width:100%;height:120px;background:#f3f6fa;border-radius:8px}#message{min-height:26px;color:#9c2636}a{color:#174b8e}
</style>
<h1>AtomS3R Free-foot</h1><div class="muted">0.47.18 · 必要Qの直接逆算版 · 足角度はMEKF基準の暫定校正・観測用</div>
<p id="connection">接続を確認中…</p>
<section class="card"><div class="grid"><div><div class="label">状態</div><div class="value" id="state">—</div></div><div><div class="label">残り時間</div><div class="value" id="remaining">—</div></div><div><div class="label">胴体の左右揺動 · MEKF</div><div class="value" id="pitch">—</div></div><div><div class="label">指令 / 実測電流</div><div class="value" style="font-size:1.5rem" id="current">—</div></div></div>
<p id="guide">起動後は静止させてください。LEDが点灯したら直立させ、左右マーカーが見える状態で2秒以上静止します。</p>
<button id="start" disabled>30秒測定を開始</button><button id="stop">停止</button><button id="clear" disabled>ログを消去・次の測定へ</button>
<div id="message" role="status"></div><p class="muted">開始・終了のLED同期はそれぞれ5秒。制御は既存のAutonomous、固定3ms補償、300mA / 最大100msパルスです。</p></section>
<section class="card"><h2>左右足角度 · 胴体に対する相対角</h2><div class="grid"><div><div class="label">右足 · 上段マーカー A</div><div class="value" id="right">—</div></div><div><div class="label">左足 · 下段マーカー B</div><div class="value" id="left">—</div></div><div><div class="label">カメラ実測 / 目標</div><div class="value" style="font-size:1.5rem" id="fps">— / 15 fps</div></div></div>
<canvas id="markers" width="640" height="120" aria-label="マーカー検出位置。上段が右足、下段が左足。"></canvas>
<p class="muted" id="foot-status">ゼロ点は起動ごとに1回だけ確定します。</p><p class="muted">角度の正方向はマーカーが左へ動く方向です。検出失敗・古い画像・設定範囲外を区別して表示し、その状態もログに保存します。候補を区別できないときは角度を無効にします。足を地面に固定して胴体を前後に傾けると、この相対角は変わります。</p><details><summary>検出画像を確認</summary><p class="muted">待機中に更新される1枚の画像です。測定完了後は測定前の画像を保持します。ログ保存後に「次の測定へ」進むと画像更新を再開します。線は検出に使った帯、丸は選択位置、黄色は未確定の位置・別候補です。</p><button id="preview" disabled>検出画像を取得</button><button id="preview-save" disabled>画像付き診断を保存</button><p class="muted" id="preview-status">画像はまだ取得していません。</p><canvas id="preview-image" width="640" height="480" style="height:auto;display:none" aria-label="カメラ画像と同じフレームの検出位置"></canvas></details></section>
<section class="card"><h2>前後傾斜と足角度の比較</h2>
<p id="mekf-axes" class="muted">MEKFの更新を待っています。</p>
<p>両足を地面に固定して、胴体と両足が直立した姿勢を基準にします。基準の後、胴体を前後の片方向へ約5°・10°・15°・20°と傾け、10°・直立へ戻す順に、設定範囲内で記録してください。各姿勢で約3秒間静止します。</p>
<button id="pose-base" disabled>直立基準を取得</button><button id="pose-add" disabled>この姿勢を追加</button><button id="pose-cancel" disabled>姿勢取得を中止</button><button id="pose-save" disabled>比較JSONを保存</button><button id="pose-reset" disabled>保存後に比較をやり直す</button>
<p id="pose-status" role="status">胴体と両足を直立させ、基準を取得してください。</p>
<div style="overflow-x:auto"><table><thead><tr><th>姿勢</th><th>胴体前後変化</th><th>右の差</th><th>左の差</th><th>yaw変化</th><th>確認</th></tr></thead><tbody id="pose-rows"></tbody></table></div>
<p class="muted">「差」は基準からの足相対角の変化＋胴体前後角の変化です。足を固定した平面運動では0°が目安になります。yaw・左右傾斜が大きい場合は参考値とします。各姿勢の取得では角度換算係数を変更しません。現在は固定2姿勢から求めた係数を使用しています。最大12姿勢の画像と診断をまとめて保存します。再読み込みする前に比較JSONを保存してください。</p>
</section>
<section class="card"><h2>測定ログ</h2><p>測定終了後にログを確定します。中断した場合は「取得・再開」で続きから取得できます。画面を再読み込みしても、端末に保存済みの部分を再利用します。</p>
<button id="download" disabled>RWLOGを取得・再開</button><button id="cancel" disabled>取得を一時停止</button><button id="csv" disabled>足角度CSVを保存</button>
<progress id="progress" value="0" max="1"></progress><div id="transfer" role="status">測定待ち</div><p class="muted">RWLOGにIMU・制御イベント・電流・LED同期・足角度をまとめて保存します。USB診断ログは書き込みページから保存できます。</p></section>
<details class="card"><summary>診断情報</summary><p class="muted">MEKFのroll・pitch・yaw、クォータニオン、有効性・更新時刻を保存します。3軸角度は補償・測定ゼロ差し引き前の推定値です。</p><button id="diagnostics">診断JSONを保存</button><pre id="diagnostic-view">—</pre></details>
<script>
'use strict';
// Pure analysis of frozen frame-delivery snapshots. No device writes or fits.
const PoseComparison = (() => {
  const limits = Object.freeze({samples:5, interval_ms:800, min_span_us:2500000,
    max_span_us:12000000, max_age_ms:300, max_sensor_age_us:300000,
    max_gyro_dps:2, max_accel_error_g:0.08, max_tilt_spread_deg:0.8,
    max_yaw_spread_deg:1.5, max_marker_spread_px:3, max_poses:12, max_trace:600});
  const wrap = x => ((x + 180) % 360 + 360) % 360 - 180;
  const mean = a => a.reduce((s,x) => s + x, 0) / a.length;
  const vector = v => Array.isArray(v) && v.length === 3 && v.every(Number.isFinite);
  function orientation(q) {
    if (!q || ![q.w,q.x,q.y,q.z].every(Number.isFinite)) throw Error('姿勢データが不完全です');
    const n = Math.hypot(q.w,q.x,q.y,q.z);
    if (Math.abs(n - 1) > 0.01) throw Error('姿勢データの長さが不正です');
    const [w,x,y,z] = [q.w,q.x,q.y,q.z].map(v => v / n);
    const up = [2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y)];
    const deg = 180 / Math.PI;
    return {roll_deg:Math.atan2(up[1],up[2])*deg,
      pitch_deg:Math.atan2(-up[0],Math.hypot(up[1],up[2]))*deg,
      yaw_deg:Math.atan2(2*(w*z+x*y),1-2*(y*y+z*z))*deg};
  }
  function sameSession(a,b) {
    return a.boot_id === b.boot_id && a.run_id === b.run_id && a.revision === b.revision &&
      a.right_zero_x === b.right_zero_x && a.left_zero_x === b.left_zero_x;
  }
  function validateFrame(m) {
    if (!m || !Number.isInteger(m.boot_id) || !Number.isInteger(m.run_id) ||
        typeof m.revision !== 'string' || m.state_id !== 2)
      throw Error('測定前の待機状態で取得してください');
    if (!m.timestamp_valid || !Number.isFinite(m.age_ms) || m.age_ms < 0 || m.age_ms > limits.max_age_ms ||
        !Number.isSafeInteger(m.frame_us) || !Number.isSafeInteger(m.delivered_us) || m.frame_us <= 0 ||
        m.delivered_us < m.frame_us || m.delivered_us - m.frame_us > limits.max_sensor_age_us ||
        !Number.isInteger(m.sequence) || !m.zero_ready || !m.right_valid || !m.left_valid ||
        !m.right?.valid || !m.left?.valid ||
        ![m.right_deg,m.left_deg,m.right_zero_x,m.left_zero_x,m.right.x,m.left.x].every(Number.isFinite))
      throw Error('左右マーカーと新しい画像を確認してください');
    const k = m.mekf, input = k?.inputs;
    if (!k?.valid || !k.fresh || k.frame !== 'mekf' || k.euler_order !== 'ZYX' ||
        !Number.isFinite(k.age_us) || k.age_us < 0 || k.age_us > limits.max_sensor_age_us ||
        !input?.valid || input.frame !== 'mekf' || input.axis_order !== 'xyz' ||
        !Number.isFinite(input.accel_age_us) || input.accel_age_us < 0 || input.accel_age_us > limits.max_sensor_age_us ||
        !vector(input.accel_g) || !vector(input.gyro_dps) || !vector(input.gyro_bias_dps))
      throw Error('新しいIMU診断が必要です。ファームウェアと接続を確認してください');
    orientation(k.quaternion);
    const rate = Math.hypot(...input.gyro_dps.map((v,i) => v - input.gyro_bias_dps[i]));
    if (rate > limits.max_gyro_dps || Math.abs(Math.hypot(...input.accel_g) - 1) > limits.max_accel_error_g)
      throw Error('動きを検出しました。姿勢を保って再取得してください');
  }
  function summarize(frames, baseline = false) {
    if (frames.length !== limits.samples) throw Error('静止姿勢の試料が不足しています');
    frames.forEach(validateFrame);
    const first = frames[0], last = frames[frames.length - 1];
    if (!frames.every(m => sameSession(m,first))) throw Error('再起動・測定・ゼロ点の変更を検出しました。基準を取り直してください');
    for (let i=1;i<frames.length;i++) {
      if (frames[i].sequence === frames[i-1].sequence || frames[i].frame_us <= frames[i-1].frame_us ||
          frames[i].delivered_us <= frames[i-1].delivered_us)
        throw Error('画像が更新されていません');
    }
    const span = last.delivered_us - first.delivered_us;
    if (span < limits.min_span_us || span > limits.max_span_us) throw Error('取得間隔が不適切です。再取得してください');
    const angles = frames.map(m => orientation(m.mekf.quaternion));
    const summary = {boot_id:first.boot_id, run_id:first.run_id, revision:first.revision,
      right_zero_x:first.right_zero_x, left_zero_x:first.left_zero_x,
      first_frame_us:first.frame_us, last_frame_us:last.frame_us, span_us:span,
      right_in_range:frames.every(m => m.right_in_range === true),
      left_in_range:frames.every(m => m.left_in_range === true), spread:{}};
    for (const [key,values,maxSpread] of [
      ...['roll_deg','pitch_deg','yaw_deg'].map(key => [key,
        angles.map(a => angles[0][key] + wrap(a[key]-angles[0][key])),
        key === 'yaw_deg' ? limits.max_yaw_spread_deg : limits.max_tilt_spread_deg]),
      ['right_x',frames.map(m => m.right.x),limits.max_marker_spread_px],
      ['left_x',frames.map(m => m.left.x),limits.max_marker_spread_px]]) {
      summary[key] = mean(values); summary.spread[key] = Math.max(...values) - Math.min(...values);
      // Heading may drift even when the camera, feet and gravity tilt are still.
      // Preserve that evidence; comparison flags it instead of blocking capture.
      if (key !== 'yaw_deg' && summary.spread[key] > maxSpread) throw Error('姿勢または足の位置が動いています。静止させて再取得してください');
    }
    for (const side of ['right','left']) summary[side+'_deg'] = mean(frames.map(m => m[side+'_deg']));
    const accelRoll = frames.map(m => Math.atan2(m.mekf.inputs.accel_g[1],m.mekf.inputs.accel_g[2])*180/Math.PI);
    summary.accel_roll_deg = mean(accelRoll.map(v => accelRoll[0] + wrap(v-accelRoll[0])));
    if (baseline && (Math.abs(summary.roll_deg)>6 || Math.abs(summary.pitch_deg)>6 ||
        Math.abs(summary.right_deg)>3 || Math.abs(summary.left_deg)>3))
      throw Error('基準は胴体と両足を直立させて取得してください');
    return summary;
  }
  function compare(base,pose) {
    if (!sameSession(base,pose) || pose.first_frame_us <= base.last_frame_us)
      throw Error('基準との取得条件が変わりました。記録を保存し、比較をやり直してください');
    const delta = {roll_deg:wrap(pose.roll_deg-base.roll_deg), pitch_deg:wrap(pose.pitch_deg-base.pitch_deg),
      yaw_deg:wrap(pose.yaw_deg-base.yaw_deg), accel_roll_deg:wrap(pose.accel_roll_deg-base.accel_roll_deg),
      right_deg:pose.right_deg-base.right_deg, left_deg:pose.left_deg-base.left_deg};
    const flags = [];
    if (Math.abs(delta.yaw_deg)>3) flags.push('yaw_changed');
    if (base.spread.yaw_deg>limits.max_yaw_spread_deg || pose.spread.yaw_deg>limits.max_yaw_spread_deg) flags.push('yaw_unstable');
    if (Math.abs(delta.pitch_deg)>2) flags.push('sideways_changed');
    if (!base.right_in_range || !pose.right_in_range || !base.left_in_range || !pose.left_in_range) flags.push('outside_range');
    if (Math.abs(delta.roll_deg)>30) flags.push('large_tilt');
    return {delta, right_residual_deg:delta.right_deg+delta.roll_deg,
      left_residual_deg:delta.left_deg+delta.roll_deg, planar_check:flags.length===0, flags};
  }
  return {limits, wrap, orientation, sameSession, validateFrame, summarize, compare};
})();
if (typeof module !== 'undefined') module.exports = PoseComparison;

'use strict';
const $ = id => document.getElementById(id);
let previewRunning = false, previewEvidence = null;
let poseRunning = false, poseCancelled = false, poseSession = null, poseSaved = false;
let latest = null, lastSeen = 0, refreshInFlight = false, commandInFlight = false;
let transferRunning = false, cancelTransfer = false, completedFile = null, completedName = '', completedFoot = null;
const nap = ms => new Promise(resolve => setTimeout(resolve, ms));
function crc32(data, crc = 0) {
  crc = ~crc;
  for (const b of data) {
    crc ^= b;
    for (let bit = 0; bit < 8; ++bit) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (~crc) >>> 0;
}
async function request(path, {method = 'GET', kind = 'json', timeout = 3000} = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeout);
  try {
    const response = await fetch(path, {method, signal: controller.signal, cache: 'no-store'});
    if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
    // Abort remains armed through body consumption, including response.json().
    if (kind === 'bytes') return new Uint8Array(await response.arrayBuffer());
    if (kind === 'text') return await response.text();
    return await response.json();
  } finally { clearTimeout(timer); }
}
function controls() {
  const fresh = latest && Date.now() - lastSeen < 3500;
  const busy = commandInFlight || previewRunning || poseRunning || !!latest?.command?.pending;
  const building = latest?.export_phase === 'building';
  $('start').disabled = !fresh || busy || transferRunning || !latest.ready || latest.running || latest.export_phase !== 'empty';
  $('clear').disabled = !fresh || busy || transferRunning || building || latest.running || !['FINISHED', 'ESTOP'].includes(latest.state);
  $('download').disabled = !fresh || busy || transferRunning || !latest.downloadable || latest.running;
  $('cancel').disabled = !transferRunning;
  $('csv').disabled = !completedFoot;
  $('preview').disabled = !fresh || busy || transferRunning || building || latest.running || !latest.foot?.preview_available;
  $('preview-save').disabled = !previewEvidence || previewRunning;
  const poseReady = fresh && latest.controller_fresh && !busy && !transferRunning && !building && !latest.running &&
    latest.state === 'READY_TO_MEASURE' && latest.foot?.zero_ready && latest.foot?.preview_available && latest.mekf?.inputs?.valid;
  $('pose-base').disabled = !poseReady || !!poseSession;
  $('pose-add').disabled = !poseReady || !poseSession?.baseline || poseSession.poses.length >= PoseComparison.limits.max_poses;
  $('pose-cancel').disabled = !poseRunning;
  $('pose-save').disabled = !poseSession?.baseline || poseRunning;
  $('pose-reset').disabled = !poseSession || poseRunning || (!!poseSession.baseline && !poseSaved);
}
const format = (n, digits = 2) => Number.isFinite(n) ? n.toFixed(digits) : '—';
function drawMarkers(f) {
  const ctx = $('markers').getContext('2d'); ctx.clearRect(0, 0, 640, 120);
  for (const [name, x, y, range] of [['A / 右', f.right_x, 35, f.range?.right_support_x ?? [42, 173]],
      ['B / 左', f.left_x, 90, f.range?.left_support_x ?? [43.5, 177.5]]]) {
    const [lo, hi] = range;
    ctx.fillStyle = '#dce8e5'; ctx.fillRect(lo * 2, y - 13, (hi - lo) * 2, 26);
    ctx.strokeStyle = '#aab8c6'; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(640, y); ctx.stroke();
    ctx.fillStyle = '#304962'; ctx.font = '12px system-ui'; ctx.fillText(name, 5, y - 16);
    if (Number.isFinite(x)) { ctx.fillStyle = '#145fad'; ctx.beginPath(); ctx.arc(x * 2, y, 7, 0, 2 * Math.PI); ctx.fill(); }
  }
}
function footIssues(f, stale, terminal) {
  const issues = [];
  if (terminal) issues.push('最終フレームを表示');
  else if (stale) issues.push('画像更新なし');
  if (f.frame_valid === false) issues.push('画像取得失敗');
  else if (f.frame_timestamp_valid === false) issues.push('画像時刻が無効');
  const reasons = {low_contrast: '未検出（明暗差不足）', low_weight: '未検出（白領域不足）',
    bad_shape: '白領域の幅が不適合', ambiguous: '候補を区別できません',
    track_jump: '検出位置が急変', reacquiring: '再捕捉中'};
  for (const [side, label] of [['right', '右'], ['left', '左']]) {
    if (f[side + '_valid']) {
      if (f[side + '_in_range'] === false) issues.push(label + '：設定範囲外');
    } else {
      const reason = f[side + '_reason'];
      if (reasons[reason]) issues.push(label + '：' + reasons[reason]);
      else if (reason === 'no_frame' && f.frame_valid !== false) issues.push(label + '：画像なし');
      else if (!reason) issues.push(label + '：未検出または未校正'); // Older status data.
    }
  }
  if (f.overflow) issues.push('記録容量超過');
  return issues.length ? ' · ' + issues.join(' · ') : '';
}
function render(s) {
  $('connection').textContent = s.controller_fresh ? '接続中' : '接続中 · 制御状態の更新が停止';
  $('state').textContent = s.state; $('remaining').textContent = format(s.remaining_ms / 1000, 1) + ' s';
  $('pitch').textContent = format(s.pitch_deg) + '°';
  $('current').textContent = `${s.motor_mA} / ${s.actual_mA} mA`;
  const f = s.foot, stale = !Number.isFinite(f.age_ms) || f.age_ms < 0 || f.age_ms > 500;
  const terminal = ['FINISHED', 'ESTOP'].includes(s.state);
  $('right').textContent = f.right_valid && (!stale || terminal) ? format(f.right_deg) + '°' : '—';
  $('left').textContent = f.left_valid && (!stale || terminal) ? format(f.left_deg) + '°' : '—';
  $('fps').textContent = terminal ? '停止中' : `${format(stale ? 0 : f.fps, 1)} / 15 fps`;
  $('foot-status').textContent = `${f.zero_ready ? 'ゼロ点確定' : 'ゼロ点待ち（' + (f.zero_samples ?? 0) + '枚）'} · 記録${f.frames ?? 0}枚 · 画像取得失敗${f.failures ?? 0}回` +
    footIssues(f, stale, terminal);
  if (s.running) $('guide').textContent = '測定中。画面は更新されます。足角度は制御入力に使用しません。';
  else if (s.downloadable) $('guide').textContent = 'ログを保存してください。次の測定には「ログを消去・次の測定へ」を使います。';
  else if (!f.available) $('guide').textContent = 'カメラを初期化できませんでした。診断情報を保存してください。';
  else if (stale || f.frame_valid === false || f.frame_timestamp_valid === false) $('guide').textContent = 'カメラ画像の更新を確認しています。この状態が続く場合は診断JSONを保存してください。';
  else if (!f.zero_ready && f.zero_reason === 'position_mismatch') $('guide').textContent = 'ゼロ点候補が基準位置から大きく外れています。両足を直立させ、「検出画像を確認」で白四角を選んでいるか確認してください。';
  else if (!f.zero_ready && f.zero_reason === 'marker_moving') $('guide').textContent = '足の位置が動いているためゼロ点を取り直しています。胴体と両足を静止させてください。';
  else if (!f.zero_ready && f.zero_reason === 'marker_invalid') $('guide').textContent = '左右の白四角を確認しています。「検出画像を確認」で選択位置を確認できます。';
  else if (!f.zero_ready) $('guide').textContent = `胴体と両足を直立させ、白四角が見える状態で2秒以上静止してください。姿勢誤差 ${format(s.upright.error_deg, 1)}° / 角速度 ${format(s.upright.gyro_dps, 1)}°/s`;
  else if (!f.right_valid || !f.left_valid) $('guide').textContent = '未検出の足があります。マーカーの見え方を確認してください。この姿勢の診断JSONを保存すると原因の確認に使えます。';
  else $('guide').textContent = s.ready ? '直立姿勢を保ち、測定を開始してください。' : 'IMUの初期化・静止確認を待っています。';
  $('diagnostic-view').textContent = JSON.stringify(s, null, 2);
  $('mekf-axes').textContent = s.mekf?.valid && s.mekf.fresh
    ? `前後 roll ${format(s.mekf.roll_deg)}° · 左右 pitch ${format(s.mekf.pitch_deg)}° · yaw ${format(s.mekf.yaw_deg)}°`
    : 'MEKFの更新を待っています。';
  drawMarkers(f); controls();
}
async function refresh() {
  if (refreshInFlight) return;
  refreshInFlight = true;
  let received = false;
  try {
    const s = await request('/status.json', {timeout: 2500}); received = true;
    adoptStatus(s); render(latest);
  } catch (error) {
    lastSeen = 0;
    $('connection').textContent = received || error instanceof SyntaxError
      ? '状態データ・画面更新のエラー（自動再試行）'
      : '装置から応答がありません（自動再試行）';
    controls();
  } finally { refreshInFlight = false; }
}
function adoptStatus(s) {
  if (!s || typeof s.state !== 'string' || typeof s.export_phase !== 'string' ||
      !s.foot || !s.upright || !s.command ||
      typeof s.command.pending !== 'boolean' ||
      !Number.isInteger(s.command.completed) || !Number.isInteger(s.command.submitted) ||
      ['running', 'ready', 'downloadable', 'controller_fresh'].some(key => typeof s[key] !== 'boolean'))
    throw new Error('装置の状態データが不完全です');
  latest = s; lastSeen = Date.now();
  if (poseSession && !poseSaved) {
    poseSession.trace.push(JSON.parse(JSON.stringify({client_time_ms:Date.now(), boot_id:s.boot_id,
      run_id:s.run_id, revision:s.revision, state:s.state, mekf:s.mekf, controller_fresh:s.controller_fresh,
      foot:{sequence:s.foot.sequence, age_ms:s.foot.age_ms, right_deg:s.foot.right_deg, left_deg:s.foot.left_deg,
        right_x:s.foot.right_x, left_x:s.foot.left_x, right_valid:s.foot.right_valid, left_valid:s.foot.left_valid}})));
    if (poseSession.trace.length > PoseComparison.limits.max_trace) { poseSession.trace.shift(); ++poseSession.trace_dropped; }
  }
}
async function poll() {
  try { await refresh(); }
  catch (error) { $('connection').textContent = '画面更新のエラー（自動再試行）'; }
  finally { setTimeout(poll, 800); }
}
async function command(path) {
  commandInFlight = true; controls(); $('message').textContent = '要求を送信中…';
  const before = latest?.command?.submitted ?? 0;
  try {
    await request(path, {method: 'POST', kind: 'text'});
    for (let i = 0; i < 12; ++i) {
      const s = await request('/status.json'); adoptStatus(s); render(s);
      if (!s.command.pending && (s.command.completed > before || path === '/stop')) {
        $('message').textContent = s.command.result || '要求を処理しました'; return;
      }
      await nap(150);
    }
    $('message').textContent = '処理結果を確認中です。状態表示を確認してください。';
  } catch (error) {
    $('message').textContent = `通信結果が未確認です。状態を確認してください: ${error.message}`;
  } finally { commandInFlight = false; controls(); }
}
// A cache is optional for one-session downloads. IndexedDB additionally allows
// reload/reconnect resume; every cached chunk is revalidated before reuse.
let cachePromise;
const memoryCache = new Map();
function openCache() {
  if (!cachePromise) cachePromise = new Promise(resolve => {
    try {
      const r = indexedDB.open('freefoot-rwlog-v2', 1);
      r.onupgradeneeded = () => r.result.createObjectStore('chunks');
      r.onsuccess = () => resolve(r.result); r.onerror = () => resolve(null); r.onblocked = () => resolve(null);
    } catch { resolve(null); }
  });
  return cachePromise;
}
async function cacheGet(key) {
  const db = await openCache();
  if (!db) return memoryCache.get(key);
  return new Promise(resolve => {
    const tx = db.transaction('chunks', 'readonly'), r = tx.objectStore('chunks').get(key);
    r.onsuccess = () => resolve(r.result); r.onerror = () => resolve(undefined);
  });
}
async function cachePut(key, value) {
  memoryCache.set(key, value);
  const db = await openCache(); if (!db) return;
  await new Promise(resolve => {
    const tx = db.transaction('chunks', 'readwrite');
    tx.objectStore('chunks').put(value, key);
    tx.oncomplete = resolve; tx.onabort = resolve; tx.onerror = resolve;
  });
}
async function pruneCache(token) {
  const prefix = token + ':';
  for (const key of memoryCache.keys()) if (!key.startsWith(prefix)) memoryCache.delete(key);
  const db = await openCache(); if (!db) return;
  await new Promise(resolve => {
    try {
      const tx = db.transaction('chunks', 'readwrite'), r = tx.objectStore('chunks').openCursor();
      r.onsuccess = () => {
        const cursor = r.result;
        if (cursor) { if (!String(cursor.key).startsWith(prefix)) cursor.delete(); cursor.continue(); }
      };
      tx.oncomplete = resolve; tx.onabort = resolve; tx.onerror = resolve;
    } catch { resolve(); }
  });
}
function validateChunk(bytes, offset, length) {
  if (!(bytes instanceof Uint8Array) || bytes.byteLength !== length + 16) throw new Error('チャンク長の不一致');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint32(0, true) !== 0x31484346 || view.getUint32(4, true) !== offset || view.getUint32(8, true) !== length)
    throw new Error('チャンク識別情報の不一致');
  if (crc32(bytes.subarray(16)) !== view.getUint32(12, true)) throw new Error('チャンクCRC不一致');
  return bytes.subarray(16);
}
function saveBlob(blob, name) {
  const url = URL.createObjectURL(blob), a = document.createElement('a');
  a.href = url; a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(url), 60000);
}
async function download() {
  transferRunning = true; cancelTransfer = false; controls();
  try {
    $('transfer').textContent = 'ログを確定中…';
    await request('/export/prepare', {method: 'POST', kind: 'text'});
    let m;
    do {
      if (cancelTransfer) throw new Error('一時停止しました。取得・再開で続けられます。');
      m = await request('/export/manifest');
      if (m.phase === 'error') throw new Error(m.error);
      if (m.phase !== 'ready') { $('transfer').textContent = `ログ確定中 ${m.hashed_bytes} / ${m.bytes || '?'} bytes`; await nap(400); }
    } while (m.phase !== 'ready');
    if (!/^[a-f0-9]{16}$/.test(m.token) || !Number.isInteger(m.bytes) || m.bytes < 114 || m.bytes > 9 * 1024 * 1024 || m.chunk_bytes !== 4096)
      throw new Error('ログ仕様が一致しません。ページを更新してください。');
    await pruneCache(m.token);
    const file = new Uint8Array(m.bytes); $('progress').max = m.bytes; $('progress').value = 0;
    for (let offset = 0; offset < m.bytes;) {
      if (cancelTransfer) throw new Error('一時停止しました。取得・再開で続けられます。');
      const length = Math.min(m.chunk_bytes, m.bytes - offset), key = `${m.token}:${offset}`;
      let payload, packet = await cacheGet(key);
      if (packet) { try { payload = validateChunk(packet, offset, length); } catch { packet = null; } }
      for (let retry = 0; !payload && retry < 5; ++retry) {
        if (cancelTransfer) throw new Error('一時停止しました。');
        try {
          packet = await request(`/export/chunk?token=${m.token}&offset=${offset}&length=${length}`, {kind: 'bytes'});
          payload = validateChunk(packet, offset, length); await cachePut(key, packet);
        } catch (error) {
          if (retry === 4 || error.message.startsWith('409:')) throw error;
          $('transfer').textContent = `${offset} bytesまで取得済み · 再試行 ${retry + 1}/5`;
          await nap(250 * (retry + 1));
        }
      }
      file.set(payload, offset); offset += length; $('progress').value = offset;
      $('transfer').textContent = `${(100 * offset / m.bytes).toFixed(1)}% · ${offset} / ${m.bytes} bytes`;
    }
    const v = new DataView(file.buffer), crc = crc32(file.subarray(0, file.length - 4));
    if (crc !== m.crc32 || crc !== v.getUint32(file.length - 4, true)) throw new Error('ファイル全体のCRCが一致しません');
    const headerSize = v.getUint16(10, true), metadataLength = v.getUint32(24, true);
    if (headerSize !== 110 || metadataLength > file.length - headerSize - 4) throw new Error('RWLOGヘッダー不一致');
    const metadata = JSON.parse(new TextDecoder().decode(file.subarray(headerSize, headerSize + metadataLength)));
    completedFoot = metadata.foot_frames ?? null; completedFile = file; completedName = m.filename;
    saveBlob(new Blob([file], {type: 'application/octet-stream'}), m.filename);
    $('transfer').textContent = `CRC検証完了 · ${m.bytes} bytes · 足角度 ${completedFoot?.length ?? 0}行`;
  } catch (error) {
    $('transfer').textContent = `${error.message}（保存済みの部分は保持しています）`;
  } finally { transferRunning = false; controls(); }
}
function footCsv(rows) {
  if (!rows.length) return '';
  const keys = Object.keys(rows[0]);
  const esc = v => v == null ? '' : '"' + String(v).replaceAll('"', '""') + '"';
  return keys.map(esc).join(',') + '\r\n' + rows.map(row => keys.map(k => esc(row[k])).join(',')).join('\r\n') + '\r\n';
}
function validatePreviewManifest(m) {
  if (!m || m.format !== 'gray8' || m.width !== 160 || m.height !== 120 ||
      m.source_width !== 320 || m.source_height !== 240 || m.bytes !== 19200 ||
      !Number.isInteger(m.token) || m.token <= 0 || m.token > 0xffffffff ||
      !Number.isInteger(m.crc32) || m.crc32 < 0 || m.crc32 > 0xffffffff ||
      !Number.isInteger(m.sequence) || !m.right || !m.left)
    throw new Error('画像診断の形式が不正です');
}
function previewReason(reason) {
  return ({detected:'検出', no_frame:'画像なし', low_contrast:'明暗差不足', low_weight:'白領域不足',
    bad_shape:'幅が不適合', ambiguous:'候補が競合', track_jump:'位置が急変', reacquiring:'再捕捉中',
    waiting:'待機中', body_moving:'胴体の静止待ち', marker_invalid:'マーカー確認待ち',
    position_mismatch:'基準位置から外れています', marker_moving:'足の静止待ち',
    collecting:'取得中', ready:'確定'})[reason] ?? '未確認';
}
function drawPreview(m, pixels) {
  const small = document.createElement('canvas'); small.width = m.width; small.height = m.height;
  const smallCtx = small.getContext('2d'), bitmap = smallCtx.createImageData(m.width, m.height);
  for (let i = 0; i < pixels.length; ++i) {
    bitmap.data[i * 4] = bitmap.data[i * 4 + 1] = bitmap.data[i * 4 + 2] = pixels[i];
    bitmap.data[i * 4 + 3] = 255;
  }
  smallCtx.putImageData(bitmap, 0, 0);
  const canvas = $('preview-image'), ctx = canvas.getContext('2d');
  canvas.style.display = 'block'; ctx.imageSmoothingEnabled = false;
  ctx.drawImage(small, 0, 0, canvas.width, canvas.height);
  for (const [side, label] of [['right', 'A / 右足'], ['left', 'B / 左足']]) {
    const c = m[side];
    if (!c.candidates) continue;
    const x = c.x * 2, y = c.scan_y * 2;
    ctx.strokeStyle = ctx.fillStyle = c.valid ? '#43ff9c' : '#ffdc55'; ctx.lineWidth = 2;
    ctx.strokeRect(x - c.width, y - 16, c.width * 2, 32);
    ctx.beginPath(); ctx.arc(x, y, 6, 0, Math.PI * 2); ctx.stroke();
    ctx.font = 'bold 16px system-ui'; ctx.fillText(label, Math.max(4, Math.min(x + 10, 540)), Math.max(20, y - 22));
    if (c.alternate_x >= 0 && c.alternate_y >= 0) {
      ctx.strokeStyle = '#ffdc55'; ctx.beginPath(); ctx.arc(c.alternate_x * 2, c.alternate_y * 2, 9, 0, Math.PI * 2); ctx.stroke();
    }
  }
}
async function readPreviewPixels(m, cancelled = () => false) {
    const pixels = new Uint8Array(m.bytes);
    for (let offset = 0; offset < m.bytes; offset += 4096) {
      if (cancelled()) throw Error('取得を中止しました');
      const length = Math.min(4096, m.bytes - offset);
      const packet = await request(`/vision/chunk?token=${m.token}&offset=${offset}&length=${length}`, {kind:'bytes'});
      pixels.set(validateChunk(packet, offset, length), offset);
      await nap(20);
    }
    if (crc32(pixels) !== m.crc32) throw new Error('画像全体のCRCが不一致です');
    // Store exact gray pixels plus matching metadata in one downloadable JSON.
    // Never substitute the separately polled live status for this frame.
    let binary = ''; for (const value of pixels) binary += String.fromCharCode(value);
    return {...m, pixels_gray8_base64: btoa(binary)};
}
function showEvidence(evidence) {
    const {pixels_gray8_base64:binary,...m} = evidence;
    const pixels = Uint8Array.from(atob(binary), c => c.charCodeAt(0));
    drawPreview(m, pixels); previewEvidence = evidence;
    $('preview-status').textContent = `画像 #${m.sequence} · 取得要求の${format(m.age_ms, 0)} ms前のフレーム · 右 ${previewReason(m.right.reason)} / 左 ${previewReason(m.left.reason)} · ゼロ点 ${previewReason(m.zero_reason)}。姿勢を変えたら再取得してください。`;
}
async function capturePreview() {
  if (previewRunning || poseRunning || transferRunning || latest?.running) return;
  previewRunning = true; controls(); $('preview-status').textContent = '画像を取得中…';
  try {
    const m = await request('/vision/capture', {method:'POST'});
    validatePreviewManifest(m);
    showEvidence(await readPreviewPixels(m));
  } catch (error) {
    $('preview-status').textContent = `画像取得に失敗しました。${previewEvidence ? '表示と保存の対象は前回の画像です。' : ''}再取得してください: ${error.message}`;
  } finally { previewRunning = false; controls(); }
}
function renderPoses() {
  const tbody = $('pose-rows'); tbody.replaceChildren();
  if (!poseSession?.baseline) return;
  const rows = [{label:'直立基準',comparison:null},...poseSession.poses];
  for (const [i,row] of rows.entries()) {
    const c = row.comparison, tr = document.createElement('tr');
    const notes = {yaw_changed:'yaw変化大',yaw_unstable:'静止中のyaw変化',sideways_changed:'左右傾斜あり',outside_range:'設定範囲外',large_tilt:'傾斜大'};
    for (const value of [row.label || `姿勢 ${i}`,
      ...[c?.delta.roll_deg,c?.right_residual_deg,c?.left_residual_deg,c?.delta.yaw_deg].map(v => c ? format(v) + '°' : '0.00°'),
      c ? (c.planar_check ? '比較用' : '参考：' + c.flags.map(f => notes[f]).join('・'))
        : (poseSession.baseline.summary.spread.yaw_deg>PoseComparison.limits.max_yaw_spread_deg ? '基準：静止中のyaw変化' : '基準')]) {
      const td = document.createElement('td'); td.textContent = value; tr.appendChild(td);
    }
    tbody.appendChild(tr);
  }
}
async function capturePose(baseline) {
  controls();
  if ($(baseline ? 'pose-base' : 'pose-add').disabled) return;
  if (baseline) poseSession = {schema:'freefoot-static-poses-v1', created_at:new Date().toISOString(),
    assumptions:['feet_fixed_to_ground','fore_aft_rotation_about_mekf_x'],
    residual_semantics:'delta_foot_body_relative_plus_delta_body_roll; planar approximation, not absolute foot attitude',
    timing_semantics:'camera exposure and delivery-time control snapshot differ; hold each pose stationary',
    trace_semantics:'existing status polls; not a high-rate IMU integration log',
    limits:PoseComparison.limits, baseline:null, poses:[], trace:[], trace_dropped:0};
  poseRunning = true; poseCancelled = false; poseSaved = false; controls();
  const frames = [];
  try {
    for (let i=0;i<PoseComparison.limits.samples;i++) {
      if (i) await nap(PoseComparison.limits.interval_ms);
      if (poseCancelled) throw Error('取得を中止しました');
      if (Date.now()-lastSeen >= 3500 || latest?.running || latest?.state !== 'READY_TO_MEASURE' ||
          !latest.controller_fresh || latest.command.pending) throw Error('接続と待機状態を確認してください');
      $('pose-status').textContent = `その姿勢を保ってください… ${i+1}/${PoseComparison.limits.samples}`;
      const m = await request('/vision/capture', {method:'POST'});
      validatePreviewManifest(m); PoseComparison.validateFrame(m); frames.push(m);
    }
    const summary = PoseComparison.summarize(frames, baseline);
    const comparison = baseline ? null : PoseComparison.compare(poseSession.baseline.summary, summary);
    const last = frames[frames.length-1];
    $('pose-status').textContent = '静止を確認しました。最後の画像を保存しています…';
    const image = await readPreviewPixels(last, () => poseCancelled);
    if (poseCancelled) throw Error('取得を中止しました');
    const record = {label:baseline ? '直立基準' : `姿勢 ${poseSession.poses.length+1}`, summary, comparison, frames, image};
    if (baseline) poseSession.baseline = record; else poseSession.poses.push(record);
    showEvidence(image); renderPoses();
    $('pose-status').textContent = baseline ? '基準を記録しました。足を固定したまま本体を傾け、「この姿勢を追加」を押してください。'
      : `姿勢 ${poseSession.poses.length} を記録しました。次の姿勢を追加するか、比較JSONを保存してください。`;
  } catch (error) {
    $('pose-status').textContent = error.message + ' 取得済みの姿勢は保持しています。';
    if (baseline && !poseSession.baseline) poseSession = null;
  } finally { poseRunning = false; controls(); }
}
$('start').onclick = () => command('/start-energy-control-autonomous');
$('stop').onclick = () => { poseCancelled = true; return command('/stop'); };
$('clear').onclick = () => command('/clear');
$('download').onclick = download;
$('cancel').onclick = () => { cancelTransfer = true; };
$('csv').onclick = () => saveBlob(new Blob(['\ufeff', footCsv(completedFoot)], {type: 'text/csv;charset=utf-8'}), completedName.replace(/\.rwlog$/, '_foot.csv'));
$('preview').onclick = capturePreview;
$('preview-save').onclick = () => saveBlob(new Blob([JSON.stringify(previewEvidence, null, 2)], {type:'application/json'}), `freefoot-vision-diagnostics-${previewEvidence.sequence}.json`);
$('diagnostics').onclick = () => saveBlob(new Blob([JSON.stringify(latest, null, 2)], {type: 'application/json'}), 'freefoot-diagnostics.json');
$('pose-base').onclick = () => capturePose(true);
$('pose-add').onclick = () => capturePose(false);
$('pose-cancel').onclick = () => { poseCancelled = true; };
$('pose-save').onclick = () => {
  saveBlob(new Blob([JSON.stringify(poseSession, null, 2)], {type:'application/json'}), 'freefoot-pose-comparison.json');
  poseSaved = true; controls();
};
$('pose-reset').onclick = () => {
  if ($('pose-reset').disabled) return;
  poseSession = null; poseSaved = false; renderPoses(); controls();
  $('pose-status').textContent = '胴体と両足を直立させ、基準を取得してください。';
};
poll();

</script></html>
)FREEFOOT";
