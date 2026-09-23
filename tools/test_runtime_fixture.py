#!/usr/bin/env python3
"""Validate real C++ maximum-capacity serialization and corrupted-file handling."""
from pathlib import Path
import json, struct, tempfile
import convert_rwlog_to_csv as converter
source = Path('/tmp/runtime-fixture.rwlog')
data = source.read_bytes()
header = converter.parse_header(data)
metadata = json.loads(data[110:110+header['metadata_json_size']], parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
assert metadata['metadata_json_final_bytes'] == header['metadata_json_size']
assert not metadata['metadata_event_detail_truncated']
assert len(metadata['energy_control_autonomous_peak_events']) == 256
assert len(metadata['energy_control_autonomous_zero_cross_events']) == 256
assert len(metadata['foot_frames']) == 768
with tempfile.TemporaryDirectory() as tmp:
    output = Path(tmp)/'converted'
    converter.convert(source, output)
    assert len((output/'foot_angles.csv').read_text().splitlines()) == 769
    corrupt = bytearray(data); corrupt[-10] ^= 1
    bad = Path(tmp)/'bad.rwlog'; bad.write_bytes(corrupt)
    try:
        converter.convert(bad, Path(tmp)/'bad_csv')
        raise AssertionError('corruption accepted')
    except ValueError as error:
        assert 'CRC' in str(error)
print('maximum RWLOG JSON, complete event/foot counts, CSV and corrupt-file refusal PASS')
