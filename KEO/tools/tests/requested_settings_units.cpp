#include "gui/requested_settings.h"
#include "base/config_values.h"
#include "base/keo_tr.h"
#include "check.h"
#include <cstring>
bool ApplyRenderConfig(const RenderConfig&){return true;}bool BenchWindowInForeground(){return false;}int BenchLoadedZoneCount(){return 0;}
static const ConfigKey*Key(RequestedSettingsSnapshot&s,const char*n,int&index,int&module){const ConfigModule*m=NULL;const ConfigKey*k=FindConfigKey(n,&m);module=int(m-kConfigModules);for(index=0;m->keys+index!=k;++index){}return k;}
int main(){RequestedSettingsSnapshot s;SettingsStaging copy;std::memset(&copy,0x5a,sizeof(copy));Check(!s.Ready()&&!s.Copy(&copy),"before capture getters refuse");
 const ConfigModule*m=NULL;const ConfigKey*k=NULL;Check(!s.Key(0,0,&m,&k)&&!m&&!k,"before capture metadata refuses");
 movement::g_movementCfg.routingEnabled=false;movement::g_movementCfg.groupCohesionEnabled=true;movement::g_movementCfg.formationGatherPaceEnabled=true;pathfind::g_pathfindCfg.playerRepathTierEnabled=true;
 Check(s.Capture(kConfigModules,kConfigModuleCount)&&s.Ready()&&s.Copy(&copy),"capture owns parsed requested values");int index,module;Key(s,"groupCohesion",index,module);Check(s.Key(module,index,&m,&k)&&!std::strcmp(k->name,"groupCohesion"),"bounded index metadata returns existing key");
 Check(reinterpret_cast<movement::MovementConfig*>(copy.module[module].state)->groupCohesionEnabled,"requested child ON retained");
 movement::ApplyRoutingSwitch(NULL);SettingsStaging after;Check(s.Copy(&after)&&!movement::g_movementCfg.groupCohesionEnabled,"effective routing OFF applied separately");
 Check(reinterpret_cast<movement::MovementConfig*>(after.module[module].state)->groupCohesionEnabled,"effective mutation cannot overwrite snapshot");
 Check(!reinterpret_cast<movement::MovementConfig*>(after.module[module].state)->routingEnabled,"requested master remains OFF");
 reinterpret_cast<movement::MovementConfig*>(after.module[module].state)->groupCohesionEnabled=false;Check(s.Copy(&after)&&reinterpret_cast<movement::MovementConfig*>(after.module[module].state)->groupCohesionEnabled,"getter copy is independent");
 Check(!s.Copy(NULL),"NULL destination refuses");Check(!s.Key(-1,0,&m,&k)&&!s.Key(10,0,&m,&k)&&!s.Key(0,-1,&m,&k)&&!s.Key(0,128,&m,&k),"invalid callback indices refuse");
 ConfigModule bad=kConfigModules[0];bad.stateSize=CONFIG_STATE_MAX+1;Check(!s.Capture(&bad,1)&&!s.Ready(),"oversized metadata clears availability");
 Check(!s.Capture(kConfigModules,CONFIG_MODULE_MAX+1)&&!s.Ready(),"module overflow refuses");Check(!s.Capture(NULL,1)&&!s.Capture(kConfigModules,0),"missing module table refuses");
 bad=kConfigModules[0];ConfigKey rows[CONFIG_STAGE_MAX];for(int i=0;i<CONFIG_STAGE_MAX;++i)rows[i]=bad.keys[0];bad.keys=rows;Check(!s.Capture(&bad,1),"missing row terminator refuses bounded");
 rows[1].name=NULL;rows[0]=kConfigModules[0].keys[0];rows[0].offset=CONFIG_STATE_MAX;bad.keys=rows;Check(!s.Capture(&bad,1),"out of state offset refuses before StageModule");
 Check(!RequestedConfig().Ready(),"global snapshot initially unavailable");movement::g_movementCfg.routingEnabled=false;movement::g_movementCfg.groupCohesionEnabled=true;Check(CaptureRequestedSettings()&&RequestedConfig().Copy(&after),"global startup capture shares owned helper");
 Check(KeoTr(NULL)&&!*KeoTr(NULL)&&!std::strcmp(KeoTr("KEO"),"KEO"),"host identity seam has no catalog header for empty key");return CheckExit("requested_settings_units");}
