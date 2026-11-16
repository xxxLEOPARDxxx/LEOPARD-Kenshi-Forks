#include "resources/resource_load_probe.h"
#include "resources/resource_texture_retention.h"
#include "resources/resource_geometry.h"
#include "fixes/world/morph_pose_scope.h"
#ifndef RESOURCE_LOAD_PROBE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#endif

namespace resource_load_probe_detail {
typedef void(*NativeFn)(void*);
typedef void*(*FactoryProcessFn)(void*,void*);
typedef void(*SetMeshDataFn)(void*,const void*);
typedef void(*EntityInitialiseFn)(void*,bool);
typedef void*(*TextureFn)(void*,void*);
// Windows x64 passes nontrivial 16-byte SharedPtr by an indirect temporary in
// RDX. The native callee owns its destruction. Raw forwarding neither copies nor
// destroys it; RCX=this and R8=body/attachment data remain exact.
typedef void(*AppearanceFn)(void*,void*,void*);
static NativeFn originalFactory=NULL,originalResource=NULL;
static FactoryProcessFn originalFactoryProcess=NULL;
static SetMeshDataFn originalSetMeshData=NULL;
static EntityInitialiseFn originalEntityInitialise=NULL;
static TextureFn originalTexture2D=NULL;
static AppearanceFn originalBody=NULL,originalAttachment=NULL;
static AppearanceFn originalMeshCallback=NULL;
typedef void*(*MorphLookupFn)(void*,void*,void*,bool,float,bool);
typedef void*(*MorphCreateFn)(const void*,void*,void*,bool);
typedef void*(*MorphSwitchFn)(void*,void*);
typedef void(*AnimationBodyFn)(void*,void*);
typedef void(*PoseBlendFn)(float,const void*,const void*,void*);
static MorphLookupFn originalMorphLookup=NULL;
static MorphCreateFn originalMorphCreate=NULL;
static MorphSwitchFn originalMorphSwitch=NULL;
static AnimationBodyFn originalAnimationBody=NULL;
static NativeFn originalAttachmentBuild=NULL;
static PoseBlendFn originalPoseBlend=NULL;
struct Stats {
    unsigned __int64 calls,totalUs,maxUs,over2ms,over20ms,windowMaxUs;
    Stats():calls(0),totalUs(0),maxUs(0),over2ms(0),over20ms(0),windowMaxUs(0){}
};
static Stats factory,resource;
static Stats factoryProcess,setMeshData,entityInitialise;
static Stats texture2D,textureArray,body,attachment;
static Stats meshCallback;
static Stats morphLookup,morphCreate,morphSwitch,animationBody,attachmentBuild,poseBlend;
static bool factoryInstalled=false,resourceInstalled=false;
static bool factoryProcessInstalled=false,setMeshDataInstalled=false,entityInitialiseInstalled=false;
static bool texture2DInstalled=false,bodyInstalled=false,attachmentInstalled=false,arrayMeasured=false;
static bool meshCallbackInstalled=false;
static bool morphLookupInstalled=false,morphCreateInstalled=false,morphSwitchInstalled=false;
static bool animationBodyInstalled=false,attachmentBuildInstalled=false,poseBlendInstalled=false;
static double nextReport=0;
static unsigned __int64 AddBounded(unsigned __int64 a,unsigned __int64 b){
    const unsigned __int64 cap=~0ULL;return b>cap-a?cap:a+b;
}
static void Sample(Stats&s,long elapsedUs){
    const unsigned __int64 us=elapsedUs>0?static_cast<unsigned __int64>(elapsedUs):0;
    s.calls=AddBounded(s.calls,1);s.totalUs=AddBounded(s.totalUs,us);
    if(us>s.maxUs)s.maxUs=us;
    if(us>s.windowMaxUs)s.windowMaxUs=us;
    if(us>2000)s.over2ms=AddBounded(s.over2ms,1);
    if(us>20000)s.over20ms=AddBounded(s.over20ms,1);
}
static void Invoke(NativeFn native,void*self,Stats&s){
    // Cached admission only. No QPC/stat/log/heap/lock on the disabled path.
    if(!DetailedLogEnabled()||!IsMainThread()){native(self);return;}
    const LONGLONG begin=QpcNow();
    // Do not catch native C++/SEH exceptions or add cleanup to their unwind.
    // Only successfully completed calls contribute a sample.
    native(self);
    Sample(s,QpcDeltaUs(begin,QpcNow()));
}
static void Factory(void*self){Invoke(originalFactory,self,factory);}
struct ChildTimes {
    unsigned __int64 factoryProcess;
    unsigned __int64 entity,mesh,tex2D,array,appearanceBody,appearanceAttachment,meshCallback;
    unsigned __int64 morphLookup,morphCreate,morphSwitch,animationBody,attachmentBuild,poseBlend;
};
static void ReadChildren(ChildTimes&t){
    t.factoryProcess=factoryProcess.totalUs;
    t.entity=entityInitialise.totalUs;t.mesh=setMeshData.totalUs;t.tex2D=texture2D.totalUs;
    t.array=textureArray.totalUs;t.appearanceBody=body.totalUs;t.appearanceAttachment=attachment.totalUs;
    t.meshCallback=meshCallback.totalUs;
    t.morphLookup=morphLookup.totalUs;t.morphCreate=morphCreate.totalUs;t.morphSwitch=morphSwitch.totalUs;
    t.animationBody=animationBody.totalUs;t.attachmentBuild=attachmentBuild.totalUs;t.poseBlend=poseBlend.totalUs;
}
struct SlowUpdate {double completedAt;long elapsedUs;ChildTimes children;};
static SlowUpdate slowUpdates[16];
static unsigned slowRead=0,slowCount=0;static unsigned __int64 slowOverwritten=0;
static void Resource(void*self){
    if(!DetailedLogEnabled()||!IsMainThread()){originalResource(self);return;}
    ChildTimes before;ReadChildren(before);const LONGLONG begin=QpcNow();
    originalResource(self); // native exceptions unchanged; no retained state
    const long us=QpcDeltaUs(begin,QpcNow());Sample(resource,us);
    if(us<10000)return;
    ChildTimes after;ReadChildren(after);SlowUpdate record;
    record.completedAt=ElapsedSec();record.elapsedUs=us;
    record.children.factoryProcess=after.factoryProcess-before.factoryProcess;
    record.children.entity=after.entity-before.entity;record.children.mesh=after.mesh-before.mesh;
    record.children.tex2D=after.tex2D-before.tex2D;record.children.array=after.array-before.array;
    record.children.appearanceBody=after.appearanceBody-before.appearanceBody;
    record.children.appearanceAttachment=after.appearanceAttachment-before.appearanceAttachment;
    record.children.meshCallback=after.meshCallback-before.meshCallback;
    record.children.morphLookup=after.morphLookup-before.morphLookup;
    record.children.morphCreate=after.morphCreate-before.morphCreate;
    record.children.morphSwitch=after.morphSwitch-before.morphSwitch;
    record.children.animationBody=after.animationBody-before.animationBody;
    record.children.attachmentBuild=after.attachmentBuild-before.attachmentBuild;
    record.children.poseBlend=after.poseBlend-before.poseBlend;
    if(slowCount==16){slowRead=(slowRead+1)%16;--slowCount;slowOverwritten=AddBounded(slowOverwritten,1);}
    slowUpdates[(slowRead+slowCount)%16]=record;++slowCount;
}
static void* FactoryProcess(void*self,void*item){
    if(!DetailedLogEnabled()||!IsMainThread())return originalFactoryProcess(self,item);
    const LONGLONG begin=QpcNow();
    void*result=originalFactoryProcess(self,item);
    Sample(factoryProcess,QpcDeltaUs(begin,QpcNow()));return result;
}
static void SetMeshData(void*entity,const void*data){
    if(!DetailedLogEnabled()||!IsMainThread()){originalSetMeshData(entity,data);return;}
    const LONGLONG begin=QpcNow();originalSetMeshData(entity,data);
    Sample(setMeshData,QpcDeltaUs(begin,QpcNow()));
}
static void EntityInitialise(void*entity,bool force){
    if(!DetailedLogEnabled()||!IsMainThread()){originalEntityInitialise(entity,force);return;}
    const LONGLONG begin=QpcNow();originalEntityInitialise(entity,force);
    Sample(entityInitialise,QpcDeltaUs(begin,QpcNow()));
}
static void CompleteTextureCapture(void*result){
    try{resource_texture_retention::ObserveLoadedResult(result);}catch(...){}
}
static void*Texture2D(void*self,void*out){
    if(!DetailedLogEnabled()||!IsMainThread()){
        void*result=originalTexture2D(self,out);CompleteTextureCapture(result);return result;
    }
    const LONGLONG begin=QpcNow();void*result=originalTexture2D(self,out);
    Sample(texture2D,QpcDeltaUs(begin,QpcNow()));CompleteTextureCapture(result);return result;
}
static void InvokeAppearance(AppearanceFn native,void*self,void*resource,void*data,Stats&s){
    if(!DetailedLogEnabled()||!IsMainThread()){native(self,resource,data);return;}
    const LONGLONG begin=QpcNow();native(self,resource,data);
    Sample(s,QpcDeltaUs(begin,QpcNow()));
}
static void AppearanceBody(void*self,void*resource,void*data){InvokeAppearance(originalBody,self,resource,data,body);}
static void AppearanceAttachment(void*self,void*resource,void*data){InvokeAppearance(originalAttachment,self,resource,data,attachment);}
static void MeshCompletionCallback(void*self,void*resource,void*entity){InvokeAppearance(originalMeshCallback,self,resource,entity,meshCallback);}
// Measurements only: no scene/resource operation is moved between threads.
// Lookup's fifth/sixth total arguments occupy stack slots, not XMM4/R9.
static void* GetMorphedBodyMesh(void*self,void*data,void*entity,bool unique,float ugliness,bool shaved){
    if(!DetailedLogEnabled()||!IsMainThread())return originalMorphLookup(self,data,entity,unique,ugliness,shaved);
    const LONGLONG begin=QpcNow();void*result=originalMorphLookup(self,data,entity,unique,ugliness,shaved);
    Sample(morphLookup,QpcDeltaUs(begin,QpcNow()));return result;
}
static void* CreateMorphedMesh(const void*name,void*entity,void*data,bool shaved){
    if(!DetailedLogEnabled()||!IsMainThread())return morph_pose_scope::Create(originalMorphCreate,name,entity,data,shaved);
    const LONGLONG begin=QpcNow();void*result=morph_pose_scope::Create(originalMorphCreate,name,entity,data,shaved);
    Sample(morphCreate,QpcDeltaUs(begin,QpcNow()));return result;
}
static void CompleteFinalMeshCapture(void*result){
    try{resource_geometry::ObserveFinalEntity(result);}catch(...){}
}
static void* SwitchBodyMesh(void*entity,void*ownedMesh){
    // Native owns the indirect16 temporary, including its unwind/destruction.
    // Input Entity may be destroyed/replaced; do not inspect either after call.
    if(!DetailedLogEnabled()||!IsMainThread()){
        void*result=originalMorphSwitch(entity,ownedMesh);CompleteFinalMeshCapture(result);return result;
    }
    const LONGLONG begin=QpcNow();void*result=originalMorphSwitch(entity,ownedMesh);
    Sample(morphSwitch,QpcDeltaUs(begin,QpcNow()));CompleteFinalMeshCapture(result);return result;
}
static void SetBodyEntity(void*self,void*entity){
    if(!DetailedLogEnabled()||!IsMainThread()){originalAnimationBody(self,entity);return;}
    const LONGLONG begin=QpcNow();originalAnimationBody(self,entity);
    Sample(animationBody,QpcDeltaUs(begin,QpcNow()));
}
static void BuildAttachments(void*self){Invoke(originalAttachmentBuild,self,attachmentBuild);}
static void PoseBlend(float weight,const void*offsets,const void*normals,void*target){
    if(!DetailedLogEnabled()||!IsMainThread()){originalPoseBlend(weight,offsets,normals,target);return;}
    const LONGLONG begin=QpcNow();originalPoseBlend(weight,offsets,normals,target);
    Sample(poseBlend,QpcDeltaUs(begin,QpcNow()));
}
// Startup-only admission. Catch unreadable module headers/bytes, never native
// wrapper exceptions. Function contracts contain the full 32-byte prefix.
static bool ImageMatches(HMODULE module,unsigned long stamp,unsigned long size){
    __try{
        if(!module)return false;
        const unsigned char*base=reinterpret_cast<const unsigned char*>(module);
        const IMAGE_DOS_HEADER*dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1024*1024)return false;
        const IMAGE_NT_HEADERS64*nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64
            &&nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC
            &&nt->FileHeader.TimeDateStamp==stamp&&nt->OptionalHeader.SizeOfImage==size;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void* AdmitModule(HMODULE module,unsigned long stamp,unsigned long size,uintptr_t rva,const unsigned char*bytes){
    if(!ImageMatches(module,stamp,size)||size<32||rva>size-32)return NULL;
    __try{
        unsigned char*target=reinterpret_cast<unsigned char*>(module)+rva;
        return memcmp(target,bytes,32)==0?target:NULL;
    }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
static void Append(std::ostringstream&out,const char*name,const Stats&s,bool installed){
    out<<" "<<name<<"={installed="<<(installed?1:0)<<" calls="<<s.calls
       <<" totalUs="<<s.totalUs<<" maxUs="<<s.maxUs
       <<" over2ms="<<s.over2ms<<" over20ms="<<s.over20ms<<" windowMaxUs="<<s.windowMaxUs<<"}";
}
#ifndef RESOURCE_LOAD_PROBE_TEST
static bool InstallAdmitted(const char*name,void*target,void*detour,void**original){
    if(!target){LogError(std::string("Resource loading probe: ")+name+" module/function contract refused; diagnostic unavailable");return false;}
    if(KenshiLib::AddHook(target,detour,original)!=KenshiLib::SUCCESS||!*original){
        // Creation may publish an original before enabling fails. Keep it for
        // any armed forwarding wrapper; partial setup never admits the feature.
        LogError(std::string("Resource loading probe: ")+name+" hook failed; diagnostic unavailable");return false;
    }
    return true;
}
static bool Install(const char*name,uintptr_t rva,const unsigned char*bytes,NativeFn detour,NativeFn*original){
    void*target=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(GetModuleHandleA(NULL))+rva);
    // Exact bytes also refuse a prior profiler/other detour instead of replacing it.
    if(memcmp(target,bytes,32)!=0){
        LogError(std::string("Resource loading probe: ")+name+" native contract refused; diagnostic unavailable");return false;
    }
    if(KenshiLib::AddHook(target,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(original))!=KenshiLib::SUCCESS||!*original){
        LogError(std::string("Resource loading probe: ")+name+" hook failed; diagnostic unavailable");return false;
    }
    return true;
}
#endif
}

#if !defined(RESOURCE_LOAD_PROBE_TEST) || defined(RESOURCE_LOAD_PROBE_INIT_TEST)
namespace resource_load_probe_detail {
static bool captureAttempted=false,captureInitializationComplete=false,diagnosticsAttempted=false;
static void EnsureDiagnosticHooks(){
    // The mandatory output capture must finish its own attempt first. Mark
    // timing admission before installing anything so reentrant reports cannot
    // install a second set. Failed contracts/hooks retain one-shot refusal.
    if(!DetailedLogEnabled()||!IsMainThread()||!captureInitializationComplete||diagnosticsAttempted)return;
    diagnosticsAttempted=true;
    if(NativeProfileIsGog())return;
    static const unsigned char factoryBytes[32]={0x48,0x89,0x5c,0x24,0x20,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x48,0x89,0x6c,0x24,0x58,0x48,0x8d,0x4c,0x24,0x50,0x33,0xed,0x8b,0xfd,0xe8,0x96,0xa8,0xab,0xff};
    static const unsigned char resourceBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xe8,0xfd,0xff,0xff,0x48,0x81,0xec,0xf0,0x02,0x00,0x00,0x48,0xc7,0x45,0x68,0xfe,0xff};
    factoryInstalled=Install("factoryDrain",0x582880,factoryBytes,&Factory,&originalFactory);
    resourceInstalled=Install("resourceFinish",0x44BBA0,resourceBytes,&Resource,&originalResource);
    static const unsigned char processBytes[32]={0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xa8,0xfe,0xff,0xff,0x48,0x81,0xec,0x20,0x02,0x00,0x00,0x48,0xc7,0x45,0xc0};
    static const unsigned char meshBytes[32]={0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,0x48,0x8b,0x05,0x45};
    static const unsigned char entityBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa1,0x48,0x81,0xec,0xe0,0x00,0x00,0x00,0x48,0xc7,0x45,0x97,0xfe,0xff,0xff,0xff,0x48};
    HMODULE exe=GetModuleHandleA(NULL);
    const bool steam=ImageMatches(exe,0x65d604d7,0x232c000);
    factoryProcessInstalled=InstallAdmitted("factoryProcess",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x580CE0,processBytes):NULL,reinterpret_cast<void*>(&FactoryProcess),reinterpret_cast<void**>(&originalFactoryProcess));
    setMeshDataInstalled=InstallAdmitted("setMeshData",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x447850,meshBytes):NULL,reinterpret_cast<void*>(&SetMeshData),reinterpret_cast<void**>(&originalSetMeshData));
    HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");
    void*entity=steam?AdmitModule(ogre,0x5ca5f929,0x9c9000,0xBFAC0,entityBytes):NULL;
    if(entity&&reinterpret_cast<void*>(GetProcAddress(ogre,"?_initialise@Entity@Ogre@@QEAAX_N@Z"))!=entity)entity=NULL;
    entityInitialiseInstalled=InstallAdmitted("entityInitialise",entity,reinterpret_cast<void*>(&EntityInitialise),reinterpret_cast<void**>(&originalEntityInitialise));
    static const unsigned char bodyBytes[32]={0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xa8,0xfd,0xff,0xff,0x48,0x81,0xec,0x20,0x03,0x00,0x00,0x48,0xc7,0x45,0xc8};
    static const unsigned char attachmentBytes[32]={0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x48,0x83,0xec,0x70,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x9c,0x24,0xb0,0x00,0x00};
    attachmentInstalled=InstallAdmitted("appearanceAttachment",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x536590,attachmentBytes):NULL,reinterpret_cast<void*>(&AppearanceAttachment),reinterpret_cast<void**>(&originalAttachment));
    // Internal Boost callback dispatcher used by both mesh-finish routes.
    // Exact native entry, not the short jump thunk. Same owned16 indirect ABI.
    static const unsigned char callbackBytes[32]={0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83,0xec,0x50,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x8b,0xda,0x48,0x8b,0xd1,0x48,0x83,0x39,0x00,0x75,0x33,0x48};
    meshCallbackInstalled=InstallAdmitted("meshCompletionCallback",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x458860,callbackBytes):NULL,reinterpret_cast<void*>(&MeshCompletionCallback),reinterpret_cast<void**>(&originalMeshCallback));
    // Independently admitted immutable native contracts; stock fallback on refusal.
    static const unsigned char morphLookupBytes[32]={0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0x98,0x48,0x81,0xec,0x30,0x01,0x00,0x00,0x48,0xc7,0x44,0x24,0x48,0xfe,0xff};
    void* morphLookupSite=steam?AdmitModule(exe,0x65d604d7,0x232c000,0x7c030,morphLookupBytes):NULL;
    morphLookupInstalled=InstallAdmitted("morphLookup",morphLookupSite,reinterpret_cast<void*>(&GetMorphedBodyMesh),reinterpret_cast<void**>(&originalMorphLookup));
    static const unsigned char animationBodyBytes[32]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x54,0x41,0x55,0x48,0x83,0xec,0x30,0x45,0x33,0xed,0x4c,0x8b,0xe2,0x48,0x8b,0xf1,0x41,0x8b,0xfd,0x44};
    void* animationBodySite=steam?AdmitModule(exe,0x65d604d7,0x232c000,0x5b8910,animationBodyBytes):NULL;
    animationBodyInstalled=InstallAdmitted("animationBody",animationBodySite,reinterpret_cast<void*>(&SetBodyEntity),reinterpret_cast<void**>(&originalAnimationBody));
    static const unsigned char attachmentBuildBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xc8,0x48,0x81,0xec,0x10,0x01,0x00,0x00,0x48,0xc7,0x44,0x24,0x30,0xfe,0xff,0xff,0xff};
    void* attachmentBuildSite=steam?AdmitModule(exe,0x65d604d7,0x232c000,0x537ba0,attachmentBuildBytes):NULL;
    attachmentBuildInstalled=InstallAdmitted("attachmentBuild",attachmentBuildSite,reinterpret_cast<void*>(&BuildAttachments),reinterpret_cast<void**>(&originalAttachmentBuild));
    static const unsigned char poseBlendBytes[32]={0x48,0x8b,0xc4,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x60,0x48,0xc7,0x44,0x24,0x30,0xfe,0xff,0xff,0xff,0x48,0x89,0x58,0x10,0x48,0x89,0x68,0x18};
    void* poseBlendSite=steam?AdmitModule(ogre,0x5ca5f929,0x9c9000,0x1a9be0,poseBlendBytes):NULL;
    if(poseBlendSite&&reinterpret_cast<void*>(GetProcAddress(ogre,"?softwareVertexPoseBlend@Mesh@Ogre@@SAXMAEBV?$map@_KVVector3@Ogre@@U?$less@_K@std@@V?$STLAllocator@U?$pair@$$CB_KVVector3@Ogre@@@std@@V?$CategorisedAllocPolicy@$0A@@Ogre@@@2@@std@@0PEAVVertexData@2@@Z"))!=poseBlendSite)poseBlendSite=NULL;
    poseBlendInstalled=InstallAdmitted("poseBlend",poseBlendSite,reinterpret_cast<void*>(&PoseBlend),reinterpret_cast<void**>(&originalPoseBlend));
    KEO_DETAIL("Resource appearance probe: six preparation seams measured; native threads, ownership, scheduling and stock morph cache unchanged");
    // Prefer the common completion boundary. Do not touch a body site already
    // modified by an unknown component. Body-specific timing is fallback only.
    if(!meshCallbackInstalled)bodyInstalled=InstallAdmitted("appearanceBody",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x538590,bodyBytes):NULL,reinterpret_cast<void*>(&AppearanceBody),reinterpret_cast<void**>(&originalBody));
    KEO_DETAIL(std::string("Resource completion attribution: texture2D=")+(texture2DInstalled?"installed":"unavailable")+" appearanceBody="+(bodyInstalled?"installed":"unavailable")+" appearanceAttachment="+(attachmentInstalled?"installed":"unavailable")+" array=existing bridge only; scheduling and readiness unchanged");
    KEO_DETAIL(std::string("Resource loading probe: factoryDrain=")+(factoryInstalled?"installed":"unavailable")+" resourceFinish="+(resourceInstalled?"installed":"unavailable")+" factoryProcess="+(factoryProcessInstalled?"installed":"unavailable")+" setMeshData="+(setMeshDataInstalled?"installed":"unavailable")+" entityInitialise="+(entityInitialiseInstalled?"installed":"unavailable")+" (completed calls; inclusive elapsed, not CPU time; generic factory/entities, not NPC-only; overlapping totals must not be summed)");
}
}
void ResourceLoadProbeInit(){
    using namespace resource_load_probe_detail;
    if(!IsMainThread()||captureAttempted)return;captureAttempted=true;
    morph_pose_scope::SetCreateHookReady(false);
    if(NativeProfileIsGog()){
        captureInitializationComplete=true;diagnosticsAttempted=true;
        KEO_DETAIL("Resource loading probe: optional native seams not admitted for GOG");return;
    }
    // This consumer hook also supplies completed native TexturePtr outputs to
    // retention. It is required independently of optional timing/Debug state.
    HMODULE exe=GetModuleHandleA(NULL);
    const bool steam=ImageMatches(exe,0x65d604d7,0x232c000);
    static const unsigned char textureBytes[32]={0x40,0x56,0x57,0x41,0x54,0x48,0x81,0xec,0x00,0x01,0x00,0x00,0x48,0xc7,0x84,0x24,0xa0,0x00,0x00,0x00,0xfe,0xff,0xff,0xff,0x48,0x89,0x9c,0x24,0x30,0x01,0x00,0x00};
    texture2DInstalled=InstallAdmitted("texture2D",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x44ACD0,textureBytes):NULL,reinterpret_cast<void*>(&Texture2D),reinterpret_cast<void**>(&originalTexture2D));
    // The existing Switch seam supplies the final morph replacement to the
    // geometry cache even with diagnostics OFF. Never add a second detour.
    static const unsigned char morphSwitchBytes[32]={0x48,0x89,0x54,0x24,0x10,0x56,0x57,0x41,0x54,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x50,0x48,0x89,0x6c,0x24,0x68};
    void* morphSwitchSite=steam?AdmitModule(exe,0x65d604d7,0x232c000,0x71920,morphSwitchBytes):NULL;
    morphSwitchInstalled=InstallAdmitted("morphSwitch",morphSwitchSite,reinterpret_cast<void*>(&SwitchBodyMesh),reinterpret_cast<void**>(&originalMorphSwitch));
    // The functional body-morph scope needs this seam even with Debug OFF.
    // Diagnostics reuse the same wrapper instead of adding another detour.
    static const unsigned char morphCreateBytes[32]={0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x28,0xfe,0xff,0xff,0xff};
    void* morphCreateSite=steam?AdmitModule(exe,0x65d604d7,0x232c000,0x71a80,morphCreateBytes):NULL;
    morphCreateInstalled=InstallAdmitted("morphCreate",morphCreateSite,reinterpret_cast<void*>(&CreateMorphedMesh),reinterpret_cast<void**>(&originalMorphCreate));
    morph_pose_scope::SetCreateHookReady(morphCreateInstalled&&originalMorphCreate!=NULL);
    captureInitializationComplete=true;
    EnsureDiagnosticHooks();
}
#endif
void ResourceLoadProbeReport(){
    using namespace resource_load_probe_detail;
    if(!DetailedLogEnabled()||!IsMainThread())return;
#if !defined(RESOURCE_LOAD_PROBE_TEST) || defined(RESOURCE_LOAD_PROBE_INIT_TEST)
    EnsureDiagnosticHooks(); // existing owner heartbeat, before the report throttle
#endif
    const double now=ElapsedSec();
    if(nextReport==0){nextReport=now+10;return;}
    if(now<nextReport)return;nextReport=now+10;
    std::ostringstream out;out<<"Resource loading probe: cumulative completed inclusiveElapsed";
    Append(out,"factoryDrain",factory,factoryInstalled);
    Append(out,"resourceFinish",resource,resourceInstalled);
    Append(out,"factoryProcess",factoryProcess,factoryProcessInstalled);
    Append(out,"setMeshData",setMeshData,setMeshDataInstalled);
    Append(out,"entityInitialise",entityInitialise,entityInitialiseInstalled);
    Append(out,"texture2D",texture2D,texture2DInstalled);
    Append(out,"textureArray",textureArray,arrayMeasured);
    Append(out,"appearanceBody",body,bodyInstalled);
    Append(out,"appearanceAttachment",attachment,attachmentInstalled);
    Append(out,"meshCompletionCallback",meshCallback,meshCallbackInstalled);
    Append(out,"morphLookup",morphLookup,morphLookupInstalled);
    Append(out,"morphCreate",morphCreate,morphCreateInstalled);
    Append(out,"morphSwitch",morphSwitch,morphSwitchInstalled);
    Append(out,"animationBody",animationBody,animationBodyInstalled);
    Append(out,"attachmentBuild",attachmentBuild,attachmentBuildInstalled);
    Append(out,"poseBlend",poseBlend,poseBlendInstalled);
    LogMsg(out.str());
    factory.windowMaxUs=resource.windowMaxUs=factoryProcess.windowMaxUs=setMeshData.windowMaxUs=entityInitialise.windowMaxUs=0;
    texture2D.windowMaxUs=textureArray.windowMaxUs=body.windowMaxUs=attachment.windowMaxUs=0;
    meshCallback.windowMaxUs=0;
    morphLookup.windowMaxUs=morphCreate.windowMaxUs=morphSwitch.windowMaxUs=0;
    animationBody.windowMaxUs=attachmentBuild.windowMaxUs=poseBlend.windowMaxUs=0;
    for(unsigned n=0;n<slowCount;++n){
        const SlowUpdate&r=slowUpdates[(slowRead+n)%16];std::ostringstream detail;
        detail<<"Resource completion slow update: completedAt="<<r.completedAt<<" elapsedUs="<<r.elapsedUs
              <<" factoryProcessUs="<<r.children.factoryProcess
              <<" entityUs="<<r.children.entity<<" meshUs="<<r.children.mesh<<" texture2DUs="<<r.children.tex2D
              <<" textureArrayUs="<<r.children.array<<" appearanceBodyUs="<<r.children.appearanceBody
              <<" appearanceAttachmentUs="<<r.children.appearanceAttachment
              <<" meshCallbackUs="<<r.children.meshCallback
              <<" morphLookupUs="<<r.children.morphLookup<<" morphCreateUs="<<r.children.morphCreate
              <<" morphSwitchUs="<<r.children.morphSwitch<<" animationBodyUs="<<r.children.animationBody
              <<" attachmentBuildUs="<<r.children.attachmentBuild<<" poseBlendUs="<<r.children.poseBlend
              <<" inclusive children overlap; last16 ring, overwritten="<<slowOverwritten;
        LogMsg(detail.str());
    }
    slowRead=slowCount=0;
}
__int64 ResourceLoadProbeArrayBegin(){
    if(!DetailedLogEnabled()||!IsMainThread())return -1;
    return QpcNow();
}
void ResourceLoadProbeArrayEnd(__int64 begin){
    if(begin<0)return;
    resource_load_probe_detail::arrayMeasured=true;
    resource_load_probe_detail::Sample(resource_load_probe_detail::textureArray,QpcDeltaUs(begin,QpcNow()));
}
