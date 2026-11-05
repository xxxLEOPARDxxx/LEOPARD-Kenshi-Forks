"""Actual production handlers + bounded header, PROD (no KEO_DEBUG), VC10 Od/O2.
--old-control suppresses only the added refusal event to prove the regression RED.
No game/native calls: boundaries are counted fakes, actual handler bodies unchanged.
"""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[2];out=root/'build/zone-lifecycle-diag';out.mkdir(parents=True,exist_ok=True)
def body(file,name):
 s=(root/file).read_text(encoding='utf-8-sig');a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);e=b+1;n=1
 while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
hook=body('src/zone/handoff/zone_lifecycle_hooks.cpp','static bool __fastcall hook_activateZoneMap(')
if '--old-control' in sys.argv:hook=hook.replace('if (DetailedLogEnabled() && IsMainThread())','if (false && DetailedLogEnabled() && IsMainThread())')
first=body('src/zone/handoff/zone_first_time.cpp','void ZlLogFirstTimeDiag(')
give=body('src/zone/handoff/zone_first_time.cpp','bool ZlGiveToGame(')
# Actual PROD caller/declaration must both expose the private finalize event.
for name in ['src/zone/preload/preload_prepare.cpp','src/zone/preload/preload_internal.h']:
 s=(root/name).read_text(encoding='utf-8-sig');line=s.index('ZlLogFirstTimeDiag(');assert s.rfind('#ifdef KEO_DEBUG',0,line)<=s.rfind('#endif',0,line)
