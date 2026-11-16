"""Compile exact production geometry Init/request wrappers, controlled SDK only."""
from pathlib import Path
import os,subprocess,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/geometry-hook-review';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'{vc}/bin/amd64;'+env['PATH'])
p=ROOT/'src/resources/resource_geometry.cpp';raw=p.read_bytes();source=raw.decode('utf-8').replace('\r\n','\n')
wrapper='typedef Ogre::Entity*(*CreateFn)'+source.split('typedef Ogre::Entity*(*CreateFn)',1)[1].split('\n#endif',1)[0]
init='void Init(){'+source.split('void Init(){',1)[1].rsplit('\n}',1)[0]
assert init.count('#ifdef RESOURCE_GEOMETRY_TEST')==1
init=init.split('#ifdef RESOURCE_GEOMETRY_TEST')[0]+init.split('#else',1)[1].replace('#endif','')
body=wrapper+'\n'+init;rows=[]
def run(name,text,opt='O2',expected=0):
 inc=OUT/'geometry_hook_actual.inc';inc.write_text(text,encoding='utf-8');exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(OUT),str(ROOT/'leopard/test_geometry_hook.cpp'),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe)]
 r=subprocess.run(args,env=env,capture_output=True,text=True);(OUT/(name+'-compile.txt')).write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],cwd=OUT,env=env,capture_output=True,text=True);output=r.stdout+r.stderr;(OUT/(name+'-run.txt')).write_text(output,encoding='utf-8');print(output.strip(),flush=True)
 assert r.returncode==expected,(name,r.returncode,output)
 if expected:assert 'CHECK line' in output
 rows.append(dict(name=name,exit=r.returncode))
run('actual-O2',body)
if '--red' not in sys.argv:
 run('actual-Od',body,'Od')
 assert '||!originalCreate' in body
 run('null-readiness',body.replace('||!originalCreate',''),expected=1)
 needle='LogError("Geometry retention: native mesh-request hook failed");'
 assert needle in body
 run('erase-trampoline',body.replace(needle,'originalCreate=NULL;'+needle),expected=1)
 assert raw==p.read_bytes()
 (OUT/'verification.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(raw).hexdigest(),commands=rows,game_execution=False),indent=2),encoding='utf-8')
