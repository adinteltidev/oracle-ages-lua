#!/usr/bin/env python3
"""Archive the exact committed tree, excluding ROMs and reference submodule contents."""
import argparse
import io
import json
import subprocess
import tarfile
from pathlib import Path


def create(output, root=None):
    root = Path(root) if root else Path(__file__).resolve().parents[2]
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    # git archive reads committed blobs, not a working tree changed by a test.
    committed = subprocess.check_output(['git', '-C', str(root), 'archive', '--format=tar', 'HEAD'])
    tree = subprocess.check_output(['git', '-C', str(root), 'ls-tree', '-r', '-z', 'HEAD'])
    submodules = {}
    for record in tree.split(b'\0'):
        if not record:
            continue
        metadata, name = record.split(b'\t', 1)
        mode, kind, sha = metadata.decode().split()
        if mode == '160000' and kind == 'commit':
            submodules[name.decode()] = sha
    with tarfile.open(fileobj=io.BytesIO(committed)) as source, tarfile.open(output, 'w:gz') as tar:
        for member in source:
            if member.isdir():
                continue
            name = member.name
            if Path(name).suffix.lower() in {'.gb', '.gbc', '.sav', '.nro', '.elf', '.o', '.obj', '.bak', '.tmp'}:
                continue
            if name.startswith('roms/') or name in {'WINDOWS-VERIFICATION.json', 'SWITCH-VERIFICATION.json'}:
                continue
            if not member.isfile():
                raise ValueError('Unexpected source entry: ' + name)
            data = source.extractfile(member).read()
            entry = tarfile.TarInfo('oracle-ages-native/' + name)
            entry.size = len(data)
            entry.mode = 0o755 if member.mode & 0o111 else 0o644
            tar.addfile(entry, io.BytesIO(data))
        if submodules:
            data = (json.dumps(submodules, indent=2) + '\n').encode()
            entry = tarfile.TarInfo('oracle-ages-native/REFERENCE-SUBMODULES.json')
            entry.size = len(data)
            tar.addfile(entry, io.BytesIO(data))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    create(p.parse_args().output)
