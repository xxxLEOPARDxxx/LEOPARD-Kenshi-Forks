"""Exercise exact production installation bodies with controlled SDK outcomes."""
from pathlib import Path
import os,subprocess,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/resource-probe-admission';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'{vc}/bin/amd64;'+env['PATH'])
path=ROOT/'src/resources/resource_load_probe.cpp';original=path.read_bytes();source=original.decode('utf-8').replace('\r\n','\n')
body=source.split('static bool InstallAdmitted(',1)[1].split('\n#endif',1)[0];body='static bool InstallAdmitted('+body
inc=OUT/'resource_probe_admission_actual.inc';inc.write_text(body,encoding='utf-8');rows=[]
def run(name,text,opt='O2',expected=0):
 inc.write_text(text,encoding='utf-8');exe=OUT/(name+'.exe')
 command=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(OUT),str(ROOT/'leopard/test_resource_probe_admission.cpp'),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe)]
 compiled=subprocess.run(command,env=env,capture_output=True,text=True);(OUT/(name+'-compile.txt')).write_text(compiled.stdout+compiled.stderr,encoding='utf-8');assert compiled.returncode==0,'Compilation failure is not a test result'
 result=subprocess.run([str(exe)],env=env,capture_output=True,text=True);output=result.stdout+result.stderr;(OUT/(name+'.txt')).write_text(output,encoding='utf-8');print(output.strip(),flush=True)
 assert result.returncode==expected,(name,result.returncode,output)
 if expected:assert 'CHECK line'in output
 rows.append(dict(name=name,exit_code=result.returncode))
run('actual-O2',body)
if '--red'not in sys.argv:
 run('actual-Od',body,'Od')
 assert body.count('||!*original')==2
 for index,part in enumerate(body.split('static bool Install(')):
  prefix=''if index==0 else 'static bool Install('
  other=body[len(part):]if index==0 else body.split('static bool Install(',1)[0]
  restored=part.replace('LogError(std::string("Resource loading probe: ")+name+" hook failed;', '*original=NULL;LogError(std::string("Resource loading probe: ")+name+" hook failed;',1)
  assert restored!=part
  run('lost-trampoline-'+str(index),prefix+restored+other if index==0 else other+prefix+restored,expected=1)
  missing=part.replace('||!*original','',1)
  run('null-readiness-'+str(index),prefix+missing+other if index==0 else other+prefix+missing,expected=1)
 inc.write_text(body,encoding='utf-8');assert original==path.read_bytes()
 (OUT/'verification.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(original).hexdigest(),commands=rows,canonical_changed=False),indent=2),encoding='utf-8')
