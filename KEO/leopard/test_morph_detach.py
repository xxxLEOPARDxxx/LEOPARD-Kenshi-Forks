"""Instrument exact production pointer comparisons, never rewrite canonical code."""
from pathlib import Path
import os,subprocess,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/morph-detach-review';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'D:/SteamLibrary/steamapps/common/Kenshi;{vc}/bin/amd64;'+env['PATH'])
p=ROOT/'src/fixes/world/morph_pose_scope.cpp';raw=p.read_bytes();source=raw.decode('utf-8').replace('\r\n','\n')
body='static void Detach('+source.split('static void Detach(',1)[1].split('\nstatic void Finish(',1)[0]
assert body.count('list[i]==c->borrowed[j]')==1
instrumented=body.replace('list[i]==c->borrowed[j]','Match(list[i],c->borrowed[j])')
instrumented=instrumented.replace('list[i]==c->borrowed[i]','Match(list[i],c->borrowed[i])')
rows=[]
def run(name,text,opt='O2',expected=0):
 inc=OUT/'morph_detach_actual.inc';inc.write_text(text,encoding='utf-8');exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+opt,'/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/ID:/DEV/Kenshi/deps/boost_1_60_0','/I'+str(OUT),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include',str(ROOT/'leopard/test_morph_detach.cpp'),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
 r=subprocess.run(args,env=env,capture_output=True,text=True);(OUT/(name+'-compile.txt')).write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],cwd=OUT,env=env,capture_output=True,text=True);output=r.stdout+r.stderr;(OUT/(name+'-run.txt')).write_text(output,encoding='utf-8');print(output.strip(),flush=True)
 assert r.returncode==expected and '*-*-* OGRE Shutdown' in output,(name,r.returncode,output)
 if expected:assert 'CHECK line' in output and 'CRASH' not in output
 rows.append(dict(name=name,exit=r.returncode))
run('actual-O2',instrumented)
if '--red' not in sys.argv:
 run('actual-Od',instrumented,'Od')
 start=instrumented.index(' bool complete=');end=instrumented.index(' size_t write=0;')
 run('quadratic-only',instrumented[:start]+instrumented[end:],expected=1)
 assert 'complete=Match(list[i],c->borrowed[i]);' in instrumented
 run('size-only',instrumented.replace('complete=Match(list[i],c->borrowed[i]);','complete=(Match(list[i],c->borrowed[i]),true);'),expected=1)
 run('erase-mixed-owned',instrumented.replace('if(!borrowed)list[write++]=list[i];','if(false)list[write++]=list[i];'),expected=1)
 run('disabled-cleanup',instrumented.replace('if(!c->targetLive','if(!c->enabled||!c->targetLive'),expected=1)
 assert raw==p.read_bytes()
 (OUT/'verification.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(raw).hexdigest(),commands=rows,game_execution=False),indent=2),encoding='utf-8')
