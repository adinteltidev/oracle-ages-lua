"""Check persistence of a naturally reached Ages scene across two app launches.

Usage: test_app_ages_resume.py APP ROM MOVEMENT_ARTIFACTS OUTPUT
Run native_movement with ORACLES_TEST_ARTIFACTS first. SDL dummy drivers keep this
test automated; the packaged app is also exercised separately with Cocoa/CoreAudio.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import png

app, rom, movement, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
if not rom.is_file() or not (movement / "native-wall.state").is_file():
    print("skip: Ages ROM or native movement state missing")
    raise SystemExit(77)
output.mkdir(parents=True, exist_ok=True)
cache = output / "cache"
cache.mkdir(exist_ok=False)
env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")


def run(name, args, extra_env=None):
    try:
        result = subprocess.run([str(app), *map(str, args)], env=dict(env, **(extra_env or {})),
                                capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired as error:
        captured = error.stderr or b""
        (output / (name + ".log")).write_text(captured.decode(errors="replace") if isinstance(captured, bytes) else captured)
        raise
    (output / (name + ".log")).write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.returncode, result.stderr)
    assert "test tap: no" not in result.stderr, result.stderr
    return result


def valid_save(data):
    for offset in (0x10, 0x1000):
        block = data[offset:offset + 0x550]
        if len(block) != 0x550 or block[2:10] != b"Z21216-0":
            continue
        checksum = sum(int.from_bytes(block[i:i + 2], "little") for i in range(2, 0x550, 2)) & 0xffff
        if checksum == int.from_bytes(block[:2], "little"):
            return True
    return False


run("import", [rom, "--cache", cache, "--frames", "1"])
game = cache / "ages"
shutil.copyfile(movement / "native-wall.state", game / "state_auto")
sram = (movement / "native-sram.sav").read_bytes()
assert valid_save(sram), "Natural gameplay did not produce a valid Ages save file"
(game / "sram.sav").write_bytes(sram)
w, h, rows, _ = png.Reader(filename=str(movement / "native-wall.png")).asRGB8()
assert (w, h) == (160, 144)
(game / "state_auto.thumb").write_bytes(b"".join(bytes(row) for row in rows))
mod = cache / "mods" / "resume-observer"
mod.mkdir(parents=True)
(mod / "mod.ini").write_text("id=resume-observer\nname=Resume observer\napi=2\ngames=ages\n")
(mod / "main.lua").write_text("""
local seen = false
oracles.on_frame(function()
  if not seen then
    oracles.log(string.format("RESUMED %d %d %d %d %d",
      oracles.rd(0xcc2d), oracles.rd(0xcc30), oracles.rd(0xd00d),
      oracles.rd(0xd00b), oracles.rd(0xc6aa)))
    seen = true
  end
end)
""")
results = []
for number in (1, 2):
    shot = output / f"resume-{number}.png"
    result = run(f"resume-{number}", ["--cache", cache], {
        "ORACLES_TEST_KEYS": "20:X,120:Escape,140:Up,150:X,190:Z",
        "ORACLES_SHOT": f"80:{shot}",
        "ORACLES_DIAGNOSTICS": "1",
    })
    assert "RESUMED 0 138 124 56 12" in result.stderr, result.stderr
    assert shot.is_file() and shot.stat().st_size > 1000
    assert (game / "state_auto").is_file() and valid_save((game / "sram.sav").read_bytes())
    results.append({"launch": number, "group": 0, "room": 138, "x": 124, "y": 56,
                    "health": 12, "sram_checksum_valid": True,
                    "auto_state_sha256": hashlib.sha256((game / "state_auto").read_bytes()).hexdigest()})
(output / "result.json").write_text(json.dumps({"status": "passed", "launches": results}, indent=2) + "\n")
print("OK: Ages resumed at room 00:8A, X=124, Y=56, health=12 in two separate app processes.")
