"""Compile unchanged production factory with owned native boundaries; never run game code."""
from pathlib import Path
import os,subprocess,sys
RED='--red-control'in sys.argv
OLD='--old-vtable-control'in sys.argv
FIXED_RED='--old-fixed-target-control'in sys.argv
R=Path('D:/DEV/Kenshi/forks/KEO');O=R/('build/steel-jobs-factory-old-vtable'if OLD else'build/steel-jobs-factory-red'if RED else'build/steel-jobs-factory');O.mkdir(parents=True,exist_ok=True)

# Frozen actual native loop and hand comparator; keeps the model tied to this EXE.
import hashlib
sys.path.insert(0,'D:/DEV/Kenshi/ours/SkillLevelUpInfo/tests/_tools')
import pefile
_native=Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
assert hashlib.sha256(_native.read_bytes()).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
_pe=pefile.PE(str(_native),fast_load=True)
assert _pe.get_data(5274976,66).hex()=='488b8398000000488b0c06488b41703968447520807808000f8449010000488b41104883c110498bd4ff500884c00f8533010000ffc74883c6083bbb9000000072be'
assert _pe.get_data(839712,60).hex()=='448b42084183f80b7509443941087503b001c38b410c39420c751e8b411439421475168b4118394218750e8b421039411075064439410874d732c0c3'
assert _pe.get_data(3242784,19).hex()=='8b442428895104448841088941104489490cc3'

