"""Each changed production copy must compile, then fail a runtime assertion."""
from pathlib import Path
import os,subprocess,hashlib,json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/save-string-controls';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
p=ROOT/'src/fixes/world/save_string_transfer.cpp';fixture=ROOT/'leopard/test_save_string_transfer.cpp';s=p.read_text();before={str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in [p,fixture]}
controls=[('copy-instead-of-transfer','list->data[list->count].swap(*value);','list->data[list->count]=*value;std::string empty;value->swap(empty);'),('ignore-owner','IsMainThread()&&',''),('ignore-live','&&fixes::g_fixesCfg.saveStringTransferEnabled',''),('ignore-admission','&&admitted',''),('ignore-caller','&&ParserCaller(KEO_SAVE_STRING_CALLER())',''),('ignore-cap-limit','||list->capacity>65536',''),('ignore-short-source','||value->size()<=15',''),('ignore-alias','if(!(argument+sizeof(std::string)<=begin||argument>=begin+bytes))return false;',''),('ignore-empty-destination','slot.empty()&&',''),('ignore-destination-storage','&&slot.capacity()==15',''),('ignore-count-increment','++list->count;',''),('ignore-proof','if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))','if(false)')]
rows=[]
for name,old,new in controls:
 assert s.count(old)==1,(name,s.count(old));d=OUT/name;d.mkdir(exist_ok=True);changed=d/'production.cpp';changed.write_text(s.replace(old,new))
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/DKEO_SAVE_STRING_SOURCE="'+changed.as_posix()+'"','/I'+str(ROOT/'src'),str(fixture),'/Fo'+str(d/'test.obj'),'/Fe'+str(d/'test.exe')]
 c=subprocess.run(cmd,env=env,capture_output=True,text=True);(d/'compile.log').write_text(c.stdout+c.stderr);assert c.returncode==0,(name,c.stdout,c.stderr)
 r=subprocess.run([str(d/'test.exe')],cwd=ROOT,env=env,capture_output=True,text=True);(d/'run.log').write_text(r.stdout+r.stderr);assert r.returncode==1 and 'CHECK line' in r.stdout,(name,r.returncode,r.stdout,r.stderr)
 rows.append({'name':name,'compile_exit':0,'run_exit':1,'output':r.stdout});print(name,'CHECK exit1',flush=True)
assert before=={str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in [p,fixture]}
(OUT/'verification.json').write_text(json.dumps({'controls':rows,'canonical_files':before,'game_execution':False,'process_access':False},indent=2))
print('PASS',len(rows),'compiled production save-string controls')
