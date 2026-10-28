#!/usr/bin/env python3
"""Inspect ELF/NRO structure and the native library; this is not a hardware test."""
import argparse,hashlib,json,pathlib,struct,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--llvm-bin',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
elf=(a.build/'oracles-switch.elf').read_bytes();nro=(a.build/'oracles-switch.nro').read_bytes()
assert elf[:6]==b'\x7fELF\x02\x01','Expected ELF64 little endian'
h=struct.unpack_from('<16sHHIQQQIHHHHHH',elf)
assert h[1]==3 and h[2]==183,'Expected AArch64 PIE'
ph=[struct.unpack_from('<IIQQQQQQ',elf,h[5]+i*h[9]) for i in range(h[10])]
loads=[x for x in ph if x[0]==1]
assert len(loads)==3 and [x[1] for x in loads]==[5,4,6], 'Unexpected segment permissions'
assert loads[0][3]<=h[4]<loads[0][3]+loads[0][5], 'Entry point outside text'
assert not any(x[0]==3 for x in ph), 'Unexpected ELF interpreter'
sections=[struct.unpack_from('<IIQQQQIIQQ',elf,h[6]+i*h[11]) for i in range(h[12])]
relocations=[]
for section in sections:
 if section[1]==4 and section[2]&2:
  assert section[9]==24
  relocations.extend(struct.unpack_from('<QQq',elf,j) for j in range(section[4],section[4]+section[5],24))
assert relocations and all((x[1]&0xffffffff)==1027 for x in relocations),'Non-relative dynamic relocation; check libnx loader support'
dynamic=next(x for x in ph if x[0]==2)
tags=[struct.unpack_from('<qQ',elf,j)[0] for j in range(dynamic[2],dynamic[2]+dynamic[5],16)]
assert 1 not in tags,'Unexpected DT_NEEDED library dependency'
assert nro[16:20]==b'NRO0','Missing NRO header'
size=struct.unpack_from('<I',nro,24)[0]
segments=[struct.unpack_from('<II',nro,32+i*8) for i in range(3)]
assert size<=len(nro) and all(off+sz<=size and off%4096==0 for off,sz in segments)
assert nro[size:size+4]==b'ASET','Missing NRO assets'
mod=struct.unpack_from('<I',nro,4)[0]
assert mod<size and nro[mod:mod+4]==b'MOD0','Invalid libnx module header'
icon_off,icon_size,nacp_off,nacp_size,romfs_off,romfs_size=struct.unpack_from('<QQQQQQ',nro,size+8)
assert icon_size>0 and nacp_size==0x4000 and romfs_size==0,'Missing metadata or unexpected embedded RomFS'
assert size+icon_off+icon_size<=len(nro) and size+nacp_off+nacp_size<=len(nro)
lib=a.build/'liboraclesnative.a'
members=subprocess.check_output([str(a.llvm_bin/'llvm-ar'),'t',str(lib)],text=True).splitlines()
assert len(members)>800
assert not any(x.endswith(('cpu.c.o','cpu.c.obj','cpu.o','cpu.obj')) for x in members),'Interpreter object present'
logo=bytes.fromhex('CEED6666CC0D000B03730083000C000D0008111F8889000EDCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E')
assert logo not in elf and logo not in nro,'ROM header embedded'
undefined=subprocess.check_output([str(a.llvm_bin/'llvm-nm'),'--undefined-only',str(a.build/'oracles-switch.elf')],text=True).strip()
strong=[x for x in undefined.splitlines() if ' U ' in ' '+x+' ']
assert not strong,'Unresolved symbols: '+repr(strong)
r={'format':'NRO / ELF64 AArch64 PIE','nro_bytes':len(nro),'nro_sha256':hashlib.sha256(nro).hexdigest(),'elf_sha256':hashlib.sha256(elf).hexdigest(),'entry':h[4],'segments':segments,'dynamic_relative_relocations':len(relocations),'native_objects':len(members),'interpreter_object':False,'rom_header_embedded':False,'romfs_bytes':romfs_size,'icon_bytes':icon_size,'nacp_bytes':nacp_size,'undefined_weak_symbols':undefined.splitlines(),'hardware_tested':False}
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
