"""Compile extracted production lifecycle/report entry points with host boundaries.
Verifies disabled logging does not skip zone release/age work or render upkeep.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task3-maintenance';OUT.mkdir(exist_ok=True)
def body(file,marker):
 s=(ROOT/file).read_text(encoding='utf-8');a=s.index(marker);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
zone=body('src/zone/retention/zone_leak_report.cpp','void ZoneLeakReport(')
render=body('src/render/render_levers.cpp','void RenderLeversMainThreadTick(')
trace=body('src/movement/movement_trace.cpp','void MovementTraceFrame(')
code=r'''
#include <Windows.h>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstring>
#include "base/log_admission.h"
bool detailed=false;bool DetailedLogEnabled(){return detailed;}unsigned logged=0,formatted=0,privReads=0;
void LogMsg(const std::string&){++logged;}void LogMsgDeferrable(const char*){}void LogError(const std::string&){}
std::string Expensive(){++formatted;return "detail";}
#define KLIB_MEMBER(a,p,m,off) ((uintptr_t)(p)+(off))
const int ZONE_GRID_COUNT=4096,ZONE_GRID_MAX=63,OFF_ZONE_CONTENT=0;
const unsigned char ZL_REGISTERED=1,ZL_PROCESSED=2;
struct Record{unsigned char flags;double lastInRadius,orphanSince;};Record g_zl[4096];
struct Preload{void*zoneEntry;int gridX,gridY;}preloadedZones[1];int numPreloaded=0;
struct Entry{void*content;bool loading,access;};Entry zones[4096];
namespace zone{struct Cfg{bool zoneLifeUnloadEnabled;}g_zoneCfg={true};}
int g_zlRelGone=0,g_zlRelSetAB=0,g_zlOrphans=-1,g_zlLive=4096;unsigned releases=0,clears=0,retentionBuilds=0;
int ZoneCell(int x,int y){return x*64+y;}void*GetZoneEntry(void*,int x,int y){return zones+ZoneCell(x,y);}
bool IsZoneLoading(void*p){return ((Entry*)p)->loading;}bool IsZoneAccessible(void*p){return ((Entry*)p)->access;}
bool ZlBuildRetention(void*){++retentionBuilds;return true;}bool ZlUnloadUnavailable(){return false;}
unsigned ZoneResetFenceGeneration(){return 7;}void ZoneResetFenceClear(int,int){++clears;}
void ZlRelease(int c){++releases;g_zl[c].flags=0;}bool ZoneInSetA(void*,void*){return false;}bool ZoneInSetB(void*,void*){return false;}
bool ZlRetentionNear(int){return true;}bool ZoneResetFenceHolds(int,int,unsigned){return false;}int ZoneResetFenceCount(){return 0;}
long ZlPrivateMB(){++privReads;return 1;}int ZoneCycleSetBSize(void*){return 0;}void ZlAppendStep2Tokens(std::ostringstream&){}void ZlAppendStep3Tokens(std::ostringstream&){}
ZONE
unsigned upkeep[7]={};void ParticleLevers_MainThreadTick(bool){++upkeep[0];}void ShadowReach_MainThreadTick(){++upkeep[1];}
void GpuParams_MainThreadTick(){++upkeep[2];}void OldAnim_MainThreadTick(){++upkeep[3];}void EmptyPass_MainThreadTick(){++upkeep[4];}
void Foliage_MainThreadTick(){++upkeep[5];}void GpuUpload_MainThreadTick(){++upkeep[6];}
double clockNow=0,s_windowStart=-1;LONG s_loops=0;const double RENDER_STATS_INTERVAL=30;
double ElapsedSec(){return clockNow;}unsigned drains=0;std::string RenderLeverStatsLine(){++drains;s_loops=0;s_windowStart=clockNow;return detailed?"render":"";}
RENDER
long s_taken=0,s_overruns=0,s_torn=0,s_samples=0,s_written=0,s_bytes=0,s_drops=0,s_results=0,s_cut=0,s_unmatched=0,s_openFail=0;
int s_armed=0,s_armState=0,s_closingCount=0,s_closing[2]={0},s_resultsHook=0,s_take=0;double s_lastLine=0;const double TRACE_LINE_SECONDS=30;
unsigned traceResets=0,traceReads=0,traceArms=0;void MovementTraceReset(){++traceResets;s_closingCount=0;}
void ArmOnce(){if(s_armState)return;++traceArms;s_armState=1;s_armed=1;}
bool PathResultTraceTake(long*,int*,long*,long*){++traceReads;return false;}void Attribute(int){}void WriteOrder(int){}
TRACE
unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %s\n",#x);return 1;}}while(0)
int main(){KEO_DETAIL(Expensive());KEO_DETAIL_DEFERRED(Expensive().c_str());C(!formatted&&!logged);
 for(int i=0;i<4096;++i){g_zl[i].flags=1;g_zl[i].orphanSince=-1;}
 // One live retained record must have its age refreshed; 4095 empty records release.
 zones[0].content=&zones;ZoneLeakReport(&zones,100);C(releases==4095&&clears==4095&&g_zlRelGone==4095&&retentionBuilds==1);C(g_zl[0].lastInRadius==100&&g_zlOrphans==0&&!privReads&&!logged);
 RenderLeversMainThreadTick(false);clockNow=31;RenderLeversMainThreadTick(true);for(int i=0;i<7;++i)C(upkeep[i]==2);C(drains==1&&!logged);
 detailed=true;KEO_DETAIL(Expensive());C(formatted==1&&logged==1);ZoneLeakReport(&zones,200);C(g_zl[0].lastInRadius==200&&privReads==1&&logged==2);
 detailed=false;MovementTraceFrame(1);C(!traceResets&&!traceReads&&!traceArms);
 detailed=true;MovementTraceFrame(1);C(traceArms==1&&traceReads==1&&s_armed);
 detailed=false;for(int i=0;i<1000;++i)MovementTraceFrame(1);C(traceResets==1&&traceReads==1&&!s_armed);
 detailed=true;MovementTraceFrame(1);C(traceArms==1&&traceReads==2&&s_armed);
 printf("Actual producer/lifecycle/render maintenance %u checks PASS\n",checks);return 0;
}
'''.replace('ZONE\n',zone+'\n').replace('RENDER\n',render+'\n').replace('TRACE\n',trace+'\n')
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
def run(name,code,passed=True):
 cpp=OUT/(name+'.cpp');cpp.write_text(code,encoding='utf-8');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(cpp),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(OUT/(name+'.exe'))],env=env,capture_output=True,text=True);(OUT/(name+'.build.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(OUT/(name+'.exe'))],capture_output=True,text=True);(OUT/(name+'.run.log')).write_text(r.stdout+r.stderr);print(name,r.stdout.strip());assert (r.returncode==0)==passed
run('actual',code)
run('bad-zone-early-return',code.replace('void ZoneLeakReport(void* zoneMgr, double now)\n{','void ZoneLeakReport(void* zoneMgr, double now)\n{ if (!DetailedLogEnabled()) return;'),False)
run('bad-render-early-return',code.replace('void RenderLeversMainThreadTick(bool saveLoading)\n{','void RenderLeversMainThreadTick(bool saveLoading)\n{ if (!DetailedLogEnabled()) return;'),False)
run('bad-trace-repeat-cleanup',code.replace('if (s_armed) { MovementTraceReset(); s_armed = 0; }','MovementTraceReset();'),False)
print('3 compiled maintenance/idle-cleanup mutants rejected')
