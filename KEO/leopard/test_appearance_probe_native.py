from pathlib import Path
import re,struct,hashlib,importlib.util,json
s=Path(__file__).resolve().parents[1];game=s.parents[1]
spec=importlib.util.spec_from_file_location('pe',s/'tools/kenshilib/check_imports.py')
pem=importlib.util.module_from_spec(spec);spec.loader.exec_module(pem)
desc=json.loads((s/'leopard/appearance-seam-descriptors-20261115.json').read_text())
source=(s/'src/resources/resource_load_probe.cpp').read_text()
def exports(pe):
 at=pe.offset(pe.directories[0][0]);count=pe.u32(at+24)
 names=pe.offset(pe.u32(at+32));ords=pe.offset(pe.u32(at+36));funcs=pe.offset(pe.u32(at+28))
 return {pe.string(pe.u32(names+4*i)):pe.u32(funcs+4*pe.u16(ords+2*i)) for i in range(count)}
kl=pem.PE(game/'KenshiLib.dll');symbols=exports(kl)
table=struct.unpack('<7630I',(game/'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes())
names=['morphLookup','morphCreate','morphSwitch','animationBody','attachmentBuild','poseBlend']
checks=0
for seam,name in zip(desc['seams'],names):
 path=game/seam['module'];pe=pem.PE(path);data=path.read_bytes()
 expectedSHA=desc['ogreSHA256' if name=='poseBlend' else 'exeSHA256']
 assert hashlib.sha256(data).hexdigest()==expectedSHA
 rva=int(seam['rva'],16);actual=pe.data[pe.offset(rva):pe.offset(rva)+32]
 declared=bytes(int(b.strip(),16) for b in re.search(r'unsigned char '+name+r'Bytes\[32\]=\{([^}]+)\}',source)[1].split(','))
 assert len(declared)==32 and declared==actual==bytes.fromhex(seam['prefix32']),name
 checks+=1
 if name=='poseBlend':
  assert exports(pe)[seam['symbol']]==rva
  assert 'GetProcAddress(ogre,"'+seam['symbol']+'")' in source
 else:
  stub=symbols[seam['symbol']];assert (stub-0x15B46)//6==seam['klibSlot'] and (stub-0x15B46)%6==0
  assert table[seam['klibSlot']]==rva
 checks+=1
print('PASS',checks,'actual image/prefix/export/table preparation contracts')
