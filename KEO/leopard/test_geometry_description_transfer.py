"""Production-copy controls: ordinary CHECK exit1 and clean native teardown only."""
from pathlib import Path
import os,subprocess,json,hashlib,re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/geometry-description-transfer';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env.update(INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64',PATH=f'D:/SteamLibrary/steamapps/common/Kenshi;{vc}/bin/amd64;'+env['PATH'])
production=ROOT/'src/resources/resource_geometry.cpp';fixture=ROOT/'leopard/test_resource_geometry.cpp'
files=[production,fixture,ROOT/'leopard/test_retention_alloc.h']
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in files}
source=production.read_text(encoding='utf-8');test=fixture.read_text(encoding='utf-8')
controls=[
 ('copy-instead-of-transfer','e.desc.buffers.swap(d.buffers);','e.desc.buffers=d.buffers;'),
 ('redundant-description-copy','Entry empty;empty.touch=touch;','Description redundant=d;Entry empty;empty.touch=touch;'),
 ('stale-consumed-bytes','d.gpuBytes=d.cpuBytes=0;',''),
]
rows=[]
for name,old,new in controls:
 assert source.count(old)==1,name
 directory=OUT/name;directory.mkdir(exist_ok=True)
 changed=directory/'production.cpp';changed.write_text(source.replace(old,new),encoding='utf-8')
 anchor='#include "../src/resources/resource_geometry.cpp"';assert test.count(anchor)==1
 private=directory/'fixture.cpp';private.write_text(test.replace(anchor,'#include "'+changed.as_posix()+'"'),encoding='utf-8')
 exe=directory/'test.exe'
 command=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(private),'/Fo'+str(directory/'test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
 compiled=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True)
 (directory/'compile.txt').write_text(compiled.stdout+compiled.stderr,encoding='utf-8');assert compiled.returncode==0,(name,'compile failure is not an accepted control')
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,text=True)
 output=result.stdout+result.stderr;(directory/'run.txt').write_text(output,encoding='utf-8')
 checks=re.findall(r'^line \d+: .+$',output,re.M)
 assert result.returncode==1 and checks and '*-*-* OGRE Shutdown' in output and 'crash RIP=' not in output,(name,result.returncode,output[-1000:])
 rows.append(dict(control=name,exit_code=1,checks=checks));print(name,'CHECK exit1; clean native shutdown',flush=True)
assert hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in files}
(OUT/'verification.json').write_text(json.dumps(dict(controls=rows,source=hashes,canonical_fixture_changed=False,game_execution=False),indent=2),encoding='utf-8')
print('PASS three compiled geometry-description controls')
