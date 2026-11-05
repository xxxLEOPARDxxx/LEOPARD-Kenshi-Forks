"""Production planner boundary clocks + actual shared active clock and replan policy.
No game/native calls. Extracts the current tick, capture/merge timestamp expressions
and resume conversion; fake native seams only observe their arguments/work count.
"""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/planner-pause-tests';OUT.mkdir(exist_ok=True)
tick=(ROOT/'src/planner/planner_tick.cpp').read_text();capture=(ROOT/'src/planner/planner_capture.cpp').read_text();merge=(ROOT/'src/planner/planner_merge.cpp').read_text()
def body(source,marker):
 a=source.index(marker);b=source.index('{',a);end=b+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[a:end]
clock=body(tick,'double planner_tick_detail::BehaviorNow(') if 'double planner_tick_detail::BehaviorNow(' in tick else 'double BehaviorNow(double rawNow){return rawNow;}'
clock=clock.replace('double planner_tick_detail::BehaviorNow(', 'double BehaviorNow(')
capture_expr=re.search(r'double now = ([^;]+);',body(capture,'int PlannerNoteOrder(')).group(1)
merge_expr=re.search(r'double now = ([^;]+);',body(merge,'bool PlannerMergeGather(')).group(1)
resume_prefix=body(merge,'void PlannerResumeFromGather(').split('TakeSnapshot();')[0]
resume_convert=re.search(r'now = BehaviorNow\(now\);',resume_prefix)
hold_expr=re.search(r'^\s+(.+ - m->holdTime)\),$',body(tick,'static PlanDropWhy SlotDropDue('),re.M).group(1)
code=r'''
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "planner/plan_policy.h"
#include "movement/order_outcome_table.h"
using namespace planner;
unsigned checks=0;bool paused=false;double raw=0,seenTick=-1,seenMerge=-1,seenReport=-1;
int mode=PLANNER_ON,calls=0,resets=0,preFrames=0;unsigned gen=1,s_storeGen=0;
double s_lastTick=0;const double TICK_SECONDS=.25;const int PLAN_SLOTS=1,PLAYER_SCAN_MAX=1,REPLANS_PER_TICK=4;
struct PlayerChar{int unused;};
void C(bool b,const char*s){++checks;if(!b){std::printf("FAIL %s\n",s);std::exit(1);}}
bool ZonePauseIsPaused(){return paused;}double ElapsedSec(){return raw;}
int PlanStoreMode(){return mode;}void PlannerPreArrivalFrame(){++preFrames;}unsigned CgStoreGen(){return gen;}
void PlanStoreReset(){++resets;}void TakeSnapshot(){}int ReadPlayers(PlayerChar*){return 1;}bool PlanStoreKey(int){return true;}
bool TickSlot(int,const PlayerChar*,int,double now,bool){seenTick=now;++calls;return false;}
void PlannerWaterRefresh(){}void MergeTick(double now){seenMerge=now;}void PlannerReportTick(double now){seenReport=now;}
@CLOCK@
double CaptureNow(){return @CAPTURE@;}double MergeNow(){return @MERGE@;}
double ResumeNow(double now){@RESUME@ return now;}
double HoldAge(double now,double held){struct Hold{double holdTime;}value={held};const Hold*m=&value;return @HOLD@;}
@TICK@
PlanReplanIn Due(double now,double planned){PlanReplanIn in={};in.now=now;in.planTime=planned;in.distToPortal=100;in.waitSeconds=10;return in;}
int main(){OOT_ResetForTest();raw=100;double planned=CaptureNow();C(planned==100,"capture begins in active seconds");
 PlannerTick(NULL,101);C(calls==1&&resets==1&&seenTick==101,"initial actual tick stamps current active domain");
 paused=true;PlannerTick(NULL,102);int atPause=calls;PlannerTick(NULL,701);
 C(calls==atPause,"paused actual tick does not scan or replan slots");
 raw=701;C(CaptureNow()==101,"order issued while paused uses frozen active clock");C(MergeNow()==101,"merge lookup shares capture active domain");C(ResumeNow(701)==101,"gather resume rewrites share active domain");
 paused=false;PlannerTick(NULL,702);C(seenTick==102&&seenMerge==102,"resume does not age planner or merge through paused600s");C(HoldAge(seenTick,101)==1,"gather hold is not converted from active time twice");C(seenReport==702,"diagnostic heartbeat retains wall-time cadence");C(PlanReplanDue(Due(seenTick,planned))==PRW_NONE,"long pause cannot expire120s plan");
 PlanReplanIn in=Due(seenTick,planned);in.completeSince=101;C(PlanReplanDue(in)==PRW_NONE,"completion timeout excludes paused interval");in.completeSince=0;in.waitSince=101;in.loadedChangedSincePlan=1;C(PlanReplanDue(in)==PRW_NONE,"section wait timeout excludes paused interval");
 PlannerTick(NULL,708);in.now=seenTick;in.waitSince=0;in.completeSince=101;C(PlanReplanDue(in)==PRW_NONE,"completion remains below8 active seconds");PlannerTick(NULL,709);in.now=seenTick;C(PlanReplanDue(in)==PRW_ARRIVAL_TIMEOUT,"completion still expires at8 active seconds");
 in.completeSince=0;in.waitSince=101;PlannerTick(NULL,711);in.now=seenTick;C(PlanReplanDue(in)==PRW_WAIT,"wait still expires at10 active seconds");
 PlannerTick(NULL,820);C(PlanReplanDue(Due(seenTick,planned))==PRW_AGE,"plan still expires at120 active seconds");
 paused=true;++gen;PlannerTick(NULL,821);C(resets==2,"generation reset still runs during pause");int before=calls;mode=PLANNER_OFF;PlannerTick(NULL,1400);C(calls==before,"routing OFF performs no planner work");
 std::printf("PASS %u actual planner clock boundary controls\n",checks);return 0;}
'''
code=code.replace('@CLOCK@',clock).replace('@CAPTURE@',capture_expr).replace('@MERGE@',merge_expr).replace('@RESUME@',resume_convert.group(0) if resume_convert else '').replace('@HOLD@',hold_expr).replace('@TICK@',body(tick,'void PlannerTick('))
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=os.environ.copy();env['INCLUDE']=f'{VC}/include;{SDK}/Include;{ROOT}/src';env['LIB']=f'{VC}/lib/amd64;{SDK}/Lib/x64'
def run(name,source,opt,negative=False):
 file=OUT/(name+'.cpp');file.write_text(source);exe=OUT/(name+'.exe')
 p=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(file),str(ROOT/'src/planner/plan_policy.cpp'),str(ROOT/'src/movement/order_outcome_table.cpp'),str(ROOT/'src/movement/order_outcome_policy.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=OUT,capture_output=True,text=True);print(p.stdout+p.stderr,end='');assert p.returncode==0
 p=subprocess.run([str(exe)],capture_output=True,text=True);print(p.stdout+p.stderr,end='');assert (p.returncode!=0 and 'FAIL ' in p.stdout) if negative else p.returncode==0
for opt in ['Od','O2']:run('clock-'+opt,code,opt)
mutants=[('raw-capture','double CaptureNow(){return '+capture_expr+';}','double CaptureNow(){return ElapsedSec();}'),('raw-merge','double MergeNow(){return '+merge_expr+';}','double MergeNow(){return ElapsedSec();}'),('raw-resume','double ResumeNow(double now){now = BehaviorNow(now); return now;}','double ResumeNow(double now){return now;}'),('raw-tick','TickSlot(slot, players, nPlayers, activeNow,','TickSlot(slot, players, nPlayers, now,'),('raw-merge-tick','MergeTick(activeNow);','MergeTick(now);'),('double-hold','return '+hold_expr+';}','return OOT_ActiveNow(now, ZonePauseIsPaused()) - m->holdTime;}')]
for name,before,after in mutants:
 assert before in code,name
 run('mutant-'+name,code.replace(before,after,1),'O2',True)
print('PASS six compiled planner clock negative controls')
