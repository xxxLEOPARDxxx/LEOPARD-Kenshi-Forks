#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <new>
#include "plugin/hook_manifest.h"
#include "plugin/independent_hook_batch.h"
struct Prologue{uintptr_t rva;};
static Prologue g_hookPrologues[HOOK_ROW_COUNT];
static volatile LONG s_installed[HOOK_ROW_COUNT],s_gatePassed[HOOK_ROW_COUNT];
// BATCH_GLOBALS
static unsigned checks=0,queued=0,applied=0,immediate=0,verified=0,completed=0,refused=0;
static int activeMask=15,applyResult=0,queueFailure=-1;
static bool sdk=true,fresh=true,reenter=false,throwCompletion=false;
static void**nativeOriginal[16];static HookRowId rows[4]={HOOK_CHARACTER_UPDATE_ONSCREEN_CHECK,HOOK_CHARACTER_PAUSED_UPDATE,HOOK_ACTIVEPLATOON_CALC_POS,HOOK_FACTION_RELATIONS_UPDATE};
static void* originals[4];static int installed=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);throw 77;}}while(0)
static int Native(int n){return n+7;}
static void Complete(const char*why){++completed;if(why)++refused;if(throwCompletion)throw 33;}
static void OtherComplete(const char*why){C(why!=NULL);}
static bool VerifyPrologueByRva(uintptr_t){++verified;return fresh;}
static void*GameAddr(uintptr_t rva){return (void*)(rva+0x10000);}
static HookAdmit HookInstallAdmitActual(bool v,bool ok,bool passed){return(v?ok:passed)?HOOK_ADMIT:HOOK_REFUSE_PROLOGUE;}
#define HookInstallAdmit HookInstallAdmitActual
static int Queue(size_t id,void*,void*,void**o){C(id==0x11110000);int i=int(queued++);nativeOriginal[i]=o;*o=i==queueFailure?NULL:(void*)&Native;return i==queueFailure?1:0;}
static int Apply(size_t id){C(id==0x11110000);++applied;C(completed==0);for(unsigned i=0;i<queued;++i)if((activeMask&(1<<i))&&int(i)!=queueFailure){C(*nativeOriginal[i]!=NULL);C(((int(*)(int))*nativeOriginal[i])(3)==10);}if(reenter){unsigned n=verified;void*saved=originals[0];HookInstallIndependentRow(rows[0],(void*)1,&originals[0],&installed,true,OtherComplete);C(verified==n&&originals[0]==saved&&s_independentDone[rows[0]]==Complete);}return applyResult;}
static bool BeginIndependentHooks(){return sdk&&s_independentBatch.Begin(0x11110000,Queue,Apply);}
const char*HookInstallRow(HookRowId id,void*,void**o,int*n,bool){++immediate;*o=(void*)&Native;s_installed[id]=1;if(n)++*n;return NULL;}
// ADAPTER_PRODUCTION
static void Reset(){s_independentBatch.~HookBatch();new(&s_independentBatch)HookBatch();s_independentPhase=false;s_independentCount=NULL;memset(s_independentDone,0,sizeof(s_independentDone));memset(s_independentClaimed,0,sizeof(s_independentClaimed));memset((void*)s_installed,0,sizeof(s_installed));memset((void*)s_gatePassed,0xff,sizeof(s_gatePassed));memset(originals,0,sizeof(originals));queued=applied=immediate=verified=completed=refused=0;installed=0;sdk=fresh=true;reenter=throwCompletion=false;queueFailure=-1;applyResult=0;activeMask=15;for(int i=0;i<HOOK_ROW_COUNT;++i)g_hookPrologues[i].rva=i+1;}
int main(){try{
 for(int mask=0;mask<16;++mask){Reset();activeMask=mask;applyResult=mask==15?0:1;reenter=true;{IndependentStartupScope scope(&installed);for(int i=0;i<4;++i)HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,Complete);C(queued==0&&installed==0&&completed==0);scope.Finish();C(queued==4&&applied==1&&installed==(mask==15?4:0)&&completed==4&&refused==(mask==15?0:4));for(int i=0;i<4;++i)C(originals[i]==(void*)&Native);}C(!s_independentPhase&&s_independentCount==NULL);fresh=false;unsigned v=verified;for(int i=0;i<4;++i){void*saved=originals[i];HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,OtherComplete);C(saved==originals[i]);}C(v==verified&&immediate==0);}
 for(int bad=0;bad<4;++bad){Reset();queueFailure=bad;{IndependentStartupScope scope(&installed);for(int i=0;i<4;++i)HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,Complete);scope.Finish();}C(installed==3&&refused==1&&applied==1);}
 Reset();sdk=false;{IndependentStartupScope scope(&installed);for(int i=0;i<4;++i)HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,Complete);scope.Finish();}C(immediate==4&&queued==0&&installed==4&&completed==4);
 Reset();fresh=false;{IndependentStartupScope scope(&installed);HookInstallIndependentRow(rows[0],(void*)1,&originals[0],&installed,true,Complete);scope.Finish();}C(queued==0&&installed==0&&refused==1);
 Reset();{IndependentStartupScope scope(&installed);HookInstallIndependentRow(HOOK_SHOW_LOADING_MESSAGE,(void*)1,&originals[0],&installed,true,OtherComplete);scope.Finish();}C(immediate==0&&queued==0);
 Reset();throwCompletion=true;try{IndependentStartupScope scope(&installed);HookInstallIndependentRow(rows[0],(void*)1,&originals[0],&installed,true,Complete);scope.Finish();}catch(int x){C(x==33);}C(!s_independentPhase&&s_independentCount==NULL&&originals[0]==(void*)&Native);
 const HookRowId added[] = {HOOK_GRAPH_POSITION_LOOKUP, HOOK_NAVMESH_FACE_AABB,
    HOOK_SPEED_GROUP_GET_SPEED, HOOK_SCORE_FIND_FOOD_ON_GROUND};
 for (int mask=0; mask<256; ++mask) {
  Reset();
  void* extra[4] = {NULL,NULL,NULL,NULL};
  activeMask=mask;
  applyResult=mask==255?0:1;
  {
   IndependentStartupScope scope(&installed);
   for(int i=0;i<4;++i) HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,Complete);
   for(int i=0;i<4;++i) HookInstallIndependentRow(added[i],(void*)1,&extra[i],&installed,true,Complete);
   C(queued==0&&installed==0&&completed==0);
   scope.Finish();
  }
  C(queued==8&&applied==1&&completed==8);
  C(installed==(mask==255?8:0)&&refused==(mask==255?0:8));
  for(int i=0;i<4;++i) C(extra[i]==(void*)&Native);
 }
 std::printf("PASS %u actual batch adapter controls\n",checks);return 0;
 }catch(...){return 1;}}
