#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <ModConfigMenu.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL MCM line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
namespace PluginSettings { static std::string Path() { return "TauntExpansion.ini"; } }
static const char* Tr(const char* s) { return s; }
static void LoadConfig() {}
static int rows=0,sections=0,infos=0,begins=0;
static std::string lastKey;
static void Begin(MCM_Api*,const char* id,const char* title,const char* path,MCM_ApplyFn apply) {
    ++begins;CHECK(!strcmp(id,"TauntExpansion"));CHECK(!strcmp(title,"Taunt Expansion"));
    CHECK(!strcmp(path,"TauntExpansion.ini"));CHECK(apply==&LoadConfig);
}
static void Info(MCM_Api*,const char*) { ++infos; }
static void Section(MCM_Api*,const char*) { ++sections; }
static void Toggle(MCM_Api*,const char*,const char* key,const char*,const char*,int def,unsigned flags) {
    ++rows;lastKey=key;CHECK(flags==0);
    CHECK(def==(!strcmp(key,"Enabled") || !strcmp(key,"LogOnlyWhenTauntPresent") || !strcmp(key,"LogOnlyChangedSelection")));
}
static void Number(MCM_Api*,const char*,const char* key,const char*,const char*,float def,float lo,float hi,int places,unsigned flags) {
    ++rows;lastKey=key;CHECK(flags==0 && lo==0 && places==2);
    if (!strcmp(key,"Multiplier"))CHECK(def==5 && hi==100);
    else {CHECK(!strcmp(key,"RetargetMultiplier"));CHECK(def==2 && hi==10);}
}
static void Integer(MCM_Api*,const char*,const char* key,const char*,const char*,int def,int lo,int hi,unsigned flags) {
    ++rows;lastKey=key;CHECK(flags==0);
    if (!strcmp(key,"MinimumLogIntervalMs"))CHECK(def==500 && lo==0 && hi==60000);
    else {CHECK(!strcmp(key,"MaximumCandidatesPerEntry"));CHECK(def==32 && lo==1 && hi==512);}
}
#include "mcm_under_test.h"
int main() {
    MCM_Api api={};api.beginMod=Begin;api.section=Section;api.toggle=Toggle;api.number=Number;api.integer=Integer;
    api.version=1;MCM_Describe(&api);
    CHECK(begins==1 && rows==9 && sections==2 && infos==0 && lastKey=="Debug");
    begins=rows=sections=infos=0;api.version=6;api.info=Info;MCM_Describe(&api);
    CHECK(begins==1 && rows==9 && sections==2 && infos==1 && lastKey=="Debug");
    puts("PASS: actual MCM_Describe v1/v6 contract, nine live settings, defaults/ranges, Debug last and default off");
    return 0;
}
