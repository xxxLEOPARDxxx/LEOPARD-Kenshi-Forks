"""Pinned file/frame contracts; no game or process execution/access."""
from pathlib import Path
import hashlib,json,re,struct,sys,os
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
game=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');checks=0
def check(v):
 global checks
 checks+=1
 assert v,checks
def read(r,n):return game.data[game.offset(r):game.offset(r)+n]
def relative(r):return r+5+struct.unpack('<i',read(r+1,4))[0]
check(hashlib.sha256(game.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
s=(ROOT/'src/fixes/world/save_instance_lookup.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',s)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',s);check(len(rows)==10)
for r,n in rows:check(read(int(r,16),len(arrays[n]))==arrays[n])
check(len(arrays['proof0'])==306 and len(arrays['proof1'])==466 and len(arrays['proof2'])==186)
check(relative(0x6cab42)==relative(0x100667)==0x515c3 and relative(0x515c3)==0xfe310)
check(relative(0x6cab7d)==0x21643 and relative(0x21643)==0x100630)
check(read(0x100633,4)==bytes.fromhex('55574154') and read(0x10063b,7)==bytes.fromhex('4881ec40010000'))
check(read(0x6cab47,5)==bytes.fromhex('4889442470'))
check(read(0x6cab1f,9)==bytes.fromhex('57415441554883ec50'))
check(relative(0x6c21c2)==0x2d0a1 and relative(0x2d0a1)==0x6cab10)
check(relative(0x6cab59)==0x24037 and relative(0x24037)==0x691a0)
check(relative(0x691d3)==0xed5c12)
slot=0xed5c12+6+struct.unpack('<i',read(0xed5c14,4))[0];check(slot==0x2244b08)
name=struct.unpack('<Q',read(slot,8))[0];check(game.string(name+2)=='memcmp')
check(('MSVCR100.dll','memcmp') in game.imports())
check(3*8+0x140+8==0x160 and 0x160+8+0x70==0x1d8)
check(0x160+8+3*8+0x50==0x1d0 and 0x160-3*8==0x148 and 0x160-2*8==0x150)
check('*(const uintptr_t*)slot==innerReturn' in s and 'slot+0x160' in s and 'slot+0x1d8' in s)
out=ROOT/'build/save-instance-native-contract.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps(dict(checks=checks,proof_bytes=sum(len(arrays[n])for _,n in rows),game_sha256=hashlib.sha256(game.data).hexdigest(),site='100667',node_frame_offset='1D8',game_execution=False,process_access=False),indent=2),encoding='utf-8')
print('PASS',checks,'pinned saved-object lookup contracts')
