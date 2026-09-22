#include <Arduino.h>
#include <M5Unified.h>
#include <WebServer.h>

#include "config.h"
#include "experiment_runner.h"
#include "imu_manager.h"
#include "psram_logger.h"
#include "roller485_manager.h"
#include "upright_pose_guide.h"
#include "web_ui.h"
#include "run_control_worker.h"

WebServer server(Config::HTTP_PORT);
PsramLogger logger;
ImuManager imu;
Roller485Manager roller;
ExperimentRunner runner;
WebUi web;
RunControlWorker run_control;

static bool runControlStep(void*);
static void captureRunState(void*, RunControlSnapshot&);

static constexpr UBaseType_t kConsumerPriority = 2;
static_assert(kConsumerPriority < RunControlWorker::kPriority, "Run control must preempt HTTP");
static_assert(RunControlWorker::kPriority < 6, "BMI270 reader must preempt run control");

static uint32_t startup_guide_boot_ms = 0;
static uint32_t startup_upright_since_ms = 0;
static bool startup_guide_prompt_announced = false;
static bool startup_upright_confirmed = false;

static void displayLine(const char* line1, const char* line2 = "") {
  if (!M5.Display.width()) return;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(0, 4);
  M5.Display.println(line1);
  if (line2 && line2[0]) M5.Display.println(line2);
}

static void updateStartupPoseGuide() {
  // Once a measurement starts, ExperimentRunner owns the LED completely so the
  // existing START/MID/END video synchronization pattern is never modified.
  if (startup_upright_confirmed || runner.running()) return;

  const uint32_t now_ms = millis();
  if (static_cast<uint32_t>(now_ms - startup_guide_boot_ms) <
      UprightPoseGuide::GUIDE_LED_ON_AFTER_BOOT_MS) {
    return;
  }
  if (!startup_guide_prompt_announced) {
    startup_guide_prompt_announced = true;
    Serial.println("Startup guide: 10 s elapsed; LED ON until upright pose is stable");
    displayLine("Stand upright", "LED ON until stable");
  }
  digitalWrite(Config::SYNC_LED_PIN, HIGH);
  const ImuReading& r = imu.reading();
  const bool fresh = imu.ok() && r.last_gyro_update_us != 0 &&
      static_cast<uint32_t>(micros() - r.last_gyro_update_us) <= 10000UL;
  const char* reason = "stable_hold";
  if (!imu.acquisitionHealthy()) reason = "imu_init_or_latched_fault";
  else if (!fresh) reason = "waiting_fresh_imu";
  else if (UprightPoseGuide::accelNormG(r) < UprightPoseGuide::UPRIGHT_MIN_ACCEL_NORM_G ||
           UprightPoseGuide::accelNormG(r) > UprightPoseGuide::UPRIGHT_MAX_ACCEL_NORM_G) reason = "accel_norm_out_of_range";
  else if (UprightPoseGuide::directionErrorDeg(r) > UprightPoseGuide::UPRIGHT_MAX_DIRECTION_ERROR_DEG) reason = "not_upright";
  else if (UprightPoseGuide::gyroNormDps(r) > UprightPoseGuide::UPRIGHT_MAX_GYRO_NORM_DPS) reason = "still_moving";
  imu.setStartupGuideState(reason, false, 0);
  if (!fresh || !UprightPoseGuide::isUprightStableSample(r)) {
    startup_upright_since_ms = 0;
    return;
  }
  if (startup_upright_since_ms == 0) startup_upright_since_ms = now_ms;
  imu.setStartupGuideState("stable_hold", false, now_ms - startup_upright_since_ms);
  if (static_cast<uint32_t>(now_ms - startup_upright_since_ms) <
      UprightPoseGuide::UPRIGHT_STABLE_HOLD_MS) {
    return;
  }
  startup_upright_confirmed = true;
  imu.setStartupGuideState("upright_ready", true, now_ms - startup_upright_since_ms);
  digitalWrite(Config::SYNC_LED_PIN, LOW);
  Serial.printf("Startup guide: upright confirmed; gravity error=%.2f deg, norm=%.3f g\n",
                UprightPoseGuide::directionErrorDeg(r), UprightPoseGuide::accelNormG(r));
  displayLine("Upright ready", "Start from Web UI");
}

