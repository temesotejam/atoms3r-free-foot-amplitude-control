from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[1]

config = (ROOT / "src/config.h").read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
tracker_h = (ROOT / "src/foot_angle_tracker.h").read_text(encoding="utf-8")
tracker = (ROOT / "src/foot_angle_tracker.cpp").read_text(encoding="utf-8")
runner = (ROOT / "src/experiment_runner.cpp").read_text(encoding="utf-8")
logger_h = (ROOT / "src/psram_logger.h").read_bytes()
logger = (ROOT / "src/psram_logger.cpp").read_bytes()
converter = (ROOT / "tools/convert_rwlog_to_csv.py").read_bytes()
log_types = (ROOT / "src/log_types.h").read_text(encoding="utf-8")
web = (ROOT / "src/web_ui.cpp").read_text(encoding="utf-8")

# Fixed-foot V46al-R2 binary logger/download path remains byte-identical.
assert hashlib.sha1(b"blob " + str(len(logger_h)).encode() + b"\0" + logger_h).hexdigest() == "63a16781660142a5e3a82721f90cadd9cc2ce9b7"
assert hashlib.sha1(b"blob " + str(len(logger)).encode() + b"\0" + logger).hexdigest() == "e61167fba2869ad948df37d999a7bcb6e5346817"
assert hashlib.sha1(b"blob " + str(len(converter)).encode() + b"\0" + converter).hexdigest() == "7a2c1229376e1ec204a3d9305cedf0b67af7a231"
assert "RWLOG_FORMAT_VERSION = 51" in logger.decode()
assert "sizeof(LogSample) == 258" in log_types

# Physical marker mapping and calibration slopes.
assert "upper image lane (Marker A) = RIGHT foot" in tracker
assert "lower image lane (Marker B) = LEFT foot" in tracker
assert "kRightDegPerPx = 0.167779119f" in tracker
assert "kLeftDegPerPx = 0.162645305f" in tracker

# Existing upright gate performs only zero-offset calibration.
assert "foot_angles.setStartupUprightGate(upright_sample)" in main
assert "foot_angles.lockStartupZero()" in main
assert "pixel-to-angle slopes stay frozen" in main
assert "zero_right_x_px_" in tracker and "zero_left_x_px_" in tracker

# Camera is isolated on low-priority core-0 task; control remains core-1.
assert "kTaskPriority = 1" in tracker
assert "kTaskCore = 0" in tracker
assert "RunControlWorker::kPriority" in main

# Observation must not enter ExperimentRunner controller/model source.
for token in ("FootAngleTracker", "right_foot_angle", "left_foot_angle",
              "right_angle_deg", "left_angle_deg"):
    assert token not in runner, token

# Sidecar logging is separate; RWLOG remains the fixed baseline.
assert "/download/foot-angle.csv" in web
assert "foot_angle_log_downloadable" in web
assert "foot_angle_zero_not_ready_hold_upright" in web
assert "beginRun(logger.currentRunId()" in main
assert "foot_angles.endRun()" in main

# Web UI exposes which foot-marker condition blocks zeroing without restoring
# the heavy startup diagnostics JSON to the 1 Hz status path.
assert 'startupDiagnosticsJson()' not in web[web.index("String WebUi::statusJson() const"):]
assert 'right_foot_detected' in web and 'left_foot_detected' in web
assert 'foot_zero_samples' in web
assert '左右足マーカー待ち' in web
assert '右足（上側）の白マーカー' in web
assert '左足（下側）の白マーカー' in web
assert '!!j.foot_camera_ok&&!!j.foot_zero_ready' in web

print("Free-foot observation isolation guards PASS")




# Camera SCCB must never claim the BMI270 I2C1 bus.
assert 'c.pin_sccb_sda = -1;' in tracker
assert 'c.pin_sccb_scl = -1;' in tracker
assert 'c.sccb_i2c_port = I2C_NUM_0;' in tracker
assert 'i2c_param_config(I2C_NUM_0, &sccb)' in tracker
assert 'i2c_driver_install(I2C_NUM_0' in tracker
assert 'i2c_driver_delete(I2C_NUM_0)' in tracker
assert 'I2C_NUM_1' not in tracker

# Preserve the previously working M5/logger/camera/IMU startup order.
assert main.index('M5.begin(cfg);') < main.index('const bool psram_ok = logger.begin();')
assert main.index('const bool psram_ok = logger.begin();') < main.index('const bool foot_ok = foot_angles.begin();')
assert main.index('const bool foot_ok = foot_angles.begin();') < main.index('const bool imu_ok = imu.begin();')

assert 'foot_camera_error' in web
assert 'imu_error' in web


# Wi-Fi AP is now brought up immediately after M5.begin and before heavy
# logger/camera/IMU/roller initialization.
ap_begin = main.index('const bool ap_ok = web.begin(')
assert main.index('M5.begin(cfg);') < ap_begin
assert ap_begin < main.index('const bool psram_ok = logger.begin();')
assert ap_begin < main.index('const bool foot_ok = foot_angles.begin();')
assert ap_begin < main.index('const bool imu_ok = imu.begin();')
assert 'WiFi AP FAIL' in main
assert 'for (int attempt = 0; attempt < 3 && !ap_ready_; ++attempt)' in web
assert 'ap_ready_ = WiFi.softAP' in web


# Temporary Wi-Fi diagnostic AP removes cached credentials/WPA from the test.
assert 'AtomS3R_FREEFOOT_DIAG' in config
assert 'static constexpr char AP_PASS[] = "";' in config
assert 'WiFi.softAP(Config::AP_SSID, nullptr' in web
assert 'softAPgetStationNum()' in main
assert 'heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)' in main
