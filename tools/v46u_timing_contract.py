"""Strict inverse of reviewed timing-only changes for retained baseline tests."""
from pathlib import Path
import json
from v46ac_delay_comp_contract import normalize_config as normalize_v46ac_config
from v46ab_no_prediction_contract import normalize_config as normalize_v46ab_config
from v46aa_control_zero_contract import normalize_log_types as normalize_v46aa_log_types
from v46z_comparison_zero_contract import normalize_log_types as normalize_v46z_log_types
from v46ak_observation_contract import normalize_file as normalize_v46ak_file
from v46al_control_contract import normalize_file as normalize_v46al_file
ROOT=Path(__file__).resolve().parents[1]
def normalize_free_foot_main(data):
    data=data.replace('#include "esp_heap_caps.h"\n','')
    data=data.replace('#include "foot_angle_tracker.h"\n','')
    data=data.replace('FootAngleTracker foot_angles;\n','')
    data=data.replace('''  const bool upright_sample = fresh && UprightPoseGuide::isUprightStableSample(r);
  foot_angles.setStartupUprightGate(upright_sample);
  imu.setStartupGuideState(reason, false, 0);
  if (!upright_sample) {
    startup_upright_since_ms = 0;
    return;
  }
''','''  imu.setStartupGuideState(reason, false, 0);
  if (!fresh || !UprightPoseGuide::isUprightStableSample(r)) {
    startup_upright_since_ms = 0;
    return;
  }
''')
    data=data.replace('''  // The existing IMU upright/still gate also defines the boot-specific foot
  // angle zero. Marker A (upper) is the right foot; Marker B (lower) is left.
  // Only the zero offset is calibrated here; the pixel-to-angle slopes stay frozen.
  if (!foot_angles.lockStartupZero()) {
    imu.setStartupGuideState("waiting_foot_markers", false,
                             now_ms - startup_upright_since_ms);
    displayLine("Hold upright", "Waiting foot markers");
    return;
  }
  foot_angles.setStartupUprightGate(false);
''','')
    data=data.replace('''  const FootAngleSnapshot foot = foot_angles.snapshot();
  Serial.printf("Startup guide: upright confirmed; gravity error=%.2f deg, norm=%.3f g; "
                "foot zero R=%.3f px L=%.3f px samples=%lu\\n",
                UprightPoseGuide::directionErrorDeg(r), UprightPoseGuide::accelNormG(r),
                foot.zero_right_x_px, foot.zero_left_x_px,
                static_cast<unsigned long>(foot.zero_samples));
''','''  Serial.printf("Startup guide: upright confirmed; gravity error=%.2f deg, norm=%.3f g\\n",
                UprightPoseGuide::directionErrorDeg(r), UprightPoseGuide::accelNormG(r));
''')
    data=data.replace('''  const bool foot_ok = foot_angles.begin();
  Serial.printf("Foot angle tracker: %s mapping=upper:right/lower:left mode=observation_only error=%s\\n",
                foot_ok ? "OK" : "FAILED", foot_angles.lastError());

''','')
    data=data.replace('''  // Bring up the AP before camera/PSRAM/IMU worker allocations. The route
  // handlers only run from loop(), so storing references here is safe even
  // though the subsystems are initialized below.
  const bool ap_ok = web.begin(server, runner, imu, roller, logger, foot_angles);
  Serial.printf("WiFi AP: %s SSID=%s auth=%s IP=%s heap=%u largest=%u\\n",
                ap_ok ? "OK" : "FAILED", Config::AP_SSID,
                Config::AP_PASS[0] ? "WPA2" : "OPEN",
                WiFi.softAPIP().toString().c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  if (!ap_ok) displayLine("WiFi AP FAIL", Config::AP_SSID);

''','')
    data=data.replace('''  Serial.printf("AP SSID: %s status=%s IP=%s stations=%u heap=%u largest=%u\\n",
                Config::AP_SSID, web.accessPointReady() ? "READY" : "FAILED",
                WiFi.softAPIP().toString().c_str(),
                static_cast<unsigned>(WiFi.softAPgetStationNum()),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  Serial.println("Open http://192.168.4.1/ and start Autonomous Energy Control V7");
  if (web.accessPointReady()) displayLine("V46q / V7 ready", Config::AP_SSID);
  else displayLine("WiFi AP FAIL", Config::AP_SSID);''',
                      '''  web.begin(server, runner, imu, roller, logger);
  Serial.printf("AP SSID: %s\\n", Config::AP_SSID);
  Serial.println("Open http://192.168.4.1/ and start Autonomous Energy Control V7");
  displayLine("V46q / V7 ready", Config::AP_SSID);''')
    data=data.replace('''  // The camera task is observation-only and owns a sidecar log. Close that
  // sidecar only after the run-control worker has fully released the runner.
  if (!runner.running() && foot_angles.runActive()) {
    foot_angles.endRun();
  }

''','')
    data=data.replace('''  if (runner.running()) {
    if (!foot_angles.runActive()) {
      foot_angles.beginRun(logger.currentRunId(), static_cast<uint32_t>(logger.runStartUs()));
    }
    updateAcquisitionContext();
''','''  if (runner.running()) {
    updateAcquisitionContext();
''')
    return data

