#!/usr/bin/env python3
"""Run selected portable Windows tests; compare every requested replay checkpoint explicitly."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('rom', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--executor', help='Wine executable, when running the PE on another OS')
parser.add_argument('--wine-paths', action='store_true')
parser.add_argument('--full-replay', action='store_true', help='Compare all 4825 Ages reference checkpoints')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build, rom, output = (p.resolve() for p in (args.build, args.rom, args.output))
assert hashlib.sha256(rom.read_bytes()).hexdigest() == '0b56b78a9e45452e98c33edd111234931f1e034dc097f6f23082eb8db6055474', 'Unexpected ROM'
output.mkdir(parents=True, exist_ok=False)
temporary = output / 'temporário João'
temporary.mkdir()
movement = output / 'movement'
movement.mkdir()

def target_path(path):
    return ('Z:' if args.wine_paths else '') + str(path)

environment = dict(os.environ, ORACLES_TEST_ARTIFACTS=target_path(movement))
environment.update(TMP=target_path(temporary), TEMP=target_path(temporary))
runs = []

def run(name, exe, arguments=(), timeout=300):
    command = ([args.executor] if args.executor else []) + [str(build / exe)]
    command += [target_path(a) if isinstance(a, Path) else str(a) for a in arguments]
    started = time.monotonic()
    with (output / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    text = (output / (name + '.log')).read_text(encoding='utf-8', errors='replace')
    assert result.returncode == 0, f'{name}: exit {result.returncode}; see {output / (name + ".log")}'
    runs.append({'name': name, 'exit_code': result.returncode, 'seconds': round(time.monotonic() - started, 3)})
    print('PASS:', name, flush=True)
    return text

run('platform-keys', 'test_platform_keys.exe')
run('ui', 'test_ui.exe')
native = run('native-ages', 'test_native_ages.exe', timeout=600)
assert re.search(r'verified 333 checkpoints over 20000 frames\s', native), native
assert 'verified 333 checkpoints over 20000 frames with state reloads' in native, native
motion = run('movement', 'test_native_movement.exe', timeout=300)
assert '19 one-pixel updates' in motion and 'X=124, Y=56, walls=03, health=12' in motion, motion
assert not list(temporary.iterdir()), 'Temporary state file was not cleaned up'

observed_path = output / 'observed.ref'
frames = 289518 if args.full_replay else 20000
run('replay', 'oracles-native-run.exe',
    ['--rom', rom, '--boot-state', root / 'tas/ages-boot.state', '--tas', root / 'tas/ages-consoleverified.inputs',
     '--frames', frames, '--ref-out', observed_path], timeout=3600)

def trace(path):
    records = {}
    for line in path.read_text().splitlines():
        frame, digest = line.split()
        assert int(frame) not in records, f'Duplicate reference frame: {frame}'
        records[int(frame)] = int(digest, 16)
    assert records, f'Empty trace: {path}'
    return records

reference, observed = trace(root / 'tas/ages.ref'), trace(observed_path)
required = reference if args.full_replay else {f: h for f, h in reference.items() if f <= max(observed)}
assert len(required) == 4825 if args.full_replay else len(required) >= 333, len(required)
for frame, expected in required.items():
    assert observed.get(frame) == expected, f'Missing/mismatched checkpoint {frame}: {observed.get(frame)} != {expected}'
result = {'status': 'passed', 'execution_environment': 'Wine on ' + platform.system() if args.executor else platform.platform(),
          'windows_real_or_vm_tested': not args.executor and os.name == 'nt',
          'tests': runs, 'prefix_checkpoints': 333, 'reload_checkpoints': 333,
          'movement': {'room': '00:8a', 'start_x': 72, 'free_x': 91, 'wall_x': 124, 'y': 56, 'health': 12},
          'full_replay': args.full_replay, 'reference_checkpoints_matched': len(required),
          'observed_checkpoints': len(observed), 'additional_observed_checkpoints': len(set(observed) - set(required)),
          'temporary_files_cleaned': True}
(output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
