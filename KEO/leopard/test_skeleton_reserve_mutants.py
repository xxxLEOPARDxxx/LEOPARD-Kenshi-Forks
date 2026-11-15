"""Compile actual-production negative controls; only CHECK exit1 is accepted."""
from pathlib import Path
import os,subprocess,json,sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/skeleton-reserve-mutants';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'D:/SteamLibrary/steamapps/common/Kenshi;{vc}/bin/amd64;'+env['PATH'])
source=(ROOT/'src/fixes/world/skeleton_reserve.cpp').read_text(encoding='utf-8');fixture=(ROOT/'leopard/test_skeleton_reserve.cpp').read_text(encoding='utf-8')
controls=[
 ('missing-preparation','Prepare(self);','(void)self;'),
 ('resize-logical-vector','KEO_SKELETON_RESERVE_OPERATION(bones,span);','KEO_SKELETON_RESERVE_OPERATION(bones,span);bones.resize(span);'),
 ('unloaded-master','||!master->isLoaded()',''),
 ('nonempty-destination','bones.empty()&&',''),
 ('lost-last-handle','span<=256','span<256'),
 ('missing-owner','IsMainThread()&&admitted&&','admitted&&'),
 ('missing-admission','IsMainThread()&&admitted&&','IsMainThread()&&'),
 ('missing-toggle','&&fixes::g_fixesCfg.skeletonReserveEnabled',''),
 ('missing-native-call','    original(self);','    (void)self;'),
 ('swallowed-native-error','    original(self);','    try{original(self);}catch(...){}'),
 ('throwing-error-fallback','try{LogError("Skeleton preparation: storage reserve failed; native loading retained");}catch(...){}','LogError("Skeleton preparation: storage reserve failed; native loading retained");'),
 ('ignored-native-proof','if(memcmp((char*)module+proofs[i].rva','if(false&&memcmp((char*)module+proofs[i].rva'),
 ('ignored-image-id','||stamp!=0x5ca5f929||size!=0x9c9000',''),
 ('repeated-install','if(tried)return;tried=true;','tried=true;'),
]
rows=[]
for name,old,new in controls:
 assert source.count(old)==1,(name,source.count(old))
 d=OUT/name;d.mkdir(exist_ok=True);changed=d/'production.cpp';changed.write_text(source.replace(old,new),encoding='utf-8')
 cpp=d/'fixture.cpp';cpp.write_text(fixture.replace('#define KEO_SKELETON_SOURCE "../src/fixes/world/skeleton_reserve.cpp"','#define KEO_SKELETON_SOURCE "'+changed.as_posix()+'"'),encoding='utf-8')
 exe=d/'test.exe'
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(cpp),'/Fo'+str(d/'test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
 c=subprocess.run(cmd,env=env,capture_output=True,text=True);(d/'compile.txt').write_text(c.stdout+c.stderr,encoding='utf-8');assert c.returncode==0,(name,c.stdout,c.stderr)
 r=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True);(d/'run.txt').write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==1 and 'FAIL line' in r.stdout,(name,r.returncode,r.stdout[-1200:],r.stderr)
 rows.append({'control':name,'compile':c.returncode,'run':r.returncode});print(name,'CHECK exit1',flush=True)
(OUT/'verification.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('PASS',len(rows),'compiled negative controls')
