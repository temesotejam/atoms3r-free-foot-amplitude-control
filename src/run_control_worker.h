#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include "timing_deadline.h"

// The Arduino task owns the controller while idle. After the Start HTTP
// response, ownership is transferred to this worker until END_SYNC/ESTOP.
// Web handlers use snapshots and a stop request, never the live controller.
struct RunControlSnapshot {
  bool running = false;
  uint8_t state_id = 0;
  uint16_t run_id = 0;
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
  using Step = bool (*)(void*);  // returns whether the run is still active
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
    Timing recent[16] = {};
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
  // Called only by the Arduino owner after its HTTP handler has returned.
  bool start() {
    if (!ready() || active()) return false;
    RunControlSnapshot next{};
    capture_(context_, next);
    if (!next.running) return false;
    const uint32_t now_us = micros();
    // Initialize the audit while exclusively idle; no large zero-fill in a lock.
    audit_ = Audit{};
    audit_.epoch_us = now_us;
    last_step_start_us_ = now_us;
    portENTER_CRITICAL(&mux_);
    snapshot_ = next;
    stop_requested_ = false;
    active_ = true;
    portEXIT_CRITICAL(&mux_);
    xTaskNotifyGive(task_);
    return true;
  }
  RunControlSnapshot snapshot() const {
    portENTER_CRITICAL(&mux_);
    const RunControlSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&mux_);
    return copy;
  }
  bool requestStop() {
    portENTER_CRITICAL(&mux_);
    const bool accepted = active_;
    if (accepted) { stop_requested_ = true; ++audit_.stop_requests; }
    portEXIT_CRITICAL(&mux_);
    return accepted;
  }
  // Called by the exclusive controller owner before each acquisition delivery.
  bool takeStopRequest() {
    portENTER_CRITICAL(&mux_);
    const bool requested = stop_requested_;
    stop_requested_ = false;
    if (requested) ++audit_.stop_consumed;
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
                              uint32_t done_us, uint32_t runner_us) {
    if (!measurement || !fresh) return;
    portENTER_CRITICAL(&mux_);
    audit_.sample_completion.add(static_cast<uint32_t>(done_us - sample_us), 2500);
    audit_.runner_work.add(runner_us, 2500);
    portEXIT_CRITICAL(&mux_);
  }
  String diagnosticsJson() const {
    Audit a;
    portENTER_CRITICAL(&mux_);
    a = audit_;
    portEXIT_CRITICAL(&mux_);
    String json;
    json.reserve(3000);
    json = "{\"revision\":\"v46p_run_control_worker_20260914\"";
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
    json += ",\"all_observed_within_budget\":" + String(a.sample_completion.passed() ? "true" : "false");
    json += ",\"scope\":\"RUNNING_fresh_gyro_only;host_acquisition_to_runner_return;not_sensor_capture_to_motor_apply\"}";
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
    const bool still_running = step_(context_);
    RunControlSnapshot next{};
    capture_(context_, next);
    portENTER_CRITICAL(&mux_);
    snapshot_ = next;
    // This is the final shared-controller operation of the worker. Once false,
    // it must not access runner/imu.reading/logger again until the next Start.
    if (!still_running) active_ = false;
    portEXIT_CRITICAL(&mux_);
  }
  void loop() {
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      while (active()) oneStep();
    }
  }
  Step step_ = nullptr;
  Capture capture_ = nullptr;
  void* context_ = nullptr;
  TaskHandle_t task_ = nullptr;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool active_ = false, stop_requested_ = false;
  RunControlSnapshot snapshot_;
  Audit audit_;
  uint32_t last_step_start_us_ = 0;
};
