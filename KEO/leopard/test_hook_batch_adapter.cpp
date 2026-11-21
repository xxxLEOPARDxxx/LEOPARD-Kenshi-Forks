#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <new>
#include <sstream>
#include "plugin/hook_manifest.h"
#include "plugin/independent_hook_batch.h"
#include "render/module_hooks.h"
struct Prologue{uintptr_t rva;const char*name;};
static Prologue g_hookPrologues[HOOK_ROW_COUNT];
static volatile LONG s_installed[HOOK_ROW_COUNT],s_gatePassed[HOOK_ROW_COUNT];
// BATCH_GLOBALS
static unsigned checks=0,queued=0,applied=0,immediate=0,verified=0,completed=0,refused=0;
static int activeMask=15,applyResult=0,queueFailure=-1;
static bool sdk=true,fresh=true,reenter=false,throwCompletion=false;
static void**nativeOriginal[HookBatch::CAPACITY];static uintptr_t queuedAddresses[HookBatch::CAPACITY];static HookRowId rows[4]={HOOK_CHARACTER_UPDATE_ONSCREEN_CHECK,HOOK_CHARACTER_PAUSED_UPDATE,HOOK_ACTIVEPLATOON_CALC_POS,HOOK_FACTION_RELATIONS_UPDATE};
static void* originals[4];static int installed=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);throw 77;}}while(0)
static int Native(int n){return n+7;}
typedef int (*CancelFn)(int);
namespace game {
static struct {
 CancelFn orig_stopCharactersMovement,orig_addJobSelected,orig_addTaskNearest;
} g_hookOrig;
}
static int hook_stopCharactersMovement(int n){return game::g_hookOrig.orig_stopCharactersMovement(n);}
static int hook_addJobSelected(int n){return game::g_hookOrig.orig_addJobSelected(n);}
static int hook_addTaskNearest(int n){return game::g_hookOrig.orig_addTaskNearest(n);}
static unsigned cancelFlags=0,cancelErrors=0;
static void IslandSetCancelHooksInstalled(bool stop,bool job,bool task){cancelFlags=stop+2*job+4*task;}
static void LogError(const std::string&){++cancelErrors;}
#define KEO_DETAIL(x) do{}while(0)
static void Complete(const char*why){++completed;if(why)++refused;if(throwCompletion)throw 33;}
static void CompleteAux(int,const char*why){Complete(why);}
static void OtherComplete(const char*why){C(why!=NULL);}
static bool VerifyPrologueByRva(uintptr_t){++verified;return fresh;}
static void*GameAddr(uintptr_t rva){return (void*)(rva+0x10000);}
static HookAdmit HookInstallAdmitActual(bool v,bool ok,bool passed){return(v?ok:passed)?HOOK_ADMIT:HOOK_REFUSE_PROLOGUE;}
#define HookInstallAdmit HookInstallAdmitActual
static int Queue(size_t id,void*address,void*,void**o){C(id==0x11110000);int i=int(queued++);nativeOriginal[i]=o;queuedAddresses[i]=(uintptr_t)address;*o=i==queueFailure?NULL:(void*)&Native;return i==queueFailure?1:0;}
static int Apply(size_t id){C(id==0x11110000);++applied;C(completed==0);for(unsigned i=0;i<queued;++i)if((activeMask&(1<<i))&&int(i)!=queueFailure){C(*nativeOriginal[i]!=NULL);C(((int(*)(int))*nativeOriginal[i])(3)==10);}if(reenter){unsigned n=verified;void*saved=originals[0];HookInstallIndependentRow(rows[0],(void*)1,&originals[0],&installed,true,OtherComplete);C(verified==n&&originals[0]==saved&&s_independentDone[rows[0]]==Complete);}return applyResult;}
static bool BeginIndependentHooks(){return sdk&&s_independentBatch.Begin(0x11110000,Queue,Apply);}
const char*HookInstallRow(HookRowId id,void*,void**o,int*n,bool){++immediate;*o=(void*)&Native;s_installed[id]=1;if(n)++*n;return NULL;}
void* ResolveModuleSite(const ModuleSite& site){return GameAddr(site.rva);}
static bool CheckModuleHookSite(const ModuleSite&, void*, bool* sharedOut, char (&)[96]){
 ++verified;*sharedOut=fresh;return fresh;
}
static bool EnableModuleHook(const ModuleSite&, void*, void*, void** original, const char*){
 ++immediate;*original=(void*)&Native;return true;
}
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
    HOOK_SPEED_GROUP_GET_SPEED, HOOK_SCORE_FIND_FOOD_ON_GROUND,
    HOOK_RESET_UNLOAD_ZONES, HOOK_PROCESS_LOADING, HOOK_STOP_CHARACTERS_MOVEMENT,
    HOOK_ADD_JOB_SELECTED, HOOK_ADD_TASK_NEAREST};
 const int addedCount=sizeof(added)/sizeof(added[0]);
 const StartupAuxHookId auxiliary[] = {STARTUP_AUX_INST_EMPTY_SKIP, STARTUP_AUX_OGRE_JOIN_SPIN,
    STARTUP_AUX_SKELETON_RESERVE, STARTUP_AUX_MESH_WEIGHTS,
    STARTUP_AUX_BONE_IMPORT, STARTUP_AUX_BONE_INDEX,
    STARTUP_AUX_MATERIAL_RESERVE, STARTUP_AUX_SAVE_READ_BUFFER, STARTUP_AUX_SAVE_STRING_TRANSFER};
 const int auxiliaryCount=sizeof(auxiliary)/sizeof(auxiliary[0]);
 const int total=4+addedCount+auxiliaryCount;
 const int allActive=(1<<total)-1;
 const int priorMask=(1<<(total-2))-1;
 for (int control=0; control<=priorMask+total+1; ++control) {
  const int mask=control<=priorMask?control:control==priorMask+total+1?allActive:
                 allActive&~(1<<(control-priorMask-1));
  Reset();
  void* extra[addedCount] = {};
  void* auxiliaryOriginals[auxiliaryCount] = {};
  activeMask=mask;
  applyResult=mask==allActive?0:1;
  {
   IndependentStartupScope scope(&installed);
   for(int i=0;i<4;++i) HookInstallIndependentRow(rows[i],(void*)1,&originals[i],&installed,true,Complete);
   for(int i=0;i<addedCount;++i) HookInstallIndependentRow(added[i],(void*)1,&extra[i],&installed,true,Complete);
   for(int i=0;i<auxiliaryCount;++i)
    C(QueueStartupAuxHook(auxiliary[i],(void*)(uintptr_t)(16+i),(void*)1,&auxiliaryOriginals[i],CompleteAux));
   C(queued==0&&installed==0&&completed==0);
   scope.Finish();
  }
  C(queued==total&&applied==1&&completed==total);
  C(installed==(mask==allActive?total-auxiliaryCount:0)&&refused==(mask==allActive?0:total));
  C(s_allowFurtherNativeBatches==(mask==allActive));
  for(int i=0;i<addedCount;++i) C(extra[i]==(void*)&Native);
  for(int i=0;i<auxiliaryCount;++i) C(auxiliaryOriginals[i]==(void*)&Native);
 }
 C(!QueueStartupAuxHook(STARTUP_AUX_SAVE_READ_BUFFER,(void*)16,(void*)1,&originals[0],CompleteAux));
 const HookRowId cancelRows[] = {HOOK_STOP_CHARACTERS_MOVEMENT,HOOK_ADD_JOB_SELECTED,HOOK_ADD_TASK_NEAREST};
 for(int mode=0;mode<5;++mode){
  Reset();
  cancelFlags=cancelErrors=0;
  game::g_hookOrig.orig_stopCharactersMovement=game::g_hookOrig.orig_addJobSelected=game::g_hookOrig.orig_addTaskNearest=NULL;
  for(int i=0;i<3;++i)g_hookPrologues[cancelRows[i]].name="cancel test hook";
  sdk=mode!=1;
  if(mode==2)queueFailure=1;
  if(mode==3){activeMask=3;applyResult=1;}
  if(mode==4)fresh=false;
  {
   IndependentStartupScope scope(&installed);
   InstallCancelHooks(&installed,NULL);
   if(sdk)C(queued==0&&immediate==0&&installed==0&&cancelFlags==0);
   scope.Finish();
   if(sdk&&fresh){
    C(queued==3&&applied==1);
    for(int i=0;i<3;++i)C(queuedAddresses[i]==(uintptr_t)GameAddr(g_hookPrologues[cancelRows[i]].rva));
   }
   FinalizeCancelHooks();
  }
  C(cancelFlags==(mode<2?7u:mode==2?5u:0u));
  C(installed==(mode<2?3:mode==2?2:0));
  C(cancelErrors==(mode<2?0u:mode==2?1u:3u));
  if(fresh){
   C(hook_stopCharactersMovement(2)==9&&hook_addTaskNearest(3)==10);
   if(mode!=2)C(hook_addJobSelected(4)==11);
  }
 }
 const ModuleSite site={"synthetic render hook","test.dll",NULL,1024,{}};
 for(int mode=0;mode<4;++mode){
  Reset();
  bool shared=false;void* original=NULL;
  sdk=mode!=1;fresh=mode!=3;applyResult=mode==2?1:0;
  {
   IndependentStartupScope scope(&installed);
   InstallStartupModuleHook(STARTUP_AUX_INST_EMPTY_SKIP,site,(void*)1,&original,&shared,CompleteAux);
   C(verified==1&&shared==fresh&&queued==0&&installed==0);
   if(sdk&&fresh)C(completed==0&&immediate==0);
   scope.Finish();
  }
  C(completed==1&&refused==(mode>=2?1u:0u)&&immediate==(mode==1?1u:0u));
  C(queued==(mode==0||mode==2?1u:0u)&&original==(fresh?(void*)&Native:NULL));
 }
 std::printf("PASS %u actual batch adapter controls\n",checks);return 0;
 }catch(...){return 1;}}
