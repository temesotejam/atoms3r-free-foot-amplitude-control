from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[1]

main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
camera = (ROOT / "src/camera_coexistence.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/camera_coexistence.h").read_text(encoding="utf-8")
patch = (ROOT / "src/camera_task_priority_patch.cpp").read_text(encoding="utf-8")
pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")

def git_blob_sha(path):
    data = (ROOT / path).read_bytes()
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

# Fixed-foot controller/estimator/web/logger sources remain byte-identical.
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

# Proven boot ordering is unchanged: M5/logger/IMU first, Camera borrows I2C0,
# then Roller485 owns I2C0 for the rest of runtime.
assert main.index("M5.begin(cfg);") < main.index("const bool psram_ok = logger.begin();")
assert main.index("const bool psram_ok = logger.begin();") < main.index("const bool imu_ok = imu.begin();")
assert main.index("const bool imu_ok = imu.begin();") < main.index("const bool camera_ok = camera_probe.begin();")
assert main.index("const bool camera_ok = camera_probe.begin();") < main.index("const bool roller_ok = roller.begin();")

# SCCB must never touch BMI270 I2C1 and runtime capture never uses SCCB.
assert "I2C_NUM_1" not in camera
assert "i2c_param_config(I2C_NUM_0, &sccb)" in camera
assert "c.sccb_i2c_port = I2C_NUM_0;" in camera
assert "i2c_driver_delete(I2C_NUM_0)" in camera

# True one-shot contract:
#  idle -> XCLK ON -> receiver ON -> exactly one frame -> receiver OFF ->
#  XCLK OFF -> caller, followed by a guaranteed post-capture quiet interval.
for token in (
    'extern "C" void cam_stop(void);',
    'extern "C" void cam_start(void);',
    'extern "C" camera_fb_t* cam_take(TickType_t timeout);',
    "CAM_CLK_IDX",
    "SIG_GPIO_OUT_IDX",
    "gpio_matrix_out(PIN_CAM_XCLK, CAM_CLK_IDX",
    "gpio_matrix_out(PIN_CAM_XCLK, SIG_GPIO_OUT_IDX",
    "setXclkEnabled(true);",
    "cam_start();",
    "camera_fb_t* fb = cam_take(",
    "cam_stop();",
    "setXclkEnabled(false);",
    "kMinimumIdleMs = 300",
    "vTaskDelay(pdMS_TO_TICKS(kMinimumIdleMs))",
):
    assert token in camera, token

capture = camera[camera.index("camera_fb_t* OneShotCamera::acquire") :
                 camera.index("void OneShotCamera::release")]
assert capture.index("setXclkEnabled(true);") < capture.index("cam_start();")
assert capture.index("cam_start();") < capture.index("cam_take(")
assert capture.index("cam_take(") < capture.index("cam_stop();")
assert capture.index("cam_stop();") < capture.index("setXclkEnabled(false);")
assert "esp_camera_fb_get()" not in camera

# One-shot idle does not power-cycle the sensor and never re-enters SCCB.
assert "digitalWrite(PIN_CAM_POWER_N, HIGH);" in camera  # failure cleanup only
task = camera[camera.index("void OneShotCamera::taskLoop()"):]
assert "PIN_CAM_POWER_N" not in task
assert "I2C_NUM_0" not in task

# No foot-angle/marker/control coupling yet.
for token in ("foot_angle", "right_foot", "left_foot", "marker", "centroid", "deg_per_px"):
    assert token not in camera.lower(), token

# cam_task remains below Roller/Wi-Fi during the short active capture window.
assert '--wrap=xTaskCreatePinnedToCore' in pio
assert 'strcmp(pcName, "cam_task") == 0' in patch
assert 'kCameraInternalTaskPriority = 3' in patch
assert 'static_assert(kCameraInternalTaskPriority < Config::ROLLER_IO_TASK_PRIORITY' in patch

# Diagnostics prove the idle state on hardware.
assert 'server.on("/camera-health", HTTP_GET' in main
assert 'oneshot=%u receiver_active=%u xclk_active=%u' in main
assert 'capture_us=%lu max_capture_us=%lu' in main

print("PASS: true one-shot camera acquisition with XCLK and receiver gating")
