
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
struct Request { unsigned char bytes[0xa0]; int& pri(){return *reinterpret_cast<int*>(bytes+44);} int& kind(){return *reinterpret_cast<int*>(bytes+40);} };
struct Manager {unsigned char bytes[0x300];};
volatile LONG p12DiagSubmitBoostsState6=0,p12DiagSubmitBoostsOrder=0,tiered=0;
namespace pathfind {struct Diag{volatile LONG playerRequestsInFlight;};Diag g_pathDiag={};}
__declspec(thread) int squadBoostTier=0;__declspec(thread) bool squadBoostFromRepathTier=false;
const size_t OFF_REQ_WATER_COST_MULT=0x34;void PlayerRepathTierNoteTiered(){InterlockedIncrement(&tiered);}
namespace planner{void PlannerWaterOnSubmit(float*){}}
namespace game {struct Orig {void(*orig_pathReqSubmit)(void*,void*,bool);};Orig g_hookOrig={};}
void hook_pathReqSubmit(void* sectionMgr, void* requestObj, bool highPriority)
{
	planner::PlannerWaterOnSubmit(requestObj ? (float*)((uintptr_t)requestObj + OFF_REQ_WATER_COST_MULT) : NULL);
	game::g_hookOrig.orig_pathReqSubmit(sectionMgr, requestObj, highPriority);

	if (squadBoostTier > 0)
	{
		int gamePri = *(int*)((uintptr_t)requestObj + 44);
		if (squadBoostTier > gamePri)
		{
			*(int*)((uintptr_t)requestObj + 44) = squadBoostTier;
			if (squadBoostFromRepathTier)
			{
				InterlockedIncrement(&p12DiagSubmitBoostsState6);
				PlayerRepathTierNoteTiered();
				// Not counted into playerRequestsInFlight: that counter feeds
				// the 4x A* search-budget boost in hook_findPathFull, a
				// separate lever this change does not touch. Only the queue
				// tier (req+44) changes here.
			}
			else
			{
				InterlockedIncrement(&p12DiagSubmitBoostsOrder);
				InterlockedIncrement(&pathfind::g_pathDiag.playerRequestsInFlight);
			}
		}
	}


	squadBoostTier = 0;
	squadBoostFromRepathTier = false;
}
struct Context {Context*previous;void*request;void*queue;int tier,nativePriority;bool repath,admitted,consumed;};
__declspec(thread) Context*t_submit=NULL;
unsigned checks=0,failures=0;void C(bool ok,const char*msg){++checks;if(!ok){++failures;printf("FAIL %s\n",msg);fflush(stdout);ExitProcess(1);}}
struct Harness {bool proposed,retire,submitLocked,nested,cppThrow,sehThrow;int observed,observedCounter;bool observedLocked;unsigned submits,enqueues,writes;void*published;HANDLE publish,done;Request*request;};
__declspec(thread) Harness*harness=NULL;
volatile LONG capabilityMask=7,routingEnabled=1;
const uintptr_t NativeSubmitEnqueueReturn=0x3aaf60,NativeCancelEnqueueReturn=0x3aafd5;
bool TryPriorityBeforePublish(void*queue,void**item,uintptr_t caller)
{
 Context*c=t_submit;
 if(!c||!c->admitted||c->consumed||c->tier<=0||!queue||queue!=c->queue||!item||caller!=NativeSubmitEnqueueReturn) return false;
 if(!c->request||*item!=c->request) return false;
 Request*r=reinterpret_cast<Request*>(c->request);
 const int nativePriority=r->pri();
 if(nativePriority!=c->nativePriority||r->kind()!=0||c->tier<=nativePriority) return false;
 c->consumed=true;r->pri()=c->tier;
 if(c->repath){InterlockedIncrement(&p12DiagSubmitBoostsState6);PlayerRepathTierNoteTiered();}
 else {InterlockedIncrement(&p12DiagSubmitBoostsOrder);InterlockedIncrement(&pathfind::g_pathDiag.playerRequestsInFlight);}
 if(harness)++harness->writes;
 return true;
}
void ProposalSubmit(void*manager,void*request,bool highPriority)
{
 Context frame={t_submit,request,manager?static_cast<unsigned char*>(manager)+0xb8:NULL,squadBoostTier,highPriority?20:10,squadBoostFromRepathTier,
  (InterlockedCompareExchange(&capabilityMask,0,0)==7&&InterlockedCompareExchange(&routingEnabled,0,0)!=0),false};
 t_submit=&frame;
 __try {
  planner::PlannerWaterOnSubmit(request?reinterpret_cast<float*>(static_cast<unsigned char*>(request)+OFF_REQ_WATER_COST_MULT):NULL);
  game::g_hookOrig.orig_pathReqSubmit(manager,request,highPriority);
 } __finally {t_submit=frame.previous;}
 // Deliberately no request reads/writes here: original may have published it.
}
void Observe(Harness*h)
{
 h->observed=h->request->pri();h->observedLocked=h->submitLocked;
 h->observedCounter=InterlockedCompareExchange(&pathfind::g_pathDiag.playerRequestsInFlight,0,0);
 if(h->retire){DWORD oldProtect=0;C(VirtualProtect(h->request,4096,PAGE_NOACCESS,&oldProtect)!=FALSE,"owned request retirement");}
}
DWORD WINAPI Worker(void*p){Harness*h=static_cast<Harness*>(p);if(WaitForSingleObject(h->publish,10000)!=WAIT_OBJECT_0)return 3;Observe(h);SetEvent(h->done);return 0;}
void OriginalEnqueue(void*queue,void**item)
{
 Harness*h=harness;++h->enqueues;h->published=*item;h->request=reinterpret_cast<Request*>(*item);
 if(h->publish){SetEvent(h->publish);C(WaitForSingleObject(h->done,10000)==WAIT_OBJECT_0,"controlled consumer completed before native return");}
 else Observe(h);
}
void NativeSubmit(void*manager,void*request,bool highPriority)
{
 Harness*h=harness;++h->submits;h->submitLocked=true;
 Request*r=reinterpret_cast<Request*>(request);r->kind()=0;r->pri()=highPriority?20:10;
 if(h->cppThrow)throw std::runtime_error("owned original C++ unwind");
 if(h->sehThrow)RaiseException(0xe0424242,0,0,NULL);
 if(h->nested){
  h->nested=false;Context*outer=t_submit;Manager other;Request inner={};int tier=squadBoostTier;bool source=squadBoostFromRepathTier;
  squadBoostTier=0;squadBoostFromRepathTier=false;ProposalSubmit(&other,&inner,false);
  C(t_submit==outer,"nested submit restores exact outer context");C(inner.pri()==10,"nested NPC keeps native priority");squadBoostTier=tier;squadBoostFromRepathTier=source;
 }
 void*queue=static_cast<unsigned char*>(manager)+0xb8;void*item=request;
 if(h->proposed)TryPriorityBeforePublish(queue,&item,NativeSubmitEnqueueReturn);
 OriginalEnqueue(queue,&item);h->submitLocked=false;
}
unsigned Invoke(bool proposed,void*manager,void*request,bool highPriority)
{
 __try{if(proposed)ProposalSubmit(manager,request,highPriority);else hook_pathReqSubmit(manager,request,highPriority);return 0;}
 __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION||GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return GetExceptionCode();}
}
void ResetCounters(){InterlockedExchange(&p12DiagSubmitBoostsState6,0);InterlockedExchange(&p12DiagSubmitBoostsOrder,0);InterlockedExchange(&tiered,0);InterlockedExchange(&pathfind::g_pathDiag.playerRequestsInFlight,0);}
void Race(bool proposed,bool retire,bool high)
{
 Manager mgr;Harness h={};h.proposed=proposed;h.retire=retire;h.observed=-1;
 Request*r=static_cast<Request*>(VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));C(r!=NULL,"owned request allocation");if(!r)return;
 h.publish=CreateEventA(NULL,FALSE,FALSE,NULL);h.done=CreateEventA(NULL,FALSE,FALSE,NULL);HANDLE worker=CreateThread(NULL,0,Worker,&h,0,NULL);
 C(h.publish&&h.done&&worker,"owned consumer/event creation");harness=&h;squadBoostTier=45;squadBoostFromRepathTier=!high;ResetCounters();
 unsigned code=Invoke(proposed,&mgr,r,high);C(WaitForSingleObject(worker,10000)==WAIT_OBJECT_0,"owned consumer joined");DWORD result=9;GetExitCodeThread(worker,&result);C(result==0,"owned consumer exit");
 C(h.submits==1&&h.enqueues==1,"original submit and enqueue exactly once");C(h.observedLocked,"consumer observes while native submit mutex still held");
 C(h.observed==(proposed?45:high?20:10),"consumer priority matches publication boundary");
 C(code==(!proposed&&retire?EXCEPTION_ACCESS_VIOLATION:0),"no postpublication request touch in prototype");
 if(!retire)C(r->pri()==45,"current and prototype eventually carry tier45");
 if(high)C(h.observedCounter==(proposed?1:0),"order accounting precedes prototype publication");
 else C(pathfind::g_pathDiag.playerRequestsInFlight==0,"repath accounting does not manufacture order token");
 DWORD oldProtect=0;VirtualProtect(r,4096,PAGE_READWRITE,&oldProtect);VirtualFree(r,0,MEM_RELEASE);CloseHandle(worker);CloseHandle(h.publish);CloseHandle(h.done);
 C(t_submit==NULL,"submit context gone after return");harness=NULL;squadBoostTier=0;squadBoostFromRepathTier=false;
 printf("controlled %s high=%d retired=%d observed=%d code=%08x\n",proposed?"prototype":"CURRENT",high,retire,h.observed,code);
}
void Predicates()
{
 Manager mgr;Request request={},other={};void*q=static_cast<unsigned char*>(static_cast<void*>(&mgr))+0xb8;void*item=&request;
 Context outer={NULL,&request,q,45,20,false,true,false};t_submit=&outer;request.kind()=0;request.pri()=20;
 C(!TryPriorityBeforePublish(static_cast<unsigned char*>(q)+0x40,&item,NativeSubmitEnqueueReturn)&&request.pri()==20&&!outer.consumed,"result queue cannot borrow input context");
 void*wrong=&other;other.kind()=0;other.pri()=20;C(!TryPriorityBeforePublish(q,&wrong,NativeSubmitEnqueueReturn)&&other.pri()==20&&!outer.consumed,"unrelated pointer cannot borrow request tier");
 C(!TryPriorityBeforePublish(q,&item,NativeCancelEnqueueReturn)&&request.pri()==20&&!outer.consumed,"cancel callsite cannot consume request context");
 request.pri()=-1;C(!TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==-1,"negative cancel priority stays unchanged");request.pri()=20;
 request.kind()=2;C(!TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==20,"cancel kind stays unchanged");request.kind()=0;
 request.pri()=60;C(!TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==60,"foreign higher tier is not lowered");request.pri()=20;
 C(!TryPriorityBeforePublish(q,NULL,NativeSubmitEnqueueReturn)&&!outer.consumed,"null item pointer refuses before read");
 C(TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==45&&outer.consumed,"matching synchronous request is tiered before publication");
 request.pri()=20;C(!TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==20,"one consumption per context even if priority is reset");
 t_submit=NULL;C(!TryPriorityBeforePublish(q,&item,NativeSubmitEnqueueReturn)&&request.pri()==20,"no active scope no behavior");
 for(int mask=0;mask<8;++mask)for(int on=0;on<2;++on)for(int high=0;high<2;++high){
  Harness h={};h.proposed=true;harness=&h;capabilityMask=mask;routingEnabled=on;request.pri()=99;squadBoostTier=45;squadBoostFromRepathTier=false;
  ProposalSubmit(&mgr,&request,high!=0);const bool enabled=mask==7&&on;
  C(h.observed==(enabled?45:high?20:10)&&h.writes==(enabled?1:0),"partial hook or routingOFF degrades to native without late write");C(t_submit==NULL,"partial/refused scope cleaned");
 }
 capabilityMask=7;routingEnabled=1;harness=NULL;
}
void NestedAndUnwind()
{
 Manager mgr;Request request={};Harness h={};h.proposed=true;h.nested=true;harness=&h;squadBoostTier=45;squadBoostFromRepathTier=false;
 ProposalSubmit(&mgr,&request,true);C(request.pri()==45&&h.submits==2&&h.enqueues==2&&h.writes==1,"nested NPC cannot erase captured outer player tier");C(!t_submit,"nested context fully cleaned");
 Context outer={NULL,NULL,NULL,0,0,false,false,false};t_submit=&outer;h.cppThrow=true;bool caught=false;
 try{ProposalSubmit(&mgr,&request,true);}catch(const std::runtime_error&){caught=true;}C(caught&&t_submit==&outer,"C++ unwind restores prior context");h.cppThrow=false;h.sehThrow=true;
 C(Invoke(true,&mgr,&request,true)==0xe0424242&&t_submit==&outer,"native unwind restores prior context");t_submit=NULL;harness=NULL;
}
struct ThreadCase {int tier,observed;unsigned writes;};
DWORD WINAPI Publisher(void*arg){ThreadCase*c=static_cast<ThreadCase*>(arg);Manager mgr;Request req={};Harness h={};h.proposed=true;harness=&h;squadBoostTier=c->tier;squadBoostFromRepathTier=false;ProposalSubmit(&mgr,&req,false);c->observed=h.observed;c->writes=h.writes;return t_submit?1:0;}
void Threads()
{
 ThreadCase a={45,0,0},b={0,0,0};HANDLE ta=CreateThread(NULL,0,Publisher,&a,0,NULL),tb=CreateThread(NULL,0,Publisher,&b,0,NULL);C(ta&&tb,"two original-caller test threads created");
 C(WaitForSingleObject(ta,10000)==WAIT_OBJECT_0&&WaitForSingleObject(tb,10000)==WAIT_OBJECT_0,"two publisher threads joined");DWORD ea=9,eb=9;GetExitCodeThread(ta,&ea);GetExitCodeThread(tb,&eb);
 C(!ea&&!eb&&a.observed==45&&a.writes==1&&b.observed==10&&!b.writes,"TLS player/NPC contexts isolated across original caller threads");CloseHandle(ta);CloseHandle(tb);
}
int main(int argc,char**argv)
{
 game::g_hookOrig.orig_pathReqSubmit=NativeSubmit;
 if(argc>1&&strcmp(argv[1],"--require-current-priority")==0){Manager mgr;Request req={};Harness h={};harness=&h;squadBoostTier=45;hook_pathReqSubmit(&mgr,&req,true);C(h.observed==45,"intended current consumer must see45 (known RED)");printf("current intended property failures=%u\n",failures);return failures?1:0;}
 Race(false,false,true);Race(false,false,false);Race(true,false,true);Race(true,false,false);Race(false,true,true);Race(true,true,true);
 Predicates();NestedAndUnwind();Threads();
 printf("publication characterization/prototype %u checks, %u failures\n",checks,failures);return failures?1:0;
}
