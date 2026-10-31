#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include "../TauntPolicy.h"
#define CHECK(x) do { if (!(x)) { printf("FAIL selection line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
struct AI {};
struct RootObject {};
struct Character {};
struct hand { bool valid;operator bool()const{return valid;} };
template<class T>struct lektor {};
static TauntExpansion::Config g_config;
static bool g_stopped=false,hasTaunt=true;
static TauntExpansion::ObservationCache g_observations;
static int originals=0,scans=0,logs=0;
static RootObject target;
static bool Original(AI*,RootObject** out,lektor<Character*>&,float& score,hand old,bool law,bool combat) {
    ++originals;CHECK(old.valid && law && !combat);*out=&target;score=42;return true;
}
static bool (*g_originalChooseBestAttackTarget)(AI*,RootObject**,lektor<Character*>&,float&,hand,bool,bool)=Original;
static bool AnyCandidateIsTaunting(const lektor<Character*>&) { ++scans;return hasTaunt; }
static void LogObservation(AI*,RootObject* selected,const lektor<Character*>&,float score,const hand&,bool,bool,bool result) {
    ++logs;CHECK(selected==&target && score==42 && result);
}
#include "selection_under_test.h"
int main() {
    AI ai;RootObject* selected=NULL;lektor<Character*> enemies;float score=0;hand old={true};
    SelectionCall c={&ai,&selected,&enemies,&score,&old,true,false};
    CHECK(RunSelection(&c));CHECK(originals==1 && scans==0 && logs==0 && selected==&target && score==42);
    for (size_t i=0;i<g_observations.Capacity();++i)CHECK(!g_observations.entries[i].occupied);
    g_config.debug=true;hasTaunt=false;
    CHECK(RunSelection(&c));CHECK(originals==2 && scans==1 && logs==0);
    hasTaunt=true;g_config.minimumLogIntervalMs=0;
    CHECK(RunSelection(&c));CHECK(originals==3 && scans==2 && logs==1);
    CHECK(RunSelection(&c));CHECK(originals==4 && scans==3 && logs==1);
    g_config.logOnlyChangedSelection=false;
    CHECK(RunSelection(&c));CHECK(originals==5 && logs==2);
    g_stopped=true;CHECK(RunSelection(&c));CHECK(originals==6 && logs==2 && scans==4);
    puts("PASS: actual production RunSelection preserves original outputs; Debug0 scans/cache/logs bypass; debug filtering/rate identity/stop");
}
