"""Private offline pinned morph-scope native contracts. No process/game execution."""
from pathlib import Path
import sys,struct,hashlib,json,re,os
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
ogre=PE(GAME/'OgreMain_x64.dll');game=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');sdk=PE(GAME/'KenshiLib.dll');checks=0

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

def imported_call(p,r):
 check(read(p,r,2)==b'\xff\x15')
 iat=r+6+struct.unpack('<i',read(p,r+2,4))[0]
 name=struct.unpack('<Q',read(p,iat,8))[0]+2
 return p.string(name)

for p,expected in [(ogre,'7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'),(game,'504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'),(sdk,'5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b')]:check(hashlib.sha256(p.data).hexdigest()==expected)
e=exports(ogre)
for symbol,r in [('?clone@Pose@',0x229bf0),('?removeAllPoses@Mesh@',0x1ab3c0),('?clone@Mesh@',0x1a3ee0),('?createManual@MeshManager@',0x1be970),('?getMesh@Entity@',0xc09b0),('?softwareVertexPoseBlend@Mesh@',0x1a9be0)]:check([v for k,v in e.items() if k.startswith(symbol)]==[r])
check(call(ogre,0x1a43fa)==0x229bf0);check(0x1a43fa+5==0x1a43ff)
check(call(ogre,0x1a3fd8)==0x1be970);check(0x1a3fd8+5==0x1a3fdd)
check(call(ogre,0x1a3e1f)==0x1ab3c0)
check(imported_call(game,0x71ac9).startswith('?getMesh@Entity@'))
check(imported_call(game,0x71ae1).startswith('?clone@Mesh@'));check(0x71ae1+6==0x71ae7)
check(imported_call(game,0x71cdc)=='?removeAllPoses@Mesh@Ogre@@QEAAXXZ');check(0x71cdc+6==0x71ce2)
check(call(game,0x71d07)==0x3e694) # native switchBodyMesh import thunk
check(read(ogre,0xc09b0,8)==bytes.fromhex('488d8178010000c3'))
check(read(ogre,0x1ab490,8)==bytes.fromhex('488d8148030000c3'))
# Source-pin sensitivity: native destroy checks old Mesh use-count after old Entity destruction.
check(read(game,0x44a5ec,3)==bytes.fromhex('8b5020'))
source=(ROOT/'src/fixes/world/morph_pose_scope.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16) for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
for label,p in [('ogreProofs',ogre),('gameProofs',game)]:
 block=re.search(r'static const Proof '+label+r'\[\]=\{([^;]+)',source).group(1)
 rows=re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',block);check(len(rows)==3)
 for r,n in rows:check(read(p,int(r,16),len(arrays[n]))==arrays[n])
check('caller==gameBase+0x71ae7' in source and 'caller==ogreBase+0x1a43ff' in source and 'caller!=ogreBase+0x1a3fdd' in source)
check('caller==gameBase+0x71ce2' in source)
check('new(c->sourceStorage) Ogre::MeshPtr(source)' in source and 'new(c->targetStorage) Ogre::MeshPtr(target)' in source)
check('Ogre::MeshPtr(source.get()' not in source and 'Ogre::MeshPtr(target.get()' not in source)
check('borrowed[512]' in source and 'depth>=8' in source)
check('for(Context*c=tracked;c;c=c->previous)Detach(c,m);' in source)
check('__finally{cloning=previous;}' in source)
check('static_assert(offsetof(MeshAccess,mPoseList)==0x348' in source)
check('sizeof(Ogre::PoseList)==32&&sizeof(Ogre::MeshPtr)==16' in source)
check('HardwareBuffer' not in source and 'softwareVertexPoseBlend' not in source)
fixture=(ROOT/'leopard/test_morph_pose_scope.cpp').read_text()
check(fixture.index('C(DiskHash(sdk,')<fixture.index('((Init)((char*)sdk+0x14884))()'))
check(call(sdk,0x227f)==0x14884)
check(read(sdk,0x14884,32)==bytes.fromhex('40534883ec60488b05ff150d004833c4488944245833db48391d862b0d007408'))
check('InitRVAs(' not in source and '0x14884' not in source)
functions=[('game',0x71a80,0x2f5),('game',0x44a4c0,0x1bb),('ogre',0x1a3ee0,0x6d5),('ogre',0x229bf0,0x115),('ogre',0x1ab3c0,0x78),('ogre',0x1a3660,0xb4),('ogre',0x580030,0x5d)]
record={'checks':checks,'images':{k:hashlib.sha256(p.data).hexdigest() for k,p in [('game',game),('ogre',ogre),('sdk',sdk)]},'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'functions':[{'image':which,'rva':hex(r),'bytes':n,'sha256':hashlib.sha256(read(game if which=='game' else ogre,r,n)).hexdigest()}for which,r,n in functions],'game_execution':False,'external_process_access':False,'test_only_minHook_init':'0x14884','max_borrowed_per_scope':512,'max_retained_contexts':8}
out=ROOT/'build/morph-pose-scope-native-contract.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(record,indent=2),encoding='utf-8')
print('PASS',checks,'offline pinned morph-pose contracts')
