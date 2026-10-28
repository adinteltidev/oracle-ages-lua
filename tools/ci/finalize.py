#!/usr/bin/env python3
"""Bind final distributed package bytes to their build manifest."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('manifest',type=Path);a=p.parse_args()
d=json.loads(a.manifest.read_text())
d['packages']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(a.manifest.parent.iterdir()) if f.is_file() and f!=a.manifest}
if not d['packages']:raise ValueError('No distributable package produced')
a.manifest.write_text(json.dumps(d,indent=2)+'\n')
