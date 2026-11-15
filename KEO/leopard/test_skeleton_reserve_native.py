"""Offline pinned Ogre implementation/ABI evidence. Never opens a process."""
from pathlib import Path
import sys,os,hashlib,re,struct,json
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
p=PE(GAME/'OgreMain_x64.dll');checks=0

def check(condition):
 global checks
 checks+=1
 assert condition,checks

def read(r,n):
 o=p.offset(r);return p.data[o:o+n]
def call(r):
 check(read(r,1)==b'\xe8');return r+5+struct.unpack('<i',read(r+1,4))[0]
check(hashlib.sha256(p.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
pe=struct.unpack_from('<I',p.data,0x3c)[0];opt=pe+24
check(p.u32(pe+8)==0x5ca5f929 and p.u32(opt+56)==0x9c9000)
e=p.offset(p.directories[0][0]);names=p.offset(p.u32(e+32));ords=p.offset(p.u32(e+36));funcs=p.offset(p.u32(e+28));exports={p.string(p.u32(names+i*4)):p.u32(funcs+4*p.u16(ords+i*2)) for i in range(p.u32(e+24))}
check(exports['?loadImpl@OldSkeletonInstance@Ogre@@MEAAXXZ']==0x1e6db0)
check(call(0x1e681e)==0x342030)
check(call(0x342df4)==0x3469e0 and call(0x343170)==0x3469e0)
check(read(0xbfb9e,5)==b'\xba\xf8\x02\x00\x00' and call(0xbfbc4)==0x1e6800)
# Native constructor vtable supplies exactly the two fixture probe signatures.
check(read(0x1e6824,3)==b'\x48\x8d\x05')
vt=0x1e6824+7+struct.unpack('<i',read(0x1e6824+3,4))[0]
check(struct.unpack('<Q',read(vt+0x168,8))[0]-p.image_base==0x342e40)
check(struct.unpack('<Q',read(vt+0x178,8))[0]-p.image_base==0x342bc0)
source=(ROOT/'src/fixes/world/skeleton_reserve.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in vals.split(',')) for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',source);check(len(rows)==4)
for r,n in rows:check(read(int(r,16),len(arrays[n]))==arrays[n])
check('static_assert(offsetof(BonesAccess,mBoneList)==0x178' in source)
check('static_assert(sizeof(Ogre::Skeleton::BoneList)==32' in source)
check('memcpy(&master,static_cast<const char*>(self)+0x290,sizeof(master))' in source)
check(not re.search(r'(?:static|reinterpret)_cast\s*<[^>]*OldSkeletonInstance',source))
functions=[(0x342030,0xed),(0x1e6800,0x8c),(0x1e6db0,0xa2),(0x1e6c00,0x1a3),(0x342bc0,0x280),(0x342e40,0x37f),(0x3469e0,0x36)]
record={'checks':checks,'module_sha256':hashlib.sha256(p.data).hexdigest(),'functions':[{'rva':hex(r),'size':n,'sha256':hashlib.sha256(read(r,n)).hexdigest()}for r,n in functions],'process_access':False}
out=ROOT/'build/skeleton-reserve-native-contract.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(record,indent=2),encoding='utf-8')
print('PASS',checks,'offline pinned native contracts')
