"""Compiled runtime-RED controls; compilation failure/crash is NOT accepted."""
from pathlib import Path
import os, subprocess, json, hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/retention-victim-mutants';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=';'.join([str(vc/'bin/amd64'),str(sdk/'Bin'),'D:/SteamLibrary/steamapps/common/Kenshi',env['PATH']])
results=[]
def run(name,file,before,after,fixture='test_retention_victim_index.cpp'):
    directory=OUT/name;directory.mkdir(exist_ok=True)
    original=(ROOT/'src/resources'/file).read_text();assert original.count(before)==1,(name,'mutation anchor')
    mutated=original.replace(before,after)
    if file.endswith('.h'):
        path=directory/'resources'/file;path.parent.mkdir(exist_ok=True);path.write_text(mutated)
        test=(ROOT/'leopard'/fixture).read_text()
    else:
        path=directory/'production.cpp';path.write_text(mutated)
        test=(ROOT/'leopard'/fixture).read_text().replace('#include "../src/resources/'+file+'"','#include "'+path.as_posix()+'"')
    # A deliberate CHECK failure exits before stack-owned Ogre/loader teardown:
    # an intentionally inconsistent mutant must be rejected at its assertion,
    # rather than crashing later during fixture destruction.
    failure=';return false;}}while(0)' if file.endswith('.h') else ';return 1;}}while(0)'
    assert test.count(failure)==1
    test=test.replace(failure,';std::exit(1);}}while(0)')
    testpath=directory/'fixture.cpp';testpath.write_text(test);exe=directory/'mutant.exe'
    cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/I'+str(directory),'/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(testpath),'/Fo'+str(directory)+'\\','/Fe'+str(exe)]
    if file.endswith('.cpp'):cmd+=['/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
    b=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True);(directory/'build.log').write_bytes(b.stdout+b.stderr)
    assert b.returncode==0,(name,'BUILD FAILED (not accepted)',(b.stdout+b.stderr).decode(errors='replace'))
    r=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True,timeout=30);(directory/'run.log').write_bytes(r.stdout+r.stderr)
    assert r.returncode==1,(name,'NOT ASSERTION RED',r.returncode,(r.stdout+r.stderr).decode(errors='replace'))
    diagnostic='\n'.join(x for x in (r.stdout+r.stderr).decode(errors='replace').splitlines() if x.startswith('line '))
    assert diagnostic,(name,'no CHECK diagnostic')
    results.append({'name':name,'compiled':True,'exit':r.returncode,'diagnostic':diagnostic,'mutated_sha256':hashlib.sha256(mutated.encode()).hexdigest()})
    print(name+': compiled expected RED '+diagnostic,flush=True)
h='retention_victim_index.h'
run('tie-map-comparator-reversed',h,'return map_->key_comp()(a->first,b->first);','return map_->key_comp()(b->first,a->first);')
run('signed-modular-clock-order',h,'if(a->second.touch<b->second.touch)return true;\n        if(b->second.touch<a->second.touch)return false;','if(static_cast<__int64>(a->second.touch-b->second.touch)<0)return true;\n        if(static_cast<__int64>(b->second.touch-a->second.touch)<0)return false;')
run('ignore-touch-change',h,'void Changed(Iterator i){Repair(i->second.victimSlot);}','void Changed(Iterator i){}')
run('no-oldtouch-insert-repair',h,'i->second.victimSlot=slot;Repair(slot);return true;','i->second.victimSlot=slot;return true;')
run('no-arbitrary-remove-repair',h,'if(slot<size_)Repair(slot);','if(false)Repair(slot);')
run('wrong-moved-slot',h,'nodes_[slot]->second.victimSlot=slot;','nodes_[slot]->second.victimSlot=size_;')
run('reset-leaves-slots',h,'nodes_[size_]->second.victimSlot=~0u;nodes_[size_]=Iterator();','nodes_[size_]=Iterator();')
for kind,file,fixture in [('geometry','resource_geometry.cpp','test_resource_geometry.cpp'),('texture','resource_texture_retention.cpp','test_resource_texture_retention.cpp')]:
    run(kind+'-no-hot-touch-repair',file,'state->victims.Changed(i);','(void)i;',fixture)
    needle='if(!Promote(m,d,touch))' if kind=='geometry' else 'if(!Promote(r,bytes,touch))'
    replacement=needle.replace(',touch)',',++state->clock)')
    run(kind+'-refresh-newtouch',file,needle,replacement,fixture)
    needle='if(state->models.find(m.get())!=state->models.end())return false;' if kind=='geometry' else 'if(state->textures.find(r.get())!=state->textures.end())return false;'
    run(kind+'-duplicate-clears-cache',file,needle,'',fixture)
    run(kind+'-exception-without-clear',file,'}catch(...){\n        Clear();','}catch(...){',fixture)
run('geometry-ignore-buffer-count-cap','resource_geometry.cpp','return state->buffers.size()+unique<=MaxBuffers;','return true;','test_resource_geometry.cpp')
receipt={'controls':results,'count':len(results),'acceptance':'Each compiled successfully and returned CHECK failure exit 1; crashes/compile failures rejected.'}
(ROOT/'leopard/retention-victim-mutants-20261116.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('compiled runtime RED controls='+str(len(results)))
