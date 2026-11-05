"""Actual planner destination wrapper with owned normal/nested/unwind seams.
No game process, native calls or real CharMovement lifetime assumptions.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/planner-tls-tests';OUT.mkdir(exist_ok=True)
s=(ROOT/'src/planner/planner_hooks.cpp').read_text();a=s.index('static void hook_setDestinationVec3(');b=s.index('{',a);end=b+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
actual=s[a:end]
fixture=r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
struct PlannerTls{uintptr_t cm;float dest[3];int haveDest,slot,islandSeen,flipped,edgeSeen;};
__declspec(thread) PlannerTls t_plan;
__declspec(thread) int mode,depth,calls;unsigned checks=0;
volatile LONG s_armed=1;struct Counters{volatile LONG rung17,roadPreempt,staleRerequest;}counter;
Counters*PlannerCountersGet(){return &counter;}int PlanStoreFind(uintptr_t p){return int(p&15);}
#define OFF_CMOV_EDGE_COUNTER 0
void C(bool b,const char*s){++checks;if(!b){std::printf("FAIL %s\n",s);std::exit(1);}}
static void hook_setDestinationVec3(void*,const float*,int,bool);
void Original(void*cm,const float*dest,int priority,bool notVertical){++calls;C(t_plan.cm==(uintptr_t)cm&&t_plan.haveDest==(dest!=NULL)&&t_plan.slot==(s_armed?PlanStoreFind((uintptr_t)cm):-1),"original sees only its own synchronous planner context");C(priority==3&&notVertical,"arguments forwarded unchanged");if(mode==1)throw std::runtime_error("owned C++ original failure");if(mode==2)RaiseException(0xe1122334,0,0,NULL);if(mode==3&&!depth){PlannerTls saved=t_plan;++depth;int child=0;float p[3]={4,5,6};hook_setDestinationVec3(&child,p,3,true);--depth;C(std::memcmp(&saved,&t_plan,sizeof(saved))==0,"nested return restores exact outer context");}t_plan.islandSeen=1;t_plan.flipped=1;t_plan.edgeSeen=1;}
void(*orig_setDestinationVec3)(void*,const float*,int,bool)=Original;
@BODY@
void SehCall(void*cm,const float*dest){__try{hook_setDestinationVec3(cm,dest,3,true);}__except(GetExceptionCode()==0xe1122334?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){}}
void Case(int m){PlannerTls saved;std::memset(&saved,0x37,sizeof(saved));saved.cm=123;saved.slot=2;t_plan=saved;mode=m;depth=calls=0;int cm=0;float dest[3]={1,2,3};if(m==2)SehCall(&cm,dest);else{try{hook_setDestinationVec3(&cm,m==4?NULL:dest,3,true);}catch(const std::exception&){C(m==1,"original C++ exception reaches caller unchanged");}}C(std::memcmp(&saved,&t_plan,sizeof(saved))==0,"normal/C++/SEH path restores exact previous TLS");C(calls==(m==3?2:1),"native original runs exactly once per invocation");}
DWORD WINAPI OtherThread(void*){Case(0);Case(1);Case(2);Case(3);return 0;}
int main(){for(int m=0;m<5;++m)Case(m);s_armed=0;Case(0);s_armed=1;PlannerTls main=t_plan;HANDLE thread=CreateThread(NULL,0,OtherThread,NULL,0,NULL);C(thread!=NULL,"owned independent TLS thread");C(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,"owned TLS test thread joined");CloseHandle(thread);C(std::memcmp(&main,&t_plan,sizeof(main))==0,"other thread cannot replace Main planner TLS");std::printf("PASS %u actual planner TLS unwind controls\n",checks);return 0;}
'''.replace('@BODY@',actual)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=os.environ.copy();env['INCLUDE']=f'{VC}/include;{SDK}/Include';env['LIB']=f'{VC}/lib/amd64;{SDK}/Lib/x64'
def run(name,code,opt,negative=False):
 p=OUT/(name+'.cpp');p.write_text(code);exe=OUT/(name+'.exe');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(p),'/Fo'+str(OUT/'tls.obj'),'/Fe'+str(exe)],env=env,capture_output=True,text=True);print(r.stdout+r.stderr,end='');assert r.returncode==0
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout+r.stderr,end='');assert (r.returncode!=0 and 'FAIL ' in r.stdout) if negative else r.returncode==0
for opt in ['Od','O2']:run('tls-'+opt,fixture,opt)
run('mutant-cleared',fixture.replace('t_plan = saved;','std::memset(&t_plan,0,sizeof(t_plan));'),'O2',True)
# Make restoration normal-return-only again; the exact current body otherwise stays.
normal=fixture.replace('__try\n\t{','\n\t{',1).replace('__finally\n\t{','\n\t{',1)
assert normal!=fixture
run('mutant-normal-only',normal,'O2',True)
print('PASS two compiled planner TLS negative controls')
