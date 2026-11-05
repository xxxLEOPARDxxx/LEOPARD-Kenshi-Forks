"""Actual production policy controls, owned synchronous native boundaries; no game calls."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/steel-jobs-tests';OUT.mkdir(parents=True,exist_ok=True)
code=r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <new>
#include "jobs/steel_jobs_policy.h"
using namespace keo_jobs;
static int checks=0,allocs=0;
void*operator new(size_t n){++allocs;void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){free(p);}
void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
ActorId Id(unsigned serial){ActorId a={{1,2,3,4,serial}};return a;}
JobToken Token(int type,unsigned serial=0){JobToken t={1,Id(5),type,serial,10};return t;}
struct Boundary{int calls;bool live,complete,arrive,cancel,fault;};
bool Valid(void*c,const JobToken&){return ((Boundary*)c)->live;}bool Complete(void*c){return ((Boundary*)c)->complete;}
void Native(void*c){Boundary*b=(Boundary*)c;++b->calls;if(b->fault)throw std::runtime_error("owned native fault");if(b->cancel)b->live=false;if(b->arrive)b->complete=true;}
BodyOps Ops(Boundary*b){BodyOps o={b,Valid,Complete,Native};return o;}
void Routes(){Switches on={true,true,true},off={false,false,false};RouteState state={};RoutePoint points[3]={{11,10,0,10},{22,10,0,10},{33,30,0,30}};RouteView route={points,3,10};JobToken gather=Token(GATHER_OUTPUT);
 C(FilterScore(GATHER_OUTPUT,7,off,gather,NULL,true)==0,"OFF gather blocks native score without deleting queued row");
 C(FilterScore(242,7,off,Token(242),NULL,false)==7,"native242 untouched: no migration or other-job side effects");
 for(int mask=0;mask<8;++mask){Switches s={(mask&1)!=0,(mask&2)!=0,(mask&4)!=0};C(TaskEnabled(GATHER_OUTPUT,s)==s.gather&&TaskEnabled(STOCK_COUNTER,s)==s.stock&&TaskEnabled(WAYPOINT_ROUTE,s)==s.waypoints,"three live toggles independent in every combination");}
 C(!CanAssign(STOCK_COUNTER,on,true,true,false)&&CanAssign(STOCK_COUNTER,on,true,true,true),"stock assignment only owned player counter");C(!CanAssign(GATHER_OUTPUT,off,true,true,true)&&!CanAssign(242,on,true,true,true)&&!CanAssign(GATHER_OUTPUT,on,false,true,true),"OFF/NPC/native242 never custom-assigned");
 C(SyncRoute(state,1,Id(5),route)&&state.currentSerial==11,"first route row selected");C(FilterScore(WAYPOINT_ROUTE,7,on,Token(WAYPOINT_ROUTE,11),&state,true)==7&&FilterScore(WAYPOINT_ROUTE,7,on,Token(WAYPOINT_ROUTE,22),&state,true)==0,"same-position waypoints distinct by serial");
 Boundary b={};b.live=true;BodyResult r=RunBody(off,gather,NULL,route,Ops(&b));C(r.status==BODY_SKIPPED&&!b.calls,"OFF running gather makes zero originals");r=RunBody(on,gather,NULL,route,Ops(&b));C(r.status==BODY_RAN&&b.calls==1,"enabled gather native body exactly once");
 b.calls=0;r=RunBody(on,Token(WAYPOINT_ROUTE,11),&state,route,Ops(&b));C(r.status==BODY_RAN&&b.calls==1&&state.currentSerial==11,"no arrival never advances or resets movement");b.arrive=true;r=RunBody(on,Token(WAYPOINT_ROUTE,11),&state,route,Ops(&b));C(r.status==BODY_ADVANCE&&r.nextSerial==22&&state.currentSerial==11,"arrival returns proposal without shared-state write");C(CommitWaypoint(state,Token(WAYPOINT_ROUTE,11),route,r.nextSerial)&&state.currentSerial==22,"short commit validates previous serial");C(!CommitWaypoint(state,Token(WAYPOINT_ROUTE,11),route,22),"duplicate old completion refused");
 b.complete=false;r=RunBody(on,Token(WAYPOINT_ROUTE,22),&state,route,Ops(&b));C(r.status==BODY_ADVANCE&&r.nextSerial==33&&CommitWaypoint(state,Token(WAYPOINT_ROUTE,22),route,33),"duplicate-position row still requires its own native completion");b.complete=false;r=RunBody(on,Token(WAYPOINT_ROUTE,33),&state,route,Ops(&b));C(r.nextSerial==11&&CommitWaypoint(state,Token(WAYPOINT_ROUTE,33),route,11),"last waypoint wraps only on arrival");
 b.complete=true;r=RunBody(on,Token(WAYPOINT_ROUTE,11),&state,route,Ops(&b));C(r.status==BODY_RAN,"already completed flag cannot advance twice");b.complete=false;b.cancel=true;int calls=b.calls;r=RunBody(on,Token(WAYPOINT_ROUTE,11),&state,route,Ops(&b));C(r.status==BODY_STALE&&b.calls==calls+1&&state.currentSerial==11,"cancel during native body rejects stale advance");b.live=true;b.cancel=false;b.fault=true;try{RunBody(on,Token(WAYPOINT_ROUTE,11),&state,route,Ops(&b));C(false,"fault propagated");}catch(const std::runtime_error&){}C(state.currentSerial==11,"fault cannot write route state");b.fault=false;
 JobToken old=Token(WAYPOINT_ROUTE,11);C(SyncRoute(state,2,Id(5),route)&&!CommitWaypoint(state,old,route,22),"world/load epoch invalidates old completion");C(SyncRoute(state,2,Id(6),route)&&!SameActor(state.actor,old.actor),"actor serial reuse is distinct identity");RouteState before=state;points[1].serial=11;C(!SyncRoute(state,2,Id(6),route)&&!memcmp(&state,&before,sizeof(state)),"duplicate serial snapshot fails closed unchanged");points[1].serial=22;RouteView tooMany={points,65,10};C(!SyncRoute(state,2,Id(6),tooMany),"64 point cap before reading");
 points[0].serial=33;points[2].serial=11;C(SyncRoute(state,2,Id(6),route)&&state.currentSerial==11,"reordering retains existing current serial");points[2].serial=44;C(SyncRoute(state,2,Id(6),route)&&state.currentSerial==33,"removed current restarts at first native row without deleting others");RouteView changed=route;changed.revision=11;C(!CommitWaypoint(state,old,changed,22),"changed route snapshot rejects completion");RouteView empty={NULL,0,12};C(SyncRoute(state,2,Id(6),empty)&&state.currentSerial==0,"empty native route clears only sidecar selection");JobToken zero={2,Id(6),WAYPOINT_ROUTE,0,12};C(FilterScore(WAYPOINT_ROUTE,7,on,zero,&state,true)==0,"empty route never scores zero serial");
 int allocations=allocs;for(int i=0;i<100;++i)C(SyncRoute(state,2,Id(6),route),"repeat bounded current route sync");C(allocs==allocations,"no per-AI-update heap allocations");
}
struct Item{uint64_t id;unsigned quality,owner,flags;bool source,dest;};
struct StockBoundary{Item items[12];bool live,owned,room,fail,throwMove,cancelMove,cancelFit,cancelLast;int candidates,fits,transfers,rollbacks;};
bool StockLive(void*c,const JobToken&){return ((StockBoundary*)c)->live;}bool Owned(void*c){return ((StockBoundary*)c)->owned;}
bool Candidate(void*c,const StockTypes&,unsigned slot,StockItem*out){StockBoundary&b=*(StockBoundary*)c;++b.candidates;if(slot>=12)return false;StockItem i={b.items[slot].id,55,9};*out=i;return true;}
bool Fit(void*c,const StockItem&){StockBoundary&b=*(StockBoundary*)c;++b.fits;if(b.cancelFit)b.live=false;return b.room;}
bool Transfer(void*c,const StockItem&i){StockBoundary&b=*(StockBoundary*)c;++b.transfers;Item&item=b.items[i.item-100];Item before=item;
 if(b.throwMove)throw std::runtime_error("owned strong transaction fault before mutation");
 item.source=false;if(b.fail){item=before;++b.rollbacks;return false;}item.dest=true;if(b.cancelMove||(b.cancelLast&&b.transfers==8))b.live=false;return true;}
StockOps StockCallbacks(StockBoundary*b){StockOps o={b,StockLive,Owned,Candidate,Fit,Transfer};return o;}
void Fresh(StockBoundary&b){memset(&b,0,sizeof(b));b.live=b.owned=b.room=true;for(unsigned i=0;i<12;++i){b.items[i].id=100+i;b.items[i].quality=123+i;b.items[i].owner=777;b.items[i].flags=0xbeef;b.items[i].source=true;}}
void Stocks(){uint64_t products[]={9,9,4},resources[]={1,2};TypeView p={products,3},n={resources,2},empty={NULL,0};StockTypes types={};
 C(SelectStockTypes(true,p,n,types)&&types.count==2&&types.types[0]==9&&types.types[1]==4,"gather product types deduplicated");C(SelectStockTypes(true,empty,n,types)&&!types.count,"existing empty gather row never falls back to unrelated resources");C(SelectStockTypes(false,p,n,types)&&types.count==2&&types.types[0]==1,"only no gather rows selects native resources");TypeView invalid={products,65};C(!SelectStockTypes(true,invalid,n,types)&&!types.count,"type cap rejects before read");SelectStockTypes(true,p,n,types);
 Switches on={true,true,true},off={false,false,false};StockBoundary b;Fresh(b);StockResult r=RunStock(off,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(!r.attempted&&!b.candidates&&!b.transfers,"OFF stock preserves all source items with zero callbacks");Fresh(b);b.owned=false;r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.stale&&!b.candidates&&!b.transfers,"unowned counter rejected before inventory access");
 Fresh(b);b.room=false;Item before[12];memcpy(before,b.items,sizeof(before));r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.attempted==8&&!r.moved&&!b.transfers&&!memcmp(before,b.items,sizeof(before)),"capacity rejection never removes source and respects eight attempts");
 Fresh(b);b.fail=true;memcpy(before,b.items,sizeof(before));r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(!r.moved&&b.transfers==8&&b.rollbacks==8&&!memcmp(before,b.items,sizeof(before)),"strong owned callback rollback keeps original pointer-state without loss or dup");
 Fresh(b);r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.moved==8&&b.transfers==8&&b.candidates==8,"max eight transfers per native AI update");for(int i=0;i<12;++i)C(b.items[i].quality==123+i&&b.items[i].owner==777&&b.items[i].flags==0xbeef&&b.items[i].source==(i>=8)&&b.items[i].dest==(i<8),"original item identity and every state field preserved");
 Fresh(b);b.cancelLast=true;r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.stale&&r.moved==8&&b.transfers==8,"cancel on final attempt reported stale without a ninth native callback");
 Fresh(b);b.cancelMove=true;r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.stale&&r.moved==1&&b.transfers==1,"cancel after first transaction blocks next mutation");Fresh(b);b.cancelFit=true;r=RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(r.stale&&!r.moved&&!b.transfers,"cancel between capacity and transfer revalidated");Fresh(b);b.throwMove=true;memcpy(before,b.items,sizeof(before));try{RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(false,"stock fault propagated");}catch(const std::runtime_error&){}C(b.transfers==1&&!memcmp(before,b.items,sizeof(before)),"strong callback fault has no policy retry or item loss");
 Fresh(b);int allocations=allocs;RunStock(on,Token(STOCK_COUNTER),types,StockCallbacks(&b));C(allocs==allocations,"no stock policy heap allocation");
}
void Subjects(){RoutePoint p={7,-123.25f,-0.0f,456.5f},decoded={};WaypointSubject s={};C(EncodeWaypoint(999,p,1000,s)&&s.type==999&&s.serial==7&&DecodeWaypoint(s,999,1000,decoded)&&!memcmp(&p,&decoded,sizeof(p)),"native hand payload lossless serial and3float bits");WaypointSubject saved=s;p.x=1001;C(!EncodeWaypoint(999,p,1000,s)&&!memcmp(&s,&saved,sizeof(s)),"bounds failure leaves saved subject unchanged");p.x=0;unsigned bits=0x7fc00000;memcpy(&p.y,&bits,4);C(!EncodeWaypoint(999,p,1000,s),"NaN rejected before persistence");p.y=0;p.serial=0;C(!EncodeWaypoint(999,p,1000,s),"zero serial rejected");C(!DecodeWaypoint(saved,998,1000,decoded)&&!DecodeWaypoint(saved,999,10,decoded),"wrong sentinel and world bounds fail closed");saved.serial=0;C(!DecodeWaypoint(saved,999,1000,decoded),"corrupt loaded serial rejected");}
int main(){Routes();Stocks();Subjects();printf("PASS %d actual Steel jobs policy controls\n",checks);}
'''
cpp=OUT/'policy.cpp';cpp.write_text(code)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{ROOT}/src;{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
source=ROOT/'src/jobs/steel_jobs_policy.cpp'
# Negative control removes the real OFF score gate from an extracted production copy.
if '--red-control'in sys.argv:
 changed=source.read_text(encoding='utf-8-sig').replace('if(!TaskEnabled(type,s)||t.type!=type||!t.epoch)return 0;','if(t.type!=type||!t.epoch)return 0;');assert changed!=source.read_text(encoding='utf-8-sig');source=OUT/'ungated.cpp';source.write_text(changed)
for opt in ('Od','O2'):
 label=('RED-'if '--red-control'in sys.argv else '')+opt;exe=OUT/('policy-'+label+'.exe');cmd=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp),str(source),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)];r=subprocess.run(cmd,env=env,capture_output=True,text=True);(OUT/('build-'+label+'.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(label,r.stdout.strip());(OUT/('run-'+label+'.log')).write_text(r.stdout+r.stderr)
 if '--red-control'in sys.argv:assert r.returncode==1 and'OFF gather'in r.stdout
 else:assert not r.returncode,r.stdout+r.stderr
