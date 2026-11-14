"""Expected-RED controls of the actual production collector/state/cap code."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/resource_geometry_test/mutants'
OUT.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=';'.join([str(vc/'bin/amd64'),str(sdk/'Bin'),str(ROOT.parent.parent),env['PATH']])
cl=str(vc/'bin/amd64/cl.exe')

def run(name,source_path,before,after,gpu=False):
    directory=OUT/name
    directory.mkdir(exist_ok=True)
    src=source_path.read_text()
    assert src.count(before)==1,(name,'mutation location')
    mutant=directory/'production.cpp'
    mutant.write_text(src.replace(before,after))
    test=(ROOT/'leopard'/('test_resource_geometry_gpu.cpp' if gpu else 'test_resource_geometry.cpp')).read_text()
    if gpu:
        original=(ROOT/'leopard/test_resource_gpu.cpp').read_text()
        original=original.replace('#include "../src/resources/resource_gpu_store.cpp"','#include "'+mutant.as_posix()+'"')
        (directory/'gpu_fixture.cpp').write_text(original)
        test=test.replace('#include "test_resource_gpu.cpp"','#include "gpu_fixture.cpp"')
    else:
        test=test.replace('#include "../src/resources/resource_geometry.cpp"','#include "'+mutant.as_posix()+'"')
    fixture=directory/'fixture.cpp'
    fixture.write_text(test)
    exe=directory/'mutant.exe'
    cmd=[cl,'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0',
         '/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',
         str(fixture),'/Fo'+str(directory)+'\\','/Fe'+str(exe)]
    if gpu:cmd.append(str(ROOT/'src/resources/resource_dds.cpp'))
    cmd+=['/link','d3d11.lib','dxgi.lib','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
    build=subprocess.run(cmd,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (directory/'build.log').write_bytes(build.stdout)
    assert build.returncode==0,(name,'BUILD FAILED (not an accepted negative control)')
    result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (directory/'run.log').write_bytes(result.stdout)
    assert result.returncode!=0,(name,'MUTANT SURVIVED')
    print(name+': expected RED')

geometry=ROOT/'src/resources/resource_geometry.cpp'
gpu=ROOT/'src/resources/resource_gpu_store.cpp'
run('temporary-request-instead-of-entity',geometry,'    Observe(actual);','    Observe(requested);')
run('no-native-pin',geometry,'Entry e;e.mesh=m;e.desc=d;e.touch=touch;','Entry e;e.desc=d;e.touch=touch;')
run('no-local-dedup',geometry,'if(d->buffers[i].id==b)return true;','if(false)return true;')
run('no-cpu-shadow',geometry,'b->hasShadowBuffer()?U64(b->getSizeInBytes()):0','0')
run('ignore-pose-resources',geometry,'if(m->hasManualLodLevel()||m->getNumAnimations()!=0||m->getPoseCount()!=0||m->getNumSubMeshes()>64)return false;','if(m->hasManualLodLevel()||m->getNumAnimations()!=0||m->getNumSubMeshes()>64)return false;')
run('ignore-owned-unused-vertices',geometry,'if(!Vertices(s->vertexData,d))return false;','if(!s->useSharedVertices&&!Vertices(s->vertexData,d))return false;')
run('ignore-pressure',geometry,'state->stats.pressure=!resource_gpu::GeometryStandbyAllowed(state->cap);','state->stats.pressure=false;')
run('ignore-device-change',geometry,'||state->device!=device','')
run('ignore-world-change',geometry,'||state->world!=world','')
run('independent-terrain-cap',gpu,'CacheRoom(cap-geometryBytes,Read64(allocated),bytes)','CacheRoom(cap,Read64(allocated),bytes)',True)
run('double-charge-dxgi',gpu,'||!probe||!probe(device,&budget,&usage,probeContext)||!BudgetRoom(budget,usage,0))return false;','||!probe||!probe(device,&budget,&usage,probeContext)||!BudgetRoom(budget,usage,bytes))return false;',True)
print('geometry expected-RED controls passed')
