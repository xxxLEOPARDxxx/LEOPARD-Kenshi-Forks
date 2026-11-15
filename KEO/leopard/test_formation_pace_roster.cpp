// Actual production roster poll and pace begin/publish, with real pace-table
// policy. Engine roster access and group native effects are substituted only.
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "movement/formation_pace_policy.h"
// PRODUCTION_GROUPS
static FormationGroup formationGroups[MAX_FORMATION_GROUPS];
static bool s_armed=true;
static PaceTable s_table;
static PaceEntry s_staged[PACE_TABLE_MAX];static int s_stagedCount=0;
// PRODUCTION_PACE
static uintptr_t rosterOwner=0;static const int RVA_GLOBAL_PLAYER=0;
static unsigned rosterCount=0;static uintptr_t*roster=NULL;
static unsigned countReads=0,stuffReads=0,groupPolls=0,followPolls=0,checks=0;
static const uintptr_t paced=0x1234;
static double ElapsedSec(){return 10;}
static void*GameAddr(int){return &rosterOwner;}
static unsigned GetPlayerCharCount(uintptr_t){++countReads;return rosterCount;}
static uintptr_t*GetPlayerCharStuff(uintptr_t){++stuffReads;return roster;}
static void PollFormationGroup(int,double,unsigned int count,uintptr_t*stuff){
 ++groupPolls;if(count!=rosterCount||stuff!=roster)std::abort();
 if(s_armed&&s_stagedCount==0){s_staged[0].character=paced;s_staged[0].factor=0.75f;s_stagedCount=1;}
}
static void FormationFollowPoll(double,unsigned int,uintptr_t*){++followPolls;}
// PRODUCTION_POLL
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%u %s\n",__LINE__,#x);return 1;}}while(0)
static void Seed(){
 memset(formationGroups,0,sizeof(formationGroups));
 for(int g=0;g<MAX_FORMATION_GROUPS;++g){formationGroups[g].active=true;formationGroups[g].count=2;formationGroups[g].groupId=unsigned(77+g);formationGroups[g].members[0].character=0x1111;formationGroups[g].createdTime=9;}
 PaceEntry old={paced,0.5f};PaceTablePublish(&s_table,&old,1);
 s_staged[0]=old;s_stagedCount=1;
 countReads=stuffReads=groupPolls=followPolls=0;
}
int main(){
 uintptr_t*unreadable=static_cast<uintptr_t*>(VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));C(unreadable!=NULL);
 FormationGroup saved[MAX_FORMATION_GROUPS];
 // Missing singleton, missing stuff, empty roster, and over-cap roster must
 // all revoke old speed output without touching any Character pointer.
 for(int shape=0;shape<4;++shape){
  Seed();memcpy(saved,formationGroups,sizeof(saved));rosterOwner=shape==0?0:1;roster=shape==1?NULL:unreadable;rosterCount=shape==2?0:201;
  PollFormationGroups();
  C(PaceTableRead(&s_table,paced)==1.0f); // fail first on old missing-roster path
  C(s_table.count==0&&s_stagedCount==0&&groupPolls==0&&followPolls==0);
  C(memcmp(saved,formationGroups,sizeof(saved))==0);
  C(countReads==unsigned(shape!=0)&&stuffReads==unsigned(shape!=0));
 }
 uintptr_t good[200]={paced};
 Seed();memcpy(saved,formationGroups,sizeof(saved));rosterOwner=1;roster=good;rosterCount=200;PollFormationGroups();
 C(groupPolls==MAX_FORMATION_GROUPS&&followPolls==1&&countReads==1&&stuffReads==1);
 C(PaceTableRead(&s_table,paced)==0.75f&&s_table.count==1&&s_stagedCount==1);
 C(memcmp(saved,formationGroups,sizeof(saved))==0);
 rosterOwner=0;PollFormationGroups();C(PaceTableRead(&s_table,paced)==1.0f); // valid -> invalid
 rosterOwner=1;PollFormationGroups();C(PaceTableRead(&s_table,paced)==0.75f); // invalid -> valid
 s_armed=false;s_table.count=0;s_stagedCount=0;rosterOwner=0;PollFormationGroups();C(s_table.count==0&&s_stagedCount==0);
 VirtualFree(unreadable,0,MEM_RELEASE);
 std::printf("PASS %u actual production formation roster/pace controls\n",checks);return 0;
}
