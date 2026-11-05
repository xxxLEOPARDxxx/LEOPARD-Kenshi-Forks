"""Actual OFF mixed-status callbacks, custom config and benchmark refusals."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task3-status-callbacks';OUT.mkdir(exist_ok=True)
s=(ROOT/'src/base/log.cpp').read_text(encoding='utf-8');a=s.index('void LogStatus(');b=s.index('{',a);e=b+1;d=1
while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
actual=s[a:e]
code=r'''
#include "base/config_table.h"
#include "base/log_status.h"
#include "bench/bench_sweep.h"
#include "bench/bench_group.h"
#include "bench/bench_slots.h"
#include "render/render_config.h"
#include <cstdio>
unsigned errors=0,details=0;bool detailed=false,active=false;
void LogError(const std::string&){++errors;}void LogDetail(const std::string&){if(detailed)++details;}
ACTUAL
bool ApplyRenderConfig(const RenderConfig&){return false;}bool BenchWindowInForeground(){return false;}int BenchLoadedZoneCount(){return 0;}
bool Arm(int,int,int,const BenchPinSpec&,const std::string&,std::string*){active=true;return true;}bool Active(){return active;}void Abort(const char*){active=false;}const char*Blocked(bool*){return NULL;}int Group(const char*,const char**why){*why="unknown";return -1;}
unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %s\n",#x);return 1;}}while(0)
int main(){
 LogStatus("Config: retired key 'old' ignored");LogStatus("Config: DEV-only key 'dev' ignored");LogStatus("Bench sweep: armed");LogStatus("Bench sweep: bench.sweep ignored (bench.sweep.<n> keys are set)");LogStatus("Bench: group dev empty (DEV-only levers)");LogStatus("Bench: group text sets=each levers=particleLoopingNames=unknown");C(!errors&&!details);
 ConfigLoadState state;ConfigApplyLine("bench.levers","nosuchlever",1,&state,&LogStatus);C(errors>=1&&!details);unsigned before=errors;
 ConfigApplyLine("bench.group.bad","broken",2,&state,&LogStatus);BenchGroupsResolve(&LogStatus);C(errors>before&&!details);
 before=errors;C(ParseBenchSweepFamilyKey("bench.sweep.1","not-a-leg",&LogStatus));C(errors>before&&!details);
 C(ParseBenchSweepFamilyKey("bench.sweep.1","",&LogStatus));before=errors;
 C(ParseBenchSweepFamilyKey("bench.group.Bad Name","x",&LogStatus));C(errors>before&&!details);
 C(ParseBenchSweepFamilyKey("bench.group.badvalue","each/1/3+25:reflectionHalfRate=bogus",&LogStatus));before=errors;BenchGroupsResolve(&LogStatus);C(errors>before&&!details);
 std::string many;for(int i=0;i<25;++i){if(i)many+=",";many+="swamp:1";}before=errors;
 C(ParseBenchSweepFamilyKey("bench.sweep.1",many,&LogStatus));C(errors>before&&!details);C(ParseBenchSweepFamilyKey("bench.sweep.1","",&LogStatus));
 before=errors;C(ParseBenchSweepFamilyKey("bench.sweep.9","swamp:1",&LogStatus));C(errors>before&&!details);
 for(int i=0;i<65;++i){char name[64];sprintf_s(name,"bench.group.x%d",i);ParseBenchSweepFamilyKey(name,"each/1/3+25:reflectionHalfRate=true",&LogStatus);}C(errors>before&&!details);
 BenchSweepRunner runner={Arm,Active,Abort,Blocked,LogStatus,Group};BenchSweepSetRunner(runner);
 active=true;before=errors;C(!BenchSweepStart()&&errors==before+1&&!details);active=false;
 for(int i=0;i<BENCH_SLOT_COUNT;++i)g_benchSlots[i].recorded=false;
 before=errors;C(!BenchSweepStart()&&errors==before+1&&!details);
 for(int i=0;i<BENCH_SLOT_COUNT;++i)g_benchSlots[i].recorded=true;
 C(ParseBenchSweepFamilyKey("bench.sweep.1","swamp:1:missing",&LogStatus));before=errors;
 C(!BenchSweepStart()&&errors==before+1&&!details);
 C(ParseBenchSweepFamilyKey("bench.sweep.1","swamp:1",&LogStatus));before=errors;
 C(BenchSweepStart()&&errors==before&&!details&&active);
 detailed=true;LogStatus("Bench: progress");C(details==1&&errors==before);
 printf("Actual DebugOFF custom config/group/sweep status callbacks %u checks PASS\n",checks);return 0;}
'''.replace('ACTUAL',actual)
line=next(x for x in (ROOT/'tools/tests/suites.txt').read_text().splitlines() if x.startswith('bench_group_units |'));sources=[ROOT/x for x in line.split('|')[2].split()]
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
cpp=OUT/'actual.cpp';cpp.write_text(code,encoding='utf-8');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DKEO_DEBUG','/DZONEHAND_STEP=3',str(cpp),*map(str,sources),'/Fo'+str(OUT)+'/', '/Fe'+str(OUT/'actual.exe')],env=env,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
r=subprocess.run([str(OUT/'actual.exe')],capture_output=True,text=True);(OUT/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');assert not r.returncode
entry=(ROOT/'src/plugin/plugin_entry.cpp').read_text();assert 'BenchGroupsResolve(&LogStatus)' in entry and '&BenchRunnerArmBlocked, &LogStatus,' in entry
config=(ROOT/'src/base/config.cpp').read_text();assert 'static void ConfigLog(const std::string& line)\n{\n    LogStatus(line);\n}' in config
print('Actual native config/group/sweep owner callback wiring PASS')
