#!/usr/bin/env python3
"""Check the actual PE headers, imports and resources of the Windows native app."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
parser.add_argument('--llvm-bin', type=Path, required=True)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
data = args.executable.read_bytes()
assert data[:2] == b'MZ', 'Missing DOS header'
offset = struct.unpack_from('<I', data, 0x3c)[0]
assert data[offset:offset + 4] == b'PE\0\0', 'Missing PE header'
machine = struct.unpack_from('<H', data, offset + 4)[0]
optional = offset + 24
assert machine == 0x8664 and struct.unpack_from('<H', data, optional)[0] == 0x20b, 'Expected PE32+ x64'
subsystem = struct.unpack_from('<H', data, optional + 68)[0]
assert subsystem == 2, 'Expected graphical Windows subsystem'
# CLR data directory: a native C executable has no managed runtime header.
assert struct.unpack_from('<II', data, optional + 112 + 14 * 8) == (0, 0), 'Unexpected CLR header'
imports = subprocess.check_output([str(args.llvm_bin / 'llvm-objdump'), '-p', str(args.executable)], text=True)
dlls = sorted(set(re.findall(r'DLL Name:\s*(\S+)', imports)), key=str.lower)
system = {'advapi32.dll', 'avrt.dll', 'bcrypt.dll', 'cfgmgr32.dll', 'comctl32.dll',
          'comdlg32.dll', 'crypt32.dll', 'd3d11.dll', 'd3d12.dll', 'dinput8.dll',
          'dwmapi.dll', 'dxgi.dll', 'gdi32.dll', 'hid.dll', 'imm32.dll', 'kernel32.dll',
          'msvcrt.dll', 'ntdll.dll', 'ole32.dll', 'oleaut32.dll', 'opengl32.dll',
          'powrprof.dll', 'psapi.dll', 'rpcrt4.dll', 'setupapi.dll', 'shell32.dll',
          'shlwapi.dll', 'ucrtbase.dll', 'user32.dll', 'userenv.dll', 'usp10.dll',
          'version.dll', 'winhttp.dll', 'winmm.dll', 'winspool.drv', 'ws2_32.dll'}
assert dlls, 'Import table was not decoded'
private = [d for d in dlls if d.lower() not in system and not d.lower().startswith(('api-ms-win-', 'ext-ms-win-'))]
assert not private, f'Unexplained non-system dependencies: {private}'
resources = subprocess.check_output([str(args.llvm_bin / 'llvm-readobj'), '--coff-resources', str(args.executable)], text=True)
assert 'MANIFEST' in resources and 'ICON' in resources, 'Expected manifest and icon resources'
assert b'activeCodePage' in data and b'UTF-8' in data, 'Expected UTF-8 manifest'
result = {'file': str(args.executable), 'sha256': hashlib.sha256(data).hexdigest(),
          'bytes': len(data), 'format': 'PE32+ x86-64', 'subsystem': 'Windows GUI',
          'clr_header': False, 'icon_and_manifest': True, 'utf8_manifest': True,
          'imports': dlls, 'private_dlls_required': private}
if args.output:
    args.output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
