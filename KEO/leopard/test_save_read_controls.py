"""Production-copy controls must compile then fail an ordinary runtime CHECK."""
from pathlib import Path
import os,subprocess,hashlib,json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/save-read-controls';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
p=ROOT/'src/fixes/world/save_read_buffer.cpp';fixture=ROOT/'leopard/test_save_read_buffer.cpp';s=p.read_text();before={str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in [p,fixture]}
controls=[('small-buffer','stream->rdbuf(),65536','stream->rdbuf(),4096'),('ignore-owner','&&IsMainThread()',''),('ignore-live-setting','&&fixes::g_fixesCfg.saveReadBufferEnabled',''),('ignore-caller','&&caller==gameBase+0x6c092c',''),('ignore-codecvt','if(codecvt||!file','if(!file'),('ignore-vtable','if(table!=gameBase+0x168ef18)return;','if(false)return;'),('ignore-result','if(result==self&&self','if(self'),('ignore-protection','&&protection==0x40',''),('ignore-small-file-gate','||size.QuadPart<128*1024',''),('wrong-error-reporting','InterlockedCompareExchange(&errorLogged,1,0)==0','InterlockedCompareExchange(&errorLogged,1,0)==1')]
rows=[]
for name,old,new in controls:
 assert s.count(old)==1,(name,s.count(old));d=OUT/name;d.mkdir(exist_ok=True);changed=d/'production.cpp';changed.write_text(s.replace(old,new))
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/DKEO_SAVE_READ_SOURCE="'+changed.as_posix()+'"','/I'+str(ROOT/'src'),str(fixture),'/Fo'+str(d/'test.obj'),'/Fe'+str(d/'test.exe')]
 c=subprocess.run(cmd,env=env,capture_output=True,text=True);(d/'compile.log').write_text(c.stdout+c.stderr);assert c.returncode==0,(name,c.stdout,c.stderr)
 r=subprocess.run([str(d/'test.exe')],cwd=ROOT,env=env,capture_output=True,text=True);(d/'run.log').write_text(r.stdout+r.stderr);assert r.returncode==1 and 'CHECK line' in r.stdout,(name,r.returncode,r.stdout,r.stderr)
 rows.append({'name':name,'compile_exit':0,'run_exit':1,'output':r.stdout});print(name,'CHECK exit1',flush=True)
assert before=={str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in [p,fixture]}
(OUT/'verification.json').write_text(json.dumps({'controls':rows,'canonical_files':before,'game_execution':False,'process_access':False},indent=2))
print('PASS',len(rows),'compiled production save-read controls')