code=r'''
#define KENSHILIB_EXPORT
#include "jobs/steel_jobs_factory.cpp"
#include <stdio.h>
#include <stdlib.h>
static int checks=0,calls=0,deleted=0,registrations=0,hasCalls=0;
static bool allowCustom=true,allowBase=true,wrongClass=false,failNative=false,failRegister=false,badFactory=false,templateReady=true,reenterOnGet=false;
static int lastType=0;static uintptr_t realBase=1;
static unsigned long long dataBytes[5][64],taskBytes[32];static TaskData*registry[3]={0};
static void*vtables[3][9];
static void C(bool x,const char*s){++checks;if(!x){printf("FAIL %s\n",s);exit(1);}}
static TaskData*Data(unsigned n){return (TaskData*)dataBytes[n];}
static void Delete(void*,unsigned flag){C(flag==1,"original deleting destructor flag");++deleted;}
static void Body(void*,void*){}
static int extendedCalls=0;
static void Extra5(void*,void*){++extendedCalls;}
static void Extra6(void*,void*){extendedCalls+=10;}
static void CallbackG(void*,void*){} static void CallbackM(void*,void*){} static void CallbackS(void*,void*){}
static bool Has(void*,int type){++hasCalls;return type>=291?allowCustom:allowBase;}
static TaskData*Get(int type){if(reenterOnGet&&type==(int)COLLECT_OUTPUT_RESOURCE){reenterOnGet=false;C(keo_jobs::PrepareFactoryTasks(),"same-slot native lookup reentry without our lock");}if(type>=291&&type<=293)return registry[type-291];if(type==(int)COLLECT_OUTPUT_RESOURCE)return templateReady?Data(3):0;if(type==(int)MOVE_CUS_ORDERED)return templateReady?Data(4):0;return 0;}
static void Score(){}
static TaskData*Reg(int type,const keo_jobs::ScorePmf*p,bool func,bool target,const std::string*caption,unsigned flags){
 ++registrations;C(!func&&target&&flags==1&&caption&&!caption->empty(),"native registration flags preserved");
 uintptr_t score=0;memcpy(&score,p->bytes,8);C(score==(uintptr_t)&Score,"generic AI score address, not specialized template");
 for(unsigned i=8;i<24;++i)C(!p->bytes[i],"general PMF adjustment zero");
 if(failRegister)return 0;TaskData*d=Data(type-291);memset(d,0,sizeof(TaskData));d->key=(TaskType)type;registry[type-291]=d;return d;
}
static void Perma(TaskData*d,int mode,bool fixed,int associated,int secondary){C(mode==1&&!associated&&!secondary,"native permajob setter args");d->permaJob=(PermajobType)mode;d->permaJob_FixedTarget=fixed;}
static void Req(TaskData*d,int key,bool value,bool progression){C(d->numRequirements<11,"requirement capacity before native mutation");TaskStateData*r=&d->requirements[d->numRequirements++];r->key=(StateType)key;r->val=value;r->progressionOnly=progression;}
static Tasker*Native(void*self,int type,const hand&subject,int priority,float weight,Ogre::Vector3 location,int start,int end){
 ++calls;lastType=type;if(failNative||!Has(self,type))return 0;memset(taskBytes,0,sizeof(taskBytes));Tasker*t=(Tasker*)taskBytes;
 unsigned i=type==(int)COLLECT_OUTPUT_RESOURCE?0:type==(int)MOVE_CUS_ORDERED?1:2;
 *(void***)t=&vtables[wrongClass?(i+1)%3:i][1];t->priority=(taskPriority)priority;t->weight=weight;t->location=location;t->startTime=start;t->endTime=end;t->taskData=Get(type);memcpy(&t->subject,&subject,sizeof(hand));return t;
}
namespace JobsProfile {
uintptr_t Rva(bool,Key k){uintptr_t a=0;switch(k){case FACTORY:a=(uintptr_t)&Native;break;case GET_DATA:a=(uintptr_t)&Get;break;case REGISTER_DATA:a=(uintptr_t)&Reg;break;case SET_PERMAJOB:a=(uintptr_t)&Perma;break;case ADD_REQUIREMENT:a=(uintptr_t)&Req;break;case DEFAULT_SCORE:a=(uintptr_t)&Score;break;case EMPTY_VTABLE:a=(uintptr_t)&vtables[0][1];break;case MOVE_VTABLE:a=(uintptr_t)&vtables[1][1];break;case BLANK_VTABLE:a=(uintptr_t)&vtables[2][1];break;default:a=(uintptr_t)&Body;}return a-realBase;}
unsigned TableCount(Key k){return k==EMPTY_VTABLE?7:k==MOVE_VTABLE?8:k==BLANK_VTABLE?6:0;}
bool Validate(bool,uintptr_t,Reader,void*){return !badFactory;}
bool ValidateForInstall(bool,uintptr_t,Reader,void*){return !badFactory;}
}
bool NativeProfileIsGog(){return false;}
namespace KenshiLib {
intptr_t GetRealAddress(void*){return 0;}
HookStatus AddHook(void*target,void*detour,void**orig){C(target==(void*)&Native&&detour!=0,"full native factory entry hook");*orig=(void*)&Native;return SUCCESS;}
HookStatus QueueHook(void*,void*,void**){return FAIL;}HookStatus ApplyQueuedHooks(){return FAIL;}
}
static void Reset(){using namespace keo_jobs;memset(slots,0,sizeof(slots));base=0;prepared=installed=false;original=0;memset(registry,0,sizeof(registry));memset(dataBytes,0,sizeof(dataBytes));calls=deleted=registrations=hasCalls=0;allowCustom=allowBase=true;wrongClass=failNative=failRegister=badFactory=false;templateReady=true;
 for(unsigned i=0;i<3;++i){vtables[i][0]=(void*)(uintptr_t)(0x1000+i);vtables[i][1]=(void*)&Delete;for(unsigned j=2;j<6;++j)vtables[i][j]=(void*)&Body;vtables[i][6]=(void*)&Extra5;vtables[i][7]=(void*)&Extra6;}
 Data(3)->key=COLLECT_OUTPUT_RESOURCE;Data(4)->key=MOVE_CUS_ORDERED;for(unsigned i=3;i<5;++i){Data(i)->hasAction=true;Data(i)->numRequirements=1;Data(i)->requirements[0].key=AT_LOCATION;Data(i)->requirements[0].val=true;Data(i)->numResults=1;Data(i)->results[0].key=AT_LOCATION;}
}
// Byte-pinned native admission model: Steam507D60..507DA2 checks existing
// key then +8; false rejects by type, true compares all five hand words.
// Native hand==CD020 special-cases two NULL_ITEM hands; no game code executes.
struct NativeJobRow{TaskData*data;unsigned words[5];};
static bool NativePermanentAdmission(NativeJobRow*rows,unsigned&count,TaskData*data,const hand&subject){
 unsigned words[5];memcpy(words,(const char*)&subject+8,20);
 for(unsigned i=0;i<count;++i)if(rows[i].data->key==data->key){
  if(!rows[i].data->permaJob_FixedTarget)return false;
  if((rows[i].words[0]==11&&words[0]==11)||!memcmp(rows[i].words,words,20))return false;
 }
 C(count<4,"bounded native-admission fixture rows");rows[count].data=data;memcpy(rows[count].words,words,20);++count;return true;
}
static void DistinctTargets(){
 unsigned long long firstBytes[4]={},secondBytes[4]={};hand&first=*(hand*)firstBytes;hand&second=*(hand*)secondBytes;
 first.type=____XXX___;first.container=0x41200000;first.containerSerial=0;first.index=0x41A00000;first.serial=1;memcpy(&second,&first,sizeof(hand));second.serial=2;
 NativeJobRow rows[4]={};unsigned count=0;
 C(NativePermanentAdmission(rows,count,Data(1),first)&&count==1,"first waypoint admitted");
 C(NativePermanentAdmission(rows,count,Data(1),second)&&count==2,"second distinct waypoint admitted by native fixed-target branch");
 C(!NativePermanentAdmission(rows,count,Data(1),second)&&count==2,"exact duplicate waypoint hand rejected");
 for(unsigned i=0;i<3;i+=2){count=0;first.type=BUILDING;first.container=1;first.containerSerial=2;first.index=3;first.serial=4;memcpy(&second,&first,sizeof(hand));second.index=5;
  C(NativePermanentAdmission(rows,count,Data(i),first),"first output/counter subject admitted");
  C(NativePermanentAdmission(rows,count,Data(i),second)&&count==2,"distinct output/counter subject admitted");
  C(!NativePermanentAdmission(rows,count,Data(i),second)&&count==2,"same output/counter subject remains duplicate");
 }
 bool saved=Data(1)->permaJob_FixedTarget;Data(1)->permaJob_FixedTarget=false;count=0;
 C(NativePermanentAdmission(rows,count,Data(1),first),"native false-control first accepted");
 C(!NativePermanentAdmission(rows,count,Data(1),second)&&count==1,"native false-control rejects second by type");Data(1)->permaJob_FixedTarget=saved;
}
static bool Init(){keo_jobs::FactoryCallbacks cb={CallbackG,CallbackM,CallbackS};return keo_jobs::InitializeFactory(realBase,cb,0);}
static void RegisterAll(){for(int t=291;t<=293;++t)C(keo_jobs::RegisterFactoryTask(t,"job"),"register native job metadata");C(keo_jobs::InstallFactory(),"install after all metadata");}
int main(){using namespace keo_jobs;
 unsigned long long handBytes[4]={0x11111111,0x22222222,0x33333333,0x44444444};const hand&subject=*(hand*)handBytes;Ogre::Vector3 point(123,456,789);void*self[2]={(void*)&Delete,(void*)&Has};void*repertoire=self;
 Reset();templateReady=false;C(Init(),"early factory preflight");RegisterAll();C(!PrepareFactoryTasks(),"early native templates not ready");
 C(!Create(&repertoire,291,subject,7,3.25f,point,11,22)&&!calls,"no native task allocation before base template readiness");
 templateReady=true;reenterOnGet=true;for(int type=291;type<=293;++type){Tasker*t=Create(&repertoire,type,subject,7,3.25f,point,11,22);C(t&&t->taskData==FactoryTaskData(type),"saved custom rows configure before MainTick");}
 C(PrepareFactoryTasks()&&Data(0)->numRequirements==2,"late preparation idempotent, no duplicate gather requirement");
 Reset();C(!InstallFactory(),"no hook before prepare");badFactory=true;C(!Init(),"bad startup profile refuses");badFactory=false;C(Init(),"valid startup");C(!InstallFactory(),"no hook with missing metadata");
 registry[0]=Data(0);C(!RegisterFactoryTask(291,"g")&&!registrations,"foreign custom id never overwritten");registry[0]=0;
 Data(3)->numRequirements=11;RegisterAll();C(!PrepareFactoryTasks()&&!calls,"invalid source capacity refuses before native task allocation");Data(3)->numRequirements=1;C(PrepareFactoryTasks(),"valid template finalizes later");
 DistinctTargets();
 C(registrations==3,"exactly three bounded native data allocations");C(RegisterFactoryTask(291,"renamed")&&registrations==3,"idempotent registration no new allocations");C(InstallFactory(),"idempotent hook install");
 C(Data(0)->numRequirements==2&&Data(0)->requirements[1].key==MACHINES_OUTPUTS_EMPTY&&!Data(0)->requirements[1].val,"gather empty-output skip requirement");C(Data(1)->_resultsAlwaysFalse&&Data(2)->_resultsAlwaysFalse,"waypoint and stock own completion");C(Data(2)->numResults==0&&Data(2)->numRequirements==1&&Data(2)->requirements[0].key==AT_LOCATION,"stock walks to counter without native242 migration");

 for(int type=291;type<=293;++type){Tasker*t=Create(&repertoire,type,subject,7,3.25f,point,11,22);C(t&&t->taskData==FactoryTaskData(type),"custom metadata identity");C(lastType==(type==291?(int)COLLECT_OUTPUT_RESOURCE:type==292?(int)MOVE_CUS_ORDERED:293),"native construction alias");C(t->priority==7&&t->weight==3.25f&&t->startTime==11&&t->endTime==22&&!memcmp(&t->location,&point,sizeof(point))&&!memcmp(&t->subject,&subject,sizeof(hand)),"native common argument initialization preserved");C((*(void***)t)[2]==(void*)(type==291?CallbackG:type==292?CallbackM:CallbackS),"only native body virtual slot replaced");C(FactoryNativeBody(type)==Body,"native body retained");if(type==291)C(*((unsigned char*)t+0x78)==1,"native empty-machine take output flag");}
 Tasker*derived=Create(&repertoire,292,subject,7,3.25f,point,11,22);void**dv=*(void***)derived;
 C(dv[5]==(void*)&Extra5&&dv[6]==(void*)&Extra6,"complete derived move virtuals preserved, no callback/nativeBody alias");
 ((FactoryBody)dv[5])(derived,0);((FactoryBody)dv[6])(derived,0);C(extendedCalls==11,"native move derived slot5/6 dispatch without recursion");
 C(dv[7]==0,"move vtable tail stays zero, no adjacent RTTI copied");
 for(int type=291;type<=293;++type){Tasker*t=Create(&repertoire,type,subject,7,3.25f,point,11,22);Slot*s=Find(type);unsigned count=JobsProfile::TableCount((JobsProfile::Key)(JobsProfile::EMPTY_VTABLE+type-291));for(unsigned j=1;j<count;++j)if(j!=3)C(s->privateTable[j]==vtables[type-291][j],"every original class-specific virtual retained");}
 int before=calls;allowCustom=false;C(!Create(&repertoire,291,subject,1,1,point,0,0)&&calls==before,"custom repertoire refusal before alias");allowCustom=true;allowBase=false;C(!Create(&repertoire,291,subject,1,1,point,0,0),"native base capability refusal retained");allowBase=true;
 wrongClass=true;C(!Create(&repertoire,292,subject,1,1,point,0,0)&&deleted==1,"mismatched native class deleted exactly once");wrongClass=false;failNative=true;C(!Create(&repertoire,292,subject,1,1,point,0,0)&&deleted==1,"native null no destructor call");failNative=false;
 Create(&repertoire,242,subject,1,1,point,0,0);C(lastType==242,"native242 passed through unchanged");C(!FactoryTaskData(242)&&!FactoryNativeBody(242),"no ownership of unrelated task");printf("PASS %d unchanged production factory checks\n",checks);
}
'''
if OLD:
 production=(R/'src/jobs/steel_jobs_factory.cpp').read_text(encoding='utf8');changed=production.replace('void*privateTable[9]','void*privateTable[6]').replace('JobsProfile::TableCount(keys[i])*sizeof(void*)','sizeof(slots[i].privateTable)');assert changed!=production
 code=code.replace('#include "jobs/steel_jobs_factory.cpp"',changed)
