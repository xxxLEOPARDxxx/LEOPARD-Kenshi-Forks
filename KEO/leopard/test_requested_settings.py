"""Compile actual requested-state helper and actual Finalize body; no native calls/files."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task1/requested';OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
line=next(x for x in (ROOT/'tools/tests/suites.txt').read_text().splitlines() if x.startswith('requested_settings_units |'));sources=[ROOT/x for x in line.split('|')[2].split()]
config=(ROOT/'src/base/config.cpp').read_text(encoding='utf-8');a=config.index('void FinalizeConfig()');b=config.index('{',a);e=b+1;d=1
while d:d+=(config[e]=='{')-(config[e]=='}');e+=1
body=config[a:e];assert config.count('FinalizeConfig();')==2
code=r"""
#include <Windows.h>
#include <sstream>
#include <cstring>
#include "base/config_values.h"
#include "base/worker_count.h"
#include "gui/requested_settings.h"
#include "check.h"
unsigned logged=0,clamped=0,errors=0;void LogMsg(const std::string&){++logged;}void LogError(const std::string&){++errors;}void ClampRenderConfig(){++clamped;}void LogRenderConfig(){}
bool ApplyRenderConfig(const RenderConfig&){return true;}bool BenchWindowInForeground(){return false;}int BenchLoadedZoneCount(){return 0;}
ACTUAL
int main(){movement::g_movementCfg.routingEnabled=false;movement::g_movementCfg.groupCohesionEnabled=true;movement::g_movementCfg.formationGatherPaceEnabled=true;pathfind::g_pathfindCfg.playerRepathTierEnabled=true;
 Check(!RequestedConfig().Ready(),"startup snapshot not captured eagerly");FinalizeConfig();SettingsStaging s;Check(RequestedConfig().Copy(&s)&&!errors&&clamped==1,"actual Finalize captures successfully");
 int movement=-1,path=-1;for(int i=0;i<kConfigModuleCount;++i){if(!std::strcmp(kConfigModules[i].name,"movement"))movement=i;if(!std::strcmp(kConfigModules[i].name,"pathfind"))path=i;}
 const movement::MovementConfig&m=*reinterpret_cast<const movement::MovementConfig*>(s.module[movement].state);const pathfind::PathfindConfig&p=*reinterpret_cast<const pathfind::PathfindConfig*>(s.module[path].state);
 Check(!m.routingEnabled&&m.groupCohesionEnabled&&m.formationGatherPaceEnabled&&p.playerRepathTierEnabled,"capture precedes routing master override");
 Check(!movement::g_movementCfg.groupCohesionEnabled&&!pathfind::g_pathfindCfg.playerRepathTierEnabled,"effective routing remains original behavior");
 movement::g_movementCfg.groupCohesionEnabled=false;Check(RequestedConfig().Copy(&s)&&reinterpret_cast<const movement::MovementConfig*>(s.module[movement].state)->groupCohesionEnabled,"later hook refusal cannot overwrite requested copy");
 return CheckExit("actual_finalize_requested");}
""".replace('ACTUAL',body)
helper=(ROOT/'src/gui/requested_settings.cpp').read_text(encoding='utf-8')
def run(name,unit,changed=None,defines=(),passes=True):
 folder=OUT/name;folder.mkdir(exist_ok=True);cpp=folder/'unit.cpp';cpp.write_text(unit,encoding='utf-8');selected=list(sources)
 if changed is not None:
  h=folder/'requested_settings.cpp';h.write_text(changed,encoding='utf-8');selected=[h if p.name=='requested_settings.cpp' else p for p in selected]
 r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3',*defines,str(cpp),str(ROOT/'src/base/worker_count.cpp'),*map(str,selected),'/Fo'+str(folder)+'/', '/Fe'+str(folder/'test.exe')],env=env,cwd=ROOT,capture_output=True,text=True)
 (folder/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 r=subprocess.run([str(folder/'test.exe')],capture_output=True,text=True);(folder/'run.log').write_text(r.stdout+r.stderr,encoding='utf-8')
 if (r.returncode==0)!=passes:raise AssertionError(name+': '+r.stdout+r.stderr)
 print(name,r.stdout.strip())
for label,defines in [('prod',('/DZONEHAND_STEP=3',)),('dev',('/DZONEHAND_STEP=3','/DKEO_DEBUG'))]:run(label,code,defines=defines)
call='if (!CaptureRequestedSettings()) LogError("Requested config snapshot exceeds settings bounds; editor unavailable");'
assert call in code
changed=code.replace(call,'').replace('movement::ApplyRoutingSwitch(&LogMsg);','movement::ApplyRoutingSwitch(&LogMsg);'+call)
run('capture-too-late',changed,passes=False)
unit=(ROOT/'tools/tests/requested_settings_units.cpp').read_text(encoding='utf-8')
for name,old,new in [('copy-live','*out = values_;','for(int i=0;i<moduleCount_;++i) StageModule(modules_[i], &out->module[i]);'),('state-overflow','mod.stateSize > CONFIG_STATE_MAX','false'),('lost-readiness','if (!ready_ || !out)','if (!out)'),('lost-key-bounds','module >= moduleCount_','module > moduleCount_')]:
 assert old in helper;run(name,unit,helper.replace(old,new),passes=False)
print('5 compiled requested-state/Finalize mutants rejected')
