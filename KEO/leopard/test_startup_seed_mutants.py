from pathlib import Path
import os,subprocess
s=Path(__file__).resolve().parents[1];out=s/'build/startup-seed-mutants';out.mkdir(exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'))
source=(s/'src/resources/resource_startup_native.cpp').read_text();fixture=(s/'leopard/test_resource_startup_boundary.cpp').read_text()
cases=[('one-group-per-slice','count<32&&groups<8','count<32&&groups<1'),
 ('reset-record-limit-per-group','++groups;','++groups;count=0;'),
 ('unbounded-empty-groups','count<32&&groups<8','count<32'),
 ('repeat-old-list-prefix','it!=list.end()&&skip<seedOffset','it!=list.end()&&skip<seedOffset&&skip<128'),
 ('menu-still250ms','QpcDeltaUs(menuSeedAt,frameNow)>=16000','QpcDeltaUs(menuSeedAt,frameNow)>=250000'),
 ('no-duplicate-heartbeat-limit','responsive&&(!menuSeedSeen||QpcDeltaUs(menuSeedAt,frameNow)>=16000)','responsive'),
 ('no-record-deadline','if(QpcDeltaUs(begin,QpcNow())>=1000)break;','')]
for name,before,after in cases:
 assert source.count(before)==1,(name,'unique mutation')
 d=out/name;d.mkdir(exist_ok=True);prod=d/'production.cpp';prod.write_text(source.replace(before,after),encoding='utf-8')
 test=d/'fixture.cpp';test.write_text(fixture.replace('#include "../src/resources/resource_startup_native.cpp"','#include "'+prod.as_posix()+'"'),encoding='utf-8')
 exe=d/'test.exe';args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(test),'/Fo'+str(d)+'\\','/Fe'+str(exe)]
 build=subprocess.run(args,env=env,cwd=s,capture_output=True);(d/'build.txt').write_bytes(build.stdout+build.stderr);assert build.returncode==0,(name,'compile failure not accepted RED')
 run=subprocess.run([str(exe)],env=env,cwd=s,capture_output=True);(d/'run.txt').write_bytes(run.stdout+run.stderr);assert run.returncode!=0 and b'FAIL ' in run.stdout,(name,'mutation survived')
 print(name+': expected RED')
print('PASS 7 actual startup scheduling negative controls')
