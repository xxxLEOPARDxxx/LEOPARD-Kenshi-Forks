// Real production ring, deterministic Win32 interleavings. No game or native SDK.
#include <windows.h>
#include <stdio.h>
#include "navmesh/construction/splice_ring.h"

static navmesh::SpliceRing ring;
static volatile LONG mode=0, arrived=0;
static HANDLE readyEvent, releaseEvent;
static __declspec(thread) bool paused=false;
enum { NORMAL=0, CAPACITY_RACE, DELAY_PUBLICATION };

static void AfterReservation(volatile LONG* slot)
{
    if (slot==&ring.written && mode==DELAY_PUBLICATION && !paused)
    {
        paused=true;
        SetEvent(readyEvent);
        WaitForSingleObject(releaseEvent,5000);
    }
}
static LONG TestIncrement(volatile LONG* slot)
{
    const LONG result=InterlockedIncrement(slot);
    AfterReservation(slot);
    return result;
}
static LONG TestCompareExchange(volatile LONG* slot,LONG exchange,LONG compare)
{
    const LONG result=InterlockedCompareExchange(slot,exchange,compare);
    if (slot==&ring.written && exchange==0 && compare==0 && mode==CAPACITY_RACE && !paused)
    {
        paused=true;
        if (InterlockedIncrement(&arrived)==2) SetEvent(readyEvent);
        WaitForSingleObject(releaseEvent,5000);
    }
    if (exchange!=compare && result==compare) AfterReservation(slot);
    return result;
}
#undef InterlockedIncrement
#undef InterlockedCompareExchange
#define InterlockedIncrement TestIncrement
#define InterlockedCompareExchange TestCompareExchange
#include "../../src/navmesh/construction/splice_ring.cpp"
#undef InterlockedIncrement
#undef InterlockedCompareExchange

using namespace navmesh;
static int checks=0,failed=0;
static void Check(bool ok,const char* name) { ++checks; if(!ok) { ++failed; printf("FAIL %s\n",name); } }
static int Drain(float out[64][6])
{
#if KEO_RING_HAS_REQUIRED_DRAIN
    return SpliceRingDrainRequired(&ring,out,64);
#else
    return SpliceRingDrain(&ring,out,64); // baseline has only the lossy consumer
#endif
}
static void Init()
{
    mode=NORMAL; arrived=0; SpliceRingInit(&ring);
    ResetEvent(readyEvent); ResetEvent(releaseEvent);
}
static volatile LONG results[2];
static DWORD WINAPI Producer(void* context)
{
    paused=false;
    const int i=(int)(INT_PTR)context;
    const float box[6]={(float)(100+i),1,2,3,4,5};
    results[i]=(LONG)SpliceRingTryPush(&ring,box);
    return 0;
}
static bool Join(HANDLE thread)
{
    if(!thread) return false;
    const bool joined=WaitForSingleObject(thread,6000)==WAIT_OBJECT_0;
    CloseHandle(thread);
    return joined;
}
static void CapacityRace()
{
    Init();
    for(int i=0;i<63;++i) { float box[6]={(float)i,0,0,1,1,1}; SpliceRingTryPush(&ring,box); }
    mode=CAPACITY_RACE; results[0]=results[1]=-1;
    HANDLE a=CreateThread(NULL,0,Producer,(void*)0,0,NULL);
    HANDLE b=CreateThread(NULL,0,Producer,(void*)1,0,NULL);
    const bool met=WaitForSingleObject(readyEvent,3000)==WAIT_OBJECT_0;
    SetEvent(releaseEvent);
    const bool joinedA=Join(a), joinedB=Join(b); mode=NORMAL;
    Check(met && joinedA && joinedB,"capacity interleaving exercised and joined");
    const int taken=(results[0]==SPLICE_PUSH_TAKEN)+(results[1]==SPLICE_PUSH_TAKEN);
    const int full=(results[0]==SPLICE_PUSH_FULL)+(results[1]==SPLICE_PUSH_FULL);
    Check(taken==1 && full==1 && ring.written==64,"one free slot admits exactly one producer");
    float out[64][6]; const int n=Drain(out);
    Check(n==64 && ring.lost==0 && ring.overwritten==0,"accepted bounded records are all delivered");
    bool oldKept=n>=63;
    for(int i=0;i<63 && oldKept;++i) oldKept=out[i][0]==(float)i;
    Check(oldKept,"unread older records are never overwritten by capacity race");
}
static void DelayedPublication()
{
    Init(); mode=DELAY_PUBLICATION; results[0]=-1;
    HANDLE thread=CreateThread(NULL,0,Producer,(void*)0,0,NULL);
    const bool reserved=WaitForSingleObject(readyEvent,3000)==WAIT_OBJECT_0;
    float out[64][6]; int premature=0;
    for(int i=0;i<8;++i) premature+=Drain(out);
    Check(reserved && ring.written==1,"producer paused after reservation before publication");
    Check(premature==0 && ring.read==0 && ring.lost==0,"required consumer retains active reservation over repeated drains");
    SetEvent(releaseEvent); const bool joined=Join(thread); mode=NORMAL;
    Check(joined && results[0]==SPLICE_PUSH_TAKEN,"paused producer publishes accepted record");
    const int n=Drain(out);
    Check(n==1 && out[0][0]==100 && ring.read==1 && ring.lost==0,"late accepted publication is delivered once");
    Check(Drain(out)==0,"accepted publication is never delivered twice");
}
static void FailedClaim()
{
    Init(); ring.slot[0].seq=SPLICE_SEQ_WRITING;
    ring.written=64; ring.read=64; ring.readPub=64;
    const float box[6]={7,0,0,1,1,1};
    const SplicePush result=SpliceRingTryPush(&ring,box);
    float out[64][6]; int n=0;
    for(int i=0;i<8;++i) n+=Drain(out);
    Check(result==SPLICE_PUSH_CLAIM_FAILED && ring.claimFailed==1,"failed claim is refused for native fallback");
    Check(n==0 && ring.read==65 && ring.lost==1,"explicit failed claim cannot wedge reliable drain");
}
int main()
{
    readyEvent=CreateEvent(NULL,TRUE,FALSE,NULL); releaseEvent=CreateEvent(NULL,TRUE,FALSE,NULL);
    if(!readyEvent || !releaseEvent) return 2;
    CapacityRace(); DelayedPublication(); FailedClaim();
    CloseHandle(readyEvent); CloseHandle(releaseEvent);
    printf("splice_ring_delivery: %d checks, %d failed\n",checks,failed);
    return failed?1:0;
}
