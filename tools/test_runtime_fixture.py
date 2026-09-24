#!/usr/bin/env python3
"""Validate real C++ maximum-capacity serialization and corrupted-file handling."""
from pathlib import Path
import csv, json, struct, tempfile
import convert_rwlog_to_csv as converter
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
assert diagnostics['range']['right_support_x'] == [40,173]
assert diagnostics['range']['left_support_x'] == [39,177.5]
assert diagnostics['range']['original_right_support_x'] == [42,173]
assert diagnostics['range']['original_left_support_x'] == [43.5,177.5]
assert diagnostics['range']['extension_angle_accuracy_validated'] is False
source = Path('/tmp/runtime-fixture.rwlog')
data = source.read_bytes()
header = converter.parse_header(data)
metadata = json.loads(data[110:110+header['metadata_json_size']], parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
assert metadata['metadata_json_final_bytes'] == header['metadata_json_size']
assert not metadata['metadata_event_detail_truncated']
assert len(metadata['energy_control_autonomous_peak_events']) == 256
assert len(metadata['energy_control_autonomous_zero_cross_events']) == 256
assert len(metadata['foot_frames']) == 768
assert metadata['foot_observation']['detector'] == 'sparse_rows_identity_v2'
assert metadata['foot_observation']['zero_reason'] == 'ready'
assert metadata['foot_observation']['zero_max_nominal_offset_px'] == 35
assert metadata['foot_observation']['zero_max_spread_px'] == 4
assert metadata['foot_observation']['vertical_recovery_angle_accuracy_validated'] is False
assert metadata['foot_observation']['range'] == diagnostics['range']
assert metadata['foot_observation']['right_support_x'] == [40,173]
assert metadata['foot_observation']['left_support_x'] == [39,177.5]
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
