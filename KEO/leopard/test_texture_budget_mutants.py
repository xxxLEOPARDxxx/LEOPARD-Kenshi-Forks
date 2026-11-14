from pathlib import Path
import os,subprocess
s=Path(__file__).resolve().parents[1];out=s/'build/texture-budget-mutants';out.mkdir(exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'))
env['PATH']=str(vc/'bin/amd64')+';'+str(sdk/'Bin')+';'+env['PATH']
source=(s/'src/resources/resource_gpu_store.cpp').read_text()
gpuFixture=(s/'leopard/test_resource_gpu.cpp').read_text();sharedFixture=(s/'leopard/test_resource_geometry_gpu.cpp').read_text()
cases=[('omit-loaded-texture-pins','U64 RetainedBytes(){return geometryBytes+textureRetentionBytes;}','U64 RetainedBytes(){return geometryBytes;}'),
 ('ratchet-effective-cap','own=bytes;return true;\n}', 'own=bytes;ownCeiling=sharedCap;return true;\n}')]
for name,before,after in cases:
 assert source.count(before)==1,(name,'unique production mutation')
 d=out/name;d.mkdir(exist_ok=True);prod=d/'store.cpp';prod.write_text(source.replace(before,after),encoding='utf-8')
 gf=d/'gpu-fixture.cpp';gf.write_text(gpuFixture.replace('#include "../src/resources/resource_gpu_store.cpp"','#include "'+prod.as_posix()+'"'),encoding='utf-8')
 test=d/'shared-fixture.cpp';test.write_text(sharedFixture.replace('#include "test_resource_gpu.cpp"','#include "'+gf.as_posix()+'"'),encoding='utf-8')
 exe=d/'test.exe'
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I'+str(s/'src'),str(test),str(s/'src/resources/resource_dds.cpp'),'/Fo'+str(d)+'\\','/Fe'+str(exe),'/link','d3d11.lib','dxgi.lib']
 build=subprocess.run(args,env=env,cwd=s,capture_output=True);(d/'build.txt').write_bytes(build.stdout+build.stderr);assert build.returncode==0,(name,'compile failure is not accepted RED')
 run=subprocess.run([str(exe)],env=env,cwd=s,capture_output=True);(d/'run.txt').write_bytes(run.stdout+run.stderr);assert run.returncode!=0 and b'FAIL ' in run.stdout,(name,'mutation survived')
 print(name+': expected RED')
print('PASS 2 actual production shared-budget regression controls')