def original_timing_file(path):
    data=(ROOT/path).read_text()
    if path == "src/main.cpp":
        data=normalize_free_foot_main(data)
    if path == "src/config.h":
        data=data.replace('static constexpr char AP_SSID[] = "AtomS3R_FREEFOOT_DIAG";',
                          'static constexpr char AP_SSID[] = "AtomS3CAM_Q1_SHADOW";')
        data=data.replace('static constexpr char AP_PASS[] = "";',
                          'static constexpr char AP_PASS[] = "12345678";')
    # V46al-R1 is the declared active-control delta; remove it before retained hashes.
    data=normalize_v46al_file(path, data)
    # V46ak is observation-only; remove it before checking the retained baseline.
    data=normalize_v46ak_file(path, data)
    if path == 'src/config.h':
        data=normalize_v46ac_config(data)
        data=normalize_v46ab_config(data)
    # V46aa makes the timing detector reference explicit without changing its
    # algebraic value. Reverse its identity/log extension first.
    if path == 'src/config.h':
        data=data.replace('v46aa_control_upright_zero_20260918','v46z_event_relative_angle_zero_20260918')
    if path == 'src/log_types.h':
        data=normalize_v46aa_log_types(data)
        data=normalize_v46z_log_types(data)
    # V46z changes comparison-zero observability only. Reverse its identity next.
    if path == 'src/config.h':
        data=data.replace('v46z_event_relative_angle_zero_20260918','v46y_frozen_imu_1ms_20260918')
    # V46y freezes the selected 1 ms polling specification. Reverse it to the
    # V46x comparison point first, then unwind the earlier timing-only releases.
    if path == 'src/config.h':
        data=data.replace('v46y_frozen_imu_1ms_20260918','v46x_imu_poll_500us_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 1000UL;  // V46y frozen IMU host polling specification.',
                          'IMU_POLL_PERIOD_US = 500UL;  // V46x: poll at 2 kHz to reduce data-ready discovery latency.')
    # V46x changes only the polling experiment identity/period. Reverse it to
    # V46w first, then unwind the earlier timing-only releases.
    if path == 'src/config.h':
        data=data.replace('v46x_imu_poll_500us_20260918','v46w_imu_poll_2500us_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 500UL;  // V46x: poll at 2 kHz to reduce data-ready discovery latency.',
                          'IMU_POLL_PERIOD_US = 2500UL;  // V46w: poll once per nominal 400 Hz gyro period.')
    # V46w changes only the polling experiment identity/period. Reverse these
    # before reversing V46v/V46u so retained protected-source hashes still certify
    # the unchanged controller and estimators.
    if path == 'src/config.h':
        data=data.replace('v46w_imu_poll_2500us_20260918','v46v_deadline_tightening_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 2500UL;  // V46w: poll once per nominal 400 Hz gyro period.',
                          'IMU_POLL_PERIOD_US = 1000UL;')
    # V46v timing-only changes are reversed first so retained V46u/V46s
    # protected-source hashes still certify the unchanged controller/estimators.
    if path == 'src/config.h':
        data=data.replace('v46v_deadline_tightening_20260918','v46u_timing_reader_20260915')
        data=data.replace('static constexpr uint32_t BMI270_I2C_HZ = 1000000UL;  // BMI270 Fast-mode Plus maximum.\n','')
        data=data.replace('CURRENT_AUDIT_FAST_READ_PERIOD_US = 1000UL;  // Try every 1 ms to keep valid samples within the 2 ms audit budget.',
                          'CURRENT_AUDIT_FAST_READ_PERIOD_US = 2000UL;')
    elif path == 'src/imu_manager.cpp':
        data=data.replace('  // V46v timing-only change: BMI270 supports Fast-mode Plus up to 1 MHz.\n  // Keep the same internal bus, axes, ODR and estimator path; only shorten transfers.\n  M5.Imu.setClock(Config::BMI270_I2C_HZ);\n','')
    elif path == 'src/roller485_manager.cpp':
        data=data.replace('  // V46v: try the observational current audit every 1 ms while a pulse is active.\n  // This does not alter pulse timing or current command; it only reduces sample-age slack.\n  bool current_already_fresh = false;\n',
                          '  // V46i keeps the full 2 ms current audit, but it now runs only on Core 0.\n')
        data=data.replace('    current_already_fresh = readCurrentFresh(true);','    readCurrentFresh(true);')
        data=data.replace('  // If the fast audit already obtained a valid current in this same loop,\n  // reuse it instead of immediately reading CURRENT_READBACK a second time.\n  bool ok = current_already_fresh || readCurrentFresh(command_mA_ != 0);',
                          '  bool ok = readCurrentFresh(command_mA_ != 0);')
    edits=json.loads((ROOT/'tools/v46u_timing_delta.json').read_text())
    for edit in reversed(edits):
        if edit['path'] != path:continue
        if data.count(edit['new']) != 1:raise ValueError('Timing delta changed: '+path)
        data=data.replace(edit['new'],edit['old'],1)
    return data.encode()
