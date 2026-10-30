#!/usr/bin/env python3
"""Package the opt-in phase-2 binaries with matching sources and private profiles."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
from package_lua_phase1 import sources, write_zip, cache

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['windows-build','switch-build','windows-audit','switch-audit','llvm-root','sdl-source','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--macos-build',type=Path)
    a=p.parse_args(); root=Path(__file__).resolve().parents[1]
    modules=['switch_tiles','block_puzzles','block_rules','colored_cube','levers_lava','doors']
    revision=sha(''.join(f'{m}:{sha((root/"gameplay/ages"/(m+".lua")).read_bytes())};' for m in modules).encode())
    exe=(a.windows_build/'Oracles.exe').read_bytes(); nro=(a.switch_build/'oracles-switch.nro').read_bytes()
    win=json.loads(a.windows_audit.read_text()); switch=json.loads(a.switch_audit.read_text())
    assert win['sha256']==sha(exe) and win['utf8_manifest'] and not win['private_dlls_required'] and not win['clr_header']
    assert switch['nro_sha256']==sha(nro) and not switch['interpreter_object'] and not switch['rom_header_embedded']
    builds=[a.windows_build,a.switch_build]+([a.macos_build] if a.macos_build else [])
    for build in builds:
        config=cache(build/'CMakeCache.txt')
        assert all(config.get(flag)=='ON' for flag in ['ORACLES_LUA_GAMEPLAY','ORACLES_LUA_BLOCK_PUZZLES','ORACLES_LUA_COLORED_CUBE'])
        assert config.get('ORACLES_LUA_LEVERS_LAVA', 'OFF') != 'ON', 'Use the phase-3 packager'
        assert config.get('ORACLES_LUA_DOORS', 'OFF') != 'ON', 'Use the phase-4 packager'
        assert not config.get('ORACLES_VERSION'), 'Experimental builds cannot enable stable release updates'
    assert b'oracles-lua-phase2' in exe and b'sdmc:/switch/oracle-ages-lua-phase2/' in nro
    assert revision.encode() in exe and revision.encode() in nro
    for build in [a.windows_build,a.switch_build]:
        symbols=subprocess.check_output([str(a.llvm_root/'bin/llvm-nm'),'-A','--undefined-only',str(build/'liboraclesnative.a')],text=True)
        for rule in ['present','visible','palette']:
            assert any('coloredCubeFlame.c.' in line and 'gameplay_colored_cube_'+rule in line for line in symbols.splitlines()),rule
    source,manifest=sources(root)
    metadata={'date':'2026-10-30','branch':'port/lua-gameplay','base_commit':manifest['base_commit'],'working_tree':True,
        'features':{'lua_gameplay':True,'lua_block_puzzles':True,'lua_colored_cube':True},'scripts_sha256':revision,
        'windows_binary_sha256':sha(exe),'switch_nro_sha256':sha(nro),'source_archive_sha256':sha(source),
        'windows_runtime_tested':False,'switch_hardware_tested':False,'phase2_acceptance_complete':False,
        'profiles':{'desktop':'oracles-decomp/oracles-lua-phase2','switch':'sdmc:/switch/oracle-ages-lua-phase2/'}}
    common={'BUILD.json':(json.dumps(metadata,indent=2)+'\n').encode(),'LICENSE':(root/'LICENSE').read_bytes(),
        'LEIA-ME.txt':(root/'LUA-PHASE-2-PLATFORM-TESTS.md').read_bytes(),'Sources/oracle-ages-lua-phase2-source.tar.gz':source}
    for name in ['lua/README.md','lua/lua.h','clay/LICENSE.md','fonts/OFL.txt','hqx/COPYING','stb/stb_truetype.h','xbrz/xbrz.h']:
        common['Licenses/'+name]=(root/'third_party'/name).read_bytes()
    wf=dict(common); sf=dict(common); prefix='Oracles-Windows-Lua-Phase2/'
    wf[prefix+'Oracles-Lua-Fase2.exe']=exe
    for name in ['test_lua_gameplay.exe','test_lua_block_rules.exe','test_lua_colored_cube.exe','test_lua_colored_cube_rom.exe','test_native_ages.exe','oracles-native-run.exe']:
        data=(a.windows_build/name).read_bytes(); assert revision.encode() in data,name
        if name in ['test_native_ages.exe','test_lua_colored_cube_rom.exe']:
            assert b'--rom' in data and b'--tas-dir' in data and b'activeCodePage' in data and b'UTF-8' in data
        wf[prefix+'Tests/'+name]=data
    for src,dest in [('start_lua_phase2.cmd','Iniciar-Oracles-Lua.cmd'),('validate_lua_phase2.cmd','Validar-Lua.cmd')]:
        wf[prefix+dest]=(root/'platform/windows'/src).read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')
    wf['windows-audit.json']=a.windows_audit.read_bytes();wf['Licenses/SDL3.txt']=(a.sdl_source/'LICENSE.txt').read_bytes()
    wf['Licenses/LLVM-LICENSE.TXT']=(a.llvm_root/'LICENSE.TXT').read_bytes()
    for notice in sorted((a.llvm_root/'x86_64-w64-mingw32/share/mingw32').glob('COPYING*')):wf['Licenses/MinGW/'+notice.name]=notice.read_bytes()
    sf['switch/oracle-ages-lua-phase2/oracles-lua-phase2.nro']=nro;sf['switch-audit.json']=a.switch_audit.read_bytes()
    sf['SWITCH.md']=(root/'SWITCH.md').read_bytes();sf['sdk-lock.json']=(root/'platform/switch/sdk-lock.json').read_bytes()
    for notice in (root/'platform/switch/licenses').iterdir():
        if notice.is_file():sf['Licenses/'+notice.name]=notice.read_bytes()
    for name in ['ages-boot.state','ages.ref','ages-consoleverified.inputs']:
        data=(root/'tas'/name).read_bytes();wf[prefix+'Tests/'+name]=data;sf['switch/oracle-ages-lua-phase2/tests/'+name]=data
    a.output.mkdir(parents=True,exist_ok=True)
    packages=[write_zip(a.output/'Oracles-Windows-Lua-Phase2.zip',wf),write_zip(a.output/'Oracles-Switch-Lua-Phase2.zip',sf)]
    if a.macos_build:
        stage=root/'reports/lua-phase2/macos-dmg-stage';stage.mkdir(parents=True,exist_ok=True)
        app=stage/'Oracles Lua Phase2.app'
        if app.exists():shutil.rmtree(app)
        subprocess.run(['ditto',str(a.macos_build/'Oracles.app'),str(app)],check=True)
        binary=app/'Contents/MacOS/Oracles'; assert revision.encode() in binary.read_bytes() and b'oracles-lua-phase2' in binary.read_bytes()
        info=app/'Contents/Info.plist';data=plistlib.loads(info.read_bytes());data.update(CFBundleName='Oracles Lua Phase2',CFBundleDisplayName='Oracles Lua Phase2',CFBundleIdentifier='dev.oracles-decomp.oracles.lua-phase2');info.write_bytes(plistlib.dumps(data))
        for name,data in common.items():
            path=stage/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        for name,data in wf.items():
            if name.startswith('Licenses/'):
                path=app/'Contents/Resources/Phase2'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        # A signed executable hash cannot be embedded in its own resource seal.
        subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
        mac=dict(metadata,macos_binary_sha256=sha(binary.read_bytes()),macos_notarized=False)
        (stage/'BUILD.json').write_text(json.dumps(mac,indent=2)+'\n')
        audit=root/'reports/lua-phase2/macos-audit.json'
        subprocess.run(['python3',str(root/'tools/ci/macos_audit.py'),str(app),'--output',str(audit)],check=True)
        assert json.loads(audit.read_text())['sha256']==mac['macos_binary_sha256']
        (stage/'macos-audit.json').write_bytes(audit.read_bytes())
        if not (stage/'Applications').is_symlink():(stage/'Applications').symlink_to('/Applications')
        dmg=a.output/'Oracles-macOS-Lua-Phase2-Intel.dmg'
        subprocess.run(['hdiutil','create','-volname','Oracles Lua Phase2','-srcfolder',str(stage),'-format','UDZO','-ov',str(dmg)],check=True)
        subprocess.run(['hdiutil','verify',str(dmg)],check=True)
        packages.append({'file':dmg.name,'bytes':dmg.stat().st_size,'sha256':sha(dmg.read_bytes()),'rom_or_player_saves_included':False})
    result=dict(metadata,packages=packages)
    (a.output/'oracle-lua-phase2-builds.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
