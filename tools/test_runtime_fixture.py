#!/usr/bin/env python3
"""Validate real C++ maximum-capacity serialization and corrupted-file handling."""
from pathlib import Path
import csv, json, struct, tempfile
import convert_rwlog_to_csv as converter
work=json.loads(Path('/tmp/control-work-fixture.json').read_text())
assert work['pulse_on']['log_row']==dict(count=1,max_us=251,sum_us=251,mean_us=251)
assert work['pulse_on']['log_encode']['mean_us']==100
assert work['pulse_on']['log_store']['mean_us']==151
assert work['log_substages']=='encode_includes_beta_ceilings;store_is_synchronous_psram_addSample'
assert work['target_code_placement']=='IRAM_MEKF_log_encoder_normal_update_and_Madgwick_IMU;external_calls_and_data_may_use_flash'
assert work['normal_update_compiler']=='selected_routines_GCC_O2_no_fast_math;upstream_Madgwick_2.4.0_function_bodies_unchanged'
assert work['pulse_off']['filter']['mean_us']==700
assert work['pulse_on']['filter']['count']==0
diagnostics=json.loads(Path('/tmp/mekf-diagnostics-fixture.json').read_text(),
    parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
assert diagnostics['invalid']['valid'] is False and diagnostics['invalid']['fresh'] is False
assert diagnostics['invalid']['roll_deg'] is None and diagnostics['invalid']['quaternion']['w'] is None
for axis, attitude in enumerate(diagnostics['axes']):
    assert attitude['valid'] and attitude['fresh'] and attitude['age_us'] == 2500
    assert attitude['sample_us'] == 1000000
    for index, key in enumerate(('roll_deg','pitch_deg','yaw_deg')):
        assert abs(attitude[key] - (20 if axis == index else 0)) < 0.001
    assert attitude['estimate'] == 'posterior' and attitude['euler_order'] == 'ZYX'
assert diagnostics['stale']['valid'] and not diagnostics['stale']['fresh']
assert not diagnostics['sensor_failed']['fresh']
assert diagnostics['wrapped']['age_us'] == 251 and diagnostics['wrapped']['fresh']
assert diagnostics['invalid']['inputs']['valid'] is False
assert diagnostics['invalid']['inputs']['accel_g'] == [None,None,None]
inputs=diagnostics['inputs']['inputs']
assert inputs['valid'] and inputs['frame']=='mekf' and inputs['axis_order']=='xyz'
assert inputs['accel_age_us']==3500 and inputs['accel_sample_us']==999000
assert inputs['accel_g']==[.01,.02,1]
for actual,expected in zip(inputs['gyro_dps'],[1,2,3]): assert abs(actual-expected)<.00001
for actual,expected in zip(inputs['gyro_bias_dps'],[.1,-.2,.3]): assert abs(actual-expected)<.00001
assert inputs['last_accel_update']['used'] and inputs['last_accel_update']['confidence']>.99
assert diagnostics['range']['right_support_x'] == [39,182]
assert diagnostics['range']['left_support_x'] == [37,177.5]
assert diagnostics['range']['original_right_support_x'] == [42,173]
assert diagnostics['range']['original_left_support_x'] == [43.5,177.5]
assert diagnostics['range']['extension_angle_accuracy_validated'] is False
range_evidence=json.loads(Path('tools/fixtures/foot_range_20260925.json').read_text())
for side in ('right','left'):
    lo,hi=diagnostics['range'][side+'_support_x']
    assert [lo,hi]==range_evidence['accepted_support_x'][side]
    all_lo,all_hi=range_evidence['observed_x'][side]
    assert lo<=all_lo-1 and hi>=all_hi+1
    for source in range_evidence['sources']:
        observed=source['observations'][side]
        assert lo<=observed['min_x']<=observed['max_x']<=hi
calibration=diagnostics['calibration']
assert calibration['revision']=='fixed_two_poses_20260924' and calibration['provisional']
assert abs(calibration['right_deg_per_px']-.155146317)<1e-8
assert abs(calibration['left_deg_per_px']-.155496758)<1e-8
assert calibration['fit_poses']==2 and calibration['heldout_hand_supported_poses']==2
assert not calibration['independent_angular_accuracy_validated']
evidence=json.loads(Path('tools/fixtures/foot_calibration_20260924.json').read_text())
assert calibration['source_sha256']==evidence['source_sha256']
upright,tilted,*heldout=[r['summary'] for r in evidence['records']]
for side in ('right','left'):
    slope=(upright['roll_deg']-tilted['roll_deg'])/(upright[side+'_x']-tilted[side+'_x'])
    assert abs(slope-calibration[side+'_deg_per_px'])<1e-8
    for pose in heldout:
        residual=slope*(upright[side+'_x']-pose[side+'_x'])+pose['roll_deg']-upright['roll_deg']
        assert .25<residual<.33
source = Path('/tmp/runtime-fixture.rwlog')
data = source.read_bytes()
header = converter.parse_header(data)
metadata = json.loads(data[110:110+header['metadata_json_size']], parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
assert metadata['metadata_json_final_bytes'] == header['metadata_json_size']
assert not metadata['metadata_event_detail_truncated']
assert metadata['firmware_revision']=='0.47.20-hot-update'
solver = metadata['v46s_solver_audit']
assert solver['schema_version']==2 and solver['solver_revision']=='direct_q_branch_inverse_04718'
assert 'minimum_absolute_Q_error' in solver['inverse_policy']
assert 'ff_width_ms=65535_not_computed' in solver['field_semantics']
assert len(solver['events'])==128 and solver['events'][0]['ff_width_ms']==65535
assert metadata['terminal_state']['state']=='ESTOP' and metadata['terminal_state']['motor_cmd_mA']==0
assert metadata['terminal_state']['actual_current_mA']==-7 and metadata['terminal_state']['heartbeat_us']==123456
assert metadata['terminal_state']['last_error']=='imu_acquisition_overflow_backlog_or_stale'
assert metadata['control_work_profile']['pulse_on']['log_row']['mean_us']==777
assert len(metadata['energy_control_autonomous_peak_events']) == 256
assert len(metadata['energy_control_autonomous_zero_cross_events']) == 256
assert len(metadata['foot_frames']) == 768
assert metadata['foot_observation']['detector'] == 'sparse_rows_identity_v3'
assert metadata['foot_observation']['weak_candidate_confirmation']==dict(frames=3,min_x_step_px=20,min_y_step_px=16,
    contrast_ratio_below=.5,weight_ratio_below=.1,condition='recent_track_and_xy_steps_and_both_quality_drops')
assert metadata['foot_observation']['zero_reason'] == 'ready'
assert metadata['foot_observation']['zero_max_nominal_offset_px'] == 35
assert metadata['foot_observation']['zero_max_spread_px'] == 4
assert metadata['foot_observation']['vertical_recovery_angle_accuracy_validated'] is False
assert metadata['foot_observation']['range'] == diagnostics['range']
assert metadata['foot_observation']['calibration']==calibration
assert metadata['foot_observation']['calibration_source_commit'] is None
for side in ('right','left'):
    assert metadata['foot_observation'][side+'_deg_per_px']==calibration[side+'_deg_per_px']
assert metadata['foot_observation']['right_support_x'] == [39,182]
assert metadata['foot_observation']['left_support_x'] == [37,177.5]
assert metadata['foot_frames'][0]['right_scan_y'] == 42
assert metadata['foot_frames'][0]['left_templates'] == 17
assert metadata['foot_frames'][0]['left_candidates'] == 2
assert metadata['foot_frames'][0]['left_ambiguity'] == 0.4
assert metadata['foot_frames'][0]['zero_reason'] == 'ready'
assert metadata['foot_frames'][1]['right_reason'] == 'low_contrast'
assert metadata['foot_frames'][1]['right_deg'] is None
with tempfile.TemporaryDirectory() as tmp:
    output = Path(tmp)/'converted'
    converter.convert(source, output)
    assert len((output/'foot_angles.csv').read_text().splitlines()) == 769
    with (output/'foot_angles.csv').open() as stream:
        rows=list(csv.DictReader(stream))
    assert float(rows[0]['right_scan_y']) == 42 and rows[0]['left_templates'] == '17'
    assert rows[0]['left_candidates'] == '2' and float(rows[0]['left_ambiguity']) == 0.4
    assert rows[0]['zero_reason'] == 'ready'
    assert rows[1]['right_reason'] == 'low_contrast' and rows[1]['right_deg'] == ''
    # Older RWLOG files have no recovery diagnostics; conversion leaves blanks.
    converter.write_foot_frames({'foot_frames':[{'right_deg':5}]},output)
    with (output/'foot_angles.csv').open() as stream:
        legacy=list(csv.DictReader(stream))[0]
    assert legacy['right_deg'] == '5' and legacy['right_scan_y'] == '' and legacy['right_reason'] == ''
    assert legacy['left_candidates'] == '' and legacy['zero_reason'] == ''
    corrupt = bytearray(data); corrupt[-10] ^= 1
    bad = Path(tmp)/'bad.rwlog'; bad.write_bytes(corrupt)
    try:
        converter.convert(bad, Path(tmp)/'bad_csv')
        raise AssertionError('corruption accepted')
    except ValueError as error:
        assert 'CRC' in str(error)
print('maximum RWLOG JSON, complete event/foot counts, CSV and corrupt-file refusal PASS')
