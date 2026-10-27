#!/usr/bin/env python3
"""Fetch the pinned, self-contained macOS LLVM-MinGW toolchain into a local directory."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

VERSION = '20261020'
NAME = f'llvm-mingw-{VERSION}-ucrt-macos-universal'
SHA256 = 'e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e'
URL = f'https://github.com/mstorsjo/llvm-mingw/releases/download/{VERSION}/{NAME}.tar.xz'

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('destination', type=Path)
args = parser.parse_args()
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
archive = destination / (NAME + '.tar.xz')
if not archive.exists():
    partial = archive.with_suffix('.partial')
    urllib.request.urlretrieve(URL, partial)
    partial.replace(archive)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
if digest != SHA256:
    raise SystemExit(f'Hash mismatch: {archive}: {digest}')
if not (destination / NAME / 'bin/x86_64-w64-mingw32-clang').is_file():
    subprocess.run(['tar', '-xf', str(archive), '-C', str(destination)], check=True)
manifest = {'version': VERSION, 'url': URL, 'sha256': digest, 'runtime': 'UCRT',
            'host': 'macOS universal', 'target': 'x86_64 Windows', 'path': str(destination / NAME)}
(destination / 'llvm-mingw.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(destination / NAME)
