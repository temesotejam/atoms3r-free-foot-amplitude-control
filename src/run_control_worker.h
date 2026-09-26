#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include "timing_deadline.h"
#include "runtime_diagnostics.h"
#include "mekf_attitude_diagnostics.h"
#include "control_work_profile.h"
#include "control_latency.h"

// One permanent controller owner, including idle and calibration. HTTP sends
// commands and consumes POD snapshots; it never calls the live runner/IMU.
struct RunControlSnapshot {
  bool ready = false, imu_ok = false, roller_ok = false, downloadable = false;
  bool upright_stable = false;
  uint32_t upright_epoch = 0, heartbeat_us = 0, sample_count = 0;
  uint64_t log_epoch_us = 0, measurement_epoch_us = 0, upright_since_us = 0;
  uint16_t battery_mV = 0;
  uint8_t led_state = 0, sync_event_id = 0;
  float pitch_deg = 0, rate_dps = 0, target_deg = 0;
  MekfAttitudeSnapshot mekf_attitude;
  float upright_error_deg = 180, accel_norm_g = 0, gyro_norm_dps = 0;
  uint32_t imu_sample_us = 0;
  bool running = false;
  uint8_t state_id = 0;
  uint16_t run_id = 0;
  bool energy_control_autonomous = false;
  bool pulse_active = false;
  uint32_t measure_elapsed_ms = 0;
  int16_t motor_cmd_mA = 0;
  int16_t actual_current_mA = 0;
  uint32_t remaining_ms = 0;
  char state_name[32] = {};
  char last_error[80] = {};
};

class RunControlWorker {
 public:
  static constexpr uint8_t kCore = 1;
  static constexpr uint8_t kPriority = 4;
  enum class Command : uint8_t { None, Start, Clear };
  struct CommandState {
    uint32_t submitted = 0, completed = 0;
    bool pending = false, ok = false;
    char result[80] = {};
  };
  using Step = bool (*)(void*);
  using Capture = void (*)(void*, RunControlSnapshot&);
  struct Timing {
    uint32_t offset_us = 0, period_us = 0, consume_us = 0, runner_us = 0, path_us = 0;
  };
  struct Audit {
    uint32_t epoch_us = 0, steps = 0, max_period_us = 0;
    uint32_t max_consume_us = 0, max_runner_us = 0, max_path_us = 0;
    uint32_t stop_requests = 0, stop_consumed = 0;
    int32_t observed_core = -1;
    uint32_t observed_priority = 0;
    timing_deadline::Counter sample_completion, runner_work;
    // Same completion endpoint and budget as the total, partitioned by inputs
    // captured before runner.update(). Exactly one cohort per fresh gyro.
    timing_deadline::Counter input_cohorts[2][2]; // pulse, fresh accel
    Timing recent[16] = {};
  };
  // Small lock-bounded view used by Phase 1N. It intentionally exposes only
  // deadline counters; no controller state is read from the HTTP task.
  struct Health {
    uint32_t steps = 0;
    uint32_t max_period_us = 0;
    uint32_t max_path_us = 0;
    uint32_t sample_deadline_over = 0;
    uint32_t sample_deadline_max_us = 0;
    uint32_t runner_deadline_over = 0;
    uint32_t runner_deadline_max_us = 0;
  };

