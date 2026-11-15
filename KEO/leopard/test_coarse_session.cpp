#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
static unsigned recordReads=0,checks=0;
static void(*beforeExchange)()=NULL;
static void(*afterGeneration)()=NULL;
static void AfterGenerationProbe(){if(afterGeneration){void(*fn)()=afterGeneration;afterGeneration=NULL;fn();}}
static PVOID NativeExchange(PVOID volatile*p,PVOID v){return InterlockedExchangePointer(p,v);}
#undef InterlockedExchangePointer
static PVOID SpyExchange(PVOID volatile*p,PVOID v){if(beforeExchange&&v){void(*fn)()=beforeExchange;beforeExchange=NULL;fn();}return NativeExchange(p,v);}
#define InterlockedExchangePointer SpyExchange
// PRODUCTION_STORE
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
using namespace planner;
static int nestedPromoted=-1;
static void DuringPost(){nestedPromoted=CgPromoteLive(8);}
static void DuringGeneration(){CgStoreNewWorld();CgDrainRetired();}
static CgBlock* Block(int uid,int source,unsigned gen){
 TileGraph g;g.interiorsDropped=g.bordersSkipped=0;TgSection s;memset(&s,0,sizeof(s));s.uid=uid;s.nodeCount=1;g.sections.push_back(s);TgNode n;memset(&n,0,sizeof(n));g.nodes.push_back(n);return CgBlockFromTile(g,0,source,gen);
}
static CgBlock* Live(int uid){CgBlock*b=CgLiveAcquire();if(!b)return NULL;b->uid=uid;return b;}
int main(){
 C(CgStoreCreate());recordReads=0;C(CgPromoteLive(8)==0);C(recordReads==0);
 CgBlock*b=Live(7);C(b!=NULL);CgLivePost(4,b);recordReads=0;C(CgPromoteLive(8)==1);C(recordReads>0);recordReads=0;C(CgPromoteLive(8)==0);C(recordReads==0);
 // Replacement owns exactly one pending record; every buffer returns to pool.
 for(int pass=0;pass<200;++pass){CgLivePost(1023,Live(8));CgLivePost(1023,Live(8));C(CgPromoteLive(8)==1);recordReads=0;C(CgPromoteLive(8)==0&&recordReads==0);CgDrainRetired();}
 // A late writer from an old world must not install an unreadable stale over.
 CgBlock*old=Block(9,CG_SAVE,CgStoreGen());CgStoreNewWorld();CgStats before;CgStatsGet(&before);
 C(CgPublishOver(9,old)==CGP_OUTRANKED);CgStats after;CgStatsGet(&after);C(after.stale==before.stale+1);
 CgView v;C(!CgRead(9,&v));CgDrainRetired();
 // Current over stays; stale over is retired in bounded owner slices, base remains.
 CgPublishBase(7,Block(7,CG_BASE,0));CgPublishOver(7,Block(7,CG_SAVE,CgStoreGen()));
 CgStoreNewWorld();CgPublishOver(8,Block(8,CG_SAVE,CgStoreGen()));CgStatsGet(&before);
 for(int i=0;i<CG_DIR_SLOTS/32+2;++i)CgDrainRetired();
 C(s_store->dir[7].over==NULL);C(CgRead(7,&v)&&v.block->source==CG_BASE);C(CgRead(8,&v)&&v.block->source==CG_SAVE);CgStatsGet(&after);C(after.retiredFreed>before.retiredFreed);
 // Stale builder arriving after the sweep is rejected and returned normally.
 old=Block(7,CG_SAVE,CgStoreGen()-1);C(CgPublishOver(7,old)==CGP_OUTRANKED);CgDrainRetired();C(s_store->dir[7].over==NULL);
 CgStoreDestroy();C(CgStoreCreate());recordReads=0;C(CgPromoteLive(8)==0&&recordReads==0);CgStoreDestroy();
 C(CgStoreCreate());beforeExchange=DuringPost;CgLivePost(3,Live(7));C(nestedPromoted==0);C(CgPromoteLive(8)==1);recordReads=0;C(CgPromoteLive(8)==0&&recordReads==0);
 CgLivePost(3,Live(7));beforeExchange=DuringPost;CgLivePost(3,Live(7));C(nestedPromoted==1);C(CgPromoteLive(8)==1);recordReads=0;C(CgPromoteLive(8)==0&&recordReads==0);CgDrainRetired();CgStoreDestroy();
 C(CgStoreCreate());afterGeneration=DuringGeneration;CgPublishOver(7,Block(7,CG_SAVE,CgStoreGen()));C(s_store->dir[7].over!=NULL);CgDrainRetired();C(s_store->dir[7].over==NULL);
 // Sweep stays bounded; an empty low entry held by a writer defers later
 // entries rather than losing the eventual old-generation publication.
 CgPublishOver(70,Block(70,CG_SAVE,CgStoreGen()));CgStoreNewWorld();CgDrainRetired();C(s_store->dir[70].over!=NULL);CgDrainRetired();C(s_store->dir[70].over!=NULL);CgDrainRetired();C(s_store->dir[70].over==NULL);CgStoreDestroy();
 std::printf("PASS %u actual coarse-session controls\n",checks);return 0;
}
