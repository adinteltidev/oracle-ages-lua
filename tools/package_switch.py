#!/usr/bin/env python3
"""Package an audited NRO, test fixtures and corresponding project sources; never a ROM."""
import argparse,hashlib,io,json,pathlib,tarfile,zipfile
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--audit',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
nro=(a.build/'oracles-switch.nro').read_bytes();audit=json.loads(a.audit.read_text())
assert hashlib.sha256(nro).hexdigest()==audit['nro_sha256']
assert not audit['rom_header_embedded'] and not audit['interpreter_object']
files={'switch/oracle-ages-native/oracles-switch.nro':nro,'LEIA-ME.txt':(root/'LEIA-ME-SWITCH.txt').read_bytes(),'SWITCH.md':(root/'SWITCH.md').read_bytes(),'LICENSE':(root/'LICENSE').read_bytes(),'build-audit.json':a.audit.read_bytes(),'sdk-lock.json':(root/'platform/switch/sdk-lock.json').read_bytes()}
files['SWITCH-VERIFICATION.json']=(root/'SWITCH-VERIFICATION.json').read_bytes()
for name in ['ages-boot.state','ages.ref','ages-consoleverified.inputs']:
 files['switch/oracle-ages-native/tests/'+name]=(root/'tas'/name).read_bytes()
for f in sorted((root/'platform/switch/licenses').iterdir()):
 if f.is_file():files['Licenses/'+f.name]=f.read_bytes()
# Use the Git index plus explicit changed files, filtered through gitignore.
import subprocess
names=set(subprocess.check_output(['git','-C',str(root),'ls-files','--cached','--others','--exclude-standard'],text=True).splitlines())
buf=io.BytesIO()
with tarfile.open(fileobj=buf,mode='w:gz') as tar:
 for name in sorted(names):
  f=root/name
  # The upstream Git tree also tracks compiled diagnostic Game Boy ROMs.
  # Keep their source/docs, but omit all precompiled ROMs from this delivery.
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
