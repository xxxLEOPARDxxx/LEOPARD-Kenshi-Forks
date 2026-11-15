// Includes the actual production pin-run implementation. Only its game/clock
// facade is controlled; parsing, progress and rate policy are production too.
#define NOMINMAX
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "bench/bench_pin.h"
#include "bench/bench_game.h"
static unsigned checks=0,failures=0,writes=0,facadeReads=0;static bool ready=true,failWrite=false;
static float rate=0.0091667f;static const float defaultRate=0.0091667f;static float hour=12.0f;
volatile LONG g_navMeshStopSeen=0;
bool DetailedLogEnabled(){return false;}void LogMsg(const std::string&){}
#define KEO_DETAIL(x) do{}while(0)
bool BenchClockReady(){++facadeReads;return ready;}float BenchClockDefaultRate(){++facadeReads;return defaultRate;}
float BenchClockRate(){++facadeReads;return rate;}bool BenchSetClockRate(float value){++writes;if(!ready||failWrite)return false;rate=value;return true;}
float BenchGetHour(){++facadeReads;return hour;}int BenchGetDay(){++facadeReads;return 1;}
bool BenchTransitionClear(){++facadeReads;return true;}
bool BenchWeatherSnapshot(BenchWeatherHold*){return false;}bool BenchWeatherKeep(const BenchWeatherHold&){return false;}
const char* BenchForceWeather(const char*,float,std::string*){return "not requested";}
std::string BenchHourText(float){return "12.00";}std::string BenchWeatherText(){return "none";}
// PRODUCTION_BODY
#define C(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL line%u %s\n",__LINE__,#x);}}while(0)
static BenchPinSpec Freeze(){BenchPinSpec p=BenchPinNone();p.mode=BPM_FREEZE;p.hour=12.0f;return p;}
static int Step(){std::string why;return BenchPinStep(0.1,0.0,&why);}
int main(){
    // Default-disabled benchmark must never normalize an unrelated rate.
    rate=0.25f;for(unsigned i=0;i<1000000;++i)BenchPinIdleTick();C(writes==0&&rate==0.25f);
    unsigned before=writes;BenchPinRelease();C(writes==before&&rate==0.25f);
    // Beginning and releasing without a write does not create restoration debt.
    C(BenchPinBegin(Freeze(),1.0f,0.0));BenchPinRelease();C(writes==before&&rate==0.25f);BenchPinIdleTick();C(writes==before);
    // A refused native write cannot make KEO own the preexisting foreign rate.
    C(BenchPinBegin(Freeze(),1.0f,0.0));failWrite=true;C(Step()==-1&&rate==0.25f);before=writes;BenchPinRelease();BenchPinIdleTick();C(writes==before&&rate==0.25f);failWrite=false;
    // A successful nondefault pin write incurs debt; failed restoration is
    // retried, and success stops all later idle writes.
    C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0&&rate==0.0f);failWrite=true;before=writes;BenchPinRelease();C(writes==before+1&&rate==0.0f);
    BenchPinIdleTick();C(writes==before+2&&rate==0.0f);failWrite=false;BenchPinIdleTick();C(writes==before+3&&rate==defaultRate);
    rate=0.3f;before=writes;BenchPinIdleTick();BenchPinRelease();C(writes==before&&rate==0.3f);
    // Debt survives Begin's state reset, including an unavailable/no-pin arm.
    C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0&&rate==0.0f);failWrite=true;BenchPinRelease();
    C(!BenchPinBegin(BenchPinNone(),1.0f,1.0));failWrite=false;before=writes;BenchPinIdleTick();C(writes==before+1&&rate==defaultRate);
    // A later foreign write takes ownership away from KEO, even after a
    // restore was refused; KEO must not keep retrying over the foreign value.
    C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0&&rate==0.0f);failWrite=true;BenchPinRelease();failWrite=false;rate=0.4f;before=writes;BenchPinIdleTick();C(writes==before&&rate==0.4f);BenchPinIdleTick();C(writes==before);
    // Already-equal foreign rate: holding it is not an actual KEO write.
    rate=0.0f;before=writes;C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0&&writes==before);BenchPinRelease();BenchPinIdleTick();C(writes==before&&rate==0.0f);
    // Temporary facade refusal preserves real debt until readiness returns.
    rate=defaultRate;C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0&&rate==0.0f);ready=false;before=writes;BenchPinRelease();BenchPinIdleTick();C(writes==before);ready=true;BenchPinIdleTick();C(writes==before+1&&rate==defaultRate);
    // Shutdown drops ownership without reading/writing the facade, including
    // when a pin was active and when a prior restore failed.
    C(BenchPinBegin(Freeze(),1.0f,0.0));C(Step()==0);g_navMeshStopSeen=1;before=writes;unsigned reads=facadeReads;BenchPinRelease();BenchPinIdleTick();C(writes==before&&facadeReads==reads);g_navMeshStopSeen=0;BenchPinIdleTick();C(writes==before);
    std::printf("%s %u benchmark clock ownership controls (%u failures)\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}
