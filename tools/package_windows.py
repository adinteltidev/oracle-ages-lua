#!/usr/bin/env python3
"""Package the audited Windows C executable with notices and no game data."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--audit', type=Path, required=True)
parser.add_argument('--sdl-source', type=Path, required=True)
parser.add_argument('--llvm-root', type=Path, required=True, help='LLVM-MinGW runtime license notices')
parser.add_argument('--verification', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
data = args.executable.read_bytes()
audit = json.loads(args.audit.read_text())
assert audit['sha256'] == hashlib.sha256(data).hexdigest(), 'Audit does not match the executable'
assert audit['format'] == 'PE32+ x86-64' and audit['subsystem'] == 'Windows GUI'
assert audit['private_dlls_required'] == [] and audit['utf8_manifest']
files = {'Oracles.exe': data, 'LEIA-ME.txt': (root / 'LEIA-ME-WINDOWS.txt').read_bytes(),
         'LICENSE': (root / 'LICENSE').read_bytes(), 'Licenses/SDL3.txt': (args.sdl_source / 'LICENSE.txt').read_bytes()}
if b'oracles-lua-phase3' in data:
    files['LEIA-ME.txt'] = files['LEIA-ME.txt'].decode().replace(
        r'%APPDATA%\oracles-decomp\oracles\.',
        r'%APPDATA%\oracles-decomp\oracles-lua-phase3\.').encode()
elif b'oracles-lua-phase2' in data:
    files['LEIA-ME.txt'] = files['LEIA-ME.txt'].decode().replace(
        r'%APPDATA%\oracles-decomp\oracles\.',
        r'%APPDATA%\oracles-decomp\oracles-lua-phase2\.').encode()
elif b'oracles-lua-phase1' in data:
    files['LEIA-ME.txt'] = files['LEIA-ME.txt'].decode().replace(
        r'%APPDATA%\oracles-decomp\oracles\.',
        r'%APPDATA%\oracles-decomp\oracles-lua-phase1\.').encode()
if args.verification:
    manifest = json.loads(args.verification.read_text())
    assert manifest['binary']['sha256'] == audit['sha256'], 'Manifest does not match this EXE'
    files['BUILD.json'] = args.verification.read_bytes()
files['Licenses/LLVM-LICENSE.TXT'] = (args.llvm_root / 'LICENSE.TXT').read_bytes()
for notice in sorted((args.llvm_root / 'x86_64-w64-mingw32/share/mingw32').glob('COPYING*')):
    files['Licenses/MinGW/' + notice.name] = notice.read_bytes()
notices = ['clay/LICENSE.md', 'clay/README.md', 'fonts/OFL.txt', 'fonts/README.md',
           'hqx/COPYING', 'hqx/README.md', 'lua/README.md', 'lua/lua.h',
           'stb/README.md', 'stb/stb_truetype.h', 'xbrz/README.md', 'xbrz/xbrz.h']
for name in notices:
    files['Licenses/' + name] = (root / 'third_party' / name).read_bytes()
files['SHA256SUMS.txt'] = ''.join(hashlib.sha256(value).hexdigest() + '  ' + name + '\n'
                               for name, value in sorted(files.items())).encode('utf-8')
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, value in sorted(files.items()):
        info = zipfile.ZipInfo('Oracles-Windows-x64/' + name, date_time=(1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o755 if name.endswith('.exe') else 0o644) << 16
        archive.writestr(info, value)
with zipfile.ZipFile(args.output) as archive:
    assert archive.testzip() is None
print(json.dumps({'zip': str(args.output), 'bytes': args.output.stat().st_size,
                  'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  'executable_sha256': audit['sha256'], 'files': len(files), 'rom_or_saves_included': False}, indent=2))
