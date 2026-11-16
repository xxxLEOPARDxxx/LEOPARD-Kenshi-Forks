"""Compile private production-copy controls; accept only ordinary CHECK exit 1.

The canonical fixture is never rewritten. Its production include is selected
through a compiler definition. No game executable or external process access.
"""
from pathlib import Path
import os, subprocess, json, hashlib, re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/model-clone-reserve-mutants';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'D:/SteamLibrary/steamapps/common/Kenshi;{vc}/bin/amd64;'+env['PATH'])
production=ROOT/'src/fixes/world/model_clone_reserve.cpp'
fixture=ROOT/'leopard/test_model_clone_reserve.cpp'
source=production.read_text(encoding='utf-8')
original_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in (production,fixture)}
# name, unique old text, replacement. Multi-site transformations explicitly
# state their expected count, avoiding accidental broad textual substitutions.
controls=[
 ('prepare-nop','Prepare(source,out);','(void)source;',1),
 ('missing-caller-gate','&&caller==moduleBase+0x1a3fdd','',1),
 ('missing-live-config','&&fixes::g_fixesCfg.modelCloneReserveEnabled','',2),
 ('inherited-disabled-source','Scope scope(allow?self:NULL);','Scope scope(allow?self:source);',1),
 ('missing-owner','IsMainThread()&&','',2),
 ('missing-admission','&&admitted','',2),
 ('unloaded-source','||!src->isLoaded()','',1),
 ('missing-submesh-cap','&&subCount<=64','',1),
 ('missing-pose-cap','&&poseCount<=512','',1),
 ('lost-submesh-boundary','subCount<=64','subCount<64',1),
 ('lost-pose-boundary','poseCount<=512','poseCount<512',1),
 ('nonempty-submesh-target','subTarget.empty()&&','',1),
 ('nonempty-pose-target','poseTarget.empty()&&','',1),
 ('resize-submesh-vector','KEO_MODEL_CLONE_RESERVE_OPERATION(subTarget,subCount);','KEO_MODEL_CLONE_RESERVE_OPERATION(subTarget,subCount);subTarget.resize(subCount);',1),
 ('resize-pose-vector','KEO_MODEL_CLONE_RESERVE_OPERATION(poseTarget,poseCount);','KEO_MODEL_CLONE_RESERVE_OPERATION(poseTarget,poseCount);poseTarget.resize(poseCount);',1),
 ('escaped-reserve-error','try{LogError("Model clone preparation: storage reserve failed; native cloning retained");}catch(...){}','throw;',1),
 ('escaped-logger-error','try{LogError("Model clone preparation: storage reserve failed; native cloning retained");}catch(...){}','LogError("Model clone preparation: storage reserve failed; native cloning retained");',1),
 ('missing-result-gate','&&result==out','',1),
 ('double-native-clone','return originalClone(self,out,name,group);','originalClone(self,out,name,group);return originalClone(self,out,name,group);',1),
 ('double-native-create','void*result=originalCreate(self,out,name,group,loader);','originalCreate(self,out,name,group,loader);void*result=originalCreate(self,out,name,group,loader);',1),
 ('swallowed-native-clone-error','return originalClone(self,out,name,group);','try{return originalClone(self,out,name,group);}catch(...){return out;}',1),
 ('ignored-native-proof','if(memcmp((char*)module+proofs[i].rva','if(false&&memcmp((char*)module+proofs[i].rva',1),
 ('ignored-image-identity','||stamp!=0x5ca5f929||size!=0x9c9000','',1),
 ('repeated-install','if(tried)return;tried=true;','tried=true;',1),
 ('half-pair-readiness','if(KenshiLib::AddHook((void*)(moduleBase+0x1be970)','admitted=true;if(KenshiLib::AddHook((void*)(moduleBase+0x1be970)',1),
]
rows=[]
for name,old,new,count in controls:
 assert source.count(old)==count,(name,source.count(old),count)
 directory=OUT/name;directory.mkdir(exist_ok=True)
 changed=directory/'production.cpp';changed.write_text(source.replace(old,new),encoding='utf-8')
 exe=directory/'test.exe'
 command=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/DKEO_MODEL_CLONE_SOURCE="'+changed.as_posix()+'"','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(fixture),'/Fo'+str(directory/'test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib','advapi32.lib']
 compiled=subprocess.run(command,env=env,capture_output=True,text=True)
 (directory/'compile.txt').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
 assert compiled.returncode==0,(name,'COMPILE FAILURE IS NOT A KILLED CONTROL',compiled.stdout,compiled.stderr)
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True)
 output=result.stdout+result.stderr;(directory/'run.txt').write_text(output,encoding='utf-8')
 check_lines=re.findall(r'^CHECK line\d+ .+$',output,re.MULTILINE)
 assert result.returncode==1 and check_lines and 'FIXTURE CRASH' not in output and '*-*-* OGRE Shutdown' in output,(name,'CRASH/EXCEPTION/SURVIVOR IS NOT A KILLED CONTROL',result.returncode,output[-2000:])
 rows.append({'control':name,'compile_exit':compiled.returncode,'run_exit':result.returncode,'check':check_lines,'production_copy_sha256':hashlib.sha256(changed.read_bytes()).hexdigest()})
 print(name,'CHECK exit1',flush=True)
assert original_hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in (production,fixture)}
(OUT/'verification.json').write_text(json.dumps({'controls':rows,'canonical_sources':original_hashes,'fixture_rewritten':False,'game_execution':False,'external_process_access':False},indent=2),encoding='utf-8')
print('PASS',len(rows),'compiled model-clone negative controls')
