#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// One writer (run-control), stopped-only reader (RWLOG export).
// No allocation, formatting or model evaluation in push()/clear().
namespace solver_audit {
static_assert(sizeof(float) == 4, "Replay requires float32");
struct Record {
  uint32_t index = 0;
  uint32_t t_test_ms = 0;
  uint32_t start_us = 0;
  uint32_t end_us = 0;
  uint32_t decision_us = 0;
  uint32_t free_model_us = 0;
  uint32_t solver_start_us = 0;
  uint32_t solver_end_us = 0;
  uint32_t solver_us = 0;
  uint32_t fast_solver_us = 0;
  uint32_t ff_search_us = 0;
  uint32_t selected_search_us = 0;
  uint32_t gyro_sequence = 0;
  uint32_t sample_time_us = 0;
  uint32_t entry_sample_age_us = 0;
  uint32_t exit_sample_age_us = 0;
  uint32_t pulse_id_before = 0;
  uint32_t pulse_id_after = 0;
  uint16_t model_vbat_mV = 0;
  uint16_t ff_width_ms = 65535;
  uint16_t fast_selected_width_ms = 65535;
  uint16_t selected_width_ms = 65535;
  uint16_t ff_eval_count = 0;
  uint16_t selected_eval_count = 0;
  uint16_t eval_count = 0;
  uint8_t stage = 0;
  uint8_t outcome_reason = 0;
  uint8_t state_at_exit = 0;
  uint8_t sample_time_valid = 0;
  uint8_t solver_started = 0;
  uint8_t solver_complete = 0;
  uint8_t output_executed = 0;
  uint8_t event_valid = 0;
  uint8_t ff_valid = 0;
  uint8_t selected_valid = 0;
  int8_t physical_side = 0;
  int8_t command_direction = 0;
  float i0_mA = NAN;
  float free_peak_deg = NAN;
  float target_peak_deg = NAN;
  float target_energy_j = NAN;
  float passive_energy_j = NAN;
  float q_available_mA_s = NAN;
  float integral_mA_s = NAN;
  float signed_target_current_mA = NAN;
  float tau_s = NAN;
  float base_gain = NAN;
  float correction_c = NAN;
  float correction_gain = NAN;
  float correction_limit = NAN;
  float ff_q_mA_s = NAN;
  float selected_q_mA_s = NAN;
  float corrected_target_energy_j = NAN;
};
inline uint32_t floatBits(float value) { uint32_t bits; memcpy(&bits, &value, 4); return bits; }
template<class F> class ScopeExit {
 public:
  explicit ScopeExit(F& fn) : fn_(fn) {}
  ~ScopeExit() { fn_(); }
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;
 private:
  F& fn_;
};
template<unsigned Capacity> class Buffer {
 public:
  static_assert(Capacity > 0, "Nonzero capacity required");
  void clear() { count_ = next_ = total_ = 0; }
  void push(const Record& input) {
    records_[next_] = input;
    records_[next_].index = ++total_;
    next_ = (next_ + 1U) % Capacity;
    if (count_ < Capacity) ++count_;
  }
  uint32_t count() const { return count_; }
  uint32_t overwritten() const { return total_ - count_; }
  const Record& at(unsigned index) const {
    return records_[((count_ == Capacity ? next_ : 0U) + index) % Capacity];
  }
  void appendJson(String& json) const {
    json += "{\"schema_version\":1,\"available\":true,\"solver_revision\":\"v46r_fast_solver_control_20260915\",";
    json += "\"policy\":\"diagnostic_only;elapsed_wall_time_includes_preemption;no_legacy_online;stopped_export\",";
    json += "\"capacity\":" + String(Capacity) + ",\"count\":" + String(count_);
    json += ",\"overwritten\":" + String(overwritten());
    json += ",\"stage_legend\":\"0=before_solver,1=ff_failed,2=corrected_target_failed,3=selected_failed,4=selected_complete\",";
    json += "\"timing_scope\":\"decision=start_to_return_before_audit_copy;solver=cached_setup_through_selection;ff_and_selected=search_only;sample_age=consumed_host_sample_not_sensor_timestamp\",";
    json += "\"float_fields\":[\"i0_mA\",\"free_peak_deg\",\"target_peak_deg\",\"target_energy_j\",\"passive_energy_j\",\"q_available_mA_s\",\"integral_mA_s\",\"signed_target_current_mA\",\"tau_s\",\"base_gain\",\"correction_c\",\"correction_gain\",\"correction_limit\",\"ff_q_mA_s\",\"selected_q_mA_s\",\"corrected_target_energy_j\"],\"events\":[";
    for (unsigned i = 0; i < count_; ++i) {
      const Record& e = at(i);
      if (i) json += ",";
      json += "{";
      json += "\"index\":" + String(e.index);
      json += ",\"t_test_ms\":" + String(e.t_test_ms);
      json += ",\"start_us\":" + String(e.start_us);
      json += ",\"end_us\":" + String(e.end_us);
      json += ",\"decision_us\":" + String(e.decision_us);
      json += ",\"free_model_us\":" + String(e.free_model_us);
      json += ",\"solver_start_us\":" + String(e.solver_start_us);
      json += ",\"solver_end_us\":" + String(e.solver_end_us);
      json += ",\"solver_us\":" + String(e.solver_us);
      json += ",\"fast_solver_us\":" + String(e.fast_solver_us);
      json += ",\"ff_search_us\":" + String(e.ff_search_us);
      json += ",\"selected_search_us\":" + String(e.selected_search_us);
      json += ",\"gyro_sequence\":" + String(e.gyro_sequence);
      json += ",\"sample_time_us\":" + String(e.sample_time_us);
      json += ",\"entry_sample_age_us\":" + String(e.entry_sample_age_us);
      json += ",\"exit_sample_age_us\":" + String(e.exit_sample_age_us);
      json += ",\"pulse_id_before\":" + String(e.pulse_id_before);
      json += ",\"pulse_id_after\":" + String(e.pulse_id_after);
      json += ",\"model_vbat_mV\":" + String(e.model_vbat_mV);
      json += ",\"ff_width_ms\":" + String(e.ff_width_ms);
      json += ",\"fast_selected_width_ms\":" + String(e.fast_selected_width_ms);
      json += ",\"selected_width_ms\":" + String(e.selected_width_ms);
      json += ",\"ff_eval_count\":" + String(e.ff_eval_count);
      json += ",\"selected_eval_count\":" + String(e.selected_eval_count);
      json += ",\"eval_count\":" + String(e.eval_count);
      json += ",\"stage\":" + String(e.stage);
      json += ",\"outcome_reason\":" + String(e.outcome_reason);
      json += ",\"state_at_exit\":" + String(e.state_at_exit);
      json += ",\"sample_time_valid\":" + String(e.sample_time_valid);
      json += ",\"solver_started\":" + String(e.solver_started);
      json += ",\"solver_complete\":" + String(e.solver_complete);
      json += ",\"output_executed\":" + String(e.output_executed);
      json += ",\"event_valid\":" + String(e.event_valid);
      json += ",\"ff_valid\":" + String(e.ff_valid);
      json += ",\"selected_valid\":" + String(e.selected_valid);
      json += ",\"physical_side\":" + String(e.physical_side);
      json += ",\"command_direction\":" + String(e.command_direction);
      json += ",\"f32_bits\":[";
      json += String(floatBits(e.i0_mA));
      json += "," + String(floatBits(e.free_peak_deg));
      json += "," + String(floatBits(e.target_peak_deg));
      json += "," + String(floatBits(e.target_energy_j));
      json += "," + String(floatBits(e.passive_energy_j));
      json += "," + String(floatBits(e.q_available_mA_s));
      json += "," + String(floatBits(e.integral_mA_s));
      json += "," + String(floatBits(e.signed_target_current_mA));
      json += "," + String(floatBits(e.tau_s));
      json += "," + String(floatBits(e.base_gain));
      json += "," + String(floatBits(e.correction_c));
      json += "," + String(floatBits(e.correction_gain));
      json += "," + String(floatBits(e.correction_limit));
      json += "," + String(floatBits(e.ff_q_mA_s));
      json += "," + String(floatBits(e.selected_q_mA_s));
      json += "," + String(floatBits(e.corrected_target_energy_j));
      json += "]}";
    }
    json += "]}";
  }
 private:
  Record records_[Capacity];
  uint32_t count_ = 0, next_ = 0, total_ = 0;
};
}  // namespace solver_audit
