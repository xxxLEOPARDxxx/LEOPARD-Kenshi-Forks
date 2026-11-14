from pathlib import Path
import os,subprocess
s=Path(__file__).resolve().parents[1];root=s/'build/completion-attribution-mutants';root.mkdir(exist_ok=True)
src=(s/'src/resources/resource_load_probe.cpp').read_text();fixture=(s/'leopard/test_resource_load_probe.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=str(vc/'bin/amd64')+';'+str(sdk/'Bin')+';'+env['PATH']
cases=[('wrong-texture-return','Sample(texture2D,QpcDeltaUs(begin,QpcNow()));return result;','Sample(texture2D,QpcDeltaUs(begin,QpcNow()));return NULL;'),
 ('lost-appearance-data','const LONGLONG begin=QpcNow();native(self,resource,data);','const LONGLONG begin=QpcNow();native(self,resource,NULL);'),
 ('zero-token-refused','if(begin<0)return;','if(begin<=0)return;'),
 ('wrong-ring-bound','if(slowCount==16){','if(slowCount==15){')]
for name,before,after in cases:
 assert src.count(before)==1,(name,'unique mutation')
 out=root/name;out.mkdir(exist_ok=True);mutant=out/'production.cpp';mutant.write_text(src.replace(before,after),encoding='utf-8')
 test=out/'fixture.cpp';test.write_text(fixture.replace('#include "../src/resources/resource_load_probe.cpp"','#include "'+mutant.as_posix()+'"'),encoding='utf-8')
 exe=out/'fixture.exe'
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/O2','/I'+str(s/'src'),str(test),'/Fo'+str(out)+'\\','/Fe'+str(exe)]
 build=subprocess.run(args,env=env,cwd=s,capture_output=True)
 (out/'build.txt').write_bytes(build.stdout+build.stderr);assert build.returncode==0,(name,'compile failed, not accepted RED')
 run=subprocess.run([str(exe)],env=env,cwd=s,capture_output=True)
 (out/'run.txt').write_bytes(run.stdout+run.stderr);assert run.returncode!=0,(name,'mutation survived')
 print(name+': expected RED')
print('PASS 4 compiled production negative controls; production never edited')
