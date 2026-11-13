#include <Windows.h>
#include <stdint.h>
#include <float.h>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
static unsigned checks=0, reports=0, errors=0, crimeCalls=0, timerCalls=0, bridgeCalls=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
struct hand {unsigned type,container,containerSerial,index,serial;};
struct Faction {};
enum { CRIME_MURDER=9 };
struct Crimes {
 bool accepts;float crimeExpiry;
 Crimes():accepts(true),crimeExpiry(3){}
 bool setCrime(int crime,Faction*,const hand&){CHECK(crime==CRIME_MURDER);++crimeCalls;return accepts;}
 void notifyPossibleCrimeWitnessed(float seconds){++timerCalls;crimeExpiry=seconds;}
};
struct Character {
 hand identity;Crimes crimes;Faction faction;bool dead;
 Character(unsigned index):dead(false){identity.type=1;identity.container=21;identity.containerSerial=99;identity.index=index;identity.serial=1000+index;}
 const hand& getHandle(){return identity;}Faction* getFaction(){return &faction;}
};
static bool detailed=false,modulePresent=true,exportPresent=true,raiseFault=false;
static uint32_t bridgeResult=1,words[12]={};
static bool ShouldLogExecuteDebug(){return detailed;}
static void PluginLog(const char*){++reports;}
static void ErrorLog(const char*){++errors;}
static bool TryIsCharacterDead(Character* c,bool* value){if(!c)return false;*value=c->dead;return true;}
static uint32_t __cdecl FakeBridge(const void* packet){
 ++bridgeCalls;if(raiseFault)RaiseException(0xE0000123,0,0,0);
 const uint32_t* input=static_cast<const uint32_t*>(packet);
 for(unsigned i=0;i<12;++i)words[i]=input[i];return bridgeResult;
}
static HMODULE FakeModule(const wchar_t* name){CHECK(std::wstring(name)==L"KillCounter.dll");return modulePresent?reinterpret_cast<HMODULE>(1):0;}
static FARPROC FakeExport(HMODULE,const char* name){CHECK(std::string(name)=="KillCounter_RecordExecutionV1");return exportPresent?reinterpret_cast<FARPROC>(&FakeBridge):0;}
#define GetModuleHandleW FakeModule
#define GetProcAddress FakeExport
#include "LootScootExecuteEffects.inl"
int main(){
 Character actor(7),victim(8);victim.dead=true;
 NotifyExecutionKillCounter(0,&victim);NotifyExecutionKillCounter(&actor,0);CHECK(bridgeCalls==0);
 victim.dead=false;NotifyExecutionKillCounter(&actor,&victim);CHECK(bridgeCalls==0);victim.dead=true;
 modulePresent=false;NotifyExecutionKillCounter(&actor,&victim);CHECK(bridgeCalls==0);modulePresent=true;
 exportPresent=false;NotifyExecutionKillCounter(&actor,&victim);CHECK(bridgeCalls==0);exportPresent=true;
 NotifyExecutionKillCounter(&actor,&victim);CHECK(bridgeCalls==1);CHECK(words[0]==48&&words[1]==1);
 CHECK(words[2]==1&&words[3]==21&&words[4]==99&&words[5]==7&&words[6]==1007);
 CHECK(words[7]==1&&words[8]==21&&words[9]==99&&words[10]==8&&words[11]==1008);
 bridgeResult=2;NotifyExecutionKillCounter(&actor,&victim);CHECK(bridgeCalls==2);
 raiseFault=true;NotifyExecutionKillCounter(&actor,&victim);CHECK(errors==1);raiseFault=false;
 RegisterExecutionCrime(&actor,&victim,true);CHECK(crimeCalls==0&&timerCalls==0);
 RegisterExecutionCrime(0,&victim,false);RegisterExecutionCrime(&actor,0,false);CHECK(crimeCalls==0);
 actor.crimes.accepts=false;RegisterExecutionCrime(&actor,&victim,false);CHECK(crimeCalls==1&&timerCalls==0);
 actor.crimes.accepts=true;RegisterExecutionCrime(&actor,&victim,false);CHECK(timerCalls==1&&actor.crimes.crimeExpiry==20);
 actor.crimes.crimeExpiry=40;RegisterExecutionCrime(&actor,&victim,false);CHECK(timerCalls==1&&actor.crimes.crimeExpiry==40);
 actor.crimes.crimeExpiry=20;RegisterExecutionCrime(&actor,&victim,false);CHECK(timerCalls==1);
 actor.crimes.crimeExpiry=std::numeric_limits<float>::quiet_NaN();RegisterExecutionCrime(&actor,&victim,false);CHECK(timerCalls==2&&actor.crimes.crimeExpiry==20);
 printf("%u production execution-effects checks passed\n",checks);
}
