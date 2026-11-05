"""Pinned actual EXEs + SDK receipt; no native execution, no generated fixture asset."""
from pathlib import Path
import hashlib,struct,re,json
ROOT=Path(__file__).resolve().parents[2];s=(ROOT/'src/inventory/carried_weight.cpp').read_text();out=[];checks=0
def check(b):
 global checks
 assert b;checks+=1
for name,path,sha,carry,weight,thunk,glob,appfn in (
('Steam',Path(r'D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe'),'504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e',0x5c7f40,0x746890,0x443c8,0x2132ea0,0x5b17f0),
('Gog',Path(r'D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-keo-gog-native/re_kenshi/kenshi_GOG_x64.exe'),'2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681',0x5c8250,0x7461c0,0x4443b,0x2130e10,0x5b1b00)):
 b=path.read_bytes();check(hashlib.sha256(b).hexdigest()==sha);pe=struct.unpack_from('<I',b,60)[0];n=struct.unpack_from('<H',b,pe+6)[0];off=pe+24+struct.unpack_from('<H',b,pe+20)[0];sections=[struct.unpack_from('<IIII',b,off+j*40+8)for j in range(n)]
 def read(r,k):
  for vs,va,rs,raw in sections:
   if va<=r<va+rs:return b[raw+r-va:raw+r-va+k]
  raise AssertionError(hex(r))
 def pin(key):return bytes(int(x,16)for x in re.search(r'k'+key+name+r'\[\d+\]=\{([^}]+)',s)[1].split(','))
 c=read(carry,44);check(c==pin('Carry'));check(read(weight,32)==pin('Weight'));t=read(thunk,5);check(t==pin('Thunk'))
 check(c[16]==0xe8 and carry+21+struct.unpack_from('<i',c,17)[0]==thunk)
 check(t[0]==0xe9 and thunk+5+struct.unpack_from('<i',t,1)[0]==weight)
 check(c[21:30]==bytes.fromhex('80bb48030000007408'))
 check(c[30:34]==bytes.fromhex('f30f5805') and carry+38+struct.unpack_from('<i',c,34)[0]==glob)
 check(c[38:]==bytes.fromhex('4883c4205bc3'))
 # The float global is zero-fill at runtime; disk cannot establish body weight.
 check(any(va<=glob<va+vs and glob>=va+rs for vs,va,rs,raw in sections))
 app=read(0xcd330,12);check(app[:8]==bytes.fromhex('488b8948040000e9'));a=0xcd330+12+struct.unpack_from('<i',app,8)[0];j=read(a,5);check(j[0]==0xe9 and a+5+struct.unpack_from('<i',j,1)[0]==appfn)
 check(read(appfn,15)==bytes.fromhex('488b81e8000000488b8048010000c3'))
 # Full pin rejects mutations in the global scalar opcode and thunk target too.
 mutated=bytearray(c);mutated[31]^=1;check(mutated!=pin('Carry'));mutated=bytearray(t);mutated[1]^=1;check(mutated!=pin('Thunk'))
 out.append(dict(profile=name,sha256=sha,carryRva=hex(carry),inventoryRva=hex(weight),thunkRva=hex(thunk),runtimeFloatRva=hex(glob),appearanceGetter=hex(appfn)))
check('c->animation&&c->animation->appearance?'in s)
receipt=ROOT/'build/carried-weight-tests/native-receipt.json';receipt.write_text(json.dumps(dict(checks=checks,profiles=out,contract='native carry = original inventory + runtime float32 iff carrying; appearance is nonallocating pointer loads; no runtime body value assumed'),indent=2))
print('PASS',checks,'actual Steam/GOG native boundary controls')
