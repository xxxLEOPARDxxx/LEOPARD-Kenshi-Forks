"""Pinned on-disk parser/string ownership contract. Never runs Kenshi."""
from pathlib import Path
import sys,struct,re,hashlib,json,os
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
p=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');checks=0
def check(v):
 global checks
 checks+=1
 assert v,checks
def read(r,n):return p.data[p.offset(r):p.offset(r)+n]
def relative(r):return r+5+struct.unpack('<i',read(r+1,4))[0]
check(hashlib.sha256(p.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
source=(ROOT/'src/fixes/world/save_string_transfer.cpp').read_text()
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',source);check(len(rows)==7)
for r,n in rows:check(read(int(r,16),len(arrays[n]))==arrays[n])
check(relative(0x544b7)==0xa47b0)
for r in [0x6c1eb7,0x6c1f42,0x6c206b,0x6cac1c]:check(read(r,1)==b'\xe8' and relative(r)==0x544b7)
# Native helper copies to slot[count], increments count, frees/empties arg.
check(relative(0xa4806)==0x2808d and relative(0xa4818)==0xed5628)
check(relative(0x6cac0e)==0x2808d)
check(read(0xed5628,2)==b'\xff\x25')
iat=0xed5628+6+struct.unpack('<i',read(0xed5628+2,4))[0]
imp=struct.unpack('<Q',read(iat,8))[0]
check(imp==0x80000000000004fe)
check(('mfc100u.dll','#1278') in p.imports())
# Transfer deliberately does not allocate/free. Native cleanup retains the
# original allocation domain, independently of MFC's ordinal implementation.
check('value->size()<=15' in source and 'capacity>65536' in source)
check('slot.empty()&&slot.capacity()==15' in source)
check('list->data[list->count].swap(*value)' in source and 'std::string old' not in source)
out=ROOT/'build/save-string-native-contract.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps({'checks':checks,'game_sha256':hashlib.sha256(p.data).hexdigest(),'proof_bytes':sum(len(arrays[n])for _,n in rows),'native_delete':'mfc100u.dll ordinal1278','plugin_allocations_or_frees':False,'game_execution':False,'process_access':False},indent=2))
print('PASS',checks,'offline pinned parser string contracts')
