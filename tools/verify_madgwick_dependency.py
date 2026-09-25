#!/usr/bin/env python3
"""Check that pitch extraction is tested against the exact installed library."""
from pathlib import Path
import argparse
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tools/fixtures/adafruit_ahrs_2_4_0'

def verify(library=None):
    manifest = json.loads((FIXTURES / 'provenance.json').read_text())
    for name, expected in manifest['sha256'].items():
        raw = (FIXTURES / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise RuntimeError('Madgwick upstream fixture hash mismatch: ' + name)
        if library is not None:
            actual = (library / 'src' / name).read_bytes()
            if hashlib.sha256(actual).hexdigest() != expected:
                raise RuntimeError('Installed Madgwick differs from tested 2.4.0 source: ' + name)
    print('PASS: Madgwick 2.4.0 pitch reference provenance' +
          (' and installed dependency match' if library is not None else ''))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', type=Path)
    verify(parser.parse_args().library)
