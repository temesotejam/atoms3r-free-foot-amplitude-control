#include "web_ui.h"

#include <WiFi.h>

#include "config.h"
#include "upright_pose_guide.h"
#include "run_control_worker.h"
extern RunControlWorker run_control;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AtomS3R Amplitude Control</title>
<style>
:root{font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:#17202a;background:#f4f6f8}
*{box-sizing:border-box}
body{margin:0;background:#f4f6f8}
header{background:#202a35;color:#fff;padding:18px 18px 16px}
header h1{font-size:1.25rem;margin:0 0 4px}
header p{margin:0;color:#c9d1d9;font-size:.86rem}
main{max-width:680px;margin:0 auto;padding:14px}
.card{background:#fff;border:1px solid #d9dee5;border-radius:10px;padding:15px;margin-bottom:12px}
.card h2{font-size:1rem;margin:0 0 12px}
.state{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px}
.state strong{font-size:1.08rem}
.pill{display:inline-block;padding:5px 9px;border-radius:999px;background:#eef2f6;font-size:.78rem;font-weight:700}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.metric{background:#f6f8fa;border-radius:8px;padding:10px}
.metric span{display:block;color:#66717d;font-size:.76rem;margin-bottom:3px}
.metric b{font-size:1.03rem}
.note{color:#66717d;font-size:.86rem;line-height:1.55;margin:8px 0 0}
.error{color:#b42318;font-size:.86rem;min-height:1.2em;margin:8px 0 0}
button,a.action{display:block;width:100%;border:0;border-radius:8px;padding:12px 14px;margin-top:10px;font-size:1rem;font-weight:700;text-align:center;text-decoration:none;cursor:pointer}
.primary{background:#1769e0;color:#fff}
.secondary,a.action{background:#eef2f6;color:#17202a}
.danger{background:#c4262e;color:#fff}
button:disabled,a.disabled{opacity:.4;pointer-events:none;cursor:default}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.fixed{padding:10px 12px;border-radius:8px;background:#f6f8fa;font-size:.9rem;line-height:1.55}
footer{text-align:center;color:#87919c;font-size:.76rem;padding:4px 0 14px}
@media(max-width:480px){.row{grid-template-columns:1fr}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style>
</head>
<body>
<header>
  <h1>AtomS3R Amplitude Control</h1>
  <p>V46al-R2 / 0.46.42 — A_prev active control</p>
</header>
<main>
  <div class="card">
    <div class="state">
      <div>
        <h2 style="margin-bottom:4px">現在の状態</h2>
        <strong id="stateText">接続中...</strong>
      </div>
      <span id="readyBadge" class="pill">--</span>
    </div>
    <div class="grid">
      <div class="metric"><span>MEKF角度</span><b id="angle">--</b></div>
      <div class="metric"><span>角速度</span><b id="rate">--</b></div>
      <div class="metric"><span>Motor command</span><b id="motor">--</b></div>
      <div class="metric"><span>実電流</span><b id="actual">--</b></div>
      <div class="metric"><span>バッテリー</span><b id="battery">--</b></div>
      <div class="metric"><span>残り時間</span><b id="remaining">--</b></div>
      <div class="metric"><span>右足角度（上）</span><b id="footRight">--</b></div>
      <div class="metric"><span>左足角度（下）</span><b id="footLeft">--</b></div>
    </div>
    <p id="systemInfo" class="note">状態を取得しています。</p>
    <p id="startupInfo" class="note">初期姿勢と足マーカーの状態を取得しています。</p>
    <p id="errorInfo" class="error"></p>
  </div>

  <div class="card">
    <h2>8° 振幅制御測定</h2>
    <div class="fixed">
      目標ピーク <b>8.0°</b> ／ 測定 <b>30秒</b> ／ 遅延補償：3 ms固定<br>
      足角度：上マーカー＝右足、下マーカー＝左足。既存の直立・静止検知中に起動時0°のみ校正します。<br>
      測定中はWeb表示の更新を最小限にし、制御処理を優先します。
    </div>
    <button id="energy" class="primary" disabled onclick="startEnergy()">8°測定を開始</button>
    <button id="stop" class="danger" disabled onclick="postStop()">EMERGENCY STOP</button>
  </div>

  <div class="card">
    <h2>測定データ</h2>
    <p id="logInfo" class="note">RWLOGの状態を確認しています。</p>
    <a id="rwlog" class="action disabled" href="/download/rwlog" onclick="beginDownload()">RWLOGをダウンロード</a>
    <a id="footlog" class="action disabled" href="/download/foot-angle.csv" onclick="beginDownload()">足角度CSVをダウンロード</a>
    <button id="clear" class="secondary" disabled onclick="postClear()">測定データを消去</button>
  </div>
</main>
<footer>Free-foot observation layer / V46al-R2 control and RWLOG v51 retained</footer>
<script>
let downloading=false,lastStatus={},displayFrozen=false,refreshInFlight=false,startPending=false,controlEpoch=0;
const energy=document.getElementById('energy');
const stop=document.getElementById('stop');
const clear=document.getElementById('clear');
const rwlog=document.getElementById('rwlog');
const footlog=document.getElementById('footlog');

function lock(e,v){
  if(e.tagName==='A')e.classList.toggle('disabled',v);
  else e.disabled=v;
}
const num=v=>Number.isFinite(Number(v))?Number(v):null;
const angle=v=>num(v)===null?'--':num(v).toFixed(2)+'°';
const rate=v=>num(v)===null?'--':num(v).toFixed(2)+'°/s';
const current=v=>num(v)===null?'--':Math.round(num(v))+' mA';
const voltage=v=>num(v)===null?'--':(num(v)/1000).toFixed(2)+' V';

async function post(path){
  const r=await fetch(path,{method:'POST'});
  if(!r.ok)alert(await r.text());
  await refresh();
  return r.ok;
}

async function startEnergy(){
  if(startPending||energy.disabled)return;
  startPending=true;
  controlEpoch++;
  apply(lastStatus);
  try{
    const r=await fetch('/start-energy-control-autonomous',{method:'POST'});
    if(!r.ok)alert(await r.text());
    else{
      displayFrozen=true;
      applyFrozenState();
    }
  }catch(e){
    alert('開始結果を確認できません。状態の再取得を待ってください。');
  }finally{
    startPending=false;
    controlEpoch++;
    refresh();
  }
}

async function postStop(){
  displayFrozen=false;
  await post('/stop');
}

async function postClear(){
  if(confirm('現在の測定データを消去しますか？'))await post('/clear');
}

function beginDownload(){
  downloading=true;
  apply(lastStatus);
  setTimeout(()=>{
    downloading=false;
    refresh();
  },3000);
}

function stateLabel(j){
  if(j.running)return '測定中';
  if(!j.foot_camera_ok)return '足角度カメラ異常';
  if(!j.foot_zero_ready){
    if(j.right_foot_detected&&!j.left_foot_detected)return '左足マーカー待ち';
    if(!j.right_foot_detected&&j.left_foot_detected)return '右足マーカー待ち';
    if(!j.right_foot_detected&&!j.left_foot_detected)return '左右足マーカー待ち';
    return Number(j.foot_zero_samples||0)>0?'足角度0°校正中':'直立・静止待ち';
  }
  if(j.state==='READY_TO_MEASURE')return '測定可能';
  if(j.state==='FINISHED')return '測定完了';
  if(j.state==='ESTOP')return '非常停止';
  return j.state||'UNKNOWN';
}

function apply(j){
  const running=!!j.running;
  const busy=downloading||!!j.downloading||startPending;
  const canStart=(j.state==='READY_TO_MEASURE'||j.state==='FINISHED')&&
                 !!j.foot_camera_ok&&!!j.foot_zero_ready;

  lock(energy,busy||running||!canStart);
  lock(stop,!running);
  lock(clear,busy||running);
  lock(rwlog,busy||running||j.rwlog_downloadable!=='yes');
  lock(footlog,busy||running||j.foot_angle_log_downloadable!=='yes');

  document.getElementById('stateText').textContent=stateLabel(j);
  const badge=document.getElementById('readyBadge');
  const systemReady=!!j.ready&&!!j.foot_camera_ok&&!!j.foot_zero_ready;
  badge.textContent=systemReady?'READY':'NOT READY';

  document.getElementById('angle').textContent=angle(j.pitch_mekf_control_deg);
  document.getElementById('rate').textContent=rate(j.physical_roll_rate_dps);
  document.getElementById('motor').textContent=current(j.motor_cmd_mA);
  document.getElementById('actual').textContent=current(j.roller_actual_current_mA);
  document.getElementById('battery').textContent=voltage(j.battery_mV);
  document.getElementById('remaining').textContent=num(j.remaining_s)===null?'--':num(j.remaining_s).toFixed(1)+' s';
  document.getElementById('footRight').textContent=angle(j.right_foot_angle_deg);
  document.getElementById('footLeft').textContent=angle(j.left_foot_angle_deg);

  const imu=j.imu_ok?'IMU OK':'IMU NG';
  const roller=j.roller_ok?'Roller OK':'Roller NG';
  const foot=j.foot_zero_ready?'Foot 0° OK':(j.foot_camera_ok?'Foot 0° WAIT':'Foot CAM NG');
  const rdet=j.right_foot_detected?'R:検出':'R:未検出';
  const ldet=j.left_foot_detected?'L:検出':'L:未検出';
  const samples=Number(j.foot_zero_samples||0);
  document.getElementById('systemInfo').textContent=
    imu+' / '+roller+' / '+foot+' / '+rdet+' / '+ldet+' / zero samples '+samples+
    ' / 目標 '+Number(j.energy_control_autonomous_target_peak_deg||8).toFixed(1)+'° / 遅延補償 3 ms';

  let zeroDetail='直立・静止と左右マーカーを確認しています。';
  if(!j.foot_camera_ok)zeroDetail='足角度カメラを初期化できていません。';
  else if(j.foot_zero_ready)zeroDetail='左右足の起動時0°校正は完了しています。';
  else if(!j.right_foot_detected&&!j.left_foot_detected)zeroDetail='右足・左足の白マーカーを検出できていません。';
  else if(!j.right_foot_detected)zeroDetail='右足（上側）の白マーカーを検出できていません。';
  else if(!j.left_foot_detected)zeroDetail='左足（下側）の白マーカーを検出できていません。';
  else if(samples===0)zeroDetail='左右マーカーは検出済みです。機体を直立・静止させてください。';
  else zeroDetail='左右マーカーを検出済み。0°サンプルを収集中です。';
  document.getElementById('startupInfo').textContent=
    zeroDetail+' / R='+(j.right_foot_detected?'OK':'NG')+
    ' / L='+(j.left_foot_detected?'OK':'NG')+
    ' / samples='+samples;

  const errors=[];
  if(j.last_error)errors.push(j.last_error);
  if(!j.foot_camera_ok&&j.foot_camera_error)errors.push('Foot: '+j.foot_camera_error);
  if(!j.imu_ok&&j.imu_error)errors.push('IMU: '+j.imu_error);
  document.getElementById('errorInfo').textContent=errors.join(' / ');

  if(j.rwlog_downloadable==='yes'){
    const footReady=j.foot_angle_log_downloadable==='yes'?' / 足角度CSV準備完了':' / 足角度CSVなし';
    document.getElementById('logInfo').textContent='RWLOG準備完了'+(j.download_filename?'：'+j.download_filename:'')+footReady;
  }else if(running){
    document.getElementById('logInfo').textContent='測定中です。終了後にRWLOGを保存できます。';
  }else{
    document.getElementById('logInfo').textContent='保存できるRWLOGはまだありません。';
  }
}

function applyFrozenState(){
  [energy,clear,rwlog,footlog].forEach(x=>lock(x,true));
  lock(stop,false);
  document.getElementById('stateText').textContent='測定中';
  document.getElementById('readyBadge').textContent='RUNNING';
  document.getElementById('systemInfo').textContent='測定処理を優先しています。';
  document.getElementById('logInfo').textContent='測定終了後にRWLOGを保存できます。';
}

async function refresh(){
  if(refreshInFlight)return;
  refreshInFlight=true;
  const epoch=controlEpoch;
  let timer;
  try{
    const controller=new AbortController();
    timer=setTimeout(()=>controller.abort(),1500);
    const r=await fetch('/status.json',{cache:'no-store',signal:controller.signal});
    clearTimeout(timer);
    if(!r.ok)throw new Error('status_failed');
    const status=await r.json();
    if(epoch!==controlEpoch)return;
    lastStatus=status;
    if(lastStatus.running){
      displayFrozen=true;
      applyFrozenState();
      return;
    }
    if(displayFrozen)displayFrozen=false;
    apply(lastStatus);
  }catch(e){
    if(!displayFrozen){
      [energy,stop,clear,rwlog,footlog].forEach(x=>lock(x,true));
      document.getElementById('stateText').textContent='通信待ち';
    }
  }finally{
    if(timer)clearTimeout(timer);
    refreshInFlight=false;
  }
}

setInterval(refresh,1000);
refresh();
</script>
</body>
</html>
)HTML";

bool WebUi::beginAccessPoint() {
  // Keep the AP bring-up identical in spirit to the fixed-foot hardware-proven path.
  // Starting only the AP here reserves Wi-Fi resources before camera/PSRAM work,
  // while the HTTP listener itself is intentionally started after subsystem init.
  WiFi.mode(WIFI_AP);
  ap_ready_ = Config::AP_PASS[0]
      ? WiFi.softAP(Config::AP_SSID, Config::AP_PASS, Config::AP_CHANNEL)
      : WiFi.softAP(Config::AP_SSID, nullptr, Config::AP_CHANNEL);
  return ap_ready_;
}

bool WebUi::begin(WebServer& server, ExperimentRunner& runner, ImuManager& imu, Roller485Manager& roller, PsramLogger& logger, FootAngleTracker& foot_angles) {
  server_ = &server;
  runner_ = &runner;
  imu_ = &imu;
  roller_ = &roller;
  logger_ = &logger;
  foot_angles_ = &foot_angles;

  server_->on("/", HTTP_GET, [this]() { handleRoot(); });
  server_->on("/status.json", HTTP_GET, [this]() { handleStatus(); });
  server_->on("/health", HTTP_GET, [this]() {
    char body[128];
    snprintf(body, sizeof(body), "ok ap=%u stations=%u heap=%u\n",
             ap_ready_ ? 1U : 0U,
             static_cast<unsigned>(WiFi.softAPgetStationNum()),
             static_cast<unsigned>(ESP.getFreeHeap()));
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(200, "text/plain; charset=utf-8", body);
  });
  server_->on("/start-energy-control-autonomous", HTTP_POST, [this]() { handleStartEnergyControlAutonomous(); });
  server_->on("/stop", HTTP_POST, [this]() { handleStop(); });
  server_->on("/clear", HTTP_POST, [this]() { handleClear(); });
  server_->on("/download/rwlog", HTTP_GET, [this]() { handleRwLog(); });
  server_->on("/download/foot-angle.csv", HTTP_GET, [this]() { handleFootAngleLog(); });
  server_->enableDelay(false);  // Empty HTTP polls must not add sleeps to idle acquisition.
  server_->begin();
  return ap_ready_;
}

void WebUi::update() {
  if (server_) server_->handleClient();
}

void WebUi::handleRoot() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (run_control.active() || runner_->running()) { server_->send(409, "text/plain", "read_after_run"); return; }
  server_->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server_->sendHeader("Pragma", "no-cache");
  server_->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebUi::handleStatus() {
  if (run_control.active()) {
    // Copy only immutable POD status; do not read runner/logger/imu.reading
    // while the higher-priority worker owns them. No network I/O in a lock.
    const RunControlSnapshot st = run_control.snapshot();
    char body[192];
    snprintf(body, sizeof(body),
        "{\"running\":%s,\"state\":\"%s\",\"motor_cmd_mA\":%d,\"roller_actual_current_mA\":%d,\"remaining_ms\":%lu}",
        st.running ? "true" : "false", st.state_name,
        static_cast<int>(st.motor_cmd_mA), static_cast<int>(st.actual_current_mA),
        static_cast<unsigned long>(st.remaining_ms));
    server_->send(200, "application/json", body);
    return;
  }
  server_->send(200, "application/json", statusJson());
}

void WebUi::handleStartEnergyControlAutonomous() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (logger_->downloading()) { server_->send(409, "text/plain", "download_in_progress"); return; }
  if (!run_control.ready()) { server_->send(503, "text/plain", "run_control_worker_not_ready"); return; }
  if (!foot_angles_ || !foot_angles_->cameraOk()) { server_->send(503, "text/plain", "foot_angle_camera_not_ready"); return; }
  if (!foot_angles_->zeroReady()) { server_->send(409, "text/plain", "foot_angle_zero_not_ready_hold_upright"); return; }
  if (server_->hasArg("timing_ms")) {
    // Old cached pages must refresh instead of silently requesting another delay.
    server_->send(400, "text/plain", "timing_selection_removed_fixed_3ms_reload_page"); return;
  }
  // Refresh from the idle mailbox before the unchanged physical start gate.
  // The run boundary is established by main AFTER this HTTP response returns.
  imu_->update();
  const bool ok = runner_->startEnergyControlAutonomousCapture();
  server_->send(ok ? 200 : 409, "text/plain", ok ? "energy_control_autonomous_started" : runner_->status().last_error);
}

void WebUi::handleStartQIdent() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  server_->send(409, "text/plain", "q_ident_frozen_use_energy_control_v0");
}void WebUi::handleStart() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (logger_->downloading()) {
    server_->send(409, "text/plain", "download_in_progress");
    return;
  }
  if (!server_->hasArg("trial")) {
    server_->send(400, "text/plain", "trial_required");
    return;
  }
  const uint8_t trial_number = static_cast<uint8_t>(server_->arg("trial").toInt());
  const bool ok = runner_->startSingleTrialTest(trial_number);
  server_->send(ok ? 200 : 409, "text/plain", ok ? "started" : "start_failed");
}

void WebUi::handleStartZeroCross() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (logger_->downloading()) {
    server_->send(409, "text/plain", "download_in_progress");
    return;
  }
  if (!server_->hasArg("pulse_width_ms")) {
    server_->send(400, "text/plain", "pulse_width_ms_required");
    return;
  }
  const int16_t current_mA = Config::ZERO_CROSS_OPERATING_CURRENT_MA;
  const int pulse_width_ms = server_->arg("pulse_width_ms").toInt();
  const bool pulse_ok = pulse_width_ms >= Config::ZERO_CROSS_TIME_SWEEP_MIN_PULSE_MS &&
                        pulse_width_ms <= Config::ZERO_CROSS_TIME_SWEEP_MAX_PULSE_MS;
  if (!pulse_ok) {
    server_->send(400, "text/plain", "invalid_zero_cross_condition");
    return;
  }
  const bool ok = runner_->startZeroCrossTest(static_cast<int16_t>(current_mA),
                                              static_cast<uint16_t>(pulse_width_ms));
  server_->send(ok ? 200 : 409, "text/plain", ok ? "zero_cross_started" : "start_failed");
}
void WebUi::handleStartIdentification() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (logger_->downloading()) { server_->send(409, "text/plain", "download_in_progress"); return; }
  const bool ok = runner_->startZeroCrossIdentificationTest();
  server_->send(ok ? 200 : 409, "text/plain", ok ? "validation_started" : "start_failed");
}

void WebUi::handleStartControl() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (logger_->downloading()) { server_->send(409, "text/plain", "download_in_progress"); return; }
  if (!server_->hasArg("target_peak_deg")) { server_->send(400, "text/plain", "target_peak_deg_required"); return; }
  const float target_peak_deg = server_->arg("target_peak_deg").toFloat();
  const int schedule_arg = server_->hasArg("q_probe_schedule_id") ?
      server_->arg("q_probe_schedule_id").toInt() : Config::ZERO_CROSS_CALIBRATION_Q_PROBE_SCHEDULE_A;
  if (schedule_arg < Config::ZERO_CROSS_CALIBRATION_Q_PROBE_SCHEDULE_A ||
      schedule_arg >= Config::ZERO_CROSS_CALIBRATION_Q_PROBE_SCHEDULE_COUNT) {
    server_->send(400, "text/plain", "invalid_q_probe_schedule");
    return;
  }
  const bool ok = runner_->startZeroCrossControlTest(
      target_peak_deg, static_cast<uint8_t>(schedule_arg));
  server_->send(ok ? 200 : 409, "text/plain", ok ? "control_started" : "start_failed");
}
void WebUi::handleZero() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (runner_->running()) {
    server_->send(409, "text/plain", "running");
    return;
  }
  runner_->zeroAngleNow();
  server_->send(200, "text/plain", "zeroed");
}

void WebUi::handleStop() {
  if (run_control.requestStop()) {
    server_->send(202, "text/plain", "stop_requested");
    return;
  }
  runner_->requestEmergencyStop("web_estop");
  server_->send(200, "text/plain", "stopped");
}

void WebUi::handleClear() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (runner_->running() || logger_->downloading()) {
    server_->send(409, "text/plain", "busy");
    return;
  }
  runner_->clearFinishedOrEstop();
  if (foot_angles_) foot_angles_->clearFinishedLog();
  server_->send(200, "text/plain", "cleared");
}

void WebUi::handleRwLog() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (runner_->running()) {
    server_->send(409, "text/plain", "measurement_running");
    return;
  }
  logger_->streamRwLog(*server_);
}

void WebUi::handleFootAngleLog() {
  if (run_control.active()) { server_->send(409, "text/plain", "run_in_progress"); return; }
  if (runner_->running()) { server_->send(409, "text/plain", "measurement_running"); return; }
  if (!foot_angles_) { server_->send(503, "text/plain", "foot_angle_tracker_unavailable"); return; }
  foot_angles_->streamCsv(*server_);
}

String WebUi::statusJson() const {
  const auto& st = runner_->status();
  const RollerTelemetry roller = roller_->telemetrySnapshot();
  const FootAngleSnapshot foot = foot_angles_ ? foot_angles_->snapshot() : FootAngleSnapshot{};

  char filename[72];
  logger_->downloadFilename(filename, sizeof(filename));
  char foot_filename[72] = {};
  if (foot_angles_) foot_angles_->downloadFilename(foot_filename, sizeof(foot_filename));

  String json;
  json.reserve(768);
  json += "{";
  json += "\"running\":" + String(runner_->running() ? "true" : "false");
  json += ",\"downloading\":" + String(logger_->downloading() ? "true" : "false");
  json += ",\"state\":\"" + String(runner_->stateName()) + "\"";
  json += ",\"ready\":" + String(st.ready ? "true" : "false");
  json += ",\"energy_control_autonomous_target_peak_deg\":" +
          String(runner_->energyControlAutonomousTargetPeakDeg(), 2);
  json += ",\"pitch_mekf_control_deg\":" + String(st.pitch_mekf_deg, 3);
  json += ",\"physical_roll_rate_dps\":" + String(st.physical_roll_rate_dps, 4);
  json += ",\"motor_cmd_mA\":" + String(st.motor_cmd_mA);
  json += ",\"remaining_s\":" + String(static_cast<float>(st.remaining_ms) / 1000.0f, 1);
  json += ",\"rwlog_downloadable\":\"" +
          String(logger_->rwlogDownloadable() ? "yes" : "no") + "\"";
  json += ",\"download_filename\":\"" + String(filename) + "\"";
  json += ",\"foot_angle_log_downloadable\":\"" +
          String(foot_angles_ && foot_angles_->logDownloadable() ? "yes" : "no") + "\"";
  json += ",\"foot_angle_download_filename\":\"" + String(foot_filename) + "\"";
  json += ",\"foot_camera_ok\":" + String(foot.camera_ok ? "true" : "false");
  json += ",\"foot_camera_error\":\"" + String(foot_angles_ ? foot_angles_->lastError() : "not_available") + "\"";
  json += ",\"foot_zero_ready\":" + String(foot.zero_ready ? "true" : "false");
  json += ",\"right_foot_angle_deg\":" + String(foot.right_angle_deg, 3);
  json += ",\"left_foot_angle_deg\":" + String(foot.left_angle_deg, 3);
  json += ",\"right_foot_detected\":" + String(foot.right_detected ? "true" : "false");
  json += ",\"left_foot_detected\":" + String(foot.left_detected ? "true" : "false");
  json += ",\"right_foot_in_range\":" + String(foot.right_in_range ? "true" : "false");
  json += ",\"left_foot_in_range\":" + String(foot.left_in_range ? "true" : "false");
  json += ",\"foot_zero_samples\":" + String(foot.zero_samples);
  json += ",\"imu_ok\":" + String(imu_->ok() ? "true" : "false");
  json += ",\"imu_error\":\"" + String(imu_->lastError()) + "\"";
  json += ",\"roller_ok\":" + String(roller_->ok() ? "true" : "false");
  json += ",\"roller_actual_current_mA\":" + String(roller.actual_current_mA);
  json += ",\"battery_mV\":" + String(roller.battery_mV);
  json += ",\"last_error\":\"" +
          String(st.last_error && st.last_error[0] ? st.last_error : logger_->lastError()) + "\"";
  json += "}";

  // Keep the browser JSON valid if an estimator value is temporarily non-finite.
  json.replace(":-Infinity", ":null");
  json.replace(":Infinity", ":null");
  json.replace(":-inf", ":null");
  json.replace(":inf", ":null");
  json.replace(":NaN", ":null");
  json.replace(":nan", ":null");
  return json;
}
