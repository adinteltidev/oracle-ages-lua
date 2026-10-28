#!/usr/bin/env python3
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('app',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
exe=a.app/'Contents/MacOS/Oracles'
arch=subprocess.check_output(['lipo','-archs',str(exe)],text=True).split()
if arch!=['x86_64']:raise ValueError('Expected Intel-only app: '+repr(arch))
subprocess.run(['codesign','--verify','--deep','--strict',str(a.app)],check=True)
linked=subprocess.check_output(['otool','-L',str(exe)],text=True).splitlines()[1:]
libs=[line.strip().split(' (')[0] for line in linked]
if any(not x.startswith(('/System/Library/','/usr/lib/')) for x in libs):raise ValueError('Unexpected external library: '+repr(libs))
a.output.write_text(json.dumps({'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'architectures':arch,'signature':'ad-hoc, verified','libraries':libs,'notarized':False},indent=2)+'\n')
