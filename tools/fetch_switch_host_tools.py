#!/usr/bin/env python3
"""Build the official NRO packagers natively; no system installation required."""
import argparse, hashlib, pathlib, subprocess, urllib.request
p=argparse.ArgumentParser();p.add_argument('--destination',type=pathlib.Path,required=True);a=p.parse_args()
root=a.destination.resolve();root.mkdir(parents=True,exist_ok=True)
source=root/'switch-tools-source';commit='22756068dd0ed6ff9734c59cb4f99ebd3f62555b'
if not source.exists():
 subprocess.run(['git','init',str(source)],check=True)
 subprocess.run(['git','-C',str(source),'fetch','--depth=1','https://github.com/switchbrew/switch-tools.git',commit],check=True)
 subprocess.run(['git','-C',str(source),'checkout','--detach','FETCH_HEAD'],check=True)
if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()!=commit:
 raise RuntimeError('Unexpected switch-tools checkout')
inc=root/'include';inc.mkdir(exist_ok=True)
header=urllib.request.urlopen('https://raw.githubusercontent.com/lz4/lz4/v1.10.0/lib/lz4.h').read()
if hashlib.sha256(header).hexdigest()!='26b82efc53d1570f3b54eef02e9c4764c1ad374ff03cac04e2ced5ea4d4c552f':
 raise RuntimeError('LZ4 header checksum mismatch')
(inc/'lz4.h').write_bytes(header)
src=source/'src'
for tool,files in [('elf2nro',['elf2nro.c','romfs.c','filepath.c']),('nacptool',['nacptool.c'])]:
 subprocess.run(['cc','-O2','-I'+str(inc),*[str(src/f) for f in files],'-o',str(root/tool)],check=True)
print(root)
