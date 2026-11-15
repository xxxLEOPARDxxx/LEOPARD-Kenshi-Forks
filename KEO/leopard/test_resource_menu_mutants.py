from pathlib import Path
import subprocess,os
s=Path(__file__).resolve().parents[1];out=s/'build/menu-startup/mutants';out.mkdir(parents=True,exist_ok=True)
src=(s/'src/resources/resource_cache_store.cpp').read_text();test=(s/'leopard/test_resource_menu_startup.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'))
cases=[('observed-also-unpaced','return startup&&On()','return On()',False),('lease-never-expires','&&DWORD(now-DWORD(Load(startupMenuAt)))<=250','&&true',False),('startup-still-paced','if(menuFast){startupBurstBytes+=got;','if(false&&menuFast){startupBurstBytes+=got;',False),
 ('demand-still-paused','const bool speculative=startup||!observed;','const bool speculative=true;',False),
 ('observed-not-promoted','void Observe(const std::string&p){Queue(p,true);}','void Observe(const std::string&p){Queue(p,false);}',False),
 ('prefetch-not-speculative','void Prefetch(const std::string&p){Queue(p,false);}','void Prefetch(const std::string&p){Queue(p,true);}',False),
 ('demand-queue-overflow','if(requests.size()<MaxQueue){requests.push_back(Request(p,demand));','if(requests.size()<=MaxQueue){requests.push_back(Request(p,demand));',False),
 ('demand-ignores-cancel','return Cancelled(e)||(se>=0&&StartupCancelled(se));','return false;',False),
 ('demand-ignores-pressure','if(!MemoryRoom(0))return ReadCapped;','if(false)return ReadCapped;',False),
 ('worker-still-pauses-all','std::string p;bool demand=false;LONG epoch=Load(generation);','if(!Load(backgroundAllowed))continue;std::string p;bool demand=false;LONG epoch=Load(generation);',True),
 ('paused-speculative-self-wake','if((more&&Load(backgroundAllowed))||RunnableLocked())SetEvent(eventHandle);','if(more||!requests.empty())SetEvent(eventHandle);',True),
 ('deferred-prefetch-lost','if(result==ReadDeferred&&!p.empty()&&!demand&&!Cancelled(epoch)){','if(false&&result==ReadDeferred&&!p.empty()&&!demand&&!Cancelled(epoch)){',True)]
for name,a,b,worker in cases:
 assert src.count(a)==1,(name,'unique mutation');d=out/name;d.mkdir(exist_ok=True)
 p=d/'production.cpp';p.write_text(src.replace(a,b),encoding='utf-8')
 body=(s/'leopard/test_resource_cache.cpp').read_text() if worker else test
 f=d/'fixture.cpp';f.write_text(body.replace('#include "../src/resources/resource_cache_store.cpp"','#include "'+p.as_posix()+'"'),encoding='utf-8')
 exe=d/'test.exe';args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I'+str(s/'src'),str(f),'/Fo'+str(d)+'\\','/Fe'+str(exe)]
 build=subprocess.run(args,cwd=s,env=env,capture_output=True);(d/'build.txt').write_bytes(build.stdout+build.stderr);assert build.returncode==0,(name,'not accepted RED compile failure')
 run=subprocess.run([str(exe)],cwd=s,env=env,capture_output=True);(d/'run.txt').write_bytes(run.stdout+run.stderr);assert run.returncode!=0,(name,'survived');print(name+': expected RED')
print('PASS',len(cases),'compiled actual pacing/lease/demand/worker controls, no production edits')
