#include <windows.h>
#include <stdint.h>
#include "check.h"
static bool mainThread=true,installed=true,s_inCall=false;
namespace navmesh {struct Config{bool cachingEnabled;}g_navmeshCfg={true};}
static void* s_origGenerate=(void*)1;
static void* s_addJob=(void*)2;
static void* s_hashZone=(void*)3;
static volatile LONG s_holdActive=0;
enum{HOOK_NAVMESH_GENERATE_ZONEMAP=1,NM_SKIP_NONE=0};
static int skip=0,calls=0;
static bool IsMainThread(){return mainThread;}
static bool HookRowInstalled(int){return installed;}
static int CellIndex(uintptr_t){return 0;}
static int JudgeCell(void*,int){return skip;}
static void hook_navMeshGenerate(void*mesh,void*zone){Check(mesh==(void*)17&&zone==(void*)19,"original hook receives native identities");++calls;}
#include "nm_full_zone.inc"
int main()
{
 Check(NmForceRebuildFullZoneReady(),"admitted native hook ready");
 installed=false;Check(!NmForceRebuildFullZoneReady()&&!NmForceRebuildFullZone((void*)17,(void*)19),"missing installed hook refuses");installed=true;
 s_origGenerate=NULL;Check(!NmForceRebuildFullZoneReady(),"missing trampoline refuses");s_origGenerate=(void*)1;
 mainThread=false;Check(!NmForceRebuildFullZone((void*)17,(void*)19),"worker refuses full native generation");mainThread=true;
 navmesh::g_navmeshCfg.cachingEnabled=false;Check(!NmForceRebuildFullZone((void*)17,(void*)19),"cache off refuses helper");navmesh::g_navmeshCfg.cachingEnabled=true;
 s_inCall=true;Check(!NmForceRebuildFullZone((void*)17,(void*)19),"nested generation refused");s_inCall=false;
 Check(!NmForceRebuildFullZone(NULL,(void*)19)&&!NmForceRebuildFullZone((void*)17,NULL),"missing objects refuse");
 skip=1;Check(!NmForceRebuildFullZone((void*)17,(void*)19),"unloaded or ineligible cell refused");skip=0;
 Check(calls==0,"all refused cases preserve original-call count");
 Check(NmForceRebuildFullZone((void*)17,(void*)19)&&calls==1,"one admitted invocation enters existing hook exactly once");
 Check(!NmForceRebuildKeyPending(),"no manual hold initially");s_holdActive=1;Check(NmForceRebuildKeyPending(),"main-thread accessor observes manual hold");
 return CheckExit("nm_full_zone_fixture");
}
