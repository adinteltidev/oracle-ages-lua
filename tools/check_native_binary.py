#!/usr/bin/env python3
"""The cut's gate: the native library links no interpreter and the native executables embed no
ROM bytes.

usage: tools/check_native_binary.py LIBRARY.a EXECUTABLE...

The library's member list must not contain cpu.c's object; each executable must not contain the
Nintendo logo (the 48 bytes every Game Boy ROM carries at $104, the one ROM content that is
the same in both games and cannot be game data the engine keeps on purpose).
"""
import argparse, subprocess

LOGO = bytes.fromhex('CEED6666CC0D000B03730083000C000D0008111F8889000EDCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E')

parser = argparse.ArgumentParser()
parser.add_argument('--ar', default='ar', help='Archive tool from the target toolchain')
parser.add_argument('library')
parser.add_argument('executables', nargs='+')
args = parser.parse_args()
lib, exes = args.library, args.executables
bad = 0
members = subprocess.run([args.ar, 't', lib], capture_output=True, text=True, check=True).stdout.split()
if not members:
    raise SystemExit(f'{lib}: empty archive; cannot verify native code')
interp = [m for m in members if m.endswith(('cpu.c.o', 'cpu.o', 'cpu.c.obj', 'cpu.obj'))]
if interp: print(f'{lib}: contains the interpreter: {interp}'); bad += 1
else: print(f'{lib}: no interpreter object ({len(members)} members)')
for exe in exes:
    data = open(exe, 'rb').read()
    if LOGO in data: print(f'{exe}: contains the Nintendo logo bytes (ROM content)'); bad += 1
    else: print(f'{exe}: no ROM header content ({len(data)} bytes)')
raise SystemExit(1 if bad else 0)
