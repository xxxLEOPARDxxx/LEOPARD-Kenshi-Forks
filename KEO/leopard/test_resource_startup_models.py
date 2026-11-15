"""Actual production byte-cache startup fixtures, no native game execution."""
from pathlib import Path
import argparse, os, subprocess
root=Path(__file__).resolve().parents[1]
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
parser=argparse.ArgumentParser();parser.add_argument('--expect-red',action='store_true');parser.add_argument('--mutants',action='store_true');args=parser.parse_args()
for opt in ('Od','O2'):
 out=root/f'build/startup-models/{opt}';out.mkdir(parents=True,exist_ok=True)
 command=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+opt,'/I'+str(root/'src'),str(root/'leopard/test_resource_startup_models.cpp'),'/Fo'+str(out/'fixture.obj'),'/Fe'+str(out/'fixture.exe')]
 build=subprocess.run(command,cwd=root,env=env,capture_output=True);(out/'compile.txt').write_bytes(build.stdout+build.stderr)
 assert build.returncode==0,build.stdout+build.stderr
 run=subprocess.run([str(out/'fixture.exe')],cwd=root,capture_output=True);(out/'run.txt').write_bytes(run.stdout+run.stderr)
 print(opt,run.stdout.decode(errors='replace').strip())
 assert (run.returncode!=0)==args.expect_red,(opt,run.returncode)
if args.mutants:
 source=(root/'src/resources/resource_cache_store.cpp').read_text(encoding='utf-8')
 fixture=(root/'leopard/test_resource_startup_models.cpp').read_text(encoding='utf-8')
 cases=[
  ('texture-only-inventory','if(!CacheableFileName(full)||','if(!Texture(full)||'),
  ('textures-before-models','w.next<w.models.size()?w.models[w.next]:w.inventory[w.next-w.models.size()]','w.next<w.inventory.size()?w.inventory[w.next]:w.models[w.next-w.inventory.size()]'),
  ('models-outside-inventory-limit','w.inventory.size()+w.models.size()>=MaxStartupFiles','w.inventory.size()>=MaxStartupFiles'),
  ('model-bucket-retained-on-reset','std::vector<std::string>().swap(models);',''),
  ('texture-only-terminal','if(w.next>=w.models.size()+w.inventory.size())','if(w.next>=w.inventory.size())'),
  ('startup-evicts-previous-models','MakeRoom(bytes,!startup)','MakeRoom(bytes,true)'),
 ]
 for name,old,new in cases:
  assert source.count(old)==1,(name,'unique mutation')
  out=root/'build/startup-models/mutants'/name;out.mkdir(parents=True,exist_ok=True)
  production=out/'production.cpp';production.write_text(source.replace(old,new),encoding='utf-8')
  copied=out/'fixture.cpp';copied.write_text(fixture.replace('#include "../src/resources/resource_cache_store.cpp"','#include "'+production.as_posix()+'"'),encoding='utf-8')
  build=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I'+str(root/'src'),str(copied),'/Fo'+str(out/'fixture.obj'),'/Fe'+str(out/'fixture.exe')],cwd=root,env=env,capture_output=True)
  (out/'compile.txt').write_bytes(build.stdout+build.stderr);assert build.returncode==0,(name,'compile failure is not runtime RED',build.stdout+build.stderr)
  run=subprocess.run([str(out/'fixture.exe')],cwd=root,capture_output=True);(out/'run.txt').write_bytes(run.stdout+run.stderr);assert run.returncode!=0,(name,'survived')
  print(name,'expected RED',run.stdout.decode(errors='replace').strip())
