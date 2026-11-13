#include <Windows.h>
#include <map>
#include <string>
#include <cstring>
#include <cstdio>
#include <vector>
#define KILLCOUNTER_BRIDGE_BUILD
#include "../ExecutionBridge.h"
enum itemType{CHARACTER=1};
struct Character;
struct hand{unsigned type,container,containerSerial,index,serial;
 hand():type(0),container(0),containerSerial(0),index(0),serial(0){}
 hand(unsigned i,unsigned s,itemType t,unsigned c,unsigned cs):type(t),container(c),containerSerial(cs),index(i),serial(s){}
 hand(Character*);
 Character* getCharacter()const;
 bool operator<(const hand&o)const{if(type!=o.type)return type<o.type;if(container!=o.container)return container<o.container;if(containerSerial!=o.containerSerial)return containerSerial<o.containerSerial;if(index!=o.index)return index<o.index;return serial<o.serial;}
};
struct MedicalSystem{bool dead,down;MedicalSystem():dead(false),down(false){}bool isDead(){return dead;}bool isUnconcious(){return down;}};
struct Character{hand handle;MedicalSystem med;bool player;Character(unsigned i=0,bool p=false):handle(i,i+100,CHARACTER,5,8),player(p){}MedicalSystem*getMedical(){return &med;}bool isPlayerCharacter(){return player;}const hand&getHandle(){return handle;}};
std::map<hand,Character*> registry;
hand::hand(Character*c){*this=c?c->handle:hand();}
Character*hand::getCharacter()const{std::map<hand,Character*>::iterator i=registry.find(*this);return i==registry.end()?NULL:i->second;}
struct KillStats{int kills,kos,limbs;KillStats():kills(0),kos(0),limbs(0){}};
struct PendingKill{Character*attacker;Character*koCreditor;PendingKill():attacker(NULL),koCreditor(NULL){}};
std::map<Character*,KillStats>g_stats;std::map<hand,PendingKill>g_pendingKills;bool g_enabled=true;
namespace PluginDiagnostics{bool Enabled(){return false;}}
void DebugLog(const std::string&){}void ErrorLog(const std::string&){}
static volatile LONG* interceptedGeneration=NULL;static HANDLE epochEntered=NULL,epochReleased=NULL;static DWORD testMainThread=0;
static LONG ReviewCompareExchange(volatile LONG* target,LONG exchange,LONG compare)
{
 LONG value=InterlockedCompareExchange(target,exchange,compare);
 if(target==interceptedGeneration && GetCurrentThreadId()!=testMainThread){SetEvent(epochEntered);WaitForSingleObject(epochReleased,2000);}
 return value;
}
#undef InterlockedCompareExchange
#define InterlockedCompareExchange ReviewCompareExchange
#include "../ExecutionBridge.inl"
#undef InterlockedCompareExchange
#include "production_pending_kills.inc"
unsigned checks=0;int failures=0;
#define C(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
KillCounterExecutionV1 Packet(Character&a,Character&v){KillCounterExecutionV1 r={};r.size=sizeof(r);r.version=1;WriteExecutionIdentity(r.actor,a.getHandle());WriteExecutionIdentity(r.victim,v.getHandle());return r;}
void Clean(){ResetExecutionBridge();g_stats.clear();g_pendingKills.clear();registry.clear();g_enabled=true;g_bridgeLifecycleReady=true;g_bridgeWorldThread=GetCurrentThreadId();}
void Add(Character&c){registry[c.handle]=&c;}
static void* guardedRequest=NULL;static bool resetDuringRead=false;
static LONG CALLBACK ResetDuringRequestRead(EXCEPTION_POINTERS* exception)
{
 if(exception->ExceptionRecord->ExceptionCode==0x80000001u &&
    exception->ExceptionRecord->ExceptionInformation[1]>=(ULONG_PTR)guardedRequest &&
    exception->ExceptionRecord->ExceptionInformation[1]<(ULONG_PTR)guardedRequest+4096)
 {
  ResetExecutionBridge();g_bridgeWorldThread=GetCurrentThreadId();resetDuringRead=true;
  return EXCEPTION_CONTINUE_EXECUTION;
 }
 return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI Worker(void*p){return KillCounter_RecordExecutionV1((const KillCounterExecutionV1*)p);}
int main(){
 testMainThread=GetCurrentThreadId();C(sizeof(KillCounterExecutionV1)==48);Character a(1,true),b(2,true),v(3);Add(a);Add(b);Add(v);Clean();Add(a);Add(b);Add(v);KillCounterExecutionV1 r=Packet(a,v);
 C(KillCounter_RecordExecutionV1(&r)==0);v.med.dead=true;v.med.down=true;
 C(KillCounter_RecordExecutionV1(&r)==1);C(g_stats[&a].kills==1);C(g_stats[&a].kos==0);
 C(KillCounter_RecordExecutionV1(&r)==2);C(g_stats[&a].kills==1);
 r.actor[4]++;C(KillCounter_RecordExecutionV1(&r)==0);r=Packet(a,v);r.victim[4]++;C(KillCounter_RecordExecutionV1(&r)==0);
 r=Packet(a,v);r.version=2;C(KillCounter_RecordExecutionV1(&r)==0);r.version=1;r.size=47;C(KillCounter_RecordExecutionV1(&r)==0);
 C(KillCounter_RecordExecutionV1(NULL)==0);C(KillCounter_RecordExecutionV1((const KillCounterExecutionV1*)1)==0);
 Clean();Add(a);Add(b);Add(v);r=Packet(b,v);g_stats[&a].kos=1;g_stats[&b].kos=7;PendingKill pk;pk.attacker=&a;pk.koCreditor=&a;g_pendingKills[v.handle]=pk;
 C(KillCounter_RecordExecutionV1(&r)==1);C(g_stats[&b].kills==1&&g_stats[&b].kos==7);C(g_stats[&a].kos==1);C(g_pendingKills.empty());
 Clean();Add(a);Add(b);Add(v);g_stats[&a].kos=1;g_stats[&b].kos=7;g_pendingKills[v.handle]=pk;UpdatePendingKills();
 C(g_stats[&a].kills==1&&g_stats[&a].kos==0);r=Packet(b,v);C(KillCounter_RecordExecutionV1(&r)==1);
 C(g_stats[&a].kills==0&&g_stats[&a].kos==1);C(g_stats[&b].kills==1&&g_stats[&b].kos==7);C(KillCounter_RecordExecutionV1(&r)==2);
 g_pendingKills[v.handle]=pk;UpdatePendingKills();C(g_pendingKills.empty());C(g_stats[&a].kills==0&&g_stats[&b].kills==1);
 Clean();Add(a);Add(v);g_stats[&a].kos=2;g_pendingKills[v.handle]=pk;r=Packet(a,v);C(KillCounter_RecordExecutionV1(&r)==1);C(g_stats[&a].kos==1&&g_stats[&a].kills==1);
 g_enabled=false;C(KillCounter_RecordExecutionV1(&r)==0);g_enabled=true;C(KillCounter_RecordExecutionV1(&r)==2);
 a.player=false;r=Packet(a,v);C(KillCounter_RecordExecutionV1(&r)==0);a.player=true;
 ForgetExecutionCharacter(&v);C(g_deathReceipts.empty());C(g_receiptReferences.empty());
 Clean();Add(a);Add(b);Add(v);CreditConfirmedDeath(&v,pk,false);ForgetExecutionCharacter(&a);g_stats.erase(&a);registry.erase(a.handle);r=Packet(b,v);C(KillCounter_RecordExecutionV1(&r)==1);C(g_stats.find(&a)==g_stats.end());C(g_stats[&b].kills==1);
 Clean();Add(a);Add(v);r=Packet(a,v);HANDLE thread=CreateThread(NULL,0,Worker,&r,0,NULL);WaitForSingleObject(thread,2000);DWORD result=0;GetExitCodeThread(thread,&result);CloseHandle(thread);
 C(result==1);C(g_stats.empty());DrainExecutionQueue();C(g_stats[&a].kills==1);DrainExecutionQueue();C(g_stats[&a].kills==1);
 Clean();Add(a);Add(v);r=Packet(a,v);g_bridgeWorldThread=GetCurrentThreadId()+1;C(KillCounter_RecordExecutionV1(&r)==1);ResetExecutionBridge();g_bridgeWorldThread=GetCurrentThreadId();DrainExecutionQueue();C(g_stats.empty());
 Clean();Add(a);Add(v);r=Packet(a,v);epochEntered=CreateEvent(NULL,TRUE,FALSE,NULL);epochReleased=CreateEvent(NULL,TRUE,FALSE,NULL);interceptedGeneration=&g_executionGeneration;
 thread=CreateThread(NULL,0,Worker,&r,0,NULL);C(WaitForSingleObject(epochEntered,2000)==WAIT_OBJECT_0);
 ResetExecutionBridge();g_bridgeWorldThread=GetCurrentThreadId();SetEvent(epochReleased);C(WaitForSingleObject(thread,2000)==WAIT_OBJECT_0);GetExitCodeThread(thread,&result);CloseHandle(thread);CloseHandle(epochEntered);CloseHandle(epochReleased);interceptedGeneration=NULL;
 C(result==0&&g_executionQueueCount==0&&g_stats.empty());
 Clean();Add(a);Add(v);r=Packet(a,v);guardedRequest=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(guardedRequest!=NULL);std::memcpy(guardedRequest,&r,sizeof(r));
 PVOID handler=AddVectoredExceptionHandler(1,ResetDuringRequestRead);C(handler!=NULL);DWORD oldProtection=0;C(VirtualProtect(guardedRequest,4096,PAGE_READWRITE|PAGE_GUARD,&oldProtection)!=0);
 C(KillCounter_RecordExecutionV1((const KillCounterExecutionV1*)guardedRequest)==0);C(resetDuringRead&&g_stats.empty()&&g_deathReceipts.empty());
 RemoveVectoredExceptionHandler(handler);VirtualFree(guardedRequest,0,MEM_RELEASE);guardedRequest=NULL;
 Clean();Add(a);std::vector<Character*>victims;for(unsigned i=0;i<4096;++i){Character*c=new Character(1000+i);c->med.dead=true;Add(*c);victims.push_back(c);PendingKill p;p.attacker=&a;C(CreditConfirmedDeath(c,p,false)==1);}
 Character extra(6000);extra.med.dead=true;Add(extra);r=Packet(a,extra);C(KillCounter_RecordExecutionV1(&r)==0);C(g_deathReceipts.size()==4096);PendingKill p;p.attacker=&a;C(CreditConfirmedDeath(&extra,p,false)==1);C(g_stats[&a].kills==4097);
 ForgetExecutionCharacter(victims[0]);r=Packet(a,extra);C(KillCounter_RecordExecutionV1(&r)==0);C(g_stats[&a].kills==4097);
 ResetExecutionBridge();C(g_deathReceipts.empty()&&g_receiptReferences.empty());for(unsigned i=0;i<victims.size();++i)delete victims[i];
 Clean();Add(a);Add(v);r=Packet(a,v);g_bridgeLifecycleReady=false;C(KillCounter_RecordExecutionV1(&r)==0);PendingKill native;native.attacker=&a;C(CreditConfirmedDeath(&v,native,false)==1);C(g_stats[&a].kills==1&&g_deathReceipts.empty());
 Clean();Add(a);Add(v);r=Packet(a,v);g_bridgeWorldThread=0;C(KillCounter_RecordExecutionV1(&r)==0);g_bridgeWorldThread=GetCurrentThreadId()+1;C(KillCounter_RecordExecutionV1(&r)==1);C(KillCounter_RecordExecutionV1(&r)==2);
 for(unsigned i=1;i<64;++i){r.victim[3]=10000+i;C(KillCounter_RecordExecutionV1(&r)==1);}r.victim[3]=20000;C(KillCounter_RecordExecutionV1(&r)==0);C(g_executionQueueCount==64);
 DrainExecutionQueue();C(g_stats[&a].kills==1&&g_executionQueueCount==0);C(g_deathReceipts.size()==1);
 printf("%u execution bridge production checks, %d failures\n",checks,failures);return failures?1:0;
}
