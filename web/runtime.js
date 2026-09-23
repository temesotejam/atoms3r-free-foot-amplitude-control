'use strict';
const $ = id => document.getElementById(id);
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
  const busy = commandInFlight || !!latest?.command?.pending;
  const building = latest?.export_phase === 'building';
  $('start').disabled = !fresh || busy || transferRunning || !latest.ready || latest.running || latest.export_phase !== 'empty';
  $('clear').disabled = !fresh || busy || transferRunning || building || latest.running || !['FINISHED', 'ESTOP'].includes(latest.state);
  $('download').disabled = !fresh || busy || transferRunning || !latest.downloadable || latest.running;
  $('cancel').disabled = !transferRunning;
  $('csv').disabled = !completedFoot;
}
const format = (n, digits = 2) => Number.isFinite(n) ? n.toFixed(digits) : '—';
function drawMarkers(f) {
  const ctx = $('markers').getContext('2d'); ctx.clearRect(0, 0, 640, 120);
  for (const [name, x, y, lo, hi] of [['A / 右', f.right_x, 35, 42, 173], ['B / 左', f.left_x, 90, 43.5, 177.5]]) {
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
  const reasons = {low_contrast: '未検出（明暗差不足）', low_weight: '未検出（白領域不足）'};
  for (const [side, label] of [['right', '右'], ['left', '左']]) {
    if (f[side + '_valid']) {
      if (f[side + '_in_range'] === false) issues.push(label + '：校正範囲外');
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
  $('foot-status').textContent = `${f.zero_ready ? 'ゼロ点確定' : 'ゼロ点待ち'} · 記録${f.frames ?? 0}枚 · 画像取得失敗${f.failures ?? 0}回` +
    footIssues(f, stale, terminal);
  if (s.running) $('guide').textContent = '測定中。画面は更新されます。足角度は制御入力に使用しません。';
  else if (s.downloadable) $('guide').textContent = 'ログを保存してください。次の測定には「ログを消去・次の測定へ」を使います。';
  else if (!f.available) $('guide').textContent = 'カメラを初期化できませんでした。診断情報を保存してください。';
  else if (stale || f.frame_valid === false || f.frame_timestamp_valid === false) $('guide').textContent = 'カメラ画像の更新を確認しています。この状態が続く場合は診断JSONを保存してください。';
  else if (!f.zero_ready) $('guide').textContent = `左右のマーカーが見える直立姿勢で静止してください。姿勢誤差 ${format(s.upright.error_deg, 1)}° / 角速度 ${format(s.upright.gyro_dps, 1)}°/s`;
  else if (!f.right_valid || !f.left_valid) $('guide').textContent = '未検出の足があります。マーカーの見え方を確認してください。この姿勢の診断JSONを保存すると原因の確認に使えます。';
  else $('guide').textContent = s.ready ? '直立姿勢を保ち、測定を開始してください。' : 'IMUの初期化・静止確認を待っています。';
  $('diagnostic-view').textContent = JSON.stringify(s, null, 2);
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
$('start').onclick = () => command('/start-energy-control-autonomous');
$('stop').onclick = () => command('/stop');
$('clear').onclick = () => command('/clear');
$('download').onclick = download;
$('cancel').onclick = () => { cancelTransfer = true; };
$('csv').onclick = () => saveBlob(new Blob(['\ufeff', footCsv(completedFoot)], {type: 'text/csv;charset=utf-8'}), completedName.replace(/\.rwlog$/, '_foot.csv'));
$('diagnostics').onclick = () => saveBlob(new Blob([JSON.stringify(latest, null, 2)], {type: 'application/json'}), 'freefoot-diagnostics.json');
poll();
