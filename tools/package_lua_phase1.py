#!/usr/bin/env python3
"""Package audited, opt-in Lua phase-1 builds and matching working-tree sources."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cache(path):
    result = {}
    for line in path.read_text().splitlines():
        if '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            result[key.split(':', 1)[0]] = value
    return result


def sources(root):
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']).split(b'\0')
    excluded = {'.gb', '.gbc', '.sav', '.nro', '.nacp', '.elf', '.o', '.obj', '.a', '.lib', '.so', '.dylib', '.pdb', '.exe', '.dll', '.dmg', '.zip', '.bak', '.tmp'}
    manifest = {'base_commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
                'tracked_diff_sha256': sha(subprocess.check_output(['git', '-C', str(root), 'diff', 'HEAD', '--binary'])),
                'kind': 'working-tree snapshot; not a clean committed release', 'files': {}}
    entries = {}
    for raw in sorted(set(names)):
        if not raw:
            continue
        name = raw.decode('utf-8')
        path = root / name
        if name.startswith(('roms/', 'build', 'reports/', 'sdcard/', 'toolchains/', 'ref/')):
            continue
        if name in {'WINDOWS-VERIFICATION.json', 'SWITCH-VERIFICATION.json'} or path.suffix.lower() in excluded:
            continue
        if path.suffix.lower() == '.state' and not (name.startswith('tas/') and name.endswith('-boot.state')):
            continue
        if path.is_symlink():
            raise ValueError('Unexpected source symlink: ' + name)
        if not path.is_file():
            continue
        entries[name] = path.read_bytes()
        manifest['files'][name] = sha(entries[name])
    entries['WORKTREE.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    output = io.BytesIO()
    with gzip.GzipFile(fileobj=output, mode='wb', mtime=0) as gz, tarfile.open(fileobj=gz, mode='w') as archive:
        for name, data in sorted(entries.items()):
            item = tarfile.TarInfo('oracle-ages-lua/' + name)
            item.size = len(data)
            item.mode = 0o755 if name.endswith(('.sh', '.py')) else 0o644
            archive.addfile(item, io.BytesIO(data))
    return output.getvalue(), manifest


def write_zip(path, files):
    files = dict(files)
    files['SHA256SUMS.txt'] = ''.join(sha(data) + '  ' + name + '\n' for name, data in sorted(files.items())).encode()
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            assert not name.startswith('/') and '..' not in Path(name).parts
            assert Path(name).suffix.lower() not in {'.gb', '.gbc', '.sav'}
            item = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = 0o644 << 16
            archive.writestr(item, data)
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        for name, data in files.items():
            assert archive.read(name) == data
    return {'file': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes()), 'rom_or_player_saves_included': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('windows-build', 'switch-build', 'windows-audit', 'switch-audit', 'llvm-root', 'sdl-source', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    exe = (a.windows_build / 'Oracles.exe').read_bytes()
    nro = (a.switch_build / 'oracles-switch.nro').read_bytes()
    windows = json.loads(a.windows_audit.read_text())
    switch = json.loads(a.switch_audit.read_text())
    assert windows['sha256'] == sha(exe) and not windows['private_dlls_required']
    assert windows['utf8_manifest'] and not windows['clr_header']
    assert switch['nro_sha256'] == sha(nro) and not switch['rom_header_embedded'] and not switch['interpreter_object']
    configs = [cache(b / 'CMakeCache.txt') for b in (a.windows_build, a.switch_build)]
    for config in configs:
        assert all(config.get(key) == 'ON' for key in ('ORACLES_LUA_GAMEPLAY', 'ORACLES_LUA_BLOCK_PUZZLES'))
        assert not config.get('ORACLES_VERSION'), 'Test packages must not enable release updates'
    revision = sha(''.join(f'{module}:{sha((root / ("gameplay/ages/" + module + ".lua")).read_bytes())};'
                          for module in ('switch_tiles', 'block_puzzles', 'block_rules')).encode())
    assert revision.encode() in exe and revision.encode() in nro
    assert b'oracles-lua-phase1' in exe and b'sdmc:/switch/oracle-ages-lua-phase1/' in nro
    for build in (a.windows_build, a.switch_build):
        symbols = subprocess.check_output([str(a.llvm_root / 'bin/llvm-nm'), '-A', '--undefined-only', str(build / 'liboraclesnative.a')], text=True)
        assert any('pushblockTrigger.c.' in line and 'gameplay_block_step' in line for line in symbols.splitlines()), 'Missing real block-to-Lua binding'
    helper = (a.windows_build / 'test_native_ages.exe').read_bytes()
    assert b'--tas-dir' in helper and b'--rom' in helper, 'Replay helper must accept portable paths'
    assert b'activeCodePage' in helper and b'UTF-8' in helper
    directed = (a.windows_build / 'test_lua_block_rules_rom.exe').read_bytes()
    assert b'--tas-dir' in directed and b'--rom' in directed
    assert b'activeCodePage' in directed and b'UTF-8' in directed
    assert revision.encode() in directed
    source_tar, source_manifest = sources(root)
    # Verify corresponding source bytes before distributing either binary.
    with tarfile.open(fileobj=io.BytesIO(source_tar), mode='r:gz') as archive:
        for module in ('switch_tiles', 'block_puzzles', 'block_rules'):
            name = 'gameplay/ages/' + module + '.lua'
            assert archive.extractfile('oracle-ages-lua/' + name).read() == (root/name).read_bytes()
        for entry in archive.getmembers():
            assert Path(entry.name).suffix.lower() not in {'.gb', '.gbc', '.sav', '.exe', '.nro'}
    build = {'date': '2026-10-30', 'base_commit': source_manifest['base_commit'], 'working_tree': True,
             'scope': 'expanded phase 1; 32 typed rules plus original trigger and switches',
             'features': {'lua_gameplay': True, 'lua_block_puzzles': True}, 'scripts_sha256': revision,
             'windows_real_or_vm_tested': False, 'switch_hardware_tested': False,
             'windows_binary_sha256': sha(exe), 'switch_nro_sha256': sha(nro),
             'source_archive_sha256': sha(source_tar), 'build_id': configs[1]['ORACLES_SWITCH_BUILD'],
             'profiles': {'windows': r'%APPDATA%\oracles-decomp\oracles-lua-phase1',
                          'switch': 'sdmc:/switch/oracle-ages-lua-phase1/'}}
    common = {'BUILD.json': (json.dumps(build, indent=2) + '\n').encode(), 'LICENSE': (root/'LICENSE').read_bytes(),
              'LEIA-ME.txt': (root/'LUA-PHASE-1-PLATFORM-TESTS.md').read_bytes(),
              'Sources/oracle-ages-lua-phase1-source.tar.gz': source_tar,
              'Licenses/Lua-README.md': (root/'third_party/lua/README.md').read_bytes(),
              'Licenses/Lua-MIT-header.txt': (root/'third_party/lua/lua.h').read_bytes()}
    wf = dict(common)
    wf.update({'Oracles-Windows-Lua-Phase1/Oracles-Lua-Fase1.exe': exe,
               'Oracles-Windows-Lua-Phase1/Tests/test_native_ages.exe': helper,
               'Oracles-Windows-Lua-Phase1/Tests/test_lua_gameplay.exe': (a.windows_build/'test_lua_gameplay.exe').read_bytes(),
               'Oracles-Windows-Lua-Phase1/Tests/test_lua_block_rules.exe': (a.windows_build/'test_lua_block_rules.exe').read_bytes(),
               'Oracles-Windows-Lua-Phase1/Tests/test_lua_block_rules_rom.exe': (a.windows_build/'test_lua_block_rules_rom.exe').read_bytes(),
               'Oracles-Windows-Lua-Phase1/Tests/oracles-native-run.exe': (a.windows_build/'oracles-native-run.exe').read_bytes(),
               'windows-audit.json': (json.dumps(dict(windows, file='Oracles-Lua-Fase1.exe'), indent=2)+'\n').encode(),
               'Licenses/SDL3.txt': (a.sdl_source/'LICENSE.txt').read_bytes(),
               'Licenses/LLVM-LICENSE.TXT': (a.llvm_root/'LICENSE.TXT').read_bytes()})
    for source, name in [('start_lua_phase1.cmd', 'Iniciar-Oracles-Lua.cmd'), ('validate_lua_phase1.cmd', 'Validar-Lua.cmd')]:
        wf['Oracles-Windows-Lua-Phase1/'+name] = (root/'platform/windows'/source).read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    for notice in sorted((a.llvm_root/'x86_64-w64-mingw32/share/mingw32').glob('COPYING*')):
        wf['Licenses/MinGW/'+notice.name] = notice.read_bytes()
    for name in ['clay/LICENSE.md', 'fonts/OFL.txt', 'hqx/COPYING', 'stb/stb_truetype.h', 'xbrz/xbrz.h']:
        wf['Licenses/'+name] = (root/'third_party'/name).read_bytes()
    sf = dict(common)
    sf['switch/oracle-ages-lua-phase1/oracles-lua-phase1.nro'] = nro
    sf['switch-audit.json'] = a.switch_audit.read_bytes()
    sf['SWITCH.md'] = (root/'SWITCH.md').read_bytes()
    sf['sdk-lock.json'] = (root/'platform/switch/sdk-lock.json').read_bytes()
    for name in ('ages-boot.state', 'ages.ref', 'ages-consoleverified.inputs'):
        data = (root/'tas'/name).read_bytes()
        wf['Oracles-Windows-Lua-Phase1/Tests/'+name] = data
        sf['switch/oracle-ages-lua-phase1/tests/'+name] = data
    for notice in sorted((root/'platform/switch/licenses').iterdir()):
        if notice.is_file(): sf['Licenses/'+notice.name] = notice.read_bytes()
    a.output.mkdir(parents=True, exist_ok=True)
    packages = [write_zip(a.output/'Oracles-Windows-Lua-Phase1-Expanded.zip', wf), write_zip(a.output/'Oracles-Switch-Lua-Phase1-Expanded.zip', sf)]
    result = dict(build, packages=packages)
    (a.output/'oracle-lua-phase1-expanded-builds.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
