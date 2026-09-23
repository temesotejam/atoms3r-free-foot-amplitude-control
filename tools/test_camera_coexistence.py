from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[1]

main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
camera = (ROOT / "src/camera_coexistence.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/camera_coexistence.h").read_text(encoding="utf-8")
serial = (ROOT / "src/camera_serial_debug.cpp").read_text(encoding="utf-8")
net = (ROOT / "src/tcp_transport_debug.cpp").read_text(encoding="utf-8")
bounded = (ROOT / "src/bounded_web_server.cpp").read_text(encoding="utf-8")
bounded_h = (ROOT / "src/bounded_web_server.h").read_text(encoding="utf-8")
patch = (ROOT / "src/camera_task_priority_patch.cpp").read_text(encoding="utf-8")
pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")

def git_blob_sha(path):
    data = (ROOT / path).read_bytes()
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

expected = {
    "src/web_ui.cpp": "c63bb11581c8252fe92f151fb97c9208175bd336",
    "src/experiment_runner.cpp": "58688977951627cdab038d73ca4d64b0c6b3d645",
    "src/psram_logger.cpp": "e61167fba2869ad948df37d999a7bcb6e5346817",
    "src/psram_logger.h": "63a16781660142a5e3a82721f90cadd9cc2ce9b7",
    "src/imu_manager.cpp": "4842be2a4d91bcd8f2895cfe046489a47a3ce64c",
    "src/roller485_manager.cpp": "c891e50cc832654034d9a6bc64848f48713741c5",
}
for path, sha in expected.items():
    assert git_blob_sha(path) == sha, path

assert main.index("M5.begin(cfg);") < main.index("const bool psram_ok = logger.begin();")
assert main.index("const bool psram_ok = logger.begin();") < main.index("const bool imu_ok = imu.begin();")
assert main.index("const bool imu_ok = imu.begin();") < main.index("const bool camera_ok = camera_probe.begin();")
assert main.index("const bool camera_ok = camera_probe.begin();") < main.index("const bool roller_ok = roller.begin();")

# WebUI source itself remains byte-identical to the fixed-foot baseline; only
# the WebServer transport object is replaced underneath it.
assert git_blob_sha("src/web_ui.cpp") == "c63bb11581c8252fe92f151fb97c9208175bd336"
assert '#include "bounded_web_server.h"' in main
assert "BoundedWriteWebServer server(Config::HTTP_PORT);" in main
assert "class BoundedWriteWebServer : public WebServer" in bounded_h
assert "kMaxProgmemWriteBytes = 512" in bounded
assert "_currentClientWrite_P(PGM_P buffer, size_t length)" in bounded
assert "_currentClient.write_P(buffer + total, want)" in bounded
assert "taskYIELD();" in bounded
assert "NETDBG,web_p_begin" in bounded
assert "NETDBG,web_p_end" in bounded

# Startup LED diagnostics are observation-only: expose every original gate
# without changing thresholds or confirmation behavior.
assert "POSEDBG,ms=%lu,reason=%s" in main
assert "direction_error_deg=%.3f" in main
assert "accel_norm_g=%.4f" in main
assert "gyro_norm_dps=%.3f" in main
assert "hold_ms=%lu" in main
assert "UPRIGHT_MAX_DIRECTION_ERROR_DEG" in (ROOT / "src/upright_pose_guide.h").read_text(encoding="utf-8")
assert "UPRIGHT_STABLE_HOLD_MS = 400UL" in (ROOT / "src/upright_pose_guide.h").read_text(encoding="utf-8")

# SCCB only at boot on I2C0; never touch BMI270 I2C1.
assert "I2C_NUM_1" not in camera
assert "i2c_param_config(I2C_NUM_0, &sccb)" in camera
assert "c.sccb_i2c_port = I2C_NUM_0;" in camera
assert "i2c_driver_delete(I2C_NUM_0)" in camera

# No background camera acquisition task exists in the serial debug build.
assert "xTaskCreatePinnedToCore(" not in camera
assert "taskLoop" not in camera

# Manual one-shot still obeys the strict XCLK/receiver gate.
capture = camera[camera.index("camera_fb_t* OneShotCamera::acquire") :
                 camera.index("void OneShotCamera::release")]
assert capture.index("setXclkEnabled(true);") < capture.index("cam_start();")
assert capture.index("cam_start();") < capture.index("cam_take(")
assert capture.index("cam_take(") < capture.index("cam_stop();")
assert capture.index("cam_stop();") < capture.index("setXclkEnabled(false);")
assert "esp_camera_fb_get()" not in camera

# USB CDC serial debugger exposes deterministic manual transitions.
for token in (
    "CAMDBG,USB serial camera debugger ready",
    "s=status c=capture_once i=force_idle p=sensor_power_off d=camera_deinit",
    "camera.debugCaptureOnce()",
    "camera.debugForceIdle()",
    "camera.debugPowerSensorOff()",
    "camera.debugDeinit()",
    "WiFi.softAPgetStationNum()",
    "MALLOC_CAP_INTERNAL",
    "MALLOC_CAP_DMA",
    "cameraSerialDebugUpdate(camera_probe, !runner.running());",
):
    assert token in serial + main, token

# Destructive debug actions cannot be triggered while the control run is active.
assert "DENIED_CONTROL_RUN_ACTIVE" in serial

# Port 81 is an independent raw-TCP probe; port 80 has a tiny WebServer probe.
for token in (
    "WiFiServer g_server(kDiagPort)",
    "constexpr uint16_t kDiagPort = 81",
    "raw tcp port 81 ok",
    "NETDBG,accept",
    "NETDBG,request",
    "NETDBG,response",
    "largest_internal",
    "largest_dma",
):
    assert token in net, token
assert 'server.on("/net-probe", HTTP_GET' in main
assert "tcpTransportDebugBegin();" in main
assert "tcpTransportDebugUpdate();" in main

# Still no image/foot-angle analysis.
for token in ("foot_angle", "right_foot", "left_foot", "centroid", "deg_per_px"):
    assert token not in camera.lower(), token

assert '--wrap=xTaskCreatePinnedToCore' in pio
assert 'strcmp(pcName, "cam_task") == 0' in patch
assert 'kCameraInternalTaskPriority = 3' in patch

print("PASS: USB serial manual camera debugger with no background capture")
