#include <windows.h>
#include <iostream>
#include <sstream>
#include "zone/handoff/zone_prep_ledger.h"
static int checks=0,collectCalls=0,inserts=0,drops=0;
#define C(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";exit(1);}}while(0)
static int CountedCollect(const ZonePrepLedger* l,ZoneIdentity* out,int cap){++collectCalls;return ZonePrepLedgerCollectReady(l,out,cap);}
const int ZONE_ADOPT_COHORT_MAX=32;
static unsigned g_worldEpoch=1;
static long g_cohortSaturated=0,g_cohorts=0;
struct Zone{bool loading,accessible;int gx,gy;};static Zone zones[4096];
static void* GetZoneEntry(void*,int gx,int gy){return &zones[gx*64+gy];}
static bool IsZoneLoading(void* z){return ((Zone*)z)->loading;}
static bool IsZoneAccessible(void* z){return ((Zone*)z)->accessible;}
static bool InsertIntoPendingSet(void*,void*){++inserts;return true;}
static void ClearCountdowns(void*,int){}
static void MarkAdopted(int gx,int gy,double now){C(ZonePrepLedgerSetState(&g_zonePrepLedger,gx,gy,ZONE_STATE_NATIVE_A,now));}
static int FindPreloadedSlot(void*){return 0;}
static void DropTrackedZone(int){++drops;}
static int ZoneCycleSetBSize(void*){return 0;}
#define KEO_DETAIL(x) do{(void)(x);}while(0)
// PRODUCTION
static void Ready(int index)
{
 int x=index/64,y=index%64;C(ZonePrepLedgerBegin(&g_zonePrepLedger,x,y,g_worldEpoch,1));
 C(ZonePrepLedgerSetState(&g_zonePrepLedger,x,y,ZONE_STATE_PRIVATE_CONTENT,2));
 C(ZonePrepLedgerSetState(&g_zonePrepLedger,x,y,ZONE_STATE_PRIVATE_NAV,3));
 C(ZonePrepLedgerSetState(&g_zonePrepLedger,x,y,ZONE_STATE_READY_FOR_ADOPTION,4));
 zones[index].loading=true;zones[index].accessible=false;
}
int main()
{
 ZonePrepLedgerInit(&g_zonePrepLedger);void* mgr=(void*)1;
 AdmitCohort(mgr,5);C(collectCalls==0&&inserts==0&&drops==0&&g_cohorts==0);
 for(int i=0;i<256;++i) AdmitCohort(mgr,5);C(collectCalls==0);
 Ready(4095);AdmitCohort(mgr,5);C(collectCalls==1&&inserts==1&&drops==1&&g_cohorts==1);
 AdmitCohort(mgr,5);C(collectCalls==1);
 for(int i=0;i<33;++i) Ready(i);AdmitCohort(mgr,6);
 C(collectCalls==2&&inserts==33&&g_cohortSaturated==1&&ZonePrepLedgerCountInState(&g_zonePrepLedger,ZONE_STATE_READY_FOR_ADOPTION)==1);
 AdmitCohort(mgr,7);C(inserts==34&&collectCalls==3&&g_cohorts==3);
 Ready(63);zones[63].loading=false;AdmitCohort(mgr,8);C(collectCalls==4&&inserts==34);
 zones[63].loading=true;AdmitCohort(mgr,9);C(collectCalls==5&&inserts==35);
 ZonePrepLedgerClearAll(&g_zonePrepLedger);++g_worldEpoch;AdmitCohort(mgr,10);C(collectCalls==5);
 Ready(128);C(ZonePrepLedgerSetState(&g_zonePrepLedger,2,0,ZONE_STATE_RETIRING,11));C(ZonePrepLedgerRelease(&g_zonePrepLedger,2,0));AdmitCohort(mgr,12);C(collectCalls==5);
 std::cout<<"PASS "<<checks<<" actual cohort + ledger controls\n";
}
