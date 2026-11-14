#define RESOURCE_LOAD_PROBE_TEST
#include <windows.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static bool enabled=true,onMain=true,throwsNative=false,sehNative=false;
static int clocks=0,mainChecks=0,nativeCalls=0;static void*seen=NULL;
static LONGLONG tick=0,nativeDuration=3000;static double seconds=0;
static std::string output;
static void*seenSecond=NULL;static bool seenForce=false;
bool DetailedLogEnabled(){return enabled;}
bool IsMainThread(){++mainChecks;return onMain;}
LONGLONG QpcNow(){++clocks;return tick;}
long QpcDeltaUs(LONGLONG a,LONGLONG b){return b<=a?0:(long)(b-a);}
double ElapsedSec(){return seconds;}
void LogMsg(const std::string&s){output+=s;output+='\n';}
void Native(void*p){++nativeCalls;seen=p;tick+=nativeDuration;if(sehNative)RaiseException(0xE0426E01,0,0,NULL);if(throwsNative)throw 17;}
void* NativeProcess(void*p,void*item){seenSecond=item;Native(p);return reinterpret_cast<void*>(0x4567);}
void NativeMesh(void*p,const void*data){seenSecond=const_cast<void*>(data);Native(p);}
void NativeEntity(void*p,bool force){seenForce=force;Native(p);}
static void*seenThird=NULL;
void* NativeTexture(void*p,void*out){seenSecond=out;Native(p);return out;}
void NativeAppearance(void*p,void*resource,void*data){seenSecond=resource;seenThird=data;Native(p);}
#include "../src/resources/resource_load_probe.cpp"
static bool CatchNativeSeh(void*p){__try{resource_load_probe_detail::Factory(p);}__except(GetExceptionCode()==0xE0426E01?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}return false;}
static bool CatchNewSeh(int which,void*p,void*item){__try{if(which==0)resource_load_probe_detail::FactoryProcess(p,item);else if(which==1)resource_load_probe_detail::SetMeshData(p,item);else resource_load_probe_detail::EntityInitialise(p,true);}__except(GetExceptionCode()==0xE0426E01?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}return false;}
static bool CatchCompletionSeh(int which,void*p,void*item){__try{if(which==0)resource_load_probe_detail::Texture2D(p,item);else if(which==1)resource_load_probe_detail::AppearanceBody(p,item,item);else resource_load_probe_detail::AppearanceAttachment(p,item,item);}__except(GetExceptionCode()==0xE0426E01?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}return false;}
static int checks=0;
void Check(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
static void ExerciseCompletionChildren(void*self,void*item){
 using namespace resource_load_probe_detail;
 originalTexture2D=&NativeTexture;originalBody=&NativeAppearance;originalAttachment=&NativeAppearance;
 void*data=reinterpret_cast<void*>(0x7777);int before=nativeCalls;nativeDuration=22000;
 Check(Texture2D(self,item)==item&&nativeCalls==before+1&&seen==self&&seenSecond==item,"texture hidden result output exact once");
 AppearanceBody(self,item,data);Check(seen==self&&seenSecond==item&&seenThird==data&&body.calls==1,"body raw indirect value and third argument exact");
 AppearanceAttachment(self,item,data);Check(seen==self&&seenSecond==item&&seenThird==data&&attachment.calls==1,"attachment exact forwarding");
 Check(texture2D.totalUs==22000&&body.totalUs==22000&&attachment.totalUs==22000,"completion children independent timings");
 enabled=false;int c=clocks,m=mainChecks;before=nativeCalls;
 Check(Texture2D(self,item)==item,"disabled texture result");AppearanceBody(self,item,data);AppearanceAttachment(self,item,data);
 __int64 token=ResourceLoadProbeArrayBegin();ResourceLoadProbeArrayEnd(token);
 Check(token==-1&&nativeCalls==before+3&&clocks==c&&mainChecks==m,"completion debug off has no clocks thread check or changed result");
 enabled=true;onMain=false;c=clocks;before=nativeCalls;
 Texture2D(self,item);AppearanceBody(self,item,data);AppearanceAttachment(self,item,data);token=ResourceLoadProbeArrayBegin();ResourceLoadProbeArrayEnd(token);
 Check(token==-1&&nativeCalls==before+3&&clocks==c,"completion offmain has no clocks");onMain=true;
 tick=0;token=ResourceLoadProbeArrayBegin();tick+=7000;ResourceLoadProbeArrayEnd(token);
 Check(token==0&&textureArray.calls==1&&textureArray.totalUs==7000,"array token zero is a valid timestamp");
 throwsNative=true;c=clocks;before=nativeCalls;bool thrown=false;
 try{Texture2D(self,item);}catch(int n){thrown=n==17;}Check(thrown&&nativeCalls==before+1&&clocks==c+1&&texture2D.calls==1,"texture exception exact and excluded");
 thrown=false;c=clocks;before=nativeCalls;try{AppearanceBody(self,item,data);}catch(int n){thrown=n==17;}
 Check(thrown&&nativeCalls==before+1&&clocks==c+1&&body.calls==1,"body exception exact and excluded");
 thrown=false;c=clocks;before=nativeCalls;try{AppearanceAttachment(self,item,data);}catch(int n){thrown=n==17;}
 Check(thrown&&nativeCalls==before+1&&clocks==c+1&&attachment.calls==1,"attachment exception exact and excluded");throwsNative=false;
 sehNative=true;for(int seam=0;seam<3;++seam){c=clocks;before=nativeCalls;Check(CatchCompletionSeh(seam,self,item)&&nativeCalls==before+1&&clocks==c+1&&texture2D.calls==1&&body.calls==1&&attachment.calls==1,"completion child native SEH unchanged and excluded");}sehNative=false;
}
static int ownedCopies=0,ownedDrops=0;static unsigned __int64 ownedSeen=0;
struct Owned16 {unsigned __int64 a,b;Owned16():a(0x1122334455667788ULL),b(0x8877665544332211ULL){}
 Owned16(const Owned16&o):a(o.a),b(o.b){++ownedCopies;}~Owned16(){++ownedDrops;}};
typedef void(*OwnedCallback)(void*,Owned16,void*);
static __declspec(noinline) void NativeOwned(void*p,Owned16 resource,void*data){ownedSeen=resource.a^resource.b;seenThird=data;Native(p);}
static void ExerciseOwnedValue(void*self,void*item){
 using namespace resource_load_probe_detail;
 originalBody=reinterpret_cast<AppearanceFn>(&NativeOwned);
 Owned16 source;int c=ownedCopies,d=ownedDrops;nativeDuration=3000;
 reinterpret_cast<OwnedCallback>(&AppearanceBody)(self,source,item);
 Check(ownedCopies==c+1&&ownedDrops==d+1&&source.a==0x1122334455667788ULL&&ownedSeen==(source.a^source.b)&&seenThird==item,"VC10 native indirect16 value is forwarded without extra copy or destructor");
 throwsNative=true;c=ownedCopies;d=ownedDrops;bool thrown=false;
 try{reinterpret_cast<OwnedCallback>(&AppearanceBody)(self,source,item);}catch(int n){thrown=n==17;}
 Check(thrown&&ownedCopies==c+1&&ownedDrops==d+1,"VC10 owned temporary unwinds exactly once across raw forwarding");throwsNative=false;
 originalBody=&NativeAppearance;
}
static void NativeCompletion(void*p){
 using namespace resource_load_probe_detail;
 nativeDuration=4000;Texture2D(p,p);nativeDuration=6000;AppearanceBody(p,p,p);
 __int64 token=ResourceLoadProbeArrayBegin();tick+=3000;ResourceLoadProbeArrayEnd(token);tick+=2000;
}
static void ExerciseSlowRing(void*self){
 using namespace resource_load_probe_detail;
 originalResource=&NativeCompletion;slowRead=slowCount=0;slowOverwritten=0;
 for(int i=0;i<20;++i){seconds=double(i);Resource(self);}
 Check(slowCount==16&&slowOverwritten==4&&slowUpdates[slowRead].completedAt==4,"slow update storage bounded and oldest overwritten");
 const SlowUpdate&r=slowUpdates[slowRead];
 Check(r.elapsedUs==15000&&r.children.tex2D==4000&&r.children.appearanceBody==6000&&r.children.array==3000&&r.children.entity==0,"slow native update captures exact completed child time deltas");
 int saved=slowCount;enabled=false;Resource(self);Check(slowCount==saved,"disabled update does not append diagnostic records");enabled=true;
 originalResource=&Native;seconds=0;
}
int main(){
 using namespace resource_load_probe_detail;
 originalFactory=&Native;originalResource=&Native;
 void*self=reinterpret_cast<void*>(0x1234);
 Factory(self);Check(nativeCalls==1&&seen==self,"factory forwards once and exact self");
 Check(factory.calls==1&&factory.totalUs==3000&&factory.maxUs==3000&&factory.over2ms==1&&factory.over20ms==0,"factory elapsed accounting");
 nativeDuration=25000;Resource(self);Check(nativeCalls==2&&resource.calls==1&&resource.totalUs==25000&&resource.over20ms==1,"resource native call and independent accounting");
 nativeDuration=1000;Factory(self);Check(factory.calls==2&&factory.totalUs==4000&&factory.maxUs==3000&&factory.over2ms==1,"sum max and thresholds aggregate");
 enabled=false;int c=clocks,m=mainChecks;Factory(self);Check(nativeCalls==4&&clocks==c&&mainChecks==m&&factory.calls==2,"disabled forwards with no timing or thread probe");
 enabled=true;onMain=false;Resource(self);Check(nativeCalls==5&&clocks==c&&resource.calls==1,"off main forwards with no timing/stat mutation");onMain=true;
 throwsNative=true;bool thrown=false;c=clocks;try{Factory(self);}catch(int n){thrown=n==17;}Check(thrown&&nativeCalls==6&&clocks==c+1&&factory.calls==2,"native exception propagates once without completed sample");throwsNative=false;
 nativeDuration=-100;Factory(self);Check(factory.calls==3&&factory.totalUs==4000&&factory.maxUs==3000,"negative duration clamps");
 nativeDuration=2000;Factory(self);Check(factory.over2ms==1,"over2ms strict boundary");nativeDuration=20000;Resource(self);Check(resource.over20ms==1&&resource.over2ms==2,"over20ms strict boundary");
 factory.calls=~0ULL;factory.totalUs=~0ULL-100;factory.over2ms=~0ULL;factory.over20ms=~0ULL;nativeDuration=25000;Factory(self);Check(factory.calls==~0ULL&&factory.totalUs==~0ULL&&factory.over2ms==~0ULL&&factory.over20ms==~0ULL,"cumulative counters saturate");
 sehNative=true;c=clocks;int beforeNative=nativeCalls;unsigned __int64 beforeSamples=factory.calls;Check(CatchNativeSeh(self)&&nativeCalls==beforeNative+1&&clocks==c+1&&factory.calls==beforeSamples,"native SEH propagates to caller without completed sample");sehNative=false;
 originalFactoryProcess=&NativeProcess;originalSetMeshData=&NativeMesh;originalEntityInitialise=&NativeEntity;
 void*item=reinterpret_cast<void*>(0x2345);nativeDuration=25000;beforeNative=nativeCalls;
 Check(FactoryProcess(self,item)==reinterpret_cast<void*>(0x4567)&&nativeCalls==beforeNative+1&&seen==self&&seenSecond==item,"factoryProcess exact arguments return and once");
 Check(factoryProcess.calls==1&&factoryProcess.totalUs==25000&&factoryProcess.maxUs==25000&&factoryProcess.over2ms==1&&factoryProcess.over20ms==1,"factoryProcess completed inclusive elapsed");
 beforeNative=nativeCalls;SetMeshData(self,item);Check(nativeCalls==beforeNative+1&&seen==self&&seenSecond==item&&setMeshData.calls==1,"static SetMeshData has entity and data only");
 EntityInitialise(self,true);Check(seen==self&&seenForce&&entityInitialise.calls==1,"entity initialise true forwarded");EntityInitialise(self,false);Check(!seenForce&&entityInitialise.calls==2,"entity initialise false forwarded");
 enabled=false;c=clocks;m=mainChecks;beforeNative=nativeCalls;
 Check(FactoryProcess(self,item)==reinterpret_cast<void*>(0x4567),"disabled factoryProcess preserves result");SetMeshData(self,item);EntityInitialise(self,true);
 Check(nativeCalls==beforeNative+3&&clocks==c&&mainChecks==m&&factoryProcess.calls==1&&setMeshData.calls==1&&entityInitialise.calls==2,"new probes debug off forward only no clock or thread checks");
 enabled=true;onMain=false;c=clocks;beforeNative=nativeCalls;
 Check(FactoryProcess(self,item)==reinterpret_cast<void*>(0x4567),"offmain factoryProcess preserves result");SetMeshData(self,item);EntityInitialise(self,false);
 Check(nativeCalls==beforeNative+3&&clocks==c&&factoryProcess.calls==1&&setMeshData.calls==1&&entityInitialise.calls==2,"new probes offmain forward only no clock or stats");onMain=true;
 throwsNative=true;c=clocks;beforeNative=nativeCalls;thrown=false;try{FactoryProcess(self,item);}catch(int n){thrown=n==17;}
 Check(thrown&&nativeCalls==beforeNative+1&&clocks==c+1&&factoryProcess.calls==1,"factoryProcess native exception unchanged and excluded");
 thrown=false;c=clocks;beforeNative=nativeCalls;try{SetMeshData(self,item);}catch(int n){thrown=n==17;}
 Check(thrown&&nativeCalls==beforeNative+1&&clocks==c+1&&setMeshData.calls==1,"setMeshData native exception unchanged and excluded");
 thrown=false;c=clocks;beforeNative=nativeCalls;try{EntityInitialise(self,true);}catch(int n){thrown=n==17;}
 Check(thrown&&nativeCalls==beforeNative+1&&clocks==c+1&&entityInitialise.calls==2,"entityInitialise native exception unchanged and excluded");throwsNative=false;
 sehNative=true;for(int seam=0;seam<3;++seam){c=clocks;beforeNative=nativeCalls;Check(CatchNewSeh(seam,self,item)&&nativeCalls==beforeNative+1&&clocks==c+1&&factoryProcess.calls==1&&setMeshData.calls==1&&entityInitialise.calls==2,"new native SEH propagates without completed samples");}sehNative=false;
 unsigned char image[4096]={0},expected[32]={0};IMAGE_DOS_HEADER*dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
 IMAGE_NT_HEADERS64*nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+0x80);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt->FileHeader.TimeDateStamp=0x5ca5f929;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt->OptionalHeader.SizeOfImage=4096;
 for(int i=0;i<32;++i)image[0x300+i]=expected[i]=static_cast<unsigned char>(i+1);
 Check(AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4096,0x300,expected)==image+0x300,"module exact image contract admitted");
 image[0x31f]^=1;Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4096,0x300,expected),"module byte32 mismatch refused");image[0x31f]^=1;
 Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f928,4096,0x300,expected),"wrong Ogre image timestamp refused");
 Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4097,0x300,expected),"wrong image size refused");
 Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4096,4090,expected),"out of image32 bytes refused");
 nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4096,0x300,expected),"wrong module architecture refused");
 Check(!AdmitModule(NULL,0x5ca5f929,4096,0x300,expected),"absent module refused");
 nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;dos->e_lfanew=-1;Check(!AdmitModule(reinterpret_cast<HMODULE>(image),0x5ca5f929,4096,0x300,expected),"malformed PE offset refused");
 const unsigned char actualEntityBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa1,0x48,0x81,0xec,0xe0,0,0,0,0x48,0xc7,0x45,0x97,0xfe,0xff,0xff,0xff,0x48};
 HMODULE actualOgre=LoadLibraryExA("..\\..\\OgreMain_x64.dll",NULL,DONT_RESOLVE_DLL_REFERENCES);
 void*actualEntity=AdmitModule(actualOgre,0x5ca5f929,0x9c9000,0xBFAC0,actualEntityBytes);
 Check(actualEntity!=NULL,"actual installed Ogre exact PE build RVA and32bytes admitted without DllMain");
 Check(actualEntity&&reinterpret_cast<void*>(GetProcAddress(actualOgre,"?_initialise@Entity@Ogre@@QEAAX_N@Z"))==actualEntity,"actual Ogre named export matches admitted32byte RVA");FreeLibrary(actualOgre);
 ExerciseCompletionChildren(self,item);
 ExerciseOwnedValue(self,item);ExerciseSlowRing(self);
 seconds=0;ResourceLoadProbeReport();Check(output.empty(),"first report establishes cadence");seconds=9.99;ResourceLoadProbeReport();Check(output.empty(),"report waits ten seconds");seconds=10;ResourceLoadProbeReport();Check(output.find("factoryDrain")!=std::string::npos&&output.find("resourceFinish")!=std::string::npos&&output.find("factoryProcess")!=std::string::npos&&output.find("setMeshData")!=std::string::npos&&output.find("entityInitialise")!=std::string::npos&&output.find("inclusiveElapsed")!=std::string::npos,"summary labels preserve attribution scope");
 Check(output.find("texture2D")!=std::string::npos&&output.find("textureArray")!=std::string::npos&&output.find("appearanceBody")!=std::string::npos&&output.find("appearanceAttachment")!=std::string::npos,"completion child labels present");
 Check(output.find("Resource completion slow update:")!=std::string::npos&&output.find("texture2DUs=4000")!=std::string::npos&&output.find("appearanceBodyUs=6000")!=std::string::npos&&slowCount==0,"slow diagnostics flush only on bounded summary cadence");
 Check(resource.windowMaxUs==0&&body.windowMaxUs==0&&resource.maxUs==25000,"window maximum resets without losing cumulative maximum");
 size_t size=output.size();seconds=19.9;ResourceLoadProbeReport();Check(output.size()==size,"report rate bounded");enabled=false;seconds=30;ResourceLoadProbeReport();Check(output.size()==size,"debug off report silent");
 printf("PASS %d actual-production-wrapper controls\n",checks);return 0;
}