void setup() {
  startup_guide_boot_ms = millis();
  // Reader priority 6 remains above this thread; system service priorities stay unchanged.
  vTaskPrioritySet(nullptr, kConsumerPriority);
  Serial.begin(Config::SERIAL_BAUD);
  delay(300);
  Serial.println();
  // V46l is the frozen controller/attitude baseline, not the acquisition revision.
  Serial.println("AtomS3R V46l MEKF dual-core motor validation");
  Serial.println("V46q acquisition 0.46.16: priority BMI270 task / timestamped queue");
  Serial.printf("IMU consumer: core=%d priority=%u; BMI270 reader core=1 priority=6\n",
                xPortGetCoreID(), static_cast<unsigned>(uxTaskPriorityGet(nullptr)));

  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  cfg.internal_imu = false;  // ImuManager initializes once, with bounded cold-start validation/retries.
  M5.begin(cfg);
  Serial.printf("V46l identity: board=%d imu_type=%d M5Unified=%s M5GFX=%s AHRS=%s base=%s attitude=%s\n",
                static_cast<int>(M5.getBoard()), static_cast<int>(M5.Imu.getType()),
                Config::RESOLVED_M5UNIFIED_VERSION, Config::RESOLVED_M5GFX_VERSION,
                Config::RESOLVED_ADAFRUIT_AHRS_VERSION, Config::V62_BASE_COMMIT,
                Config::ATTITUDE_VALIDATION_REVISION);
  displayLine("V46q IMU", "DUAL-CORE V7");

  const bool psram_ok = logger.begin();
  Serial.printf("PSRAM: %s total=%u free=%u sample_capacity=%u\n", psram_ok ? "OK" : "FAILED",
                static_cast<unsigned>(logger.psramTotal()), static_cast<unsigned>(logger.psramFree()),
                static_cast<unsigned>(logger.sampleCapacity()));
  if (!psram_ok) Serial.printf("PSRAM error: %s\n", logger.lastError());

  const bool imu_ok = imu.begin();
  Serial.printf("IMU acquisition: %s internal_i2c=%d SDA=%d SCL=%d error=%s\n",
                imu_ok ? "OK" : "FAILED", static_cast<int>(M5.In_I2C.getPort()),
                M5.In_I2C.getSDA(), M5.In_I2C.getSCL(), imu.lastError());

  const bool roller_ok = roller.begin();
  const bool roller_task_ok = roller_ok && roller.startIoTask(
      Config::ROLLER_IO_TASK_CORE, Config::ROLLER_IO_TASK_PRIORITY,
      Config::ROLLER_IO_TASK_STACK_BYTES);
  Serial.printf("Roller485: %s task_ready=%s core=%u priority=%u\n",
                roller_ok ? "OK" : "FAILED", roller_task_ok ? "OK" : "FAILED",
                Config::ROLLER_IO_TASK_CORE, Config::ROLLER_IO_TASK_PRIORITY);

  runner.begin(logger, imu, roller);
  const bool control_task_ok = run_control.begin(runControlStep, captureRunState, nullptr);
  Serial.printf("Run control worker: %s core=1 priority=4; HTTP core=1 priority=2\n",
                control_task_ok ? "OK" : "FAILED");
  web.begin(server, runner, imu, roller, logger);
  Serial.printf("AP SSID: %s\n", Config::AP_SSID);
  Serial.println("Open http://192.168.4.1/ and start Autonomous Energy Control V7");
  displayLine("V46q / V7 ready", Config::AP_SSID);
}

