#include "gui/mcm_bridge.h"
#include "gui/requested_settings.h"
#include "base/config_values.h"
#include "gui/settings_layout.h"
#include "gui/gui_config.h"
#include "check.h"
#include <cstring>
#include <limits>
#include <string>
#include <vector>
bool ApplyRenderConfig(const RenderConfig&next);bool BenchWindowInForeground(){return false;}int BenchLoadedZoneCount(){return 0;}
static unsigned saves=0,applies=0,errors=0,floorReads=0,translations=0;static bool saveOK=true,throwApply=false,mainThread=true,detailed=false,reenter=false;static MCM_SetIntFn nestedSetter=NULL;static void*nestedContext=NULL;
const char*KeoTr(const char*s){++translations;return s?s:"";}
void KeoSetTranslationReady(bool){}void KeoSetLocalizationDetailed(bool){}
static std::string text="; owner\r\nunknown=keep\r\nzoneLifeRetainRadius=1\r\nzoneLifeRetainRadius=1\r\n";
static std::vector<IniEntry> lastSavedEntries;
void LogError(const std::string&){++errors;}void LogMsg(const std::string&){}
bool IsMainThread(){return mainThread;}void SetDetailedLogEnabled(bool on){detailed=on;}bool DetailedLogEnabled(){return detailed;}
ConfigFloor KeoMcmRuntimeFloor(){++floorReads;ConfigFloor f={"zoneLifeSquadRadius",1,"Fast hopping floor"};return f;}
bool SaveIniEntries(const std::vector<IniEntry>&entries,const char*,const char*){++saves;if(reenter){reenter=false;char error[128];Check(nestedSetter(nestedContext,0,error,sizeof(error))!=0,"nested save callback refuses before another write");}if(!saveOK)return false;lastSavedEntries=entries;text=RewriteIniKeys(text,entries,NULL);return true;}
bool ApplyRenderConfig(const RenderConfig&next){++applies;if(throwApply)throw 1;g_renderCfg=next;return false;}
struct Row{std::string label,def;int kind,lo,hi;unsigned flags;MCM_GetIntFn gi;MCM_SetIntFn si;MCM_GetFloatFn gf;MCM_SetFloatFn sf;void*ud;std::vector<int>choices;Row():kind(0),lo(0),hi(0),flags(0),gi(NULL),si(NULL),gf(NULL),sf(NULL),ud(NULL){}};
static std::vector<Row>rows;static unsigned begins=0;
static void __cdecl Begin(MCM_Api*,const char*id,const char*,const char*ini,MCM_ApplyFn apply){Check(!std::strcmp(id,"KEO")&&!ini&&!apply,"callback page owns persistence");++begins;}
static void __cdecl Section(MCM_Api*,const char*){}static void __cdecl Info(MCM_Api*,const char*){}
static void __cdecl Toggle(MCM_Api*,const char*l,const char*,MCM_GetIntFn g,MCM_SetIntFn s,void*u,unsigned f){Row r;r.label=l;r.kind=1;r.gi=g;r.si=s;r.ud=u;r.flags=f;rows.push_back(r);}
static void __cdecl Integer(MCM_Api*a,const char*l,const char*t,MCM_GetIntFn g,MCM_SetIntFn s,void*u,int lo,int hi,unsigned f){Toggle(a,l,t,g,s,u,f);rows.back().kind=2;rows.back().lo=lo;rows.back().hi=hi;}
static void __cdecl Choice(MCM_Api*a,const char*l,const char*t,MCM_GetIntFn g,MCM_SetIntFn s,void*u,const int*v,const char*const*,int n,unsigned f){Toggle(a,l,t,g,s,u,f);rows.back().kind=3;rows.back().choices.assign(v,v+n);}
static void __cdecl Number(MCM_Api*,const char*l,const char*,MCM_GetFloatFn g,MCM_SetFloatFn s,void*u,float lo,float hi,int decimals,unsigned f){Row r;r.label=l;r.kind=4;r.gf=g;r.sf=s;r.ud=u;r.flags=f;r.lo=int(lo);r.hi=int(hi);Check(decimals==1,"authored foliage precision retained");rows.push_back(r);}
static void __cdecl Default(MCM_Api*,const char*v){rows.back().def=v;}
static Row*Find(const char*key){const ConfigKey*k=FindConfigKey(key,NULL);for(size_t i=0;k&&i<rows.size();++i)if(rows[i].label==k->label)return &rows[i];return NULL;}
static void Describe(MCM_Api&api){rows.clear();KeoMcmDescribe(&api);}
int main(){MCM_Api api={};api.version=5;api.beginMod=Begin;api.section=Section;api.info=Info;api.toggleFn=Toggle;api.integerFn=Integer;api.choiceFn=Choice;api.numberFn=Number;api.defaultValue=Default;
 KeoMcmDescribe(NULL);Describe(api);Check(rows.empty(),"early discovery has no unsafe callbacks");
 movement::g_movementCfg.routingEnabled=false;movement::g_movementCfg.groupCohesionEnabled=true;Check(CaptureRequestedSettings(),"requested captured before runtime refusal");movement::ApplyRoutingSwitch(NULL);
 api.version=2;Describe(api);Check(rows.empty(),"API2 retains fallback only");api.version=3;Describe(api);Check(rows.size()==33&&rows.back().label=="Detailed log"&&rows.back().def.empty(),"v3 includes RAM and forward GPU/HUD rows with Debug last");
 api.version=5;Describe(api);unsigned restart=0;for(size_t i=0;i<rows.size();++i){restart+=(rows[i].flags&MCM_RESTART)!=0;Check(rows[i].label.find('*')==std::string::npos,"MCM owns restart marker");}Check(restart==14&&rows.back().flags==0&&rows.back().def=="0","14 startup rows and live defaultOFF Debug");
 for(unsigned p=0;keo_gui::kSettingsPlaces[p].section;++p){const ConfigKey*k=FindConfigKey(keo_gui::kSettingsPlaces[p].key,NULL);Row*r=Find(k->name);Check(r&&r->flags==(k->live?0:MCM_RESTART),"each original player row preserves exact live/restart classification");if(r&&r->kind==2)Check(r->lo==int(k->lo)&&r->hi==int(k->hi),"integer range comes from original loader metadata");}
 Check(Find("carriedWeight")&&Find("carriedWeight")->def=="0"&&Find("carriedWeight")->flags==MCM_RESTART&&!keo_inventory::g_inventoryCfg.carriedWeightEnabled,"carried weight defaultOFF and restart-only, no runtime activation");
 Check(Find("resourceRamCache")&&Find("resourceRamCache")->def=="0"&&Find("resourceRamCache")->flags==0,"RAM cache opt-in and live");
 Check(Find("resourceRamGiB")&&Find("resourceRamGiB")->lo==1&&Find("resourceRamGiB")->hi==32&&Find("resourceRamGiB")->flags==0,"RAM cap one GiB steps up to32 live");
 Check(Find("resourceReadMBps")&&Find("resourceReadMBps")->lo==1&&Find("resourceReadMBps")->hi==128&&Find("resourceReadMBps")->def=="32"&&Find("resourceReadMBps")->flags==0,"bounded background IO live");
 Check(Find("resourcePrefetch")&&Find("resourcePrefetch")->def=="0"&&Find("resourcePrefetch")->flags==0,"forward terrain opt-in live");
 Check(Find("resourceGpuWarm")&&Find("resourceGpuWarm")->def=="0"&&Find("resourceGpuWarm")->flags==0,"GPU warmup opt-in live");
 Check(Find("resourceGpuGiB")&&Find("resourceGpuGiB")->lo==1&&Find("resourceGpuGiB")->hi==8&&Find("resourceGpuGiB")->def=="1"&&Find("resourceGpuGiB")->flags==0,"VRAM1to8GiB live");
 Check(Find("resourceWarmHud")&&Find("resourceWarmHud")->def=="0"&&Find("resourceWarmHud")->flags==0,"resource HUD opt-in live");
 Check(Find("k7PostDeathHold")->def=="2","custom choice default uses integer callback value not INI word");
#ifndef KEO_DEBUG
 Check(Find("k7PostDeathHold")->choices.size()==2,"PROD hides unselected observe choice");
#endif
 int got=-9;char err[256]={0};Row child=*Find("groupCohesion");Check(!child.gi(child.ud,&got)&&got==1&&!movement::g_movementCfg.groupCohesionEnabled,"getter is requested child despite effectiveOFF");
 unsigned before=saves;Check(!child.si(child.ud,1,err,sizeof(err))&&saves==before,"unchanged preference zero disk writes");Check(child.si(child.ud,2,err,sizeof(err))!=0&&saves==before,"boolean type validates");
 Check(!child.si(child.ud,0,err,sizeof(err))&&saves==before+1&&!movement::g_movementCfg.groupCohesionEnabled,"startup child saves only");Check(!child.si(child.ud,1,err,sizeof(err)),"requested child restore");
 Row retain=*Find("zoneLifeRetainRadius");int original=zone::g_zoneCfg.cfg_zoneLifeRetainRadius;KeoMcmSetRuntimeReady(false);before=floorReads;Check(!retain.si(retain.ud,3,err,sizeof(err))&&zone::g_zoneCfg.cfg_zoneLifeRetainRadius==original&&floorReads==before,"failed gate no native floor/apply");
 before=saves;saveOK=false;Check(retain.si(retain.ud,4,err,sizeof(err))!=0&&saves==before+1&&zone::g_zoneCfg.cfg_zoneLifeRetainRadius==original,"disk error leaves running value untouched");Check(!retain.gi(retain.ud,&got)&&got==3,"disk error preserves owned requested value");saveOK=true;
 Check(translations==0,"refused platform metadata and setter errors avoid native localization");KeoMcmSetPlatformReady(true);Describe(api);Check(translations>0&&!KeoMcmRuntimeReady(),"admitted platform can translate despite partial hooks");
 KeoMcmSetRuntimeReady(true);Check(!retain.si(retain.ud,2,err,sizeof(err))&&zone::g_zoneCfg.cfg_zoneLifeRetainRadius==2,"existing live row applies after save");
 Row foliage=*Find("foliagePageBudgetMs");g_renderCfg.particleStepCap=false;g_renderCfg.renderLevers=false;Check(!foliage.sf(foliage.ud,2.5f,err,sizeof(err))&&g_renderCfg.foliagePageBudgetMs==2.5f&&!g_renderCfg.particleStepCap&&!g_renderCfg.renderLevers,"render apply cannot revive unrelated refused fields");
 std::puts("CHECK finite render clamp");std::fflush(stdout);
 before=saves;unsigned clampApplies=applies;float clamped=0;
 Check(!foliage.sf(foliage.ud,100.0f,err,sizeof(err))&&saves==before+1&&applies==clampApplies+1,"finite overflow clamps and saves/applies exactly once");
 Check(!foliage.gf(foliage.ud,&clamped)&&clamped==50.0f&&g_renderCfg.foliagePageBudgetMs==50.0f&&lastSavedEntries.size()==1&&lastSavedEntries[0].key=="foliagePageBudgetMs"&&lastSavedEntries[0].value=="50"&&text.find("foliagePageBudgetMs=50\r\n")!=std::string::npos,"overflow requested disk and dirty apply use exact author maximum");
 before=saves;clampApplies=applies;Check(!foliage.sf(foliage.ud,100.0f,err,sizeof(err))&&saves==before&&applies==clampApplies,"repeated clamped maximum makes zero save/apply calls");
 Check(!foliage.sf(foliage.ud,-100.0f,err,sizeof(err))&&!foliage.gf(foliage.ud,&clamped)&&clamped==0.0f&&g_renderCfg.foliagePageBudgetMs==0.0f&&lastSavedEntries.size()==1&&lastSavedEntries[0].value=="0"&&text.find("foliagePageBudgetMs=0\r\n")!=std::string::npos,"finite underflow uses exact requested disk and live author minimum");
 before=saves;clampApplies=applies;Check(!foliage.sf(foliage.ud,-100.0f,err,sizeof(err))&&saves==before&&applies==clampApplies,"repeated clamped minimum makes zero save/apply calls");
 // Deliberately stale invalid sibling models a baseline opened before another
 // frontend/state correction. Both clamp passes inspect it; only explicit edits
 // may enter requested publication, persistence or live apply.
 std::puts("CHECK merged sibling render clamp");std::fflush(stdout);
 SettingsStaging sibling;RequestedConfig().Copy(&sibling);StagedRender(&sibling).foliageBudgetSpeed=100.0f;Check(PublishRequestedSettings(sibling),"owned stale sibling fixture published");float liveSpeed=g_renderCfg.foliageBudgetSpeed;
 before=saves;clampApplies=applies;Check(!retain.si(retain.ud,3,err,sizeof(err))&&saves==before+1&&applies==clampApplies,"cross-field render clamps do not dirty/apply unrelated live render rows");
 SettingsStaging afterSibling;RequestedConfig().Copy(&afterSibling);Check(StagedRender(&afterSibling).foliageBudgetSpeed==100.0f&&g_renderCfg.foliageBudgetSpeed==liveSpeed&&lastSavedEntries.size()==1&&lastSavedEntries[0].key=="zoneLifeRetainRadius"&&lastSavedEntries[0].value=="3"&&text.find("foliageBudgetSpeed=")==std::string::npos,"unrelated clamp normalization is excluded from requested disk and live changes");
 StagedRender(&afterSibling).foliageBudgetSpeed=liveSpeed;Check(PublishRequestedSettings(afterSibling),"owned stale sibling fixture retired");
 before=saves;Check(foliage.sf(foliage.ud,std::numeric_limits<float>::quiet_NaN(),err,sizeof(err))!=0&&saves==before,"NaN rejected before persistence");
 Row debug=rows.back();Check(!debug.si(debug.ud,1,err,sizeof(err))&&detailed,"live cached logger switch");Row choice=*Find("operatorHoldUntil");before=saves;Check(choice.si(choice.ud,123456,err,sizeof(err))!=0&&saves==before,"invalid custom choice rejected");
 Row fill=*Find("operatorFillPercent");
 bool fillSteps=fill.choices.size()==20;
 for(size_t i=0;fillSteps&&i<fill.choices.size();++i)fillSteps=fill.choices[i]==int(i+1)*5;
 Check(fillSteps&&fill.def=="100"&&!fill.flags,"fill: all 5 percent steps, live, native room by default");
 Check(fill.si(fill.ud,7,err,sizeof(err))!=0&&saves==before,"fill: rejects an intermediate value without writing");
 Check(!fill.si(fill.ud,55,err,sizeof(err))&&saves==before+1&&keo_inventory::g_inventoryCfg.operatorFillPercent==55,"fill: writes exact stepped value and applies live");
 Check(child.gi(NULL,&got)!=0&&child.gi(child.ud,NULL)!=0,"callback context and destinations bounded");mainThread=false;before=saves;unsigned translated=translations;Check(child.si(child.ud,0,err,sizeof(err))!=0&&saves==before&&translations==translated,"off-thread callback refuses without native localization");mainThread=true;
 Check(child.si(reinterpret_cast<char*>(child.ud)+1,0,err,sizeof(err))!=0,"misaligned context rejects without dereference");
 before=saves;unsigned floorBefore=floorReads,applyBefore=applies,trBefore=translations;for(unsigned n=0;n<100;++n)for(size_t i=0;i<rows.size();++i){if(rows[i].gi)rows[i].gi(rows[i].ud,&got);else{float f;rows[i].gf(rows[i].ud,&f);}}Check(saves==before&&floorReads==floorBefore&&applies==applyBefore&&translations==trBefore,"repeated getters do no IO/native floor/apply/translation work");
 SettingsStaging legacy;RequestedConfig().Copy(&legacy);SettingsStaging edit=legacy;const ConfigModule*module=NULL;const ConfigKey*key=FindConfigKey("groupCohesion",&module);*reinterpret_cast<bool*>(reinterpret_cast<char*>(edit.module[module-kConfigModules].state)+key->offset)=false;
 Check(!retain.si(retain.ud,4,err,sizeof(err)),"newer MCM edit after fallback baseline opens");Check(!KeoMcmCommitStaging(edit,legacy,err,sizeof(err))&&!retain.gi(retain.ud,&got)&&got==4,"stale fallback merges only changed fields");
 nestedSetter=child.si;nestedContext=child.ud;reenter=true;before=saves;Check(!retain.si(retain.ud,3,err,sizeof(err))&&saves==before+1,"single save survives rejected nested callback");
 Row squad=*Find("zoneLifeSquadRadius");Check(!squad.si(squad.ud,0,err,sizeof(err))&&!squad.gi(squad.ud,&got)&&got==1&&floorReads>floorBefore,"current native floor applies only on admitted edited row");
 throwApply=true;Check(foliage.sf(foliage.ud,3.5f,err,sizeof(err))!=0&&std::strstr(err,"saved")!=NULL,"post-save apply exception reported honestly");float fv=0;Check(!foliage.gf(foliage.ud,&fv)&&fv==3.5f,"post-save exception preserves committed preference");throwApply=false;
 Check(text.find("; owner\r\nunknown=keep\r\n")!=std::string::npos&&text.find("zoneLifeRetainRadius=3\r\nzoneLifeRetainRadius=3")!=std::string::npos,"flat writer preserves comments unknown keys EOL and duplicates");
 SettingsStaging observed;RequestedConfig().Copy(&observed);key=FindConfigKey("k7PostDeathHold",&module);int mi=int(module-kConfigModules),ki=int(key-module->keys);*reinterpret_cast<int*>(reinterpret_cast<char*>(observed.module[mi].state)+key->offset)=K7_HOLD_OBSERVE;observed.module[mi].slots[ki].i=K7_HOLD_OBSERVE;PublishRequestedSettings(observed);Describe(api);Row observe=*Find("k7PostDeathHold");Check(observe.choices.size()==3&&!observe.gi(observe.ud,&got)&&got==1,"PROD preserves an already selected author observe preference");
 Check(!observe.si(observe.ud,2,err,sizeof(err))&&!observe.si(observe.ud,1,err,sizeof(err)),"callbacks accept every choice offered for this page lifetime");
 SettingsStaging valid;RequestedConfig().Copy(&valid);SettingsStaging malformed=valid;key=FindConfigKey("operatorHoldUntil",&module);mi=int(module-kConfigModules);ki=int(key-module->keys);malformed.module[mi].slots[ki].i=123456;before=saves;Check(KeoMcmCommitStaging(malformed,valid,err,sizeof(err))!=0&&saves==before,"shared fallback commit rejects malformed custom choice before save");
 return CheckExit("mcm_bridge_units");}