if FIXED_RED:
 production=(R/'src/jobs/steel_jobs_factory.cpp').read_text(encoding='utf8');changed=production.replace('(data,PERMAJOB_NORMAL,true,0,0);','(data,PERMAJOB_NORMAL,false,0,0);');assert changed!=production
 code=code.replace('#include "jobs/steel_jobs_factory.cpp"',changed)
if RED:
 production=(R/'src/jobs/steel_jobs_factory.cpp').read_text(encoding='utf8');changed=production.replace('if(!vptr||!((HasTask)vptr[1])(self,type))return 0;','if(false)return 0;');assert changed!=production
 code=code.replace('#include "jobs/steel_jobs_factory.cpp"',changed)
(O/'factory.cpp').write_text(code,encoding='utf8')
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');S=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{V}/include;{S}/Include',LIB=f'{V}/lib/amd64;{S}/Lib/x64')
args=[str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2','/MD','/DBOOST_ALL_NO_LIB','/I'+str(R/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include/ogre','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(O/'factory.cpp'),'/Fo'+str(O/'factory.obj'),'/Fe'+str(O/'factory.exe')]
p=subprocess.run(args,env=env,capture_output=True,text=True);(O/'build.log').write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
p=subprocess.run([str(O/'factory.exe')],capture_output=True,text=True,timeout=10);print(p.stdout);(O/'run.log').write_text(p.stdout+p.stderr,encoding='utf8')
if FIXED_RED:assert p.returncode==1 and'second distinct waypoint admitted by native fixed-target branch'in p.stdout
elif OLD:assert p.returncode==1 and'complete derived move virtuals preserved'in p.stdout
elif RED:assert p.returncode==1 and'custom repertoire refusal before alias'in p.stdout
else:assert not p.returncode,p.stdout+p.stderr
