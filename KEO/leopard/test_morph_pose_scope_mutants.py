"""Compile private production-copy controls; accept only ordinary CHECK exit 1.

The canonical fixture is never rewritten. Its production include is selected
through a compiler definition. No game executable or external process access.
"""
from pathlib import Path
import os, subprocess, json, hashlib, re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/morph-pose-scope-mutants';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'D:/SteamLibrary/steamapps/common/Kenshi;{vc}/bin/amd64;'+env['PATH'])
production=ROOT/'src/fixes/world/morph_pose_scope.cpp'
fixture=ROOT/'leopard/test_morph_pose_scope.cpp'
source=production.read_text(encoding='utf-8')
original_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in (production,fixture)}
# name, unique old text, replacement. Multi-site transformations explicitly
# state their expected count, avoiding accidental broad textual substitutions.
controls=[
 ('missing-live-config','&&fixes::g_fixesCfg.morphPoseReuseEnabled','',1),
 ('missing-owner','&&IsMainThread()','',1),
 ('missing-clone-ready','&&cloneReady','',1),
 ('missing-create-ready','&&createReady','',1),
 ('lost-boundary512','poses.size()>512','poses.size()>128',1),
 ('exceeded-boundary512','poses.size()>512','poses.size()>513',1),
 ('wrong-pose-caller','caller==ogreBase+0x1a43ff','caller==ogreBase+0x1a4400',1),
 ('wrong-clone-caller','caller==gameBase+0x71ae7','caller==gameBase+0x71ae8',1),
 ('wrong-capture-caller','caller!=ogreBase+0x1a3fdd','caller!=ogreBase+0x1a3fde',1),
 ('missing-result-gate','||result!=out','',1),
 ('inherited-nested-clone','cloning=allow?c:NULL;','cloning=allow?c:previous;',1),
 ('retained-pins-through-switch','if(caller==gameBase+0x71ce2)','if(false)',1),
 ('mixed-owned-loss','if(!borrowed)list[write++]=list[i];','if(false)list[write++]=list[i];',1),
 ('missing-native-remove','originalRemove(m);','(void)m;',1),
 ('missing-ancestor-destination-gate','if(p->targetLive&&p->Target().get()==source.get())return;','if(false)return;',1),
 ('missing-depth-bound','depth>=8','depth>=80',1),
 ('missing-parent-cleanup','for(Context*c=tracked;c;c=c->previous)Detach(c,m);','if(current)Detach(current,m);',1),
 ('disabled-cleanup','for(Context*c=tracked;c;c=c->previous)Detach(c,m);','for(Context*c=tracked;c;c=c->previous)if(c->enabled&&Ready())Detach(c,m);',1),
 ('missing-final-pin-release','Finish(&context);','(void)context;',1),
 ('source-pin-leak','if(c->sourceLive){c->sourceLive=false;c->Source().~SharedPtr();}','if(false){c->sourceLive=false;c->Source().~SharedPtr();}',1),
 ('repeated-install','if(tried)return;tried=true;','tried=true;',1),
 ('half-pair-readiness','if(KenshiLib::AddHook((char*)ogre+0x1ab3c0','admitted=true;if(KenshiLib::AddHook((char*)ogre+0x1ab3c0',1),
 ('ignored-game-identity','||stamp!=(gog?0x65d60519:0x65d604d7)||size!=(gog?0x232a000:0x232c000)','',1),
 ('ignored-native-proof','if(memcmp((char*)m+p[i].rva','if(false&&memcmp((char*)m+p[i].rva',1),
 ('lost-clone-context','__finally{cloning=previous;}','__finally{}',1),
]
rows=[]
for name,old,new,count in controls:
 assert source.count(old)==count,(name,source.count(old),count)
 directory=OUT/name;directory.mkdir(exist_ok=True)
 changed=directory/'production.cpp';changed_source=source.replace(old,new)
 changed.write_text(changed_source,encoding='utf-8')
 exe=directory/'test.exe'
 command=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/DKEO_MORPH_POSE_SOURCE="'+changed.as_posix()+'"','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(fixture),'/Fo'+str(directory/'test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib','advapi32.lib']
 compiled=subprocess.run(command,env=env,capture_output=True,text=True)
 (directory/'compile.txt').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
 assert compiled.returncode==0,(name,'COMPILE FAILURE IS NOT A KILLED CONTROL',compiled.stdout,compiled.stderr)
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True)
 output=result.stdout+result.stderr;(directory/'run.txt').write_text(output,encoding='utf-8')
 check_lines=re.findall(r'^CHECK line\d+ .+$',output,re.MULTILINE)
 assert result.returncode==1 and check_lines and 'CRASH' not in output and '*-*-* OGRE Shutdown' in output,(name,'CRASH/EXCEPTION/SURVIVOR IS NOT A KILLED CONTROL',result.returncode,output[-2000:])
 rows.append({'control':name,'compile_exit':compiled.returncode,'run_exit':result.returncode,'check':check_lines,'production_copy_sha256':hashlib.sha256(changed.read_bytes()).hexdigest()})
 print(name,'CHECK exit1',flush=True)
assert original_hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in (production,fixture)}
(OUT/'verification.json').write_text(json.dumps({'controls':rows,'canonical_sources':original_hashes,'fixture_rewritten':False,'game_execution':False,'external_process_access':False},indent=2),encoding='utf-8')
print('PASS',len(rows),'compiled morph-pose negative controls')