fixture=r"""
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "zone/handoff/zone_lifecycle_diag.h"
static int checks=0,queries=0,clocks=0,originalCalls=0,takeCalls=0,drops=0,touches=0,flagsCalls=0,fileCalls=0,townCalls=0;
static bool debug=false,mainThread=true,refuse=false,take=false,originalAnswer=false,inA=false,inB=false,untouched=true,readOk=true,latePossible=true;
static unsigned epoch=1;static double now=0;static int ft=0,ld=1,act=0;static std::vector<std::string> logs;
void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
bool DetailedLogEnabled(){return debug;}bool IsMainThread(){return mainThread;}unsigned ZoneHandoffWorldEpoch(){return epoch;}
double ElapsedSec(){++clocks;return now;}void LogMsg(const std::string&s){logs.push_back(s);}
#define KEO_DETAIL(line) do{if(DetailedLogEnabled())LogMsg(line);}while(0)
struct Zone{int x,y;unsigned char loading;};
int GetZoneGridX(void*p){++queries;return ((Zone*)p)->x;}int GetZoneGridY(void*p){++queries;return ((Zone*)p)->y;}int GetZoneState(void*){++queries;return 2;}
bool ZoneHandoffTownGuardRefuses(int,float){return refuse;}bool ZoneHandoffTakeover(void*,void*,int){++takeCalls;return take;}
bool Native(void*,void*,__int64,int,int,float){++originalCalls;return originalAnswer;}
bool(*orig_activateZoneMap)(void*,void*,__int64,int,int,float)=Native;
@HOOK@
struct Preloaded{int gridX,gridY;bool contentProcessed,pending;};static Preloaded preloadedZones[4];
static ZoneLifecycleDiagBudget g_firstTimeDiagBudget;static unsigned char g_ftLateCell[4096];static int pendingCount=0,g_ftCleared=0,g_ftLate=0;
int ZoneCell(int x,int y){return x<0||x>63||y<0||y>63?-1:x*64+y;}
bool ReadContentLifeFlags(void*,int*f,int*l,int*a){++flagsCalls;*f=ft;*l=ld;*a=act;return readOk;}
int ZlCellTownCount(int){++townCalls;return 0;}int ZlZoneFileExists(int,int){++fileCalls;return 1;}
bool ZlLateHandoffPossible(){return latePossible;}
bool ZoneInSetA(void*,void*){return inA;}bool ZoneInSetB(void*,void*){return inB;}bool ContentUntouched(void*){return untouched;}
void DropTrackedZone(int){++drops;}void ZlTouchZone(void*,int){++touches;}
const int ZL_HANDOFF=32;
#define KLIB_MEMBER(t,base,member,off) ((base)+offsetof(Zone,loading))
@FIRST@
@GIVE@
int main(){Zone z={30,31,1};
 debug=false;refuse=true;int q=queries,c=clocks;C(!hook_activateZoneMap(NULL,&z,123,4,2,0),"OFF refusal result unchanged");C(queries==q&&clocks==c&&logs.empty()&&originalCalls==0&&takeCalls==0,"OFF refusal no diagnostic queries, native retry, or output");
 debug=true;C(!hook_activateZoneMap(NULL,&z,123,4,2,0),"ON refusal result unchanged");C(logs.size()==1&&logs.back().find("ZoneTownRefuse: zone(30,31)")!=std::string::npos&&logs.back().find("range=4 type=2 timer=0 phase=2")!=std::string::npos,"PROD refused activation identifies cell and actual arguments");
 size_t n=logs.size();for(int i=0;i<1000;++i)hook_activateZoneMap(NULL,&z,123,4,2,0);C(logs.size()==n,"same-cell every-frame refusal suppressed");
 now=6;hook_activateZoneMap(NULL,&z,123,4,2,0);C(logs.size()==n+1,"returning cell remains observable");++epoch;hook_activateZoneMap(NULL,&z,123,4,2,0);C(logs.size()==n+2,"world epoch rearms diagnostics");
 n=logs.size();q=queries;c=clocks;mainThread=false;hook_activateZoneMap(NULL,&z,123,4,2,0);C(logs.size()==n&&queries==q&&clocks==c,"foreign-thread refusal cannot inspect map");mainThread=true;
 refuse=false;originalAnswer=true;take=false;n=logs.size();q=queries;c=clocks;C(hook_activateZoneMap(NULL,&z,123,4,2,1)&&originalCalls==1&&takeCalls==1,"accepted activation native and takeover exactly once");C(logs.size()==n&&queries==q&&clocks==c,"accepted activation adds no diagnostics");
 originalAnswer=false;take=true;C(hook_activateZoneMap(NULL,&z,123,4,2,1)&&originalCalls==2&&takeCalls==2,"takeover preserves successful return");
 preloadedZones[0].gridX=30;preloadedZones[0].gridY=31;preloadedZones[0].contentProcessed=true;debug=false;q=flagsCalls;c=clocks;ZlLogFirstTimeDiag(0,&z);C(flagsCalls==q&&clocks==c&&fileCalls==0&&townCalls==0,"OFF private finalize zero added native/clock queries");
 debug=true;n=logs.size();ZlLogFirstTimeDiag(0,&z);C(logs.size()==n+1&&logs.back().find("source=private-processContent ft=0 loaded=1 activation=0 towns=0 file=1")!=std::string::npos,"PROD private finalize records actual flags and predictor inputs");
 q=fileCalls;ZlLogFirstTimeDiag(0,&z);C(logs.size()==n+1&&fileCalls==q,"finalize duplicate does not query file/town");now+=6;ft=1;ZlLogFirstTimeDiag(0,&z);C(logs.size()==n+2&&logs.back().find("lateHandoffEligible=1")!=std::string::npos,"repeat private finalize in same cell is observable");
 ++epoch;ZlLogFirstTimeDiag(0,&z);C(logs.size()==n+3,"import rearms private finalize");ld=0;now+=6;ZlLogFirstTimeDiag(0,&z);C(logs.size()==n+3,"unfinished shell never reported as finalized");ld=1;
 for(int enabled=0;enabled<=1;++enabled){debug=enabled!=0;for(int branch=0;branch<3;++branch){++epoch;inA=branch==0;inB=false;untouched=branch==1;z.loading=1;preloadedZones[0].pending=true;pendingCount=1;int d=drops,t=touches,f=g_ftCleared,l=g_ftLate;q=clocks;n=logs.size();bool r=ZlGiveToGame(NULL,0,&z,&z,"test boundary");C(r==(branch!=2),"handoff return unchanged");C(drops==d+(branch!=2)&&touches==t+(branch==1)&&g_ftCleared==f+(branch==1)&&g_ftLate==l+(branch==2),"handoff branch ownership/mutation unchanged");C(pendingCount==(branch==2?0:1)&&z.loading==(branch==1?0:1),"handoff pending/loading state unchanged");C(enabled?logs.size()==n+1:logs.size()==n&&clocks==q,"handoff diagnostic enabled only");}}
 ZoneLifecycleDiagBudget b;for(int i=0;i<64;++i)C(b.Admit(1,0,i,1,0),"bounded unique events admitted");C(!b.Admit(1,0,90,1,0)&&b.emitted==64,"64-event minute cap");C(b.Admit(1,0,90,1,60),"minute budget rearms");C(!b.Admit(1,0,90,1,61),"per-cell suppression survives window");C(b.Admit(2,0,90,1,61),"world reset rearms budget");C(b.Admit(2,0,90,1,0),"clock rewind resets budget");C(!b.Admit(2,0,90,1,-1),"invalid clock refused");
 printf("PASS %d PROD extracted lifecycle controls; fixed budget bytes=%u\n",checks,(unsigned)sizeof(b));}
""".replace('@HOOK@',hook).replace('@FIRST@',first).replace('@GIVE@',give)
cpp=out/'diagnostics.cpp';cpp.write_text(fixture)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{root}/src;{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
for opt in ('Od','O2'):
 label=('OLD-'if '--old-control'in sys.argv else '')+opt;exe=out/(label+'.exe');p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp),'/Fo'+str(out)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);(out/(label+'-build.log')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(exe)],capture_output=True,text=True);(out/(label+'-run.log')).write_text(p.stdout+p.stderr);print(label,p.stdout.strip())
 if '--old-control'in sys.argv:assert p.returncode==1 and'PROD refused activation identifies'in p.stdout
 else:assert not p.returncode,p.stdout+p.stderr
