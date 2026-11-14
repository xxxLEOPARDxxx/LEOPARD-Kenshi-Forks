"""Actual startup wrappers/menu helper, compiled with VC10; no game code executed."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];out=root/'build/resource-startup';out.mkdir(exist_ok=True,parents=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
def test(name,cpp,expected=0):
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I'+str(out),str(root/cpp),'/Fo'+str(out/(name+'.obj')),'/Fe'+str(out/(name+'.exe'))]
 subprocess.run(args,cwd=root,env=env,check=True,capture_output=True)
 run=subprocess.run([str(out/(name+'.exe'))],cwd=root,capture_output=True,text=True);(out/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout)
 assert run.returncode==expected,(name,run.returncode)
source=(root/'src/resources/resource_startup_native.cpp').read_text(encoding='utf-8');a=source.index('bool ResourceStartupTitleVisible(){');b=source.index('void ResourceStartupNativeInit(){',a);body=source[a:b]
test('boundary','leopard/test_resource_startup_boundary.cpp')
(out/'title-current.inc').write_text(body,encoding='utf-8');test('title','leopard/test_resource_startup_title.cpp')
wrong=body.replace('title->mMainWidget','title->win');assert wrong!=body
(out/'title-current.inc').write_text(wrong,encoding='utf-8');test('title-wrong-field','leopard/test_resource_startup_title.cpp',1)
(out/'title-current.inc').write_text(body,encoding='utf-8');print('Native startup/menu behavioral fixtures and wrong-field RED control PASS')
