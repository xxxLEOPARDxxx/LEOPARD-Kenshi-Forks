"""Production inclusion with actual Ogre maps/params; no game or GPU process."""
from pathlib import Path
import os,re,hashlib,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'));OUT=ROOT/'build/gpu-map-lifetime';OUT.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((GAME/'OgreMain_x64.dll').read_bytes()).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'
def body(name):return re.sub(r'^#include[^\n]*\n','',(ROOT/name).read_text(),flags=re.M)
params=body('src/render/gpu_params.cpp');upload=body('src/render/gpu_upload.cpp');fixture=(ROOT/'leopard/test_gpu_map_lifetime.cpp').read_text()
def source(a,b):return fixture.replace('// PARAMS_PRODUCTION_BODY',a).replace('// UPLOAD_PRODUCTION_BODY',b)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=';'.join([str(vc/'bin/amd64'),str(vc/'../Common7/IDE'),str(sdk/'Bin'),str(GAME),env['PATH']])
def run(name,mode,a,b):
 cpp=OUT/(name+'.cpp');cpp.write_text(source(a,b));exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+mode,'/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(cpp)]+[str(ROOT/'src/render'/n) for n in ['gpu_param_cache.cpp','upload_plan.cpp','upload_shadow.cpp']]+['/Fo'+str(OUT)+'\\','/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
 build=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-build.txt')).write_bytes(build.stdout);assert build.returncode==0,build.stdout.decode(errors='replace')
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-run.txt')).write_bytes(result.stdout);print(name+': '+result.stdout.decode(errors='replace'),end='');return result.returncode
for mode in ['Od','O2']:
 rc=run(mode,mode,params,upload)
 if '--expect-red' in sys.argv:assert rc!=0;print('expected runtime RED');break
 assert rc==0

if '--expect-red' not in sys.argv:
 mutants=[
  ('lookup-admits-unknown',params.replace('!map || !GpuNamedConstantsCacheable(params)','!map'),upload),
  ('upload-admits-unknown',params,upload.replace('if (!GpuNamedConstantsCacheable(params))','if (false)')),
  ('unknown-upload-leaves-stale-copy',params,upload.replace('InterlockedExchange(&s_invalidateCopies, 1);\n\t\treturn NULL;','/* stale tracked copy */\n\t\treturn NULL;')),
  ('revision-never-changes',params.replace('InterlockedIncrement(&s_namedConstantsRevision);','/* missed revision */'),upload),
  ('upload-uses-deletions',params,upload.replace('long mapGen = GpuNamedConstantsRevision();','long mapGen = GpuNamedConstantsDeleted();')),
  ('deletion-skips-invalidation',params.replace('InvalidateNamedConstants(namedConstants);','/* missed deletion */'),upload),
  ('rebind-counted-as-deletion',params.replace('InterlockedIncrement(&s_namedConstantsRevision);','InterlockedIncrement(&s_namedConstantsRevision);InterlockedIncrement(&s_namedConstantsDeleted);'),upload),
 ]
 for name,a,b in mutants:
  assert (a,b)!=(params,upload),name
  assert run(name,'O2',a,b)!=0,name+' survived'
  print(name+': compiled, expected runtime RED')
