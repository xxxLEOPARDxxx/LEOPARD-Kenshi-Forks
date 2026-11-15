"""Production wrapper inclusion + actual pinned Ogre lifetime/assignment fixture.
Only its standalone process's native deleting destructor is temporarily detoured.
No game process, installed file, settings, capture, renderer or GPU work.
"""
from pathlib import Path
import os,re,struct,hashlib,importlib.util,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];GAME=ROOT.parents[1]
OUT=ROOT/'build/gpu-params-rebind';OUT.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('pe',ROOT/'tools/kenshilib/check_imports.py');pem=importlib.util.module_from_spec(spec);spec.loader.exec_module(pem)
p=pem.PE(GAME/'OgreMain_x64.dll')
assert hashlib.sha256(p.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'
def read(rva,n):return p.data[p.offset(rva):p.offset(rva)+n]
assert read(0xe89fd,15)==bytes.fromhex('4881c128010000488bfae89412ffff')
assert read(0xd9cdc,14)==bytes.fromhex('488b03488913488b7b0848894b08')
assert read(0xd8443,5)==bytes.fromhex('e898300000')
assert read(0xd8434,5)==bytes.fromhex('e857feffff')
assert read(0xd82af,4)==bytes.fromhex('84d2750e')
assert read(0xd8317,9)==bytes.fromhex('418d5148e8c08a0c00')
assert read(0xd8379,5)==bytes.fromhex('e822190000')
assert read(0x10eeb1,5)==bytes.fromhex('e8da93fcff')
assert read(0xda90e,5)==bytes.fromhex('e81d030000')
assert read(0xda9a0,5)==bytes.fromhex('e88b020000')
assert read(0xdabe5,5)==bytes.fromhex('e846000000')
assert read(0xdac30,15)==bytes.fromhex('40574883ec3048c7442420feffffff')
assert struct.unpack('<Q',read(0x684050,8))[0]-p.image_base==0xdabb0
assert read(0xdabb0,32)==bytes.fromhex('48894c2408574883ec3048c7442420feffffff48895c24488bfa488bd9488d05')
assert read(0xdabd7,19)==bytes.fromhex('488b49284885c9740bba01000000e846000000')
src=(ROOT/'src/render/gpu_params.cpp').read_text()
src=re.sub(r'^#include[^\n]*\n','',src,flags=re.M)
fixture=(ROOT/'leopard/test_gpu_params_rebind.cpp').read_text()
cpp=OUT/'actual.cpp';cpp.write_text(fixture.replace('// PRODUCTION_BODY',src))
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=';'.join([str(vc/'bin/amd64'),str(sdk/'Bin'),str(GAME),env['PATH']])
for mode in ['Od','O2']:
 exe=OUT/(mode+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+mode,'/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(cpp),str(ROOT/'src/render/gpu_param_cache.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
 build=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(mode+'-build.txt')).write_bytes(build.stdout)
 assert build.returncode==0,build.stdout.decode(errors='replace')
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(mode+'-run.txt')).write_bytes(result.stdout);print(result.stdout.decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert result.returncode!=0,'old unconditional invalidation unexpectedly passed';print('expected RED');break
 assert result.returncode==0,'native wrapper fixture failed'
print('PASS actual-image map assignment/fresh manual-reload map/deleter contracts')
if '--expect-red' not in sys.argv:
 for name,before,after in [
  ('unconditional-old-map-flush','bool invalidate = oldMap == newMap || !s_nativeNamedInfoVtable;','bool invalidate = true;'),
  ('same-map-no-invalidation','bool invalidate = oldMap == newMap || !s_nativeNamedInfoVtable;','bool invalidate = !s_nativeNamedInfoVtable;'),
  ('unknown-control-block-accepted','invalidate = !info || *(const void* const*)info != s_nativeNamedInfoVtable;','invalidate = false;'),
  ('last-reference-not-invalidated','InvalidateNamedConstants(namedConstants);','/* missed map deletion */'),
  ('off-owner-deletion-no-flush','InterlockedExchange(&s_flushPending, 1);','; /* missed worker flush */'),
  ('setter-forwarded-twice','s_origSetNamed(params, namedConstantsPtr);','s_origSetNamed(params, namedConstantsPtr);s_origSetNamed(params, namedConstantsPtr);'),
 ]:
  assert src.count(before)==1,(name,'unique mutation')
  mutant=OUT/(name+'.cpp');mutant.write_text(fixture.replace('// PRODUCTION_BODY',src.replace(before,after)))
  exe=OUT/(name+'.exe')
  args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(mutant),str(ROOT/'src/render/gpu_param_cache.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe),'/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
  build=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-build.txt')).write_bytes(build.stdout);assert build.returncode==0,build.stdout.decode(errors='replace')
  result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-run.txt')).write_bytes(result.stdout);assert result.returncode!=0,name+' survived';print(name+': expected RED')
