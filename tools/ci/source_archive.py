#!/usr/bin/env python3
"""Archive committed sources only, omitting compiled test ROMs and historical manifests."""
import argparse
import io
import subprocess
import tarfile
from pathlib import Path


def create(output):
    root = Path(__file__).resolve().parents[2]
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
    with tarfile.open(output, 'w:gz') as tar:
        for name in sorted(filter(None, names)):
            path = root / name
            if path.suffix.lower() in {'.gb', '.gbc', '.sav', '.nro', '.elf', '.o', '.obj', '.bak', '.tmp'}:
                continue
            if name.startswith('roms/') or name in {'WINDOWS-VERIFICATION.json', 'SWITCH-VERIFICATION.json'}:
                continue
            if path.is_symlink():
                raise ValueError('Unexpected source symlink: ' + name)
            data = path.read_bytes()
            entry = tarfile.TarInfo('oracle-ages-native/' + name)
            entry.size = len(data)
            entry.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
            tar.addfile(entry, io.BytesIO(data))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    create(p.parse_args().output)
