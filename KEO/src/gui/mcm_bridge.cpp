#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "gui/mcm_bridge.h"
#include "gui/requested_settings.h"
#include "gui/settings_layout.h"
#include "gui/gui_config.h"
#include "base/keo_tr.h"
#include "render/render_keys.h"
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include <float.h>

// Narrow Layer0/runtime boundaries keep host tests free of game headers.
void LogError(const std::string&);bool IsMainThread();
void SetDetailedLogEnabled(bool);
namespace {
bool platformReady=false,runtimeReady=false,committing=false;
// MCM copies each offered choice list for this page lifetime. Keep the same
// admission mask even after its selected value changes, as the native editor
// does for an already selected DEV-only choice in PROD.
struct RowContext {int module,key;unsigned choiceMask;};
const unsigned MaxRows=64,MaxChoices=32;RowContext contexts[MaxRows];unsigned rowCount=0;
#ifdef KEO_DEBUG
const bool Dev=true;
#else
const bool Dev=false;
#endif
const char*Text(const char*english){return platformReady&&IsMainThread()?KeoTr(english):english;}
void ErrorLogSafe(const char*text){try{LogError(std::string("MCM: ")+text);}catch(...){}}
int Error(char*out,unsigned size,const char*text,bool log=true){const char*translated=text;try{translated=Text(text);}catch(...){}if(out&&size)strncpy_s(out,size,translated,_TRUNCATE);if(log)ErrorLogSafe(text);return 1;}
size_t Width(const ConfigKey&k){switch(k.kind){case CK_BOOL:return sizeof(bool);case CK_INT:return sizeof(int);case CK_FLOAT:return sizeof(float);case CK_DOUBLE:return sizeof(double);default:return k.size;}}
bool Meta(int m,int i,const ConfigModule**mod,const ConfigKey**key){return RequestedConfig().Key(m,i,mod,key);}
bool Context(void*u,const ConfigModule**m,const ConfigKey**k,RowContext**c){uintptr_t p=reinterpret_cast<uintptr_t>(u),base=reinterpret_cast<uintptr_t>(contexts);if(p<base||p>=base+rowCount*sizeof(RowContext)||(p-base)%sizeof(RowContext))return false;*c=contexts+(p-base)/sizeof(RowContext);return Meta((*c)->module,(*c)->key,m,k);}
bool Slot(const ConfigKey&k){return k.target||k.kind==CK_CUSTOM||k.kind==CK_DOUBLE||(k.kind==CK_INT&&!k.choices);}
int IntValue(const ConfigKey&k,int i,const ConfigModuleStage&s){if(Slot(k)){if(k.kind==CK_BOOL)return s.slots[i].b?1:0;return k.kind==CK_INT&&!k.choices?int(s.slots[i].f):s.slots[i].i;}const char*p=reinterpret_cast<const char*>(s.state)+k.offset;return k.kind==CK_BOOL?(*reinterpret_cast<const bool*>(p)?1:0):*reinterpret_cast<const int*>(p);}
float FloatValue(const ConfigKey&k,int i,const ConfigModuleStage&s){return Slot(k)?s.slots[i].f:*reinterpret_cast<const float*>(reinterpret_cast<const char*>(s.state)+k.offset);}
void SetInt(const ConfigKey&k,int i,ConfigModuleStage&s,int value){if(Slot(k)){if(k.kind==CK_BOOL)s.slots[i].b=value!=0;else if(k.kind==CK_INT&&!k.choices)s.slots[i].f=float(value);else s.slots[i].i=value;}else{char*p=reinterpret_cast<char*>(s.state)+k.offset;if(k.kind==CK_BOOL)*reinterpret_cast<bool*>(p)=value!=0;else *reinterpret_cast<int*>(p)=value;}}
void SetFloat(const ConfigKey&k,int i,ConfigModuleStage&s,float value){if(Slot(k))s.slots[i].f=value;else *reinterpret_cast<float*>(reinterpret_cast<char*>(s.state)+k.offset)=value;}
bool Fits(const ConfigModule&m,const ConfigKey&k){size_t n=Width(k);return n&&m.state&&!k.target&&k.offset<=m.stateSize&&n<=m.stateSize-k.offset;}
bool Named(const std::vector<IniEntry>&v,const char*name){for(size_t i=0;i<v.size();++i)if(v[i].key==name)return true;return false;}
void Entries(const SettingsStaging&a,const SettingsStaging&b,std::vector<IniEntry>&out){for(int m=0;m<kConfigModuleCount&&m<CONFIG_MODULE_MAX;++m)ModuleStageEntries(kConfigModules[m],a.module[m],b.module[m],&out);}
// Copy only explicit edited fields. A second frontend may have committed other
// preferences since baseline was opened; those newer values must survive.
bool Merge(const SettingsStaging&from,SettingsStaging&to,const std::vector<IniEntry>&keys){for(int m=0;m<kConfigModuleCount;++m)for(int i=0;i<CONFIG_STAGE_MAX&&kConfigModules[m].keys[i].name;++i){const ConfigKey&k=kConfigModules[m].keys[i];if(!Named(keys,k.name))continue;if(k.retired)return false;if(k.target)to.module[m].slots[i]=from.module[m].slots[i];else{if(!Fits(kConfigModules[m],k))return false;memcpy(reinterpret_cast<char*>(to.module[m].state)+k.offset,reinterpret_cast<const char*>(from.module[m].state)+k.offset,Width(k));to.module[m].slots[i]=from.module[m].slots[i];}}return true;}
struct CommitScope {CommitScope(){committing=true;}~CommitScope(){committing=false;}};
int __cdecl GetInt(void*u,int*out){try{const ConfigModule*m;const ConfigKey*k;RowContext*c;if(!out||!IsMainThread()||!Context(u,&m,&k,&c))return 1;ConfigModuleStage s;if(!RequestedConfig().CopyModule(c->module,&s))return 1;*out=IntValue(*k,c->key,s);return 0;}catch(...){return 1;}}
int __cdecl GetFloat(void*u,float*out){try{const ConfigModule*m;const ConfigKey*k;RowContext*c;if(!out||!IsMainThread()||!Context(u,&m,&k,&c))return 1;ConfigModuleStage s;if(!RequestedConfig().CopyModule(c->module,&s))return 1;float v=FloatValue(*k,c->key,s);if(!_finite(v))return 1;*out=v;return 0;}catch(...){return 1;}}
int __cdecl PutInt(void*u,int value,char*err,unsigned size){try{const ConfigModule*m;const ConfigKey*k;RowContext*c;if(!IsMainThread()||!Context(u,&m,&k,&c))return Error(err,size,"Settings are not ready.");if(k->kind==CK_BOOL&&(value<0||value>1))return Error(err,size,"Invalid setting value.");if(k->kind!=CK_BOOL&&k->kind!=CK_INT&&k->kind!=CK_CUSTOM)return Error(err,size,"Invalid setting value.");SettingsStaging old;if(!RequestedConfig().Copy(&old))return Error(err,size,"Settings are not ready.");if(k->choices){bool valid=false;for(int i=0;i<k->choiceCount;++i)if(k->choices[i].value==value&&(c->choiceMask&(1u<<i)))valid=true;if(!valid)return Error(err,size,"Invalid setting value.");}SettingsStaging next=old;SetInt(*k,c->key,next.module[c->module],value);return KeoMcmCommitStaging(next,old,err,size);}catch(...){return Error(err,size,"The setting could not be changed.");}}
int __cdecl PutFloat(void*u,float value,char*err,unsigned size){try{const ConfigModule*m;const ConfigKey*k;RowContext*c;if(!_finite(value)||!IsMainThread()||!Context(u,&m,&k,&c)||(k->kind!=CK_FLOAT&&k->kind!=CK_DOUBLE))return Error(err,size,"Invalid setting value.");SettingsStaging old;if(!RequestedConfig().Copy(&old))return Error(err,size,"Settings are not ready.");SettingsStaging next=old;SetFloat(*k,c->key,next.module[c->module],value);return KeoMcmCommitStaging(next,old,err,size);}catch(...){return Error(err,size,"The setting could not be changed.");}}
}
void KeoMcmSetPlatformReady(bool ready){platformReady=ready;if(!ready)runtimeReady=false;KeoSetTranslationReady(ready);}
void KeoMcmSetRuntimeReady(bool ready){runtimeReady=ready&&platformReady;}
bool KeoMcmRuntimeReady(){return runtimeReady;}
bool KeoMcmAvailable(){HMODULE m=GetModuleHandleA("ModConfigMenu.dll");return m&&GetProcAddress(m,"MCM_OpenPage")!=NULL;}