  bool begin(Step step, Capture capture, void* context) {
    if (task_ || !step || !capture) return false;
    step_ = step; capture_ = capture; context_ = context;
    return xTaskCreatePinnedToCore(&RunControlWorker::entry, "run_control", 16384,
        this, kPriority, &task_, kCore) == pdPASS;
  }
  bool ready() const { return task_ != nullptr; }
  bool active() const {
    portENTER_CRITICAL(&mux_);
    const bool result = active_;
    portEXIT_CRITICAL(&mux_);
    return result;
  }
  bool request(Command command) {
    if (!ready() || command == Command::None) return false;
    portENTER_CRITICAL(&mux_);
    const bool accepted = !command_state_.pending && !snapshot_.running && !stop_requested_;
    if (accepted) {
      command_ = command; command_state_.pending = true; ++command_state_.submitted;
    }
    portEXIT_CRITICAL(&mux_);
    return accepted;
  }
  Command takeCommand() {
    portENTER_CRITICAL(&mux_);
    const Command command = stop_requested_ ? Command::None : command_;
    if (command != Command::None) command_ = Command::None;
    portEXIT_CRITICAL(&mux_);
    return command;
  }
  void completeCommand(bool ok, const char* result) {
    portENTER_CRITICAL(&mux_);
    command_state_.completed = command_state_.submitted;
    command_state_.pending = false; command_state_.ok = ok;
    snprintf(command_state_.result, sizeof(command_state_.result), "%s", result ? result : "");
    portEXIT_CRITICAL(&mux_);
  }
  CommandState commandState() const {
    portENTER_CRITICAL(&mux_);
    const auto copy = command_state_;
    portEXIT_CRITICAL(&mux_);
    return copy;
  }
  void beginRunAudit(uint32_t epoch_us = 0) {
    // Owner only. The previous run has already been released by HTTP.
    control_work::profile.reset();
    control_latency::setActive(false);
    control_latency::profile.reset();
    portENTER_CRITICAL(&mux_);
    audit_ = Audit{}; audit_.epoch_us = epoch_us ? epoch_us : micros();
    last_step_start_us_ = audit_.epoch_us;
    portEXIT_CRITICAL(&mux_);
  }
  RunControlSnapshot snapshot() const {
    portENTER_CRITICAL(&mux_);
    const RunControlSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&mux_);
    return copy;
  }
  Health healthSnapshot() const {
    Health h;
    portENTER_CRITICAL(&mux_);
    h.steps = audit_.steps;
    h.max_period_us = audit_.max_period_us;
    h.max_path_us = audit_.max_path_us;
    h.sample_deadline_over = audit_.sample_completion.over;
    h.sample_deadline_max_us = audit_.sample_completion.maximum;
    h.runner_deadline_over = audit_.runner_work.over;
    h.runner_deadline_max_us = audit_.runner_work.maximum;
    portEXIT_CRITICAL(&mux_);
    return h;
  }
  bool requestStop() {
    portENTER_CRITICAL(&mux_);
    const bool accepted = ready();
    if (accepted) { stop_requested_ = true; ++audit_.stop_requests; }
    portEXIT_CRITICAL(&mux_);
    return accepted;
  }
  // Called by the exclusive controller owner before each acquisition delivery.
  bool takeStopRequest() {
    portENTER_CRITICAL(&mux_);
    const bool requested = stop_requested_;
    stop_requested_ = false;
    if (requested) {
      ++audit_.stop_consumed;
      command_ = Command::None;
      command_state_.pending = false;
      command_state_.completed = command_state_.submitted;
      command_state_.ok = false;
      snprintf(command_state_.result, sizeof(command_state_.result), "%s", "stopped");
    }
    portEXIT_CRITICAL(&mux_);
    return requested;
  }
  void recordStep(uint32_t start_us, uint32_t consume_us,
                  uint32_t runner_us, uint32_t path_us) {
    Timing t{};
    t.offset_us = start_us - audit_.epoch_us;
    t.period_us = start_us - last_step_start_us_;
    t.consume_us = consume_us; t.runner_us = runner_us; t.path_us = path_us;
    last_step_start_us_ = start_us;
    const int core = xPortGetCoreID();
    const uint32_t priority = uxTaskPriorityGet(nullptr);
    portENTER_CRITICAL(&mux_);
    if (t.period_us > audit_.max_period_us) audit_.max_period_us = t.period_us;
    if (consume_us > audit_.max_consume_us) audit_.max_consume_us = consume_us;
    if (runner_us > audit_.max_runner_us) audit_.max_runner_us = runner_us;
    if (path_us > audit_.max_path_us) audit_.max_path_us = path_us;
    audit_.recent[audit_.steps % 16] = t;
    ++audit_.steps;
    audit_.observed_core = core; audit_.observed_priority = priority;
    portEXIT_CRITICAL(&mux_);
  }
  void recordSampleCompletion(bool measurement, bool fresh, uint32_t sample_us,
                              uint32_t done_us, uint32_t runner_us,
                              bool pulse_at_entry, bool accel_fresh) {
    if (!measurement || !fresh) return;
    portENTER_CRITICAL(&mux_);
    audit_.sample_completion.add(static_cast<uint32_t>(done_us - sample_us), 2500);
    audit_.runner_work.add(runner_us, 2500);
    audit_.input_cohorts[pulse_at_entry ? 1 : 0][accel_fresh ? 1 : 0].add(
        static_cast<uint32_t>(done_us - sample_us), 2500);
    portEXIT_CRITICAL(&mux_);
  }
  String diagnosticsJson() const {
    Audit a;
    portENTER_CRITICAL(&mux_);
    a = audit_;
    portEXIT_CRITICAL(&mux_);
    String json;
    json.reserve(3000);
    json = "{\"revision\":\"permanent_control_owner_20260923\"";
    json += ",\"core\":" + String(a.observed_core);
    json += ",\"priority\":" + String(a.observed_priority);
    json += ",\"steps\":" + String(a.steps);
    json += ",\"max_period_us\":" + String(a.max_period_us);
    json += ",\"max_consume_us\":" + String(a.max_consume_us);
    json += ",\"max_runner_us\":" + String(a.max_runner_us);
    json += ",\"max_path_us\":" + String(a.max_path_us);
    json += ",\"stop_requests\":" + String(a.stop_requests);
    json += ",\"stop_consumed\":" + String(a.stop_consumed);
    json += ",\"v46u_deadline\":{\"budget_us\":2500,\"count\":" + String(a.sample_completion.count);
    json += ",\"over_budget\":" + String(a.sample_completion.over);
    json += ",\"max_us\":" + String(a.sample_completion.maximum);
    json += ",\"mean_us\":" + String(a.sample_completion.count ?
        static_cast<double>(a.sample_completion.sum) / a.sample_completion.count : 0.0, 3);
    json += ",\"runner_over_budget\":" + String(a.runner_work.over);
    json += ",\"runner_max_us\":" + String(a.runner_work.maximum);
    json += ",\"runner_mean_us\":" + String(a.runner_work.count ?
        static_cast<double>(a.runner_work.sum) / a.runner_work.count : 0.0, 3);
    json += ",\"all_observed_within_budget\":" + String(a.sample_completion.passed() ? "true" : "false");
    json += ",\"scope\":\"RUNNING_fresh_gyro_only;host_acquisition_to_runner_return;not_sensor_capture_to_motor_apply\"}";
    json += ",\"deadline_input_cohorts\":{\"revision\":\"input_partition_04712\"";
    json += ",\"classification\":\"pulse_active_before_runner;accel_fresh_at_delivery\",\"budget_us\":2500,\"groups\":[";
    for (uint8_t pulse = 0; pulse < 2; ++pulse) {
      for (uint8_t accel = 0; accel < 2; ++accel) {
        const auto& c = a.input_cohorts[pulse][accel];
        if (pulse || accel) json += ",";
        json += "{\"pulse_active\":" + String(pulse ? "true" : "false");
        json += ",\"accel_fresh\":" + String(accel ? "true" : "false");
        json += ",\"count\":" + String(c.count) + ",\"over_budget\":" + String(c.over);
        json += ",\"max_us\":" + String(c.maximum);
        json += ",\"mean_us\":" + String(c.count ? static_cast<double>(c.sum) / c.count : 0.0, 3) + "}";
      }
    }
    json += "]}";
    json += ",\"recent_steps\":[";
    const uint32_t count = a.steps < 16 ? a.steps : 16;
    for (uint32_t i = 0; i < count; ++i) {
      const Timing& t = a.recent[(a.steps - count + i) % 16];
      if (i) json += ",";
      json += "{\"offset_us\":" + String(t.offset_us);
      json += ",\"period_us\":" + String(t.period_us);
      json += ",\"consume_us\":" + String(t.consume_us);
      json += ",\"runner_us\":" + String(t.runner_us);
      json += ",\"path_us\":" + String(t.path_us) + "}";
    }
    return json + "]}";
  }

 private:
  static void entry(void* arg) { static_cast<RunControlWorker*>(arg)->loop(); }
  void oneStep() {
    const bool measurement = snapshot_.state_id == 3;
    const bool pulse = snapshot_.pulse_active;
    control_work::Scope owner_work(control_work::Stage::Owner, measurement, pulse);
    step_(context_);
    RunControlSnapshot next{};
    RuntimeDiag::phase(RuntimeDiag::Lane::Control, RuntimeDiag::Phase::Snapshot);
    {
      control_work::Scope snapshot_work(control_work::Stage::Snapshot, measurement, pulse);
      capture_(context_, next);
    }
    control_work::Scope publish_work(control_work::Stage::Publish, measurement, pulse);
    RuntimeDiag::phase(RuntimeDiag::Lane::Control, RuntimeDiag::Phase::Publish);
    portENTER_CRITICAL(&mux_);
    snapshot_ = next;
    active_ = next.running;
    portEXIT_CRITICAL(&mux_);
    RuntimeDiag::beat(RuntimeDiag::Lane::Control, next.state_id, !next.running);
    RuntimeDiag::phase(RuntimeDiag::Lane::Control, RuntimeDiag::Phase::Wait);
  }
  void loop() {
    for (;;) {
      oneStep();
      // imu.update() normally waits for delivery. Fault/idle paths must also
      // yield so a latched sensor failure cannot starve Wi-Fi or the watchdog.
      if (!active()) vTaskDelay(1);
    }
  }
  Step step_ = nullptr;
  Capture capture_ = nullptr;
  void* context_ = nullptr;
  TaskHandle_t task_ = nullptr;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool active_ = false, stop_requested_ = false;
  Command command_ = Command::None;
  CommandState command_state_;
  RunControlSnapshot snapshot_;
  Audit audit_;
  uint32_t last_step_start_us_ = 0;
};
