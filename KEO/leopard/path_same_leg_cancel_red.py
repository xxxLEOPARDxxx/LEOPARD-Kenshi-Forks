"""Actual hook_getZoneEdge extraction + unchanged PlanStore/PlanPolicy, VC10 Od/O2.
Converted from the preserved 2026-11-05 same-leg RED after baseline reproduction:
old hook SHA2561921b63e8239dc2754e3e7fa2fca13e71bfadeb82e4fcd3f5e8499176427541d.
Baseline cancellation/replacement each published retired1000 over native111.
Owned worker pauses at fake native snap while the sole Main writer changes real
PlanStore. This proves a boundary contract, not actual engine-frame reachability
or elimination of the remaining check-to-publication race.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/path-same-leg-red';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'src/planner/planner_hooks.cpp').read_text(encoding='utf-8-sig');a=s.index('static float* hook_getZoneEdge(');b=s.index('{',a);e=b+1;depth=1
while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
actual=s[a:e]
if '--old-guard-control'in sys.argv:
 start=actual.index('\n\t// Revalidate the plan after the native snap');end=actual.index('\n\tif (got == 1)',start);actual=actual[:start]+actual[end:]
fixture=r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "planner/plan_store.h"
using namespace planner;
struct PlannerTls{uintptr_t cm;float dest[3];int haveDest,slot,islandSeen,flipped,edgeSeen;};
__declspec(thread) PlannerTls t_plan;
struct DoorHitFilterBytes{const void*vtable;unsigned refAndFlags,pad;};
const int OFF_CMOV_POS=0xC4,CG_EXTERIOR_SLOTS=4096;
int s_mode=PLANNER_ON,s_legSpan=2,s_legAim=0;uintptr_t s_retRecheck=0x222,s_retCompute=0x111,returnSite=0x111;const void*s_doorFilterVtable=NULL;
#define _ReturnAddress() ((void*)returnSite)
static HANDLE entered,changed,finished;static bool blockSnap=false,separateNative=false;static int snapResult=1,tlsChange=0;
static int nativeCalls=0,snapCalls=0,checks=0,allocs=0;static float returned[3],nativeOther[3]={222,3,4};static float*returnedPointer=NULL;
static unsigned char cmBytes[512],otherCmBytes[512];static int workerSlot=-1;
void*operator new(size_t n){++allocs;void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){free(p);}
void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
float*NativeEdge(void*,float*out,const float*,const float*,float){++nativeCalls;out[0]=111;out[1]=2;out[2]=3;return separateNative?nativeOther:out;}
float*(*orig_getZoneEdge)(void*,float*,const float*,const float*,float)=NativeEdge;
int Snap(void*,const float*point,float,float,void*,float*out,unsigned*key){++snapCalls;if(blockSnap){SetEvent(entered);if(WaitForSingleObject(changed,5000)!=WAIT_OBJECT_0)ExitProcess(90);}memcpy(out,point,12);out[0]+=10;*key=7;if(tlsChange)t_plan.cm=(uintptr_t)otherCmBytes;return snapResult;}
int(*fn_getClosestPoint)(void*,const float*,float,float,void*,float*,unsigned*)=Snap;
@ACTUAL@
int Write(uintptr_t cm,float point){PlanWrite w;memset(&w,0,sizeof(w));w.cm=cm;w.verdict=PV_LEGGED;w.legCount=2;w.loadedMask=3;w.finalDest[0]=5000;w.legs[0].point[0]=point;w.legs[0].edgeA[0]=w.legs[0].edgeB[0]=point;w.legs[1].point[0]=5000;w.legs[1].isDestination=1;PlanCellOf(point,0,&w.legs[0].cellX,&w.legs[0].cellY);PlanCellOf(5000,0,&w.legs[1].cellX,&w.legs[1].cellY);return PlanStoreWrite(w);}
DWORD WINAPI Call(void*){t_plan.cm=(uintptr_t)cmBytes;t_plan.slot=PlanStoreFind(t_plan.cm);workerSlot=t_plan.slot;t_plan.haveDest=1;t_plan.dest[0]=5000;t_plan.dest[1]=t_plan.dest[2]=0;float dest[3]={5000,0,0};returnedPointer=hook_getZoneEdge(NULL,returned,(float*)(cmBytes+OFF_CMOV_POS),dest,0);SetEvent(finished);return 0;}
void Setup(){PlanStoreTestPauseInRewrite(NULL,NULL);PlanStoreReset();PlanStoreArm(PLANNER_ON);memset(cmBytes,0,sizeof(cmBytes));memset(otherCmBytes,0,sizeof(otherCmBytes));returnSite=s_retCompute;blockSnap=false;separateNative=false;snapResult=1;tlsChange=0;ResetEvent(entered);ResetEvent(changed);ResetEvent(finished);Write((uintptr_t)cmBytes,1000);}
void Wait(HANDLE h,const char*t){C(WaitForSingleObject(h,5000)==WAIT_OBJECT_0,t);}
void ReleaseDuringRewrite(void*){SetEvent(changed);if(WaitForSingleObject(finished,5000)!=WAIT_OBJECT_0)ExitProcess(91);}
void Stale(int change,int snap,bool otherReturn){Setup();snapResult=snap;separateNative=otherReturn;int slot=PlanStoreFind((uintptr_t)cmBytes);PlanView before;C(PlanStoreRead(slot,&before),"real initial store read");int nc=nativeCalls,sc=snapCalls;blockSnap=true;HANDLE worker=CreateThread(NULL,0,Call,NULL,0,NULL);C(worker!=NULL,"owned worker created");Wait(entered,"worker reached real fake-native snap boundary");
 if(change==0)PlanStoreDrop((uintptr_t)cmBytes);
 else if(change==1)Write((uintptr_t)cmBytes,2000);
 else if(change==2){PlanStoreDrop((uintptr_t)cmBytes);C(Write((uintptr_t)otherCmBytes,2000)==slot,"same slot reused by another identity");}
 else if(change==3)C(PlanStoreAdvance(slot,before.epoch,0,1),"sameepoch current leg changed by other valid actor");
 else if(change==4)PlanStoreReset();
 else if(change==5)PlanStoreArm(PLANNER_OFF);
 else if(change==6){PlanStoreTestPauseInRewrite(ReleaseDuringRewrite,NULL);Write((uintptr_t)cmBytes,2000);PlanStoreTestPauseInRewrite(NULL,NULL);}
 SetEvent(changed);Wait(worker,"owned worker joined");CloseHandle(worker);blockSnap=false;C(nativeCalls==nc+1&&snapCalls==sc+1,"native edge and snap each once, no retry on stale plan");
 C(returnedPointer==(otherReturn?nativeOther:returned)&&returned[0]==111&&returned[1]==2&&returned[2]==3,"stale plan preserves original native pointer and all output coordinates");
 if(change==1||change==6){PlanView after;C(PlanStoreRead(slot,&after)&&after.epoch!=before.epoch,"replacement has fresh epoch");Call(NULL);C(returned[0]==(snap==1?2010:2000),"next request independently steers replacement plan");}
 if(change==3){unsigned epoch;int leg;C(PlanStoreLeg(slot,&epoch,&leg)&&epoch==before.epoch&&leg==1,"sameepoch replacement leg remains untouched");}
 if(change==0||change==2||change==4||change==5){nc=nativeCalls;sc=snapCalls;Call(NULL);C(returned[0]==111&&nativeCalls==nc+1&&snapCalls==sc,"next cancelled/unowned/disarmed request is native pass-through");}
}
int main(){entered=CreateEvent(NULL,TRUE,FALSE,NULL);changed=CreateEvent(NULL,TRUE,FALSE,NULL);finished=CreateEvent(NULL,TRUE,FALSE,NULL);C(entered&&changed&&finished,"owned event handles created");
 for(int result=-1;result<=1;++result){Setup();snapResult=result;int nc=nativeCalls,sc=snapCalls;Call(NULL);C(returnedPointer==returned&&returned[0]==(result==1?1010:1000),"stable snap success/failure preserves existing planned steering");C(nativeCalls==nc+1&&snapCalls==sc+1,"stable request each native boundary once");}
 for(int snap=-1;snap<=1;++snap)for(int change=0;change<=6;++change)Stale(change,snap,false);
 Stale(0,1,true);Stale(1,0,true);
 Setup();returnSite=s_retRecheck;((float*)(cmBytes+OFF_CMOV_POS))[0]=1000;Call(NULL);PlanView advanced;C(PlanStoreRead(workerSlot,&advanced)&&advanced.legIndex==1&&returned[0]==5010,"hook own successful advance validates new leg, not old snapshot leg");
 Setup();tlsChange=1;Call(NULL);C(returned[0]==111,"changed synchronous TLS identity cannot publish old steering");
 Setup();returnSite=0x333;int sc=snapCalls;Call(NULL);C(returned[0]==111&&snapCalls==sc,"unknown native callsite stays native");
 Setup();s_mode=PLANNER_OBSERVE;sc=snapCalls;Call(NULL);C(returned[0]==111&&snapCalls==sc,"observe mode never publishes planned point");s_mode=PLANNER_ON;
 Setup();int before=allocs;for(int i=0;i<100;++i)Call(NULL);C(allocs==before,"post-snap freshness check allocates no heap");
 CloseHandle(entered);CloseHandle(changed);CloseHandle(finished);printf("PASS %d actual same-leg steering controls; worker/event handles closed\n",checks);}
'''.replace('@ACTUAL@',actual)
cpp=OUT/'path_same_leg.cpp';cpp.write_text(fixture)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{ROOT}/src;{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
for opt in ('Od','O2'):
 label=('OLD-'if '--old-guard-control'in sys.argv else '')+opt;exe=OUT/('path_same_leg_'+label+'.exe');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp),str(ROOT/'src/planner/plan_store.cpp'),str(ROOT/'src/planner/plan_policy.cpp'),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);(OUT/(label+'-build.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(label,r.stdout.strip());(OUT/(label+'-run.log')).write_text(r.stdout+r.stderr)
 if '--old-guard-control'in sys.argv:assert r.returncode==1 and'stale plan preserves original native pointer'in r.stdout
 else:assert not r.returncode,r.stdout+r.stderr
print(json.dumps({'hook_sha256':hashlib.sha256(actual.encode()).hexdigest(),'source_store':'unmodified src/planner/plan_store.cpp','source_policy':'unmodified src/planner/plan_policy.cpp','engine_frame_reachability':'not established by owned callback-boundary controls'}))
