#!/usr/bin/env python3
"""Package an audited NRO, test fixtures and corresponding project sources; never a ROM."""
import argparse,hashlib,io,json,pathlib,tarfile,zipfile
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--audit',type=pathlib.Path,required=True);p.add_argument('--verification',type=pathlib.Path,help='Manifest for this build (required in CI)');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
nro=(a.build/'oracles-switch.nro').read_bytes();audit=json.loads(a.audit.read_text())
assert hashlib.sha256(nro).hexdigest()==audit['nro_sha256']
assert not audit['rom_header_embedded'] and not audit['interpreter_object']
files={'switch/oracle-ages-native/oracles-switch.nro':nro,'LEIA-ME.txt':(root/'LEIA-ME-SWITCH.txt').read_bytes(),'SWITCH.md':(root/'SWITCH.md').read_bytes(),'LICENSE':(root/'LICENSE').read_bytes(),'build-audit.json':a.audit.read_bytes(),'sdk-lock.json':(root/'platform/switch/sdk-lock.json').read_bytes()}
profile='oracle-ages-native'
if b'sdmc:/switch/oracle-ages-lua-phase1/' in nro:profile='oracle-ages-lua-phase1'
elif b'sdmc:/switch/oracle-ages-lua/' in nro:profile='oracle-ages-lua'
if profile!='oracle-ages-native':
 nro_name='oracles-lua-phase1.nro' if profile=='oracle-ages-lua-phase1' else 'oracles-switch.nro'
 files['switch/'+profile+'/'+nro_name]=files.pop('switch/oracle-ages-native/oracles-switch.nro')
 if profile=='oracle-ages-lua-phase1':files['LEIA-ME.txt']=(root/'LUA-PHASE-1-PLATFORM-TESTS.md').read_bytes()
 else:files['LEIA-ME.txt']=b'Piloto C + Lua. Copie switch para a raiz do SD. ROM: sdmc:/switch/oracle-ages-lua/ages.gbc. Controles e detalhes em SWITCH.md.\n'
 files['Licenses/Lua-MIT-header.txt']=(root/'third_party/lua/lua.h').read_bytes()
verification=a.verification or root/'SWITCH-VERIFICATION.json'
if __import__('os').environ.get('GITHUB_ACTIONS')=='true' and not a.verification:raise ValueError('CI requires a fresh build manifest')
files['SWITCH-VERIFICATION.json']=verification.read_bytes()
if a.verification:
 manifest=json.loads(verification.read_text())
 assert manifest['binary']['sha256']==hashlib.sha256(nro).hexdigest(), 'Manifest does not match this NRO'
for name in ['ages-boot.state','ages.ref','ages-consoleverified.inputs']:
 files['switch/'+profile+'/tests/'+name]=(root/'tas'/name).read_bytes()
for f in sorted((root/'platform/switch/licenses').iterdir()):
 if f.is_file():files['Licenses/'+f.name]=f.read_bytes()
# Use the Git index plus explicit changed files, filtered through gitignore.
import subprocess
git_args=['git','-C',str(root),'ls-files','--cached']
if __import__('os').environ.get('GITHUB_ACTIONS')!='true':git_args+=['--others','--exclude-standard']
names=set(subprocess.check_output(git_args,text=True).splitlines())
buf=io.BytesIO()
with tarfile.open(fileobj=buf,mode='w:gz') as tar:
 for name in sorted(names):
  f=root/name
  # The upstream Git tree also tracks compiled diagnostic Game Boy ROMs.
  # Keep their source/docs, but omit all precompiled ROMs from this delivery.
  if name in {'SWITCH-VERIFICATION.json','WINDOWS-VERIFICATION.json'}: continue
  if f.suffix.lower() in {'.gb', '.gbc', '.sav', '.nro', '.elf', '.o', '.obj'}: continue
  assert not name.startswith(('roms/','build','sdcard/','toolchains/')) or name=='roms/.gitkeep',name
  if f.is_file():
   data=f.read_bytes();info=tarfile.TarInfo('oracle-ages-switch/'+name);info.size=len(data);info.mode=0o755 if name.endswith('.sh') else 0o644;tar.addfile(info,io.BytesIO(data))
files['Sources/oracle-ages-switch-source.tar.gz']=buf.getvalue()
files['SHA256SUMS.txt']=''.join(hashlib.sha256(v).hexdigest()+'  '+k+'\n' for k,v in sorted(files.items())).encode()
a.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o644<<16;z.writestr(info,data)
with zipfile.ZipFile(a.output) as z:assert z.testzip() is None
print(json.dumps({'zip':str(a.output),'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'bytes':a.output.stat().st_size,'rom_included':False,'hardware_tested':False},indent=2))
