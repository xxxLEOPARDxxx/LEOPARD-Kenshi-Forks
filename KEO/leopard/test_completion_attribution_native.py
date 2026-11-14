from pathlib import Path
import re,struct,hashlib,importlib.util
s=Path(__file__).resolve().parents[1];game=s.parents[1]
spec=importlib.util.spec_from_file_location('pe',s/'tools/kenshilib/check_imports.py')
pem=importlib.util.module_from_spec(spec);spec.loader.exec_module(pem)
exe=game/'RE_Kenshi/Kenshi_x64.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
pe=pem.PE(exe);source=(s/'src/resources/resource_load_probe.cpp').read_text()
checks=0
for name,rva in [('textureBytes',0x44ACD0),('bodyBytes',0x538590),('attachmentBytes',0x536590)]:
 raw=re.search(r'unsigned char '+name+r'\[32\]=\{([^}]+)\}',source)[1]
 expected=bytes(int(v.strip(),16) for v in raw.split(','))
 actual=pe.data[pe.offset(rva):pe.offset(rva)+32]
 assert len(expected)==32 and expected==actual,name
 checks+=1
kl=pem.PE(game/'KenshiLib.dll');table=struct.unpack('<7630I',(game/'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes())
export=kl.offset(kl.directories[0][0]);count=kl.u32(export+24)
names=kl.offset(kl.u32(export+32));ordinals=kl.offset(kl.u32(export+36));functions=kl.offset(kl.u32(export+28))
resolved={}
for i in range(count):
 name=kl.string(kl.u32(names+4*i));ordinal=kl.u16(ordinals+2*i);stub=kl.u32(functions+4*ordinal)
 if any(key in name for key in ['?getTexture@TextureLoadData@ResourceLoader','?buildBody@AppearanceBase','?createEntityCallback@AppearanceBase']):
  assert (stub-0x15B46)%6==0
  resolved[name]=table[(stub-0x15B46)//6]
for prefix,rva in [('?getTexture@TextureLoadData@ResourceLoader',0x44ACD0),('?buildBody@AppearanceBase',0x538590),('?createEntityCallback@AppearanceBase',0x536590)]:
 matches=[v for k,v in resolved.items() if k.startswith(prefix)];assert matches==[rva],(prefix,matches);checks+=1
bridge=(s/'src/resources/resource_gpu_bridge.cpp').read_text()
assert bridge.count('KenshiLib::AddHook(target,(void*)&LoadedArray,')==1
assert bridge.count('ResourceLoadProbeArrayBegin()')==1 and bridge.count('ResourceLoadProbeArrayEnd(begin)')==1
checks+=2
print('PASS',checks,'actual native module/byte/one-array-hook attribution checks')
