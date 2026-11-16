"""Compile actual production-copy controls, requiring CHECK exit1 + Ogre shutdown."""
from pathlib import Path
import os,subprocess,hashlib,json,re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/morph-upload-controls';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+str(sdk/'Bin')+';D:/SteamLibrary/steamapps/common/Kenshi;'+env['PATH']
cpp=ROOT/'src/fixes/world/morph_pose_scope.cpp';header=ROOT/'src/fixes/world/morph_upload_batch.h';fixture=ROOT/'leopard/test_morph_upload.cpp'
before={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in [cpp,header,fixture]}
controls=[
 ('no-batch',cpp,'   BeginUpload(c);','   (void)c;'),
 ('flush-after-native-removal',cpp,'morph_upload_batch::End(&c->upload);','(void)c;'),
 ('swallowed-normal-upload-failure',cpp,'morph_upload_batch::End(&c->upload);','morph_upload_batch::EndNoThrow(&c->upload);'),
 ('keep-suppression',header,'p->suppressHardwareUpdate(false);','p->suppressHardwareUpdate(true);'),
 ('ignore-preexisting-suppression',header,'||Access::Suppressed(tb.get())',''),
 ('ignore-system-memory',header,'||tb->isSystemMemory()',''),
 ('ignore-native-shadow-lock',header,'||tb->isLocked()',''),
]
rows=[]
for name,path,old,new in controls:
 directory=OUT/name;directory.mkdir(exist_ok=True);h=directory/'fixes/world/morph_upload_batch.h';h.parent.mkdir(parents=True,exist_ok=True)
 text=path.read_text();assert text.count(old)==1,(name,text.count(old))
 changed=directory/'production.cpp';changed.write_text(cpp.read_text().replace(old,new)if path==cpp else cpp.read_text())
 h.write_text(header.read_text().replace(old,new)if path==header else header.read_text())
 exe=directory/'test.exe'
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/DKEO_MORPH_POSE_SOURCE="'+changed.as_posix()+'"','/I'+str(directory),'/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(fixture),'/Fo'+str(directory/'test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib','advapi32.lib']
 c=subprocess.run(cmd,env=env,capture_output=True,text=True);(directory/'compile.txt').write_text(c.stdout+c.stderr);assert c.returncode==0,(name,c.stdout,c.stderr)
 r=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True);output=r.stdout+r.stderr;(directory/'run.txt').write_text(output)
 checks=re.findall(r'^CHECK line\d+ .+$',output,re.MULTILINE)
 assert r.returncode==1 and checks and 'CRASH' not in output and '*-*-* OGRE Shutdown' in output,(name,r.returncode,output[-2000:])
 rows.append({'name':name,'compile_exit':c.returncode,'run_exit':r.returncode,'checks':checks});print(name,'CHECK exit1',flush=True)
assert before=={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in [cpp,header,fixture]}
(OUT/'verification.json').write_text(json.dumps({'controls':rows,'sources':before,'game_execution':False,'process_access':False},indent=2))
print('PASS',len(rows),'compiled native upload controls')
