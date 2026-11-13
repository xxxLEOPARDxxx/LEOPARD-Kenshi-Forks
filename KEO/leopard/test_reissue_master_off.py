"""Exercise the real reissue adapter and crossing decisions with owned state.

Regression: the routing master disabled K7 but left park/growth/retry steering
live. Native world lookup and move dispatch are bounded observation seams;
this does not emulate Kenshi navigation or establish in-game route quality.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/reissue-master-off-tests'
OUT.mkdir(parents=True, exist_ok=True)


def extract(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


adapter = extract((ROOT / 'src/movement/islands_reissue.cpp').read_text(),
                  'void IslandReissuePollTick(')
orders = (ROOT / 'src/movement/island_orders.cpp').read_text()
types = (ROOT / 'src/movement/islands_reissue_internal.h').read_text()
cancel = (ROOT / 'src/movement/island_cancel_hooks.cpp').read_text()
fixture = r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
namespace movement { struct Config { bool routingEnabled; } g_movementCfg; }
@ORDER@;
@CTX@;
@DIST@
const float MISSED_ADVANCE_DIST=350,UNPARK_DIST=50,GROWTH_THRESHOLD_SQ=40;
const double RETRY_DELAY=1.5;
const int MAX_REISSUES=8;
bool paused=false,k7=false;unsigned checks=0,rebases=0,resolves=0,samples=0,polls=0,crossings=0,sends=0,detaches=0;
unsigned gen=1,sig=1;const char*lastReason="";
IslandOrder order;PollOrdersCtx ctx;
void C(bool ok,const char*name){++checks;if(!ok){std::printf("FAIL %s\n",name);std::exit(1);}}
bool ZonePauseIsPaused(){return paused;}
void K7RebasePausedClocks(bool p,double now){C(p==paused&&now==10,"clock rebase keeps exact inputs");++rebases;}
void ResolveDueReissueChecks(double){++resolves;}
bool K7FormOn(){return k7;}
void K7SampleSignatures(double,bool){++samples;}
unsigned IslandOverlayGen(){return gen;}
unsigned IslandOverlaySetBSig(){return sig;}
bool IslandEmulateCrossing(void*,void*,float,float,float,float,float*x,float*z){++crossings;*x=500;*z=0;return true;}
bool ReissueOrder(IslandOrder&,double,const char*why,bool,float,float,bool){++sends;lastReason=why;return true;}
#define KEO_DETAIL(x) ((void)0)
@CROSSING@
// All tests start after the native movement/liveness/park snapshot. The
// adapter must prevent this real crossing decision from executing while OFF.
void PollOrders(uintptr_t zm,double now){++polls;C(zm==1&&now==10,"poll keeps exact inputs");if(order.active)PollOrderCrossing(order,ctx);}
@ADAPTER@
void IslandDetachSelectedFromFormation(uintptr_t){++detaches;}
int K7DropSelected(uintptr_t){order.active=false;return 1;}
long g_k7CancelStop=0,g_k7CancelJob=0;
@STOP@
@JOB@
void Reset(bool on,int form){
 movement::g_movementCfg.routingEnabled=on;paused=false;k7=false;
 rebases=resolves=samples=polls=crossings=sends=detaches=0;gen=sig=1;lastReason="";
 std::memset(&order,0,sizeof(order));std::memset(&ctx,0,sizeof(ctx));
 order.active=true;order.destX=1000;ctx.zm=ctx.charZone=1;ctx.now=10;ctx.parkedNow=ctx.edgeParked=ctx.edge=true;
 if(form){order.parked=true;order.haveX0=true;order.seenGen=gen;order.seenSig=sig;}
 if(form==1){order.retryArmed=true;order.retryX=500;order.lastReissueTime=5;}
 if(form==2){order.x0X=100;order.seenGen=0;}
 if(form==3){order.haveX0=false;order.seenGen=0;}
}
void Tick(){IslandReissuePollTick(1,10);}
int main(){
 const char*reasons[]={"park","retry","growth","growth"};
 for(int form=0;form<4;++form){
  Reset(false,form);IslandOrder before=order;Tick();
  C(!sends,"master OFF prevents park growth retry steering");
  C(!resolves&&!samples&&!polls&&!crossings,"master OFF avoids native state reads and scans");
  C(rebases==1&&!std::memcmp(&before,&order,sizeof(order)),"OFF preserves independent tracker state and pause clock rebase");
  Reset(true,form);Tick();C(sends==1&&!std::strcmp(lastReason,reasons[form]),"master ON preserves real crossing recovery");
  C(resolves==1&&polls==1&&crossings==1&&!samples,"ON keeps recovery even with K7 separately disabled");
 }
 for(int cancel=0;cancel<3;++cancel)for(int form=0;form<4;++form){
  Reset(false,form);
  if(cancel==0)IslandNoteCancelStop((void*)1);
  if(cancel==1)IslandNoteCancelJob((void*)1,false);
  if(cancel==2)IslandNoteCancelJob((void*)1,true);
  Tick();C(!sends&&!polls,"OFF cannot resurrect retained stale target after stop or job cancel");
  C(order.active,"K7 OFF cancellation tracker semantics unchanged");
  C(detaches==(cancel==2?0u:1u),"queued job keeps formation while stop and replacing job detach");
 }
 Reset(true,0);k7=true;Tick();C(samples==1&&sends==1,"ON K7 sampling and park recovery preserved");
 Reset(true,0);k7=true;paused=true;Tick();C(rebases==1&&!samples&&!resolves&&!polls&&!sends,"pause suppresses every recovery path");
 std::printf("PASS %u actual reissue master controls\n",checks);return 0;
}
'''
for token, value in {
    '@ORDER@': extract(types, 'struct IslandOrder {'),
    '@CTX@': extract(orders, 'struct PollOrdersCtx {'),
    '@DIST@': extract(types, 'inline float Dist2('),
    '@CROSSING@': extract(orders, 'static bool PollOrderCrossing('),
    '@ADAPTER@': adapter,
    '@STOP@': extract(cancel, 'void IslandNoteCancelStop('),
    '@JOB@': extract(cancel, 'void IslandNoteCancelJob('),
}.items():
    fixture = fixture.replace(token, value)

VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = dict(os.environ, INCLUDE=f'{VC}/include;{SDK}/Include',
           LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
variants = [('Od', 'Od', fixture, False), ('O2', 'O2', fixture, False)]
guard = 'paused || !movement::g_movementCfg.routingEnabled'
if guard in adapter:
    variants.append(('missing-master-guard', 'O2', fixture.replace(guard, 'paused'), True))
for name, optimization, code, expected_red in variants:
    directory = OUT / name
    directory.mkdir(exist_ok=True)
    cpp = directory / 'test.cpp'
    cpp.write_text(code)
    command = [VC / 'bin/amd64/cl.exe', '/nologo', '/EHsc', '/MD',
               '/' + optimization, '/W3', cpp,
               '/Fo' + str(directory) + '/', '/Fe' + str(directory / 'test.exe')]
    result = subprocess.run([str(x) for x in command], env=env,
                            capture_output=True, text=True)
    (directory / 'build.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    result = subprocess.run([str(directory / 'test.exe')], capture_output=True, text=True)
    (directory / 'run.log').write_text(result.stdout + result.stderr)
    print(name, result.stdout + result.stderr, end='')
    if expected_red:
        assert result.returncode and 'FAIL master OFF prevents park growth retry steering' in result.stdout
    elif result.returncode:
        raise SystemExit(result.returncode)
