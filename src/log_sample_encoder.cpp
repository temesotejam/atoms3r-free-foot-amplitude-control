#include "log_sample_encoder.h"
#include "log_quantization.h"

// Keep the encoder compact. The 0.47.13 forced-inline O2 build duplicated the
// conversion at each field and regressed on hardware during the start pulse.
// No fast-math, reduced precision, changed rounding, or lower logging rate.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("Os", "no-fast-math")
#endif
namespace {
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#elif defined(__clang__)
__attribute__((noinline))
#endif
int16_t quantize(float value, float scale) {
  return log_quantization::scaledI16(value, scale);
}
}
void encodeLogSample(LogSample& row, const ExperimentStatus& status,
    const RollerTelemetry& roller, uint32_t now_us, uint32_t t_test_ms,
    uint32_t run_start_us, const float* beta_ceilings) {
  row.time_us = status.sync_event_id == 2 ? 0 : static_cast<uint32_t>(now_us - run_start_us);
  row.t_test_ms = t_test_ms;
  row.state_id = static_cast<uint8_t>(status.state);
  row.pulse_id = status.pulse_id;
  row.pulse_active = status.pulse_active ? 1 : 0;
  row.pulse_direction = status.pulse_direction;
  row.motor_cmd_mA = status.motor_cmd_mA;
  row.current_mA_setting = status.current_mA_setting;
  row.pulse_width_ms_setting = status.pulse_width_ms_setting;
  row.input_interval_ms = status.input_interval_ms;
  row.trial_index = status.trial_index;
  row.trial_count = status.trial_count;
  row.trial_elapsed_ms = status.trial_elapsed_ms;
  row.trial_duration_ms = status.trial_duration_ms;
  row.trial_current_mA = status.current_mA_setting;
  row.trial_pulse_width_ms = status.pulse_width_ms_setting;
  row.trial_input_interval_ms = status.input_interval_ms;
  row.trial_predicted_beta_min_x10000 = quantize(status.predicted_beta_min, 10000.0f);
  row.beta_recovery_tau_ms = static_cast<uint16_t>(lroundf(status.beta_recovery_tau_s_setting * 1000.0f));
  row.beta_model_vbat_mV = status.beta_model_vbat_mV;
  row.predicted_i_goal_mA = status.predicted_i_goal_mA;
  row.predicted_peak_current_mA = status.predicted_peak_current_mA;
  row.beta_model_vbat_status = status.beta_model_vbat_status;
  for (uint8_t i = 0; i < Config::DYNAMIC_BETA_COUNT; ++i) {
    row.beta_ceiling_series_x10000[i] = quantize(beta_ceilings[i], 10000.0f);
    row.pitch_dynamic_series_cdeg[i] = quantize(status.pitch_dynamic_beta_deg[i], 100.0f);
  }
  row.pitch_madgwick_beta1_raw_cdeg = quantize(status.pitch_madgwick_beta1_raw_deg, 100.0f);
  row.pitch_madgwick_beta1_bias_cdeg = quantize(status.pitch_madgwick_beta1_bias_deg, 100.0f);
  row.pitch_gyro_raw_cdeg = quantize(status.pitch_gyro_raw_deg, 100.0f);
  row.pitch_gyro_bias_corrected_cdeg = quantize(status.pitch_gyro_bias_corrected_deg, 100.0f);
  row.pitch_accel_only_cdeg = quantize(status.pitch_accel_only_deg, 100.0f);
  row.gyro_bias_x_cdps = quantize(status.gyro_bias_x_dps, 100.0f);
  row.gyro_bias_y_cdps = quantize(status.gyro_bias_y_dps, 100.0f);
  row.gyro_bias_z_cdps = quantize(status.gyro_bias_z_dps, 100.0f);
  row.gyro_pitch_rate_cdps = quantize(status.gyro_pitch_rate_dps, 100.0f);
  for (uint8_t i = 0; i < Config::DYNAMIC_BETA_COUNT; ++i) {
    row.beta_target_series_x10000[i] = quantize(status.beta_target_series[i], 10000.0f);
    row.beta_applied_series_x10000[i] = quantize(status.beta_smooth_series[i], 10000.0f);
  }
  row.ax_mg = quantize(status.ax_g, 1000.0f);
  row.ay_mg = quantize(status.ay_g, 1000.0f);
  row.az_mg = quantize(status.az_g, 1000.0f);
  row.gx_cdps = quantize(status.gx_dps, 100.0f);
  row.gy_cdps = quantize(status.gy_dps, 100.0f);
  row.gz_cdps = quantize(status.gz_dps, 100.0f);
  row.acc_norm_mg = quantize(status.acc_norm_g, 1000.0f);
  row.roller_actual_current_mA = roller.actual_current_mA;
  row.roller_battery_mV = roller.battery_mV;
  row.roller_current_sample_time_us = roller.current_sample_time_us;
  row.roller_current_sequence = roller.current_sequence;
  row.roller_current_age_us = roller.current_sample_time_us == 0
      ? UINT32_MAX : static_cast<uint32_t>(now_us - roller.current_sample_time_us);
  row.roller_current_read_failure_count = roller.current_read_failure_count;
  row.roller_q_meas_observed_mAms = isfinite(roller.q_meas_observed_mA_s)
      ? static_cast<int32_t>(lroundf(roller.q_meas_observed_mA_s * 1000.0f)) : LOG_NAN_I32;
  row.pulse_q_target_mAms = isfinite(status.current_audit_q_target_mA_s)
      ? static_cast<int32_t>(lroundf(status.current_audit_q_target_mA_s * 1000.0f)) : LOG_NAN_I32;
  row.pulse_q_pred_mAms = isfinite(status.current_audit_q_pred_mA_s)
      ? static_cast<int32_t>(lroundf(status.current_audit_q_pred_mA_s * 1000.0f)) : LOG_NAN_I32;
  row.roller_current_sample_count = roller.current_audit_sample_count;
  row.roller_current_valid = roller.current_valid ? 1 : 0;
  row.roller_q_meas_observed_valid = roller.q_meas_observed_valid ? 1 : 0;
  row.pitch_mekf_control_cdeg = quantize(status.pitch_mekf_deg, 100.0f);
  row.pitch_mekf_abs_cdeg = quantize(status.pitch_mekf_abs_deg, 100.0f);
  row.pitch_madgwick_dynamic_abs_cdeg = quantize(status.pitch_madgwick_dynamic_abs_deg, 100.0f);
  row.mekf_q_w_x10000 = quantize(status.mekf_q_w, 10000.0f);
  row.mekf_q_x_x10000 = quantize(status.mekf_q_x, 10000.0f);
  row.mekf_q_y_x10000 = quantize(status.mekf_q_y, 10000.0f);
  row.mekf_q_z_x10000 = quantize(status.mekf_q_z, 10000.0f);
  row.mekf_bias_x_cdps = quantize(status.mekf_bias_x_dps, 100.0f);
  row.mekf_bias_y_cdps = quantize(status.mekf_bias_y_dps, 100.0f);
  row.mekf_bias_z_cdps = quantize(status.mekf_bias_z_dps, 100.0f);
  row.mekf_accel_confidence_x10000 = quantize(status.mekf_accel_confidence, 10000.0f);
  row.mekf_accel_residual_cdeg = quantize(status.mekf_accel_residual_deg, 100.0f);
  row.mekf_accel_mag_error_mg = quantize(status.mekf_accel_mag_error_g, 1000.0f);
  row.imu_update_dt_us = status.imu_update_dt_us;
  row.imu_sample_age_us = status.imu_last_update_us != 0
      ? static_cast<uint32_t>(now_us - status.imu_last_update_us) : 0xFFFFFFFFUL;
  row.mekf_accel_used = status.mekf_accel_used ? 1 : 0;
  row.attitude_filter_adopted = 1;
  // V46z comparison-zero begin
  row.pitch_mekf_start_sync_relative_cdeg = quantize(status.pitch_mekf_start_sync_relative_deg, 100.0f);
  row.pitch_mekf_measurement_relative_cdeg = quantize(status.pitch_mekf_measurement_relative_deg, 100.0f);
  row.pitch_mekf_trial_relative_cdeg = quantize(status.pitch_mekf_trial_relative_deg, 100.0f);
  row.mekf_start_sync_zero_abs_cdeg = quantize(status.mekf_start_sync_zero_abs_deg, 100.0f);
  row.mekf_measurement_zero_abs_cdeg = quantize(status.mekf_measurement_zero_abs_deg, 100.0f);
  row.mekf_trial_zero_abs_cdeg = quantize(status.mekf_trial_zero_abs_deg, 100.0f);
  row.mekf_start_sync_zero_sample_us = status.mekf_start_sync_zero_sample_us;
  row.mekf_measurement_zero_sample_us = status.mekf_measurement_zero_sample_us;
  row.mekf_trial_zero_sample_us = status.mekf_trial_zero_sample_us;
  // V46z comparison-zero end
  // V46aa control-zero log begin
  row.pitch_mekf_detector_relative_cdeg = quantize(status.pitch_mekf_detector_relative_deg, 100.0f);
  row.mekf_detector_zero_predicted_abs_cdeg =
      quantize(status.mekf_detector_zero_predicted_abs_deg, 100.0f);
  row.mekf_detector_zero_sample_us = status.mekf_detector_zero_sample_us;
  // V46aa control-zero log end
  row.led_state = status.led_state ? 1 : 0;
  row.sync_event_id = status.sync_event_id;
  row.log_active =
      (status.state == ExperimentState::START_SYNC || status.state == ExperimentState::RUNNING_BATCH_SWEEP ||
       status.state == ExperimentState::TRIAL_REST || status.state == ExperimentState::END_SYNC) ? 1 : 0;
  row.beta_phase_state = status.beta_phase_state;
  row.beta_phase_progress_x10000 = quantize(status.beta_phase_progress, 10000.0f);
  row.beta_phase_peak_angle_cdeg = quantize(status.beta_phase_peak_angle_deg, 100.0f);
  row.beta_phase_angle_cdeg = quantize(status.beta_phase_angle_deg, 100.0f);
  row.beta_phase_ceiling_x10000 = quantize(status.beta_phase_ceiling, 10000.0f);
  row.physical_roll_abs_cdeg = quantize(status.physical_roll_abs_deg, 100.0f);
  row.current_roll_cdeg = quantize(status.current_roll_deg, 100.0f);
  row.physical_roll_rate_cdps = quantize(status.physical_roll_rate_dps, 100.0f);
  row.target_roll_cdeg = quantize(status.target_roll_deg, 100.0f);
  row.target_error_cdeg = quantize(status.target_error_deg, 100.0f);
  row.static_confirmed = status.static_confirmed ? 1 : 0;
  row.ready = status.ready ? 1 : 0;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
