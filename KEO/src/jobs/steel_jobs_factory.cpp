#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "jobs/steel_jobs_factory.h"
#include "game/gog_jobs_profile.h"
#include "game/native_profile.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include <kenshi/Tasker.h>
#include "base/klib_include_end.h"
#include <stddef.h>
#include <string.h>

static_assert(offsetof(Tasker,subject)==0x10,"Tasker subject layout");
static_assert(offsetof(Tasker,taskData)==0x70,"Tasker metadata layout");
static_assert(sizeof(Tasker)==0x78,"Tasker native base size");
static_assert(offsetof(TaskData,results)==0x50,"TaskData results layout");
static_assert(offsetof(TaskData,requirements)==0xAC,"TaskData requirements layout");
static_assert(offsetof(TaskData,hasAction)==0x134,"TaskData action layout");
static_assert(sizeof(TaskStateData)==8,"Native state record layout");
namespace keo_jobs {
namespace {
struct Slot {int type;TaskData*data;uintptr_t table;void*privateTable[9];FactoryBody body,nativeBody;volatile LONG configured,faultReported;};
static Slot slots[3];static uintptr_t base=0;static bool gog=false,prepared=false,installed=false;
static void(*logger)(const char*)=0;
typedef Tasker*(*CreateFn)(void*,int,const hand&,int,float,Ogre::Vector3,int,int);
static CreateFn original=0;
static SRWLOCK templateLock=SRWLOCK_INIT;
typedef TaskData*(*GetDataFn)(int);
// MSVC x64 passes the native 24-byte general PMF by reference. Keeping it an
// explicit storage object avoids changing its representation with AI headers.
struct ScorePmf {unsigned char bytes[24];};
typedef TaskData*(*RegisterFn)(int,const ScorePmf*,bool,bool,const std::string*,unsigned);
typedef void(*PermaFn)(TaskData*,int,bool,int,int);
typedef void(*RequirementFn)(TaskData*,int,bool,bool);
static uintptr_t Address(JobsProfile::Key k){uintptr_t r=JobsProfile::Rva(gog,k);return r&&base<=~uintptr_t(0)-r?base+r:0;}
static Slot* Find(int type){for(unsigned i=0;i<3;++i)if(slots[i].type==type)return &slots[i];return 0;}
static bool Read(void*ctx,uintptr_t rva,void*out,size_t count){uintptr_t b=(uintptr_t)ctx;
 if(!out||!count||rva>0x232C000||count>0x232C000-rva||b>~uintptr_t(0)-rva)return false;
 __try{memcpy(out,(const void*)(b+rva),count);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void Report(const char*message){if(logger)logger(message);}
// Native lookup/validation is outside our short publication lock. After one
// successful publication the hot path reads only the interlocked ready flag.
static bool PrepareSlot(Slot*slot){
 if(!slot||!slot->data)return false;
 if(InterlockedCompareExchange(&slot->configured,0,0))return true;
 int nativeType=slot->type==291?(int)COLLECT_OUTPUT_RESOURCE:(int)MOVE_CUS_ORDERED;
 const TaskData*source=((GetDataFn)Address(JobsProfile::GET_DATA))(nativeType);
 if(!source)return false;
 if(source->key!=(TaskType)nativeType||source->numResults<0||source->numResults>11||source->numRequirements<0||source->numRequirements>11
    ||(slot->type==291&&source->numRequirements>=11)){
  if(!InterlockedCompareExchange(&slot->faultReported,1,0))Report("KEO jobs: native GOAP template shape refused; custom task not created");
  return false;
 }
 if(!source->hasAction||!source->numResults||!source->numRequirements)return false;
 const int results=source->numResults,requirements=source->numRequirements;
 unsigned char resultBytes[88],requirementBytes[88];
 memcpy(resultBytes,source->results,sizeof(resultBytes));memcpy(requirementBytes,source->requirements,sizeof(requirementBytes));
 AcquireSRWLockExclusive(&templateLock);
 // POD writes only: no game call, logger, allocator or callback under this lock.
 if(!InterlockedCompareExchange(&slot->configured,0,0)){
  TaskData*data=slot->data;data->hasAction=true;
  if(slot->type!=293){data->numResults=results;memcpy(data->results,resultBytes,sizeof(resultBytes));
   data->numRequirements=requirements;memcpy(data->requirements,requirementBytes,sizeof(requirementBytes));}
  if(slot->type==291){
   // Exact native addRequirement success path (Steam60DF26..60DF4B):
   // requirements[n].key/val/progressionOnly, then count++. Bounds above.
   TaskStateData*entry=&data->requirements[data->numRequirements];entry->key=MACHINES_OUTPUTS_EMPTY;entry->val=false;entry->progressionOnly=false;++data->numRequirements;
  }
  InterlockedExchange(&slot->configured,1);
 }
 ReleaseSRWLockExclusive(&templateLock);return true;
}
static Tasker* Create(void*self,int type,const hand&subject,int priority,float weight,Ogre::Vector3 location,int start,int end){
 Slot*slot=Find(type);if(!slot)return original(self,type,subject,priority,weight,location,start,end);
 if(!slot->data||!slot->body||!self||!PrepareSlot(slot))return 0;
 // Respect the original repertoire's custom-id capability before aliasing.
 typedef bool(*HasTask)(void*,int);void**vptr=*(void***)self;
 if(!vptr||!((HasTask)vptr[1])(self,type))return 0;
 int nativeType=type==291?(int)COLLECT_OUTPUT_RESOURCE:type==292?(int)MOVE_CUS_ORDERED:type;
 Tasker*task=original(self,nativeType,subject,priority,weight,location,start,end);
 if(!task)return 0;
 // The native factory owns allocation, constructor, exception cleanup and all
 // common argument initialization. Refuse a different native derived class.
 if(*(uintptr_t*)task!=slot->table){
  typedef void(*DeleteFn)(void*,unsigned);void**table=*(void***)task;
  ((DeleteFn)table[0])(task,1);return 0;
 }
 task->taskData=slot->data;
 if(type==291)*((unsigned char*)task+0x78)=1;
 *(void***)task=&slot->privateTable[1];return task;
}
}
bool InitializeFactory(uintptr_t imageBase,const FactoryCallbacks&cb,void(*log)(const char*)){
 if(prepared)return imageBase==base&&slots[0].body==cb.gather&&slots[1].body==cb.waypoint&&slots[2].body==cb.stock;
 if(!imageBase||!cb.gather||!cb.waypoint||!cb.stock)return false;
 bool selectedGog=NativeProfileIsGog();
 if(!JobsProfile::Validate(selectedGog,imageBase,&Read,(void*)imageBase)){if(log)log("KEO jobs: native task contract refused; custom jobs disabled");return false;}
 base=imageBase;gog=selectedGog;logger=log;
 const JobsProfile::Key keys[3]={JobsProfile::EMPTY_VTABLE,JobsProfile::MOVE_VTABLE,JobsProfile::BLANK_VTABLE};
 const FactoryBody bodies[3]={cb.gather,cb.waypoint,cb.stock};
 for(unsigned i=0;i<3;++i){slots[i].type=291+i;slots[i].table=Address(keys[i]);slots[i].body=bodies[i];
  memcpy(slots[i].privateTable,(const void*)(slots[i].table-8),JobsProfile::TableCount(keys[i])*sizeof(void*));
  slots[i].nativeBody=(FactoryBody)slots[i].privateTable[3];slots[i].privateTable[3]=(void*)bodies[i];}
 prepared=true;return true;
}
bool RegisterFactoryTask(int type,const std::string&caption){
 Slot*slot=Find(type);if(!prepared||!slot||caption.empty())return false;if(slot->data)return true;
 GetDataFn get=(GetDataFn)Address(JobsProfile::GET_DATA);
 TaskData*existing=get(type);if(existing){Report("KEO jobs: custom TaskType id already owned; registration refused");return false;}
 // The same generic AI virtual-score thunk as QOL's 0x215E4 jump target,
 // not a specialized COLLECT score which may reject the new TaskType.
 ScorePmf score;memset(score.bytes,0,sizeof(score.bytes));uintptr_t scoreAddress=Address(JobsProfile::DEFAULT_SCORE);memcpy(score.bytes,&scoreAddress,sizeof(scoreAddress));
 TaskData*data=((RegisterFn)Address(JobsProfile::REGISTER_DATA))(type,&score,false,true,&caption,1);
 if(!data||data->key!=(TaskType)type)return false;
 ((PermaFn)Address(JobsProfile::SET_PERMAJOB))(data,PERMAJOB_NORMAL,true,0,0);
 // Native ctor leaves hasAction false until world-ready template publication.
 if(type==293)((RequirementFn)Address(JobsProfile::ADD_REQUIREMENT))(data,AT_LOCATION,true,false);
 if(type!=291)data->_resultsAlwaysFalse=true;
 slot->data=data;return true;
}
bool PrepareFactoryTasks(){
 if(!prepared||!slots[0].data||!slots[1].data||!slots[2].data)return false;
 bool ok=true;for(unsigned i=0;i<3;++i)if(!PrepareSlot(&slots[i]))ok=false;return ok;
}
bool InstallFactory(){
 if(installed)return true;if(!prepared||!slots[0].data||!slots[1].data||!slots[2].data)return false;
 // Revalidate immediately before publication; a competing custom factory patch
 // or overwritten vtable is not silently chained or overwritten.
 if(!JobsProfile::ValidateForInstall(gog,base,&Read,(void*)base))return false;
 if(KenshiLib::AddHook((void*)Address(JobsProfile::FACTORY),(void*)&Create,(void**)&original)!=KenshiLib::SUCCESS){Report("KEO jobs: native factory hook failed");return false;}
 installed=true;return true;
}
void*FactoryTaskData(int type){Slot*s=Find(type);return s?s->data:0;}
FactoryBody FactoryNativeBody(int type){Slot*s=Find(type);return s?s->nativeBody:0;}
uintptr_t FactoryCaptionAddress(){return prepared?Address(JobsProfile::CAPTION):0;}
uintptr_t FactoryContextMenuAddress(){return prepared?Address(JobsProfile::CONTEXT_SHOW):0;}
}
