"""Pinned on-disk native stream contracts; no game/process access."""
from pathlib import Path
import sys,hashlib,struct,re,json,os
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
p=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');checks=0
def check(value):
 global checks
 checks+=1
 assert value,checks
def read(r,n):
 o=p.offset(r);return p.data[o:o+n]
def relative(r):return r+5+struct.unpack('<i',read(r+1,4))[0]
check(hashlib.sha256(p.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
pe=p.u32(0x3c);check(p.u32(pe+8)==0x65d604d7 and p.u32(pe+24+56)==0x232c000)
source=(ROOT/'src/fixes/world/save_read_buffer.cpp').read_text()
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',source);check(len(rows)==8)
for r,n in rows:check(read(int(r,16),len(arrays[n]))==arrays[n])
check(read(0x6c0927,1)==b'\xe8' and relative(0x6c0927)==0x38b95)
check(read(0x38b95,1)==b'\xe9' and relative(0x38b95)==0x6c8330)
check(read(0x13a20,1)==b'\xe9' and relative(0x13a20)==0x1323c0)
check(struct.unpack('<Q',read(0x168ef78,8))[0]==p.image_base+0x13a20)
def imported(r):
 check(read(r,2)==b'\xff\x15')
 iat=r+6+struct.unpack('<i',read(r+2,4))[0]
 return p.string(struct.unpack('<Q',read(iat,8))[0]+2)
check(imported(0x1323ef)=='setvbuf')
check(imported(0x6c0a94).startswith('?read@?$basic_istream@'))
check('codecvt||!file||file->_base||file->_ptr||file->_cnt||file->_flag!=_IOREAD' in source)
check('size.QuadPart<128*1024' in source)
check('sizeof(std::ifstream)==0x110&&sizeof(std::filebuf)==0xa0' in source)
record={'checks':checks,'game_sha256':hashlib.sha256(p.data).hexdigest(),'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'proofs':[{'rva':r,'bytes':len(arrays[n]),'sha256':hashlib.sha256(arrays[n]).hexdigest()}for r,n in rows],'game_execution':False,'process_access':False,'native_caller':'0x6c092c','filebuf_vtable':'0x168ef18','setbuf_virtual_slot':'0x60'}
out=ROOT/'build/save-read-native-contract.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(record,indent=2))
print('PASS',checks,'offline pinned native save stream contracts')
