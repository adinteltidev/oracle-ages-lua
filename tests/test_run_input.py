#!/usr/bin/env python3
"""Drive the real desktop Run binding through SDL into a sandboxed Lua input handler."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

app, rom = sys.argv[1:3]
if not Path(rom).is_file():
    print('skip: Ages ROM missing')
    sys.exit(77)
env = dict(os.environ, ORACLES_TEST_HEADLESS='1')
with tempfile.TemporaryDirectory(prefix='oracles-run-input-') as tmp:
    cache = Path(tmp)
    def launch(args, **extra):
        r = subprocess.run([app, '--cache', tmp, *args], env=dict(env, **extra),
                           capture_output=True, text=True, timeout=90)
        assert r.returncode == 0, r.stderr
        return r.stderr
    launch([rom, '--frames', '1'])
    mod = cache / 'mods' / 'corrida'
    mod.mkdir(parents=True)
    (mod / 'mod.ini').write_text('id=corrida\napi=2\nchanges_game=yes\ngames=ages\n')
    (mod / 'main.lua').write_text('''
local previous
oracles.on_input(function(i)
  if previous ~= i.run then
    oracles.log('RUN_INPUT=' .. tostring(i.run))
    previous = i.run
  end
  oracles.run(i.run)
end)
''')
    log = launch(['--game', 'ages', '--frames', '20'], ORACLES_TEST_KEYS='3:Left Shift')
    assert 'RUN_INPUT=true' in log, log
    assert 'RUN_INPUT=false' in log.split('RUN_INPUT=true', 1)[1], log
    assert 'disabling it' not in log, log
print('ok actual SDL Left Shift -> Lua run=true -> release run=false')
