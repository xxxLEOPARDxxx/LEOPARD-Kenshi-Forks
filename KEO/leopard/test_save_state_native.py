"""Pinned file contracts for the CPU state-list handoff. No live game access."""
from pathlib import Path
import hashlib,json,re,struct,sys,os
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
game=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');ogre=PE(GAME/'OgreMain_x64.dll');checks=0
def check(v):
 global checks
 checks+=1
 assert v,checks
def read(p,r,n):return p.data[p.offset(r):p.offset(r)+n]
def relative(r):return r+5+struct.unpack('<i',read(game,r+1,4))[0]
def lea(r):return r+7+struct.unpack('<i',read(game,r+3,4))[0]
check(hashlib.sha256(game.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
check(hashlib.sha256(ogre.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
s=(ROOT/'src/fixes/world/save_state_list_transfer.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+|quaternionProof)\[\]=\{([^}]+)\}',s)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',s);check(len(rows)==6)
for r,n in rows:check(read(game,int(r,16),len(arrays[n]))==arrays[n])
check(len(arrays['proof0'])==306 and len(arrays['quaternionProof'])==26)
check(read(ogre,0x29e90,26)==arrays['quaternionProof'])
check('??4Quaternion@Ogre@@QEAAAEAV01@AEBV01@@Z' in ogre.exports())
check(read(game,0x6cabc7,2)==b'\xff\x15')
slot=0x6cabcd+struct.unpack('<i',read(game,0x6cabc9,4))[0];check(slot==0x2247000)
name_rva=struct.unpack('<Q',read(game,slot,8))[0]
check(game.string(name_rva+2)=='??4Quaternion@Ogre@@QEAAAEAV01@AEBV01@@Z')
check(('OgreMain_x64.dll','??4Quaternion@Ogre@@QEAAAEAV01@AEBV01@@Z') in game.imports())
check(read(game,0x6cab1f,9)==bytes.fromhex('57415441554883ec50'))
check(read(game,0x6cabc3,4)==bytes.fromhex('488d4f0c'))
check(read(game,0x6cabcd,8)==bytes.fromhex('488bb424a0000000'))
check(relative(0x6c21c2)==0x2d0a1 and relative(0x2d0a1)==0x6cab10)
check(read(game,0x6c218a,12)==bytes.fromhex('488d85900100004889442430'))
# Source and final object's destructor use the same string destructor callback,
# array cookie DWORD, 40-byte stride and native array delete.
check(lea(0x9aa10)==lea(0x6c222c))
check(relative(0x9aa1f)==relative(0x6c223b)==0xed5cb0)
check(relative(0x9aa27)==relative(0x6c2243)==0xed562e)
check(lea(0x9a9f9)==lea(0x6c220e))
check(read(game,0x9f914,7)==bytes.fromhex('448920488d6808'))
check(read(game,0x9f98b,7)==bytes.fromhex('48896f1089770c'))
check(0x18+0x50==0x68 and 0x68+8==0x70 and 0xa0+8==0xa8 and 0xc+0x44==0x50)
out=ROOT/'build/save-state-native-contract.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps({'checks':checks,'game_sha256':hashlib.sha256(game.data).hexdigest(),'ogre_sha256':hashlib.sha256(ogre.data).hexdigest(),'proof_bytes':sum(len(arrays[n])for _,n in rows)+26,'site':'0x6cabc7','inner_return':'0x6cabcd','outer_return':'0x6c21c7','callsite_only':True,'game_execution':False,'process_access':False},indent=2))
print('PASS',checks,'offline pinned state-list transfer contracts')
