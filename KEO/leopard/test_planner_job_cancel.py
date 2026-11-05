"""Actual job hook + actual plan store/policy, with owned native-call boundaries.

The original seam changes only owned order buffers. It is not a live-game proof
of the user's route geometry or an emulation of native pathfinding.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/planner-job-tests'
OUT.mkdir(exist_ok=True)
source = (ROOT / 'src/movement/island_cancel_hooks.cpp').read_text()


def body(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


helpers = ''
if '// BEGIN planner job cancellation' in source:
    helpers = source.split('// BEGIN planner job cancellation', 1)[1].split(
        '// END planner job cancellation', 1)[0]
hook = body(source, 'void hook_addJobSelected(')
fixture = r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "planner/plan_store.h"
#include "planner/plan_build.h"
using namespace planner;
const int ORDER_TYPE_MOVE=29;
struct K7OrderState { bool ok; long long size; uintptr_t head; int headType,curType; };
struct Actor { uintptr_t cm; K7OrderState state; bool readable; } actors[3];
uintptr_t live[3]; unsigned liveCount=3; unsigned checks=0,readCalls=0,listCalls=0;
int mode=0,nativeCalls=0,bookCalls=0,drops=0; bool nested=false;
void C(bool yes,const char* name){++checks;if(!yes){std::printf("FAIL %s\n",name);std::exit(1);}}
Actor* A(uintptr_t ch){return (Actor*)ch;}
bool TrackerPlayerList(uintptr_t** p,unsigned* n){++listCalls;*p=live;*n=liveCount;return liveCount!=0;}
bool TrackerListHas(const uintptr_t*p,unsigned n,uintptr_t ch){for(unsigned i=0;i<n;++i)if(p[i]==ch)return true;return false;}
bool K7ReadOrders(uintptr_t ch,K7OrderState*out){++readCalls;C(TrackerListHas(live,liveCount,ch),"never dereference removed actor");if(!A(ch)->readable)return false;*out=A(ch)->state;return true;}
#define OFF_CHAR_MOVEMENT 0
#define KLIB_MEMBER(domain,base,name,offset) ((uintptr_t)(base)+(offset))
void Write(int i){PlanWrite w={};w.cm=actors[i].cm;w.verdict=PV_LEGGED;w.legCount=1;w.firstLeg=0;w.finalDest[0]=1000;w.destAtPlan[0]=1000;w.waterMult=w.acidMult=1;C(PlanStoreWrite(w)>=0,"actual store accepts owned plan");}
namespace planner {void PlannerDrop(uintptr_t ch){++drops;PlanStoreDrop(A(ch)->cm);}}
void IslandNoteCancelJob(void*,bool){++bookCalls;}
void hook_addJobSelected(void*,int,void*,bool,bool,const float*);
void Original(void*pi,int task,void*subject,bool shift,bool add,const float*location){
 ++nativeCalls;C(pi==(void*)0x123&&task==17&&subject==(void*)0x456&&shift&&location&&location[0]==42,"native job arguments unchanged");
 if(mode==1||mode==2||mode==4)return; // queued, refused and native faction-filtered no-op
 if(mode==5){actors[0].state.head=999;Write(0);return;} // original triggers a fresh nested plan
 if(mode==6){liveCount=2;live[0]=(uintptr_t)&actors[1];live[1]=(uintptr_t)&actors[2];return;}
 if(mode==7){actors[0].state.curType=17;return;}
 if(mode==10){actors[0].readable=false;return;}
 if(mode==11){actors[0].cm=999;Write(0);return;}
 if(mode==12){PlanStoreReset();return;}
 if(mode==13){if(!nested){nested=true;hook_addJobSelected(pi,task,subject,shift,true,location);nested=false;}actors[0].state.head=987;return;}
 actors[0].state.size=0;actors[0].state.head=0;actors[0].state.headType=-1;
}
namespace game {struct Originals{void(*orig_addJobSelected)(void*,int,void*,bool,bool,const float*);}g_hookOrig={Original};}
@HELPERS@
@HOOK@
void Reset(int m){mode=m;PlanStoreReset();PlanStoreArm(PLANNER_ON);liveCount=3;readCalls=listCalls=0;nativeCalls=bookCalls=drops=0;nested=false;for(int i=0;i<3;++i){actors[i].cm=100+i;actors[i].state.ok=true;actors[i].state.size=1;actors[i].state.head=100+i;actors[i].state.headType=29;actors[i].state.curType=29;actors[i].readable=true;live[i]=(uintptr_t)&actors[i];Write(i);}}
bool Held(int i){return PlanStoreFind(actors[i].cm)>=0;}
void Invoke(bool add){float p[3]={42,0,0};hook_addJobSelected((void*)0x123,17,(void*)0x456,true,add,p);}
int main(){
 // The production drop policy deliberately keeps unchanged/held movement destinations.
 float dest[3]={1000,0,0},pos[3]={0,0,0};
 C(PlanDropDue(true,false,1000,dest,dest,dest,false,false)==PDW_NONE,"unchanged destination alone cannot detect canceled orders");
 C(PlanDropDue(true,false,1000,pos,dest,dest,false,true)==PDW_NONE,"halt alone deliberately preserves a route");
 Reset(0);Invoke(false);C(!Held(0),"accepted native job must retire its old route");C(Held(1)&&Held(2),"unaffected live actors keep their routes");C(nativeCalls==1&&bookCalls==1&&drops==1,"one native call and one actual cancellation");
 Reset(1);Invoke(true);C(Held(0)&&readCalls==0&&listCalls==0,"queued job preserves route with no new snapshot work");
 Reset(2);Invoke(false);C(Held(0)&&drops==0,"rejected no-op job preserves route");
 Reset(4);Invoke(false);C(Held(0)&&drops==0,"native faction-filtered no-op preserves route");
 Reset(5);Invoke(false);C(Held(0)&&drops==0,"new plan epoch created inside original is retained");
 Reset(6);Invoke(false);C(drops==0,"removed actor is not dereferenced or dropped");
 Reset(7);Invoke(false);C(!Held(0),"accepted current move-to-job transition retires route");
 Reset(9);actors[0].readable=false;Invoke(false);C(Held(0),"unreadable before state never guesses cancellation");
 Reset(10);Invoke(false);C(Held(0),"unreadable after state never guesses cancellation");
 Reset(11);Invoke(false);C(Held(0)&&drops==0,"movement identity replacement is not canceled");
 Reset(12);Invoke(false);C(drops==0,"world reset leaves no snapshot authority");
 Reset(13);Invoke(false);C(!Held(0)&&nativeCalls==2&&bookCalls==2,"nested queued job cannot overwrite outer snapshot");
 Reset(0);PlanStoreArm(PLANNER_OFF);Invoke(false);C(readCalls==0&&listCalls==0&&drops==0,"routing off adds no character or order scan");
 Reset(0);PlanStoreArm(PLANNER_OBSERVE);Invoke(false);C(!Held(0),"observe plan lifetime follows accepted native cancel too");
 std::printf("PASS %u actual job cancellation controls\n",checks);return 0;
}
'''.replace('@HELPERS@', helpers).replace('@HOOK@', hook)

VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = os.environ.copy()
env['INCLUDE'] = f'{VC}/include;{SDK}/Include;{ROOT}/src'
env['LIB'] = f'{VC}/lib/amd64;{SDK}/Lib/x64'


def run(name, code, opt, negative=False):
    file = OUT / (name + '.cpp')
    file.write_text(code)
    exe = OUT / (name + '.exe')
    args = [str(VC/'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/'+opt, '/MT', str(file)]
    args += [str(ROOT/'src/planner'/f) for f in ['plan_store.cpp', 'plan_policy.cpp', 'plan_build.cpp']]
    args += ['/Fo'+str(OUT)+'\\', '/Fe'+str(exe)]
    result = subprocess.run(args, env=env, cwd=OUT, capture_output=True, text=True)
    print(result.stdout+result.stderr, end='')
    assert result.returncode == 0
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(result.stdout+result.stderr, end='')
    assert (result.returncode != 0 and 'FAIL ' in result.stdout) if negative else result.returncode == 0


for optimization in ['Od', 'O2']:
    run('job-'+optimization, fixture, optimization)

mutants = [
    ('epoch', ' || plan.epoch != s.epoch', ''),
    ('accepted-only',
     'after.size != s.orders.size || after.head != s.orders.head || after.headType != s.orders.headType',
     'true || after.size != s.orders.size || after.head != s.orders.head || after.headType != s.orders.headType'),
]
for name, before, after in mutants:
    assert before in fixture, name
    run('mutant-'+name, fixture.replace(before, after, 1), 'O2', True)
print('PASS two compiled job cancellation negative controls')
