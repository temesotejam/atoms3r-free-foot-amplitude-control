#include <Arduino.h>
#include <M5Unified.h>
#include "esp_timer.h"
#include "config.h"
#include "camera_coexistence.h"
#include "bounded_web_server.h"
#include "experiment_runner.h"
#include "imu_manager.h"
#include "psram_logger.h"
#include "roller485_manager.h"
#include "upright_pose_guide.h"
#include "web_ui.h"
#include "run_control_worker.h"
#include "foot_observer.h"
#include "immutable_export.h"

BoundedWriteWebServer server(Config::HTTP_PORT);
OneShotCamera camera_probe;
PsramLogger logger;
ImuManager imu;
Roller485Manager roller;
ExperimentRunner runner;
RunControlWorker run_control;
FootObserver feet;
ImmutableExport log_export;
WebUi web;

static uint32_t boot_ms = 0, upright_epoch = 0;
static uint64_t upright_since_us = 0, log_epoch_us = 0, measurement_epoch_us = 0;
static bool upright_stable = false;
static void updateAcquisitionContext() {
  imu.setAcquisitionContext(runner.running(),
      runner.status().state == ExperimentState::RUNNING_BATCH_SWEEP,
      static_cast<uint8_t>(runner.status().state));
}
static void captureRunState(void*, RunControlSnapshot& out) {
  const auto& st = runner.status();
  const auto& r = imu.reading();
  out.running = runner.running(); out.state_id = static_cast<uint8_t>(st.state);
  out.run_id = st.run_id; out.energy_control_autonomous = runner.energyControlAutonomousMode();
  out.ready = st.state == ExperimentState::READY_TO_MEASURE;
  out.imu_ok = imu.acquisitionHealthy() && !imu.stale(millis()); out.roller_ok = roller.ok();
  out.downloadable = logger.rwlogDownloadable(); out.sample_count = logger.sampleCount();
  out.heartbeat_us = micros(); out.imu_sample_us = r.last_gyro_update_us;
  out.pulse_active = st.pulse_active; out.measure_elapsed_ms = st.measure_elapsed_ms;
  out.motor_cmd_mA = st.motor_cmd_mA; out.actual_current_mA = st.roller_actual_current_mA;
  out.remaining_ms = st.remaining_ms; out.battery_mV = st.roller_battery_mV;
  out.pitch_deg = st.pitch_mekf_deg; out.rate_dps = st.physical_roll_rate_dps;
  out.target_deg = runner.energyControlAutonomousTargetPeakDeg();
  out.led_state = st.led_state; out.sync_event_id = st.sync_event_id;
  out.upright_stable = upright_stable; out.upright_epoch = upright_epoch;
  out.upright_since_us = upright_since_us;
  out.log_epoch_us = log_epoch_us; out.measurement_epoch_us = measurement_epoch_us;
  out.upright_error_deg = UprightPoseGuide::directionErrorDeg(r);
  out.accel_norm_g = UprightPoseGuide::accelNormG(r); out.gyro_norm_dps = UprightPoseGuide::gyroNormDps(r);
  snprintf(out.state_name, sizeof(out.state_name), "%s", runner.stateName());
  snprintf(out.last_error, sizeof(out.last_error), "%s", st.last_error ? st.last_error : "");
}
static bool controlStep(void*) {
  const uint32_t start = micros();
  const bool was_running = runner.running();
  if (run_control.takeStopRequest()) runner.requestEmergencyStop("web_estop");
  updateAcquisitionContext();
  runner.serviceFast(); runner.updateImuDynamicBetaContext();
  const uint32_t imu_start = micros(); imu.update();
  const uint32_t imu_us = micros() - imu_start;
  if (runner.running() && (!imu.acquisitionHealthy() || imu.stale(millis())))
    runner.requestEmergencyStop("imu_acquisition_overflow_backlog_or_stale");
  const auto& r = imu.reading();
  const float norm = UprightPoseGuide::accelNormG(r);
  const bool stable = millis() - boot_ms >= 10000 && imu.acquisitionHealthy() && r.last_gyro_update_us &&
      static_cast<uint32_t>(micros() - r.last_gyro_update_us) <= 10000 &&
      isfinite(norm) && fabsf(norm - 1.0f) <= appcfg::kAutoZeroAccelNormToleranceG &&
      UprightPoseGuide::directionErrorDeg(r) <= appcfg::kAutoZeroMaxUprightErrorDeg &&
      UprightPoseGuide::gyroNormDps(r) <= appcfg::kAutoZeroMaxGyroDps;
  if (stable != upright_stable) { ++upright_epoch; upright_since_us = stable ? esp_timer_get_time() : 0; }
  upright_stable = stable;
  const auto command = run_control.takeCommand();
  if (command == RunControlWorker::Command::Start) {
    bool ok = false;
    const char* error = "clear_previous_run_first";
    if (runner.status().state == ExperimentState::READY_TO_MEASURE) {
      if (!log_export.ready()) error = "export_worker_unavailable";
      else if (!imu.acquisitionHealthy() || imu.stale(millis())) error = "imu_not_healthy";
      else if (!feet.readyToStart()) error = "foot_camera_and_upright_zero_required";
      else {
        ok = runner.startEnergyControlAutonomousCapture();
        error = ok ? "started" : runner.status().last_error;
        if (ok) {
          log_epoch_us = esp_timer_get_time() - static_cast<uint32_t>(micros() - logger.runStartUs());
          measurement_epoch_us = 0;
          feet.beginRun(runner.status().run_id, log_epoch_us);
          run_control.beginRunAudit();
        }
      }
    }
    run_control.completeCommand(ok, error);
  } else if (command == RunControlWorker::Command::Clear) {
    runner.clearFinishedOrEstop(); feet.clearRun(); log_epoch_us = measurement_epoch_us = 0;
    run_control.completeCommand(true, "cleared");
  }
  const bool measurement = runner.status().state == ExperimentState::RUNNING_BATCH_SWEEP;
  const bool fresh = r.gyro_fresh;
  const uint32_t sample_us = r.last_gyro_update_us;
  const uint32_t runner_start = micros(); runner.update();
  const uint32_t runner_us = micros() - runner_start, done = micros();
  if (!measurement_epoch_us && runner.status().state == ExperimentState::RUNNING_BATCH_SWEEP)
    measurement_epoch_us = esp_timer_get_time() - static_cast<uint64_t>(runner.status().measure_elapsed_ms) * 1000;
  if (was_running || runner.running()) {
    run_control.recordSampleCompletion(measurement, fresh, sample_us, done, runner_us);
    runner.recordTimingProbeLoop(imu_us, runner_us, done - start);
    run_control.recordStep(start, imu_us, runner_us, done - start);
  }
  runner.setLoopDt(done - start);
  if (was_running && !runner.running()) { runner.sealCompletedLog(); feet.finishRun(); }
  // The existing LED sync pattern has exclusive authority during every run.
  if (!runner.running()) {
    const auto foot = feet.snapshot();
    const bool prompt = millis() - boot_ms >= 10000 && !foot.zero_ready;
    digitalWrite(Config::SYNC_LED_PIN, prompt ? HIGH : LOW);
    imu.setStartupGuideState(foot.zero_ready ? "upright_and_foot_ready" :
        (stable ? "hold_upright_for_foot_zero" : "stand_upright"), foot.zero_ready,
        stable ? (esp_timer_get_time() - upright_since_us) / 1000 : 0);
  }
  updateAcquisitionContext();
  return runner.running();
}
void setup() {
  boot_ms = millis();
  vTaskPrioritySet(nullptr, 2);
  Serial.begin(Config::SERIAL_BAUD);
  auto cfg = M5.config(); cfg.serial_baudrate = 0; cfg.internal_imu = false; M5.begin(cfg);
  logger.begin(); imu.begin();
  // Camera SCCB borrows I2C0 only during boot, before Roller485 owns this port.
  camera_probe.begin();
  if (roller.begin()) roller.startIoTask(Config::ROLLER_IO_TASK_CORE,
      Config::ROLLER_IO_TASK_PRIORITY, Config::ROLLER_IO_TASK_STACK_BYTES);
  runner.begin(logger, imu, roller);
  run_control.begin(controlStep, captureRunState, nullptr);
  feet.begin(camera_probe, run_control);
  log_export.begin(logger);
  web.begin(server, run_control, feet, log_export);
}
void loop() { web.update(); delay(1); }
