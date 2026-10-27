"""Check persistence of a naturally reached Ages scene across two app launches.

Usage: test_app_ages_resume.py APP ROM MOVEMENT_ARTIFACTS OUTPUT
Run native_movement with ORACLES_TEST_ARTIFACTS first. SDL dummy drivers keep this
test automated; the packaged app is also exercised separately with Cocoa/CoreAudio.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import png

parser = argparse.ArgumentParser(description=__doc__)
for name in ("app", "rom", "movement", "output"):
    parser.add_argument(name, type=Path)
parser.add_argument("--executor", help="Executable prefix, e.g. the path to Wine")
parser.add_argument("--wine-paths", action="store_true", help="Map local absolute file arguments to Wine's Z: drive")
parser.add_argument("--check-shortcuts", action="store_true")
parser.add_argument("--shortcut-modifier", choices=("Ctrl", "Cmd"), default="Ctrl")
options = parser.parse_args()
app, rom, movement, output = (getattr(options, n).resolve() for n in ("app", "rom", "movement", "output"))

def target_path(path):
    return ("Z:" if options.wine_paths else "") + str(path)

if not rom.is_file() or not (movement / "native-wall.state").is_file():
    print("skip: Ages ROM or native movement state missing")
    raise SystemExit(77)
output.mkdir(parents=True, exist_ok=True)
cache = output / "cache"
cache.mkdir(exist_ok=False)
env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy", ORACLES_TEST_HEADLESS="1")
env.setdefault("SDL_RENDER_DRIVER", "software")


def run(name, args, extra_env=None):
    try:
        command = ([options.executor] if options.executor else []) + [str(app)]
        command += [target_path(arg) if isinstance(arg, Path) else str(arg) for arg in args]
        result = subprocess.run(command, env=dict(env, **(extra_env or {})),
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
(mod / "mod.ini").write_text("id=resume-observer\nname=Resume observer\napi=2\ngames=ages\n" + ("changes_game=yes\n" if options.check_shortcuts else ""))
(mod / "main.lua").write_text("""
local CHECK_SHORTCUTS = SHORTCUT_OPTION
local seen = false
local ticks = 0
oracles.on_frame(function()
  ticks = ticks + 1
  if CHECK_SHORTCUTS and ticks == 60 then
    oracles.wr(0xc6aa, 10)
  end
  if CHECK_SHORTCUTS and ticks == 61 then oracles.log("SHORTCUT_DIRTY " .. oracles.rd(0xc6aa)) end
  if CHECK_SHORTCUTS and ticks == 120 then oracles.log("SHORTCUT_RELOAD " .. oracles.rd(0xc6aa)) end
  if not seen then
    oracles.log(string.format("RESUMED %d %d %d %d %d",
      oracles.rd(0xcc2d), oracles.rd(0xcc30), oracles.rd(0xd00d),
      oracles.rd(0xd00b), oracles.rd(0xc6aa)))
    seen = true
  end
end)
""".replace("SHORTCUT_OPTION", "true" if options.check_shortcuts else "false"))
results = []
for number in (1, 2):
    shot = output / f"resume-{number}.png"
    keys = "20:X,120:Escape,140:Up,150:X,190:Z"
    if options.check_shortcuts:
        modifier = options.shortcut_modifier
        keys = f"20:X,60:{modifier}+S,110:{modifier}+R,180:Escape,200:Up,210:X,250:Z"
    result = run(f"resume-{number}", ["--cache", cache], {
        "ORACLES_TEST_KEYS": keys,
        "ORACLES_SHOT": f"80:{target_path(shot)}",
        "ORACLES_DIAGNOSTICS": "1",
    })
    assert "RESUMED 0 138 124 56 12" in result.stderr, result.stderr
    if options.check_shortcuts:
        assert "SHORTCUT_DIRTY 10" in result.stderr, result.stderr
        assert "SHORTCUT_RELOAD 12" in result.stderr, result.stderr
        assert (game / "state_1").is_file() and (game / "state_1.thumb").stat().st_size == 160 * 144 * 3
    assert shot.is_file() and shot.stat().st_size > 1000
    assert (game / "state_auto").is_file() and valid_save((game / "sram.sav").read_bytes())
    results.append({"launch": number, "group": 0, "room": 138, "x": 124, "y": 56,
                    "health": 12, "sram_checksum_valid": True,
                    "auto_state_sha256": hashlib.sha256((game / "state_auto").read_bytes()).hexdigest()})
(output / "result.json").write_text(json.dumps({"status": "passed", "launches": results, "executor": options.executor, "shortcut_test": options.check_shortcuts, "wine_paths": options.wine_paths, "headless_drivers": True, "audio_output_tested": False}, indent=2) + "\n")
print("OK: Ages resumed at room 00:8A, X=124, Y=56, health=12 in two separate app processes.")
