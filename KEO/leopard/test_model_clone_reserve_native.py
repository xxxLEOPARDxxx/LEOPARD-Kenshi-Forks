"""Offline pinned contracts for the native-only model-clone fixture. No process access."""
from pathlib import Path
import sys, os, struct, hashlib, json, re
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
ogre=PE(GAME/'OgreMain_x64.dll');sdk=PE(GAME/'KenshiLib.dll');checks=0
def check(value):
 global checks
 checks+=1
 assert value,checks
def read(p,r,n):
 o=p.offset(r);return p.data[o:o+n]
def call(p,r):
 check(read(p,r,1)==b'\xe8')
 return r+5+struct.unpack('<i',read(p,r+1,4))[0]
def exports(p):
 e=p.offset(p.directories[0][0]);names=p.offset(p.u32(e+32));ords=p.offset(p.u32(e+36));funcs=p.offset(p.u32(e+28))
 return {p.string(p.u32(names+4*i)):p.u32(funcs+4*p.u16(ords+2*i)) for i in range(p.u32(e+24))}
check(hashlib.sha256(ogre.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
check(hashlib.sha256(sdk.data).hexdigest()=='5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b')
pe=ogre.u32(0x3c);check(ogre.u32(pe+8)==0x5ca5f929 and ogre.u32(pe+24+56)==0x9c9000)
e=exports(ogre)
for substring,rva in [('?clone@Mesh@',0x1a3ee0),('?createManual@MeshManager@',0x1be970),('?getPoseList@Mesh@',0x1ab490),('?clone@Pose@',0x229bf0)]:
 check([r for n,r in e.items() if n.startswith(substring)]==[rva])
check(call(ogre,0x1a3fd8)==0x1be970)
check(0x1a3fd8+5==0x1a3fdd)
check(call(ogre,0x36a89e)==0x1a3100 and call(ogre,0x1a311a)==0x1a2ff0)
check(call(ogre,0x1a3074)==0x1afb00 and call(ogre,0x1a30aa)==0x1afb00)
check(call(ogre,0x1a4491)==0x1b2300 and call(ogre,0x1a4509)==0x1b2300)
check(read(ogre,0x1ab490,8)==bytes.fromhex('488d8148030000c3'))
source=(ROOT/'src/fixes/world/model_clone_reserve.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',source);check(len(rows)==6)
for r,n in rows:check(read(ogre,int(r,16),len(arrays[n]))==arrays[n])
# This test-only call enters MinHook initialization directly, never InitRVAs.
check(call(sdk,0x227f)==0x14884)
check(read(sdk,0x14884,32)==bytes.fromhex('40534883ec60488b05ff150d004833c4488944245833db48391d862b0d007408'))
check('?AddHook@KenshiLib@@YA?AW4HookStatus@1@PEAX0PEAPEAX@Z' in exports(sdk))
fixture=(ROOT/'leopard/test_model_clone_reserve.cpp').read_text(encoding='utf-8')
check(fixture.index('C(DiskHash(sdk,')<fixture.index('((Init)((char*)sdk+0x14884))()'))
check('5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b' in fixture)
check('static_assert(offsetof(MeshAccess,mSubMeshList)==0x170' in source)
check('static_assert(offsetof(MeshAccess,mPoseList)==0x348' in source)
check('sizeof(Ogre::Mesh::SubMeshList)==32&&sizeof(Ogre::PoseList)==32' in source)
check('Bone' not in source and 'Entity' not in source)
functions=[(0x1a28f0,0x3ca),(0x1a3ee0,0x6d5),(0x1be970,0x31),(0x1a2ff0,0x105),(0x1a3100,0x5f),(0x1ab490,8),(0x229bf0,0x115)]
record={'checks':checks,'ogre_sha256':hashlib.sha256(ogre.data).hexdigest(),'sdk_sha256':hashlib.sha256(sdk.data).hexdigest(),'functions':[{'rva':hex(r),'size':n,'sha256':hashlib.sha256(read(ogre,r,n)).hexdigest()}for r,n in functions],'sdk_private_test_init':{'rva':'0x14884','proven_call_rva':'0x227f','production_use':False},'process_access':False,'game_execution':False,'native_vector_offsets':{'submeshes':'0x170','poses':'0x348'},'native_createManual_return':'0x1a3fdd'}
out=ROOT/'build/model-clone-reserve-native-contract.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(record,indent=2),encoding='utf-8')
print('PASS',checks,'offline pinned model-clone contracts')
