// Actual Ogre maps/params/control blocks. The runner includes the unmodified
// production wrapper bodies, stripping includes only; installation is stubbed.
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include <exception>
#include <iomanip>
#include <sstream>
#include <ogre/OgreRoot.h>
#include <ogre/OgreGpuProgram.h>
#include <ogre/OgreGpuProgramParams.h>
#include "render/render_levers.h"
#include "render/render_config.h"
#include "render/module_hooks.h"
#include "render/gpu_param_cache.h"
#include "render/upload_plan.h"
#include "render/upload_shadow.h"
#include "game/klib_member_contract.h"
RenderStats g_renderStats={};RenderConfig g_renderCfg={};
static bool owner=true;
static bool refuseImage=false;
bool IsMainThread(){return owner;}
bool DetailedLogEnabled(){return false;}
void LogError(const std::string&){}
bool InstallModuleHook(const ModuleSite&,void*,void**,bool*){return false;}
void*PatchImportSlot(HMODULE,const char*,const char*,void*,void*){return NULL;}
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){if(!m)return false;const IMAGE_DOS_HEADER*d=reinterpret_cast<const IMAGE_DOS_HEADER*>(m);const IMAGE_NT_HEADERS64*n=reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const char*>(m)+d->e_lfanew);*stamp=n->FileHeader.TimeDateStamp^(refuseImage?1:0);*size=n->OptionalHeader.SizeOfImage;return true;}
// PARAMS_PRODUCTION_BODY
static HMODULE ogre=NULL;static unsigned checks=0,forwarded=0;
#define C(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL line%u %s\n",__LINE__,#x);}}while(0)
static unsigned failures=0;
struct Layout:Ogre::GpuProgramParameters {static void Check(){static_assert(offsetof(Layout,mNamedConstants)==0x128,"actual native params map offset");}};
struct PtrLayout:Ogre::GpuNamedConstantsPtr {static void Check(){static_assert(offsetof(PtrLayout,pRep)==0,"native SharedPtr raw first");static_assert(offsetof(PtrLayout,pInfo)==8,"native SharedPtr control block");}};
static_assert(sizeof(Ogre::GpuNamedConstantsPtr)==16,"native SharedPtr width");
static_assert(offsetof(Ogre::GpuNamedConstants,map)==0x20,"actual native map offset");
static_assert(sizeof(Ogre::GpuNamedConstants)==0x48,"actual native map object size");
static void*Symbol(const char*name){void*p=reinterpret_cast<void*>(GetProcAddress(ogre,name));if(!p){std::fprintf(stderr,"missing %s\n",name);std::abort();}return p;}
typedef void*(*ParamsCtor)(void*);typedef void(*Dtor)(void*);
typedef void*(*ProgramCtor)(void*,void*,const Ogre::String&,Ogre::ResourceHandle,const Ogre::String&,bool,void*);
typedef void(*FreshMap)(void*,bool);
struct NativeParams {
    __declspec(align(16)) unsigned char data[4096];
    NativeParams(){memset(data,0xcc,sizeof(data));reinterpret_cast<ParamsCtor>(Symbol("??0GpuProgramParameters@Ogre@@QEAA@XZ"))(data);}
    ~NativeParams(){reinterpret_cast<Dtor>(Symbol("??1GpuProgramParameters@Ogre@@QEAA@XZ"))(data);}
};
struct NativeProgram {
    __declspec(align(16)) unsigned char data[4096];
    NativeProgram(const char*name,unsigned handle){memset(data,0xcc,sizeof(data));Ogre::String n=name,g="General";reinterpret_cast<ProgramCtor>(Symbol("??0GpuProgram@Ogre@@QEAA@PEAVResourceManager@1@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_K1_NPEAVManualResourceLoader@1@@Z"))(data,NULL,n,handle,g,false,NULL);}
    ~NativeProgram(){reinterpret_cast<Dtor>(Symbol("??1GpuProgram@Ogre@@UEAA@XZ"))(data);}
    void*Ptr(){return data+0x270;}
    Ogre::GpuNamedConstants*Map(){return *reinterpret_cast<Ogre::GpuNamedConstants**>(Ptr());}
    void Fresh(){reinterpret_cast<FreshMap>(Symbol("?createNamedParameterMappingStructures@GpuProgram@Ogre@@IEBAX_N@Z"))(data,true);}
};
// Standalone fixture detour only. This DLL mapping belongs to this test
// process, not the game. DAC30's first15bytes contain no relative operand.
struct NativeDeleteHook {
    unsigned char saved[15];unsigned char*target;unsigned char*trampoline;
    static void Jump(unsigned char*p,const void*destination){p[0]=0xff;p[1]=0x25;memset(p+2,0,4);memcpy(p+6,&destination,8);}
    NativeDeleteHook():target(reinterpret_cast<unsigned char*>(ogre)+0xdac30),trampoline(NULL){
        static const unsigned char expected[15]={0x40,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff};
        if(memcmp(target,expected,15))std::abort();memcpy(saved,target,15);
        trampoline=static_cast<unsigned char*>(VirtualAlloc(NULL,32,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));if(!trampoline)std::abort();
        memcpy(trampoline,saved,15);Jump(trampoline+15,target+15);s_origDelete=reinterpret_cast<DeleteNamedConstants_t>(trampoline);
        DWORD prior=0;if(!VirtualProtect(target,15,PAGE_EXECUTE_READWRITE,&prior))std::abort();Jump(target,reinterpret_cast<const void*>(&hook_DeleteNamedConstants));target[14]=0x90;
        FlushInstructionCache(GetCurrentProcess(),target,15);DWORD ignored=0;VirtualProtect(target,15,prior,&ignored);
    }
    ~NativeDeleteHook(){DWORD prior=0;if(!VirtualProtect(target,15,PAGE_EXECUTE_READWRITE,&prior))std::abort();memcpy(target,saved,15);FlushInstructionCache(GetCurrentProcess(),target,15);DWORD ignored=0;VirtualProtect(target,15,prior,&ignored);VirtualFree(trampoline,0,MEM_RELEASE);}
};
static SetNamedConstants_t actualSet=NULL;static bool throwSet=false,forwardOnly=false;
static void*seenParams=NULL;static const void*seenNamed=NULL;
static unsigned seenIncomingRefs=0;
static void SetSpy(void*p,const void*n){++forwarded;seenParams=p;seenNamed=n;const Ogre::GpuNamedConstantsPtr*incoming=static_cast<const Ogre::GpuNamedConstantsPtr*>(n);seenIncomingRefs=incoming&&!incoming->isNull()?incoming->useCount():0;if(throwSet)throw 71;if(!forwardOnly)actualSet(p,n);}
static DWORD FaultSet(void*p,const void*n,bool wrapped){
    __try{if(wrapped)hook_SetNamedConstants(p,n);else actualSet(p,n);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
static size_t Values(const NativeParams&p,size_t begin,size_t end,size_t width){return (*reinterpret_cast<const unsigned char*const*>(p.data+end)-*reinterpret_cast<const unsigned char*const*>(p.data+begin))/width;}
static const MsvcString*Key(const Ogre::String&key){return reinterpret_cast<const MsvcString*>(&key);}
static const void*Definition(NativeProgram&p,const Ogre::String&key,size_t index){
    Ogre::GpuConstantDefinition d;d.constType=Ogre::GCT_FLOAT4;d.physicalIndex=index;d.logicalIndex=0;d.elementSize=4;d.arraySize=1;
    p.Map()->floatBufferSize=index+4;p.Map()->map[key]=d;return &p.Map()->map.find(key)->second;
}

namespace upload_fixture {
// UPLOAD_PRODUCTION_BODY
}
struct BufferFixture {
    unsigned char program[0x480],node[0x80],uniform[0x80],impl[0x80];
    struct Var {Ogre::String name;size_t size,offset;} var;
    const char* first; LONG paramsInfo[12];
    BufferFixture(const Ogre::String&key):first(reinterpret_cast<const char*>(node)){
        static_assert(offsetof(Var,size)==0x28,"native variable size");static_assert(sizeof(Var)==56,"native variable stride");
        memset(program,0,sizeof(program));memset(node,0,sizeof(node));memset(uniform,0,sizeof(uniform));memset(impl,0,sizeof(impl));memset(paramsInfo,0,sizeof(paramsInfo));
        *reinterpret_cast<size_t*>(program+0x458)=1;*reinterpret_cast<const char***>(program+0x450)=&first;
        *reinterpret_cast<void**>(node+0x48)=uniform;*reinterpret_cast<void**>(node+0x58)=&var;*reinterpret_cast<void**>(node+0x60)=reinterpret_cast<char*>(&var)+sizeof(var);
        *reinterpret_cast<size_t*>(uniform+8)=16;*reinterpret_cast<void**>(uniform+0x70)=impl;*reinterpret_cast<void**>(impl+0x40)=this;var.name=key;var.size=16;var.offset=0;
    }
    void* Call(NativeParams&p){void*pair[2]={p.data,paramsInfo};paramsInfo[8]=2;return upload_fixture::hook_GetConstantBuffer(program,pair);}
    void* Try(NativeParams&p,UploadShadow**pending){void*pair[2]={p.data,paramsInfo};paramsInfo[8]=2;return upload_fixture::TrySkip(program,pair,pending);}
};
static unsigned uploads=0;static float gpuValues[4]={0};
static void* UploadSpy(void*program,void**params){
    ++uploads;const char*data=static_cast<const char*>(params[0]);memcpy(gpuValues,*reinterpret_cast<const void*const*>(data+0x58),16);
    InterlockedDecrement(reinterpret_cast<volatile LONG*>(static_cast<char*>(params[1])+0x20));params[0]=params[1]=NULL;return program;
}
static DWORD TryGuarded(BufferFixture*f,NativeParams*p,UploadShadow**pending){
    __try{f->Try(*p,pending);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
int main(){
    Ogre::Root root("","","");ogre=GetModuleHandleA("OgreMain_x64.dll");C(ogre!=NULL);NativeDeleteHook deletion;
    s_nativeNamedInfoVtable=VerifiedNativeNamedInfoVtable();C(s_nativeNamedInfoVtable!=NULL);
    actualSet=reinterpret_cast<SetNamedConstants_t>(Symbol("?_setNamedConstants@GpuProgramParameters@Ogre@@QEAAXAEBV?$SharedPtr@UGpuNamedConstants@Ogre@@@2@@Z"));
    s_origSetNamed=&SetSpy;s_origGetConstDef=reinterpret_cast<GetConstDef_t>(Symbol(GET_CONST_DEF_SYMBOL));
    upload_fixture::s_getConstDef=s_origGetConstDef;upload_fixture::s_isFloat=reinterpret_cast<upload_fixture::IsFloat_t>(Symbol(upload_fixture::IS_FLOAT_SYMBOL));
    upload_fixture::s_table.Configure(16,1024*1024);Ogre::String key="tint";
    // SDK final params destruction bypasses the native deletion hook. It must
    // never leave a raw definition in either persistent cache.
    {NativeParams p;Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);
     Ogre::GpuConstantDefinition d;d.constType=Ogre::GCT_FLOAT4;d.physicalIndex=0;d.elementSize=4;d.arraySize=1;sdk->floatBufferSize=4;sdk->map[key]=d;
     actualSet(p.data,&sdk);const void*map=sdk.get();const void*def=&sdk->map.find(key)->second;
     C(CachedLookup(p.data,&key)==def);C(s_cache.Find(map,Key(key))==NULL);
     BufferFixture f(key);UploadShadow*pending=NULL;C(f.Try(p,&pending)==NULL&&pending==NULL);C(upload_fixture::s_planBuilds==0);
     LONG deleted=GpuNamedConstantsDeleted();sdk.setNull();C(GpuNamedConstantsDeleted()==deleted);C(upload_fixture::s_table.count==0);
    }
    // An unverified module/control block follows the native operation on every
    // lookup and refuses upload plans, including a formerly admitted map.
    NativeProgram a("lifetime-A",1),b("lifetime-B",2);NativeParams p,q;
    const void*def=Definition(a,key,0);Definition(b,key,4);hook_SetNamedConstants(p.data,a.Ptr());hook_SetNamedConstants(q.data,a.Ptr());
    C(CachedLookup(p.data,&key)==def&&s_cache.Find(a.Map(),Key(key))==def);
    BufferFixture f(key);UploadShadow*pending=NULL;C(f.Try(p,&pending)==NULL&&pending!=NULL);C(pending&&pending->planValid&&pending->plan[0].def==def);
    if(!pending)return 2;pending->valid=true;LONG builds=upload_fixture::s_planBuilds,deleted=GpuNamedConstantsDeleted();
    // First prove the production plan held the real map node. Erase/rebuild
    // really frees that node; poisoning only its saved pointer makes any old
    // dereference deterministic instead of depending on allocator reuse.
    a.Map()->map.clear();Definition(a,key,4);
    void*poison=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);C(poison!=NULL);pending->plan[0].def=poison;
    hook_SetNamedConstants(p.data,a.Ptr());C(GpuNamedConstantsDeleted()==deleted);
    pending=NULL;DWORD fault=TryGuarded(&f,&p,&pending);C(fault==0);C(upload_fixture::s_planBuilds==builds+1);
    C(pending&&pending->plan[0].def==&a.Map()->map.find(key)->second);VirtualFree(poison,0,MEM_RELEASE);
    // Equal data after a rebuilt plan can still skip; its params SharedPtr
    // consumption is checked separately from map ownership.
    if(pending){pending->valid=true;pending=NULL;C(f.Try(p,&pending)==&f&&pending==NULL&&f.paramsInfo[8]==1);}
    // Different native map assignment retains the shared old map/cache/plan
    // revision. Genuine final deletion still invalidates through DAC30.
    const void*old=a.Map();def=CachedLookup(q.data,&key);LONG revision=GpuNamedConstantsRevision();hook_SetNamedConstants(p.data,b.Ptr());C(s_cache.Find(old,Key(key))==def);C(GpuNamedConstantsRevision()==revision);
    a.Fresh();deleted=GpuNamedConstantsDeleted();hook_SetNamedConstants(q.data,b.Ptr());C(GpuNamedConstantsDeleted()==deleted+1&&s_cache.Find(old,Key(key))==NULL);C(GpuNamedConstantsRevision()!=revision);
    const void*verified=s_nativeNamedInfoVtable;s_nativeNamedInfoVtable=NULL;
    C(CachedLookup(p.data,&key)==&b.Map()->map.find(key)->second);BufferFixture refused(key);pending=NULL;builds=upload_fixture::s_planBuilds;
    C(refused.Try(p,&pending)==NULL&&pending==NULL&&upload_fixture::s_planBuilds==builds);s_nativeNamedInfoVtable=verified;
    // Unknown SDK replacement on a worker invalidates conservatively; native
    // exceptions/argument forwarding remain the existing exact wrapper path.
    {NativeParams custom;Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);sdk->floatBufferSize=8;sdk->map[key]=b.Map()->map[key];actualSet(custom.data,&sdk);
     s_flushPending=0;owner=false;hook_SetNamedConstants(custom.data,b.Ptr());owner=true;C(s_flushPending==1);CachedLookup(p.data,&key);C(s_flushPending==0);}
    // Owner-only cache admission does not change worker lookup forwarding.
    owner=false;const void*workerDef=CachedLookup(p.data,&key);owner=true;C(workerDef==&b.Map()->map.find(key)->second);
    // A borrowed no-free SDK control block is also unknown, even when its raw
    // map happens to be the same map another native owner is caching.
    {NativeParams external;Ogre::GpuNamedConstantsPtr noFree(b.Map(),Ogre::SPFM_NONE);actualSet(external.data,&noFree);C(!GpuNamedConstantsCacheable(external.data));
     BufferFixture noPlan(key);pending=NULL;builds=upload_fixture::s_planBuilds;C(noPlan.Try(external,&pending)==NULL&&pending==NULL&&upload_fixture::s_planBuilds==builds);}
    // Conservative worker rebind changes plan revision without incrementing
    // the actual-deletion diagnostic, then owner lookup flushes once.
    deleted=GpuNamedConstantsDeleted();revision=GpuNamedConstantsRevision();s_flushPending=0;owner=false;hook_SetNamedConstants(p.data,b.Ptr());owner=true;
    C(GpuNamedConstantsDeleted()==deleted&&GpuNamedConstantsRevision()!=revision&&s_flushPending==1);CachedLookup(p.data,&key);C(s_flushPending==0);
    unsigned before=forwarded;throwSet=true;bool caught=false;try{hook_SetNamedConstants(p.data,a.Ptr());}catch(int n){caught=n==71;}throwSet=false;C(caught&&forwarded==before+1&&seenParams==p.data&&seenNamed==a.Ptr());
    // Same actual buffer: an untracked map can overwrite the GPU content even
    // without any map setter/deletion between calls. The next tracked native
    // call must upload again rather than trust its earlier copy.
    upload_fixture::s_table.Clear();
    {NativeProgram native("alternating-owner",9);Definition(native,key,0);NativeParams tracked,untracked;actualSet(tracked.data,native.Ptr());
     Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);sdk->floatBufferSize=4;sdk->map[key]=native.Map()->map[key];actualSet(untracked.data,&sdk);
     float*av=*reinterpret_cast<float**>(tracked.data+0x58);float*bv=*reinterpret_cast<float**>(untracked.data+0x58);for(int i=0;i<4;++i){av[i]=1.0f;bv[i]=2.0f;}
     BufferFixture same(key);upload_fixture::s_orig=&UploadSpy;upload_fixture::s_skipReady=true;g_renderCfg.gpuUploadSkip=true;g_renderCfg.gpuUploadDiag=false;
     uploads=0;LONG gen=GpuNamedConstantsRevision();C(same.Call(tracked)==&same&&uploads==1&&gpuValues[0]==1.0f);
     C(same.Call(untracked)==&same&&uploads==2&&gpuValues[0]==2.0f);C(GpuNamedConstantsRevision()==gen);
     C(same.Call(tracked)==&same&&uploads==3&&gpuValues[0]==1.0f);
     C(same.Call(tracked)==&same&&uploads==3&&gpuValues[0]==1.0f); // identical tracked call still skips
     upload_fixture::s_table.Clear();
    }
    s_cache.Clear();upload_fixture::s_table.Clear();std::printf("%s %u GPU map lifetime controls (%u failures)\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}
