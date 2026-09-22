"""Strict inverse of reviewed timing-only changes for retained baseline tests."""
from pathlib import Path
import json
from v46ac_delay_comp_contract import normalize_config as normalize_v46ac_config
from v46ab_no_prediction_contract import normalize_config as normalize_v46ab_config
from v46aa_control_zero_contract import normalize_log_types as normalize_v46aa_log_types
from v46z_comparison_zero_contract import normalize_log_types as normalize_v46z_log_types
from v46ak_observation_contract import normalize_file as normalize_v46ak_file
from v46al_control_contract import normalize_file as normalize_v46al_file
ROOT=Path(__file__).resolve().parents[1]
def original_timing_file(path):
    data=(ROOT/path).read_text()
    # V46al-R1 is the declared active-control delta; remove it before retained hashes.
    data=normalize_v46al_file(path, data)
    # V46ak is observation-only; remove it before checking the retained baseline.
    data=normalize_v46ak_file(path, data)
    if path == 'src/config.h':
        data=normalize_v46ac_config(data)
        data=normalize_v46ab_config(data)
    # V46aa makes the timing detector reference explicit without changing its
    # algebraic value. Reverse its identity/log extension first.
    if path == 'src/config.h':
        data=data.replace('v46aa_control_upright_zero_20260918','v46z_event_relative_angle_zero_20260918')
    if path == 'src/log_types.h':
        data=normalize_v46aa_log_types(data)
        data=normalize_v46z_log_types(data)
    # V46z changes comparison-zero observability only. Reverse its identity next.
    if path == 'src/config.h':
        data=data.replace('v46z_event_relative_angle_zero_20260918','v46y_frozen_imu_1ms_20260918')
    # V46y freezes the selected 1 ms polling specification. Reverse it to the
    # V46x comparison point first, then unwind the earlier timing-only releases.
    if path == 'src/config.h':
        data=data.replace('v46y_frozen_imu_1ms_20260918','v46x_imu_poll_500us_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 1000UL;  // V46y frozen IMU host polling specification.',
                          'IMU_POLL_PERIOD_US = 500UL;  // V46x: poll at 2 kHz to reduce data-ready discovery latency.')
    # V46x changes only the polling experiment identity/period. Reverse it to
    # V46w first, then unwind the earlier timing-only releases.
    if path == 'src/config.h':
        data=data.replace('v46x_imu_poll_500us_20260918','v46w_imu_poll_2500us_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 500UL;  // V46x: poll at 2 kHz to reduce data-ready discovery latency.',
                          'IMU_POLL_PERIOD_US = 2500UL;  // V46w: poll once per nominal 400 Hz gyro period.')
    # V46w changes only the polling experiment identity/period. Reverse these
    # before reversing V46v/V46u so retained protected-source hashes still certify
    # the unchanged controller and estimators.
    if path == 'src/config.h':
        data=data.replace('v46w_imu_poll_2500us_20260918','v46v_deadline_tightening_20260918')
        data=data.replace('IMU_POLL_PERIOD_US = 2500UL;  // V46w: poll once per nominal 400 Hz gyro period.',
                          'IMU_POLL_PERIOD_US = 1000UL;')
    # V46v timing-only changes are reversed first so retained V46u/V46s
    # protected-source hashes still certify the unchanged controller/estimators.
    if path == 'src/config.h':
        data=data.replace('v46v_deadline_tightening_20260918','v46u_timing_reader_20260915')
        data=data.replace('static constexpr uint32_t BMI270_I2C_HZ = 1000000UL;  // BMI270 Fast-mode Plus maximum.\n','')
        data=data.replace('CURRENT_AUDIT_FAST_READ_PERIOD_US = 1000UL;  // Try every 1 ms to keep valid samples within the 2 ms audit budget.',
                          'CURRENT_AUDIT_FAST_READ_PERIOD_US = 2000UL;')
    elif path == 'src/imu_manager.cpp':
        data=data.replace('  // V46v timing-only change: BMI270 supports Fast-mode Plus up to 1 MHz.\n  // Keep the same internal bus, axes, ODR and estimator path; only shorten transfers.\n  M5.Imu.setClock(Config::BMI270_I2C_HZ);\n','')
    elif path == 'src/roller485_manager.cpp':
        data=data.replace('  // V46v: try the observational current audit every 1 ms while a pulse is active.\n  // This does not alter pulse timing or current command; it only reduces sample-age slack.\n  bool current_already_fresh = false;\n',
                          '  // V46i keeps the full 2 ms current audit, but it now runs only on Core 0.\n')
        data=data.replace('    current_already_fresh = readCurrentFresh(true);','    readCurrentFresh(true);')
        data=data.replace('  // If the fast audit already obtained a valid current in this same loop,\n  // reuse it instead of immediately reading CURRENT_READBACK a second time.\n  bool ok = current_already_fresh || readCurrentFresh(command_mA_ != 0);',
                          '  bool ok = readCurrentFresh(command_mA_ != 0);')
    edits=json.loads((ROOT/'tools/v46u_timing_delta.json').read_text())
    for edit in reversed(edits):
        if edit['path'] != path:continue
        if data.count(edit['new']) != 1:raise ValueError('Timing delta changed: '+path)
        data=data.replace(edit['new'],edit['old'],1)
    return data.encode()
