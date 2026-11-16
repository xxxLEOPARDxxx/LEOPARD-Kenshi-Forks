"""Pinned on-disk engine/caller/allocator contracts; never access a game process."""
from pathlib import Path
import sys,struct,hashlib,json,re
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/kenshilib'))
from check_imports import PE
GAME=ROOT.parents[1];ogre=PE(GAME/'OgreMain_x64.dll');game=PE(GAME/'RE_Kenshi/Kenshi_x64.exe');checks=0
def check(x):
 global checks
 checks+=1;assert x,checks
def read(p,r,n):o=p.offset(r);return p.data[o:o+n]
def call(p,r):check(read(p,r,1)==b'\xe8');return r+5+struct.unpack('<i',read(p,r+1,4))[0]
def exports(p):
 e=p.offset(p.directories[0][0]);names=p.offset(p.u32(e+32));ords=p.offset(p.u32(e+36));funcs=p.offset(p.u32(e+28))
 return {p.string(p.u32(names+4*i)):p.u32(funcs+4*p.u16(ords+2*i)) for i in range(p.u32(e+24))}
check(hashlib.sha256(ogre.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
check(hashlib.sha256(game.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
e=exports(ogre)
for n,r in [('??4Pass@Ogre@@QEAAAEAV01@AEBV01@@Z',0x212020),('??0Pass@Ogre@@QEAA@PEAVTechnique@1@GAEBV01@@Z',0x211c90),('??0TextureUnitState@Ogre@@QEAA@PEAVPass@1@AEBV01@@Z',0x37a690),('?getParent@TextureUnitState@Ogre@@QEBAPEAVPass@2@XZ',0x3b110)]:check(e[n]==r)
check(call(ogre,0x211e4b)==0x212020);check(0x211e4b+5==0x211e50)
check(call(ogre,0x2129fe)==0x37a690);check(call(ogre,0x2194e1)==0x1a1340)
check(read(ogre,0x211d87,21)==bytes.fromhex('48898e1801000048898e2001000048898e28010000'))
check(read(ogre,0x5a3a00,19)==bytes.fromhex('488b8a400000004881c118010000e90d51c7ff'))
check(read(ogre,0x3b110,8)==bytes.fromhex('488b8108020000c3'))
source=(ROOT/'src/fixes/world/material_reserve.cpp').read_text()
arrays={n:bytes(int(v,16) for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (proof\d+)\[\]=\{([^}]+)\}',source)}
for r,n in re.findall(r'\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}',source):check(read(ogre,int(r,16),len(arrays[n]))==arrays[n])
check('count>=2&&count<=64' in source and 'target.empty()' in source and 'target.capacity()<count' in source)
check('caller==moduleBase+0x211e50' in source and 'IsMainThread()&&admitted&&fixes::g_fixesCfg.materialReserveEnabled' in source)
check('return original(self,other);' in source and '||!original' in source)
check('sizeof(PassAccess::TextureUnitStates)==32&&sizeof(Ogre::Pass)==0x210' in source)
check('TextureUnitState' not in source.replace('TextureUnitStates','')) # no mismatching SDK TUS fields
def imported(p,r):
 check(read(p,r,2)==b'\xff\x15');iat=r+6+struct.unpack('<i',read(p,r+2,4))[0]
 return p.string(struct.unpack('<Q',read(p,iat,8))[0]+2)
check(imported(game,0x5320b5).startswith('?clone@Material@'))
check(imported(game,0x8413b9).startswith('?clone@Material@'))
out=ROOT/'build/material-reserve-native-contract.json';out.write_text(json.dumps(dict(checks=checks,source_sha256=hashlib.sha256(source.encode()).hexdigest(),engine_sha256=hashlib.sha256(ogre.data).hexdigest(),game_execution=False,external_process_access=False),indent=2))
print('PASS',checks,'pinned material copy/caller/allocator contracts')
