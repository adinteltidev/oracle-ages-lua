"""Compare the native C resource guard for all out-of-domain color bytes."""
import argparse
from pathlib import Path
import subprocess
import sys
import shutil
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path)
p.add_argument('rom',type=Path)
a=p.parse_args()
if not a.rom.is_file():
    print('skip: Ages ROM missing')
    sys.exit(77)
with tempfile.TemporaryDirectory(prefix='phase2-invalid-') as temporary:
    executable=Path(temporary)/a.executable.name
    shutil.copy2(a.executable,executable)
    cases=stops=0
    for color in range(256):
        if color & 0x7f < 3:
            continue
        results=[]
        for mode in ['c','lua']:
            run=subprocess.run([str(executable.resolve()),'--rom',str(a.rom.resolve()),
                                '--invalid-color',str(color),'--mode',mode],capture_output=True,timeout=20)
            results.append((run.returncode,run.stdout,run.stderr))
        if results[0]!=results[1]:
            raise AssertionError((color,results))
        if results[0][0]:
            assert results[0][0]==5 and b'data read of code byte' in results[0][2],results
            stops+=1
        cases+=1
    print(f'PASS: {cases} unexpected colors; identical C/Lua exits and diagnostics; {stops} original resource-guard exits')
