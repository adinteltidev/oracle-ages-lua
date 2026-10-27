#!/usr/bin/env python3
"""Validate release identity and assemble already-built, audited candidate assets."""
import argparse,hashlib,json,re,subprocess,zipfile
from pathlib import Path

SEMVER = re.compile(r'^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$')

def validate(tag,version,candidate=False):
    m=SEMVER.fullmatch(tag)
    if not m or '.'.join(m.groups()[:3]) != version:
        raise ValueError('Tag must be vVERSION or vVERSION-prerelease and match VERSION')
    if m[4] and any(x.isdigit() and len(x)>1 and x.startswith('0') for x in m[4].split('.')):
        raise ValueError('Invalid numeric prerelease identifier')
    if candidate and not m[4]:raise ValueError('Manual candidates require a prerelease suffix, e.g. -rc.1')
    return bool(m[4])

def prepare(directory,sha):
    directory=Path(directory)
    manifests={name:json.loads((directory/f'{name}-BUILD.json').read_text()) for name in ['windows','macos','switch']}
    for name,m in manifests.items():
        if m['commit']!=sha or m['source_dirty'] or m['rom_supplied']:
            raise ValueError('Wrong source provenance: '+name)
        if not m.get('packages'):raise ValueError('Missing package hashes: '+name)
        for filename,digest in m['packages'].items():
            if Path(filename).name!=filename or hashlib.sha256((directory/filename).read_bytes()).hexdigest()!=digest:
                raise ValueError('Package does not match build manifest: '+filename)
    smoke=json.loads((directory/'windows-SMOKE.json').read_text())
    if smoke['passed'] is not True or smoke['executable_sha256']!=manifests['windows']['binary']['sha256']:
        raise ValueError('Windows smoke test does not match this build')
    for name,package,binary in [('windows','Oracles-Windows-x64.zip','Oracles-Windows-x64/Oracles.exe'),('switch','Oracles-Switch.zip','switch/oracle-ages-native/oracles-switch.nro')]:
        with zipfile.ZipFile(directory/package) as z:
            if z.testzip():raise ValueError('Corrupt ZIP: '+package)
            if hashlib.sha256(z.read(binary)).hexdigest()!=manifests[name]['binary']['sha256']:
                raise ValueError('Packaged binary differs from audit: '+name)
    if hashlib.sha256((directory/'oracles-switch.nro').read_bytes()).hexdigest()!=manifests['switch']['binary']['sha256']:
        raise ValueError('Standalone NRO differs from packaged NRO')
    required={'Oracles-Windows-x64.zip','Oracles-macOS-Intel.dmg','Oracles-Switch.zip','oracles-switch.nro',
              'oracle-ages-native-source.tar.gz','windows-BUILD.json','macos-BUILD.json','switch-BUILD.json','windows-SMOKE.json'}
    found={f.name for f in directory.iterdir() if f.is_file()}
    if found != required:raise ValueError('Unexpected/missing release assets: '+repr(found^required))
    lines=[hashlib.sha256((directory/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in sorted(required)]
    (directory/'SHA256SUMS.txt').write_text(''.join(lines))
    (directory/'RELEASE-NOTES.md').write_text(
        f'Native Windows x64, macOS Intel and Nintendo Switch builds.\n\nCommit: `{sha}`\n\n'
        'Provide your own Ages ROM. No commercial ROM or player save is included.\n\n'
        'Switch: extract switch/ from the ZIP onto SD; requires a homebrew-capable environment. '
        'Hardware gameplay, ZL/ZR, audio and persistence must be tested on this exact NRO before publication.\n\n'
        'Windows: audited PE and native Windows smoke tests without ROM. macOS: ad-hoc signed, not notarized.\n'
        'See each BUILD manifest for tests performed/skipped and SHA256SUMS.txt for package hashes.\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
    v=sub.add_parser('validate');v.add_argument('--tag',required=True);v.add_argument('--candidate',action='store_true')
    r=sub.add_parser('prepare');r.add_argument('directory',type=Path);r.add_argument('--sha',required=True)
    a=p.parse_args()
    if a.command=='validate':
        version=Path('VERSION').read_text().strip();prerelease=validate(a.tag,version,a.candidate)
        if not a.candidate:subprocess.run(['git','merge-base','--is-ancestor','HEAD','origin/main'],check=True)
        import os
        with open(os.environ['GITHUB_OUTPUT'],'a') as f:f.write(f'tag={a.tag}\nversion={version}\nprerelease={str(prerelease).lower()}\n')
    else:prepare(a.directory,a.sha)
