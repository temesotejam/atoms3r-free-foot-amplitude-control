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

# Phase 1 must not modify the controller, Web UI, logger, IMU implementation,
# or Roller implementation. Camera is an isolated observer only.
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

# Keep the proven boot skeleton. Camera is inserted only after BMI270 startup
# and before Roller485 takes I2C0.
assert main.index("M5.begin(cfg);") < main.index("const bool psram_ok = logger.begin();")
assert main.index("const bool psram_ok = logger.begin();") < main.index("const bool imu_ok = imu.begin();")
assert main.index("const bool imu_ok = imu.begin();") < main.index("const bool camera_ok = camera_probe.begin();")
assert main.index("const bool camera_ok = camera_probe.begin();") < main.index("const bool roller_ok = roller.begin();")
assert main.index("const bool roller_ok = roller.begin();") < main.index("web.begin(server, runner, imu, roller, logger);")

# SCCB must never touch BMI270 I2C1. It temporarily owns I2C0 and releases it
# before Roller starts.
assert "I2C_NUM_1" not in camera
assert "i2c_param_config(I2C_NUM_0, &sccb)" in camera
assert "i2c_driver_install(I2C_NUM_0" in camera
assert "c.sccb_i2c_port = I2C_NUM_0;" in camera
assert "c.pin_sccb_sda = -1;" in camera
assert "c.pin_sccb_scl = -1;" in camera
assert "i2c_driver_delete(I2C_NUM_0)" in camera

# Phase 1F is the coexistence fix candidate: GC0308 remains powered and
# configured, while cam_start/cam_stop gate receiver activity to one frame per
# 200 ms. SCCB is never used again after Roller485 starts.
assert "kCameraXclkHz = 16000000UL" in camera
assert "PIXFORMAT_GRAYSCALE" in camera
assert "FRAMESIZE_QVGA" in camera
assert "c.fb_count = 1;" in camera
assert "CAMERA_FB_IN_PSRAM" in camera
assert "kTargetCaptureHz = 5" in camera
assert "kCapturePeriodMs = 200" in camera
assert 'extern "C" void cam_stop(void);' in camera
assert 'extern "C" void cam_start(void);' in camera
assert "cam_stop();" in camera
assert "cam_start();" in camera
assert camera.index("cam_stop();", camera.index("camera_fb_t* fb = esp_camera_fb_get();")) < camera.index("esp_camera_fb_return(fb)")
assert "snapshot_.receiver_gated = true;" in camera
assert "snapshot_.sensor_powered = true;" in camera
assert "xTaskCreatePinnedToCore(" in camera
assert '"camera_gate"' in camera

# Absolutely no foot-angle/marker/control coupling in Phase 1.
for token in ("foot_angle", "right_foot", "left_foot", "marker", "centroid", "deg_per_px"):
    assert token not in camera.lower(), token

# Memory pressure is observable at boot.
for token in ("MALLOC_CAP_INTERNAL", "MALLOC_CAP_DMA", "MALLOC_CAP_SPIRAM",
              "internal_free_before", "internal_free_after",
              "dma_free_before", "dma_free_after",
              "psram_free_before", "psram_free_after"):
    assert token in camera + header, token

print("PASS: isolated camera coexistence Phase 1")


# The precompiled esp32-camera cam_task normally starts at configMAX_PRIORITIES-2
# on Core 0. Linker wrapping changes only the task named "cam_task" to Priority 3.
assert '--wrap=xTaskCreatePinnedToCore' in pio
assert '__wrap_xTaskCreatePinnedToCore' in patch
assert '__real_xTaskCreatePinnedToCore' in patch
assert 'strcmp(pcName, "cam_task") == 0' in patch
assert 'kCameraInternalTaskPriority = 3' in patch
assert 'effective_priority = kCameraInternalTaskPriority' in patch
assert 'return __real_xTaskCreatePinnedToCore' in patch
assert 'static_assert(kCameraInternalTaskPriority < Config::ROLLER_IO_TASK_PRIORITY' in patch

# Minimal HTTP path is independent of WebUi internals.
assert 'server.on("/camera-health", HTTP_GET' in main
assert 'cam_task_priority=%u->%u' in main
assert 'heap_caps_get_free_size(MALLOC_CAP_INTERNAL)' in main
assert 'driver_active=%u sensor_powered=%u receiver_gated=%u receiver_active=%u' in main

print("PASS: camera cam_task priority isolation and minimal HTTP probe")