static void updateAcquisitionContext() {
  imu.setAcquisitionContext(runner.running(),
      runner.status().state == ExperimentState::RUNNING_BATCH_SWEEP,
      static_cast<uint8_t>(runner.status().state));
}
static void checkAcquisitionHealth() {
  // Added fail-closed condition; the established start and motor gates remain.
  if (runner.running() && (!imu.acquisitionHealthy() || imu.stale(millis()))) {
    runner.requestEmergencyStop("imu_acquisition_overflow_backlog_or_stale");
  }
}

static void captureRunState(void*, RunControlSnapshot& out) {
  const auto& st = runner.status();
  out.running = runner.running();
  out.state_id = static_cast<uint8_t>(st.state);
  out.run_id = st.run_id;
  out.motor_cmd_mA = st.motor_cmd_mA;
  out.actual_current_mA = st.roller_actual_current_mA;
  out.remaining_ms = st.remaining_ms;
  snprintf(out.state_name, sizeof(out.state_name), "%s", runner.stateName());
  snprintf(out.last_error, sizeof(out.last_error), "%s", st.last_error ? st.last_error : "");
}

static bool runControlStep(void*) {
  const uint32_t loop_start_us = micros();
  updateAcquisitionContext();
  if (run_control.takeStopRequest()) {
    runner.requestEmergencyStop("web_estop");
    updateAcquisitionContext();
    run_control.recordStep(loop_start_us, 0, 0, static_cast<uint32_t>(micros() - loop_start_us));
    return false;
  }
  runner.serviceFast();
  runner.updateImuDynamicBetaContext();
  const bool v46k_timing_probe_active = runner.energyControlAutonomousMode() && runner.running();
  const uint32_t imu_t0_us = micros();
  imu.update();
  checkAcquisitionHealth();
  const uint32_t imu_update_us = static_cast<uint32_t>(micros() - imu_t0_us);
  const bool timing_measurement = runner.status().state == ExperimentState::RUNNING_BATCH_SWEEP;
  const bool timing_fresh = imu.reading().gyro_fresh;
  const uint32_t timing_sample_us = imu.reading().last_gyro_update_us;
  const uint32_t runner_t0_us = micros();
  runner.update();
  const uint32_t runner_update_us = static_cast<uint32_t>(micros() - runner_t0_us);
  const uint32_t timing_done_us = micros();
  const uint32_t path_us = static_cast<uint32_t>(timing_done_us - loop_start_us);
  run_control.recordSampleCompletion(timing_measurement, timing_fresh, timing_sample_us,
                                    timing_done_us, runner_update_us);
  if (v46k_timing_probe_active) runner.recordTimingProbeLoop(imu_update_us, runner_update_us, path_us);
  runner.setLoopDt(path_us);
  updateAcquisitionContext();
  run_control.recordStep(loop_start_us, imu_update_us, runner_update_us, path_us);
  return runner.running();
}

void loop() {
  // While a run is active, this lower-priority Arduino task owns only HTTP.
  // Never put a mutex around handleClient and the controller: that would
  // reintroduce network waits into the IMU-consumer deadline.
  if (run_control.active()) {
    web.update();
    delay(1);
    return;
  }

  // Idle ownership is exclusive again after the worker's final snapshot.
  const uint32_t loop_start_us = micros();
  updateAcquisitionContext();
  runner.serviceFast();
  runner.updateImuDynamicBetaContext();
  imu.update();
  checkAcquisitionHealth();
  runner.update();
  if (!runner.running()) {
    M5.update();
    updateStartupPoseGuide();
  }
  runner.setLoopDt(static_cast<uint32_t>(micros() - loop_start_us));
  web.update();

  // Establish the V46p boundary after the Start HTTP response. Then transfer
  // ownership exactly once; never touch the live controller after start().
  if (runner.running()) {
    updateAcquisitionContext();
    if (!run_control.start()) {
      runner.requestEmergencyStop("run_control_worker_not_ready");
      updateAcquisitionContext();
    }
  }
  taskYIELD();
}
