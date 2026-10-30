#!/usr/bin/env python3
"""Exercise native PE executables on a Windows runner, without a commercial ROM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('inputs', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
records = []
with tempfile.TemporaryDirectory(prefix='oracles-ci-') as tmp:
    stage = Path(tmp)
    with zipfile.ZipFile(a.inputs / 'smoke-input.zip') as z:
        z.extractall(stage)
    env = dict(os.environ, ORACLES_TEST_HEADLESS='1', ORACLES_TEST_KEYS='20:Z')
    # A clean PATH checks that no compiler/private DLL directory is needed.
    env['PATH'] = os.pathsep.join([str(Path(os.environ['SystemRoot']) / 'System32'), os.environ['SystemRoot']])
    commands = [(['test_ui.exe'], 0, None), (['test_platform_keys.exe'], 0, None),
                (['oracles-native-run.exe'], 2, 'usage:'),
                (['oracles-native-run.exe', '--rom', str(stage / 'missing.gbc')], 2, 'cannot read'),
                (['Oracles.exe', '--cache', str(stage / 'cache')], 0, None)]
    for name in ['test_lua_gameplay.exe', 'test_lua_block_rules.exe', 'test_lua_colored_cube.exe', 'test_lua_levers_lava.exe', 'test_lua_doors.exe']:
        if (stage / name).is_file():
            commands.append(([name], 0, None))
    for arguments, expected, text in commands:
        run = subprocess.run([str(stage / arguments[0]), *arguments[1:]], env=env,
                             capture_output=True, text=True, timeout=90)
        if run.returncode != expected or text and text not in run.stdout + run.stderr:
            raise RuntimeError(f'{arguments[0]}: exit {run.returncode}; {run.stdout}\n{run.stderr}')
        records.append({'command': arguments, 'exit_code': run.returncode})
    result = {'passed': True, 'platform': 'Windows x64', 'rom_supplied': False,
              'executable_sha256': hashlib.sha256((stage / 'Oracles.exe').read_bytes()).hexdigest(),
              'checks': records}
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(result, indent=2) + '\n')