int KeoMcmCommitStaging(const SettingsStaging&edited,const SettingsStaging&baseline,char*err,unsigned size){
 bool saved=false;
 try{
  if(!IsMainThread()||committing)return Error(err,size,"Settings are not ready.");CommitScope scope;
  SettingsStaging current;if(!RequestedConfig().Copy(&current))return Error(err,size,"Settings are not ready.");
  std::vector<IniEntry>intent;Entries(edited,baseline,intent);if(intent.empty())return 0;
  // ClampRenderValues appends finite-range/fallback notes. Own its required
  // buffer on both passes; each pass is bounded by the static render table.
  std::vector<std::string> renderNotes;
  SettingsStaging clamped=edited;ClampSettings(&clamped,baseline,StagedRender(const_cast<SettingsStaging*>(&baseline)),NULL,&renderNotes);
  SettingsStaging next=current;if(!Merge(clamped,next,intent))return Error(err,size,"Invalid setting metadata.");
  if(runtimeReady&&Named(intent,"zoneLifeSquadRadius"))ApplySettingsFloor(&next,KeoMcmRuntimeFloor(),NULL);
  renderNotes.clear();ClampSettings(&next,current,StagedRender(&current),NULL,&renderNotes);
  // ClampSettings may normalize an unrelated render field. Publish only edits.
  SettingsStaging only=current;if(!Merge(next,only,intent))return Error(err,size,"Invalid setting metadata.");next=only;
  std::vector<IniEntry>entries;Entries(next,current,entries);if(entries.empty())return 0;
  // Native fallback widgets normally supply legal choices too. Validate the
  // shared commit boundary so a malformed stage cannot bypass callback checks.
  for(int m=0;m<kConfigModuleCount;++m)for(int i=0;i<CONFIG_STAGE_MAX&&kConfigModules[m].keys[i].name;++i){const ConfigKey&k=kConfigModules[m].keys[i];if(k.choices&&Named(entries,k.name)){
   int value=IntValue(k,i,next.module[m]);bool valid=false;
   for(int c=0;c<k.choiceCount;++c)if(k.choices[c].value==value)valid=true;
   if(!valid)return Error(err,size,"Invalid setting value.");
  }}
  // Preflight aligned live writes before touching the file.
  for(int m=0;m<kConfigModuleCount;++m)for(int i=0;i<CONFIG_STAGE_MAX&&kConfigModules[m].keys[i].name;++i){const ConfigKey&k=kConfigModules[m].keys[i];if(runtimeReady&&k.live&&Named(entries,k.name)){if(!Fits(kConfigModules[m],k))return Error(err,size,"Invalid setting metadata.");size_t n=Width(k);uintptr_t p=reinterpret_cast<uintptr_t>(kConfigModules[m].state)+k.offset;if((n!=1&&n!=4&&n!=8)||(p&(n-1)))return Error(err,size,"Invalid setting metadata.");}}
  if(!SaveIniEntries(entries,NULL,"MCM"))return Error(err,size,"The settings file could not be saved.");saved=true;
  if(!PublishRequestedSettings(next))return Error(err,size,"The settings were saved, but could not be applied until restart.");
  // Debug is our own cached logger, safe even when native platform admission
  // is refused. Other rows never call native apply before the gate is ready.
  const ConfigModule*debugModule=NULL;const ConfigKey*debug=FindConfigKey("Debug",&debugModule);
  if(debug&&Named(entries,"Debug")){int m=int(debugModule-kConfigModules);bool on=*reinterpret_cast<const bool*>(reinterpret_cast<const char*>(next.module[m].state)+debug->offset);keo_gui::g_guiCfg.debugLogging=on;SetDetailedLogEnabled(on);}
  if(runtimeReady){
   int render=RenderModuleIndex();RenderConfig renderNext=g_renderCfg;bool renderChanged=false;
   for(int m=0;m<kConfigModuleCount;++m)for(int i=0;i<CONFIG_STAGE_MAX&&kConfigModules[m].keys[i].name;++i){const ConfigKey&k=kConfigModules[m].keys[i];if(!k.live||!Named(entries,k.name))continue;
    if(m==render){memcpy(reinterpret_cast<char*>(&renderNext)+k.offset,reinterpret_cast<const char*>(next.module[m].state)+k.offset,Width(k));renderChanged=true;}
    else{ConfigKey one[2]={k,{}};ConfigModule filtered=kConfigModules[m];filtered.keys=one;ApplyLiveModuleRows(filtered,next.module[m],NULL);}
   }if(renderChanged)ApplyRenderConfig(renderNext);
  }return 0;
 }catch(...){return Error(err,size,saved?"The settings were saved, but could not be applied until restart.":"The setting could not be changed.");}
}

