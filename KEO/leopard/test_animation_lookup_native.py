"""Pinned Ogre file/ABI contracts, including chained unwind fragments."""
from pathlib import Path
import hashlib,json,re,struct,sys,os
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
ogre=PE(GAME/'OgreMain_x64.dll');checks=0
def check(v):
 global checks
 checks+=1
 assert v,checks
def read(r,n):return ogre.data[ogre.offset(r):ogre.offset(r)+n]
def relative(r):return r+5+struct.unpack('<i',read(r+1,4))[0]
check(hashlib.sha256(ogre.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
s=(ROOT/'src/fixes/world/equipment_animation_patch.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',s)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',s);check(len(rows)==10)
for r,n in rows:check(read(int(r,16),len(arrays[n]))==arrays[n])
check(len(arrays['proof3'])==0x2d95f-0x2d750)
check(relative(0x2b182)==0x2bd80 and relative(0x2bde8)==0x2df30 and relative(0x2bdfb)==0x2d750)
check(read(0x2bd95,4)==bytes.fromhex('80782900'))
check(read(0x2bda0,4)==bytes.fromhex('44394018'))
check(read(0x2bdc5,4)==bytes.fromhex('488d4320'))
check(read(0x2df5c,4)==bytes.fromhex('4c8b4308'))
check(read(0x2df82,2)==bytes.fromhex('8b07') and read(0x2df86,4)==bytes.fromhex('488b4708'))
check(read(0x2d7c2,4)==bytes.fromhex('4c8b4910'))
helper=(ROOT/'src/fixes/world/equipment_animation_lookup.cpp').read_text(encoding='utf-8')
check('Read<void*>(map,8),node' in helper and helper.index('void*node=allocate')<helper.index('Read<void*>(map,8),node'))
check('Read<unsigned>(last,0x18)' in helper and 'onOwnerThread' in helper)
out=ROOT/'build/animation-lookup-native-contract.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(dict(checks=checks,proof_bytes=sum(len(arrays[n])for _,n in rows),ogre_sha256=hashlib.sha256(ogre.data).hexdigest(),site='2B182',chained_hint_range='2D750..2D95F',game_execution=False,process_access=False),indent=2),encoding='utf-8');print('PASS',checks,'pinned native animation contracts')