void KeoMcmDescribe(MCM_Api*api){try{
 if(!IsMainThread()||!api||api->version<3||!api->beginMod||!api->section||!api->toggleFn||!api->integerFn||!api->choiceFn||!api->numberFn)return;
 api->beginMod(api,"KEO","KEO: Kenshi Engine Optimizations",NULL,NULL);
 if(!RequestedConfig().Ready()){if(api->info)api->info(api,Text("Settings are not ready."));return;}
 if(!runtimeReady&&api->info)api->info(api,Text("Settings are saved for the next start. Runtime changes are unavailable on this build."));
 // Validate all static contexts before registering any setting callbacks.
 unsigned prepared=0;
 for(;prepared<MaxRows&&keo_gui::kSettingsPlaces[prepared].section;++prepared){
  const keo_gui::SettingsPlace&p=keo_gui::kSettingsPlaces[prepared];const ConfigModule*m=NULL;const ConfigKey*k=FindConfigKey(p.key,&m);
  if(!k||!m||!m->defaults||strcmp(m->name,p.module)||!SettingsKeyShown(*k,false)||!Fits(*m,*k)||k->choiceCount<0||k->choiceCount>int(MaxChoices)||(k->kind!=CK_BOOL&&k->kind!=CK_INT&&k->kind!=CK_FLOAT&&k->kind!=CK_DOUBLE&&!(k->kind==CK_CUSTOM&&k->choices)))throw 1;
  RowContext&c=contexts[prepared];c.module=int(m-kConfigModules);c.key=int(k-m->keys);c.choiceMask=0;
  ConfigModuleStage current;if(!RequestedConfig().CopyModule(c.module,&current))throw 1;
  for(int i=0;i<k->choiceCount;++i)if(Dev||!k->choices[i].devOnly||k->choices[i].value==IntValue(*k,c.key,current))c.choiceMask|=1u<<i;
 }
 if(prepared==MaxRows)throw 1;rowCount=prepared;const char*section=NULL;
 for(unsigned p=0;p<prepared;++p){const keo_gui::SettingsPlace&place=keo_gui::kSettingsPlaces[p];const ConfigModule*m=NULL;const ConfigKey*k=FindConfigKey(place.key,&m);if(!k||!m||!SettingsKeyShown(*k,false)||rowCount==MaxRows||k->choiceCount>int(MaxChoices))throw 1;int module=int(m-kConfigModules),index=int(k-m->keys);const ConfigModule*checked;const ConfigKey*row;if(!Meta(module,index,&checked,&row)||!Fits(*m,*k))throw 1;
  RowContext&c=contexts[p];c.module=module;c.key=index;
  if(!section||strcmp(section,place.section)){section=place.section;api->section(api,Text(section));}
  std::string tip=Text(k->tooltip);
  if(place.group){for(unsigned first=0;first<p;++first)if(keo_gui::kSettingsPlaces[first].group&&!strcmp(keo_gui::kSettingsPlaces[first].group,place.group)){const ConfigKey*head=FindConfigKey(keo_gui::kSettingsPlaces[first].key,NULL);std::string note=Text("Works only while \"%s\" is on.");size_t at=note.find("%s");if(at!=std::string::npos)note.replace(at,2,Text(head->label));tip+=(tip.empty()?"":" ")+note;break;}}
  unsigned flags=k->live?0:MCM_RESTART;char def[64]={0};const char*base=reinterpret_cast<const char*>(m->defaults)+k->offset;
  if(k->choices){int values[MaxChoices],n=0,current=0;const char*labels[MaxChoices];if(GetInt(&c,&current))throw 1;for(int i=0;i<k->choiceCount;++i)if(c.choiceMask&(1u<<i)){values[n]=k->choices[i].value;labels[n]=Text(k->choices[i].label);++n;}api->choiceFn(api,Text(k->label),tip.c_str(),GetInt,PutInt,&c,values,labels,n,flags);int v=k->kind==CK_CUSTOM?(k->size==1?(*reinterpret_cast<const bool*>(base)?1:0):*reinterpret_cast<const int*>(base)):*reinterpret_cast<const int*>(base);sprintf_s(def,"%d",v);}
  else if(k->kind==CK_BOOL){api->toggleFn(api,Text(k->label),tip.c_str(),GetInt,PutInt,&c,flags);sprintf_s(def,"%d",*reinterpret_cast<const bool*>(base)?1:0);}
  else if(k->kind==CK_INT){api->integerFn(api,Text(k->label),tip.c_str(),GetInt,PutInt,&c,int(k->lo),int(k->hi),flags);sprintf_s(def,"%d",*reinterpret_cast<const int*>(base));}
  else{api->numberFn(api,Text(k->label),tip.c_str(),GetFloat,PutFloat,&c,k->lo,k->hi,k->stepExp,flags);sprintf_s(def,"%.17g",k->kind==CK_DOUBLE?*reinterpret_cast<const double*>(base):double(*reinterpret_cast<const float*>(base)));}
  if(api->version>=5&&api->defaultValue)api->defaultValue(api,def);
 }
 }catch(...){ErrorLogSafe("MCM page metadata could not be registered");}}
extern "C" __declspec(dllexport)void MCM_Describe(MCM_Api*api){KeoMcmDescribe(api);}
