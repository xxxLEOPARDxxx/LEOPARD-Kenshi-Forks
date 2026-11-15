// Actual Ogre maps/params/control blocks. The runner includes the unmodified
// production wrapper bodies, stripping includes only; installation is stubbed.
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include <exception>
#include <ogre/OgreRoot.h>
#include <ogre/OgreGpuProgram.h>
#include <ogre/OgreGpuProgramParams.h>
#include "render/render_levers.h"
#include "render/render_config.h"
#include "render/module_hooks.h"
#include "render/gpu_param_cache.h"
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
// PRODUCTION_BODY
static HMODULE ogre=NULL;static unsigned checks=0,forwarded=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%u %s\n",__LINE__,#x);return 1;}}while(0)
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
int main(){
    Ogre::Root root("","","");ogre=GetModuleHandleA("OgreMain_x64.dll");C(ogre!=NULL);
    NativeDeleteHook deletion;
    s_nativeNamedInfoVtable=VerifiedNativeNamedInfoVtable();C(s_nativeNamedInfoVtable==reinterpret_cast<const char*>(ogre)+0x684050);
    refuseImage=true;C(VerifiedNativeNamedInfoVtable()==NULL);refuseImage=false;
    {LONG beforeSdk=GpuNamedConstantsDeleted();Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);sdk.setNull();std::printf("SDK-owned map deletion notifications=%ld\n",GpuNamedConstantsDeleted()-beforeSdk);}
    actualSet=reinterpret_cast<SetNamedConstants_t>(Symbol("?_setNamedConstants@GpuProgramParameters@Ogre@@QEAAXAEBV?$SharedPtr@UGpuNamedConstants@Ogre@@@2@@Z"));
    s_origSetNamed=&SetSpy;s_origGetConstDef=reinterpret_cast<GetConstDef_t>(Symbol(GET_CONST_DEF_SYMBOL));
    Ogre::String key="tint";NativeProgram a("map-A",1),b("map-B",2);NativeParams p,q;
    Ogre::GpuNamedConstants*oldMap=a.Map();Ogre::GpuNamedConstants*newMap=b.Map();C(oldMap&&newMap&&oldMap!=newMap);
    const void*oldDef=Definition(a,key,0),*newDef=Definition(b,key,4);
    C(reinterpret_cast<const char*>(oldDef)-reinterpret_cast<const char*>(&a.Map()->map.begin()->first)==GPC_KEY_TO_DEF);
    C(NamedConstantsOf(p.data)==NULL);
    hook_SetNamedConstants(p.data,a.Ptr());hook_SetNamedConstants(q.data,a.Ptr());C(forwarded==2);
    C(CachedLookup(q.data,&key)==oldDef&&CachedLookup(q.data,&key)==oldDef);
    unsigned before=forwarded,incomingRefs=static_cast<const Ogre::GpuNamedConstantsPtr*>(b.Ptr())->useCount();hook_SetNamedConstants(p.data,b.Ptr());
    C(forwarded==before+1&&seenParams==p.data&&seenNamed==b.Ptr()&&NamedConstantsOf(p.data)==newMap);
    C(seenIncomingRefs==incomingRefs); // borrowed raw pointer, no transient SharedPtr copy
    C(s_cache.Find(oldMap,Key(key))==oldDef); // FAIL-FIRST: another params still owns this valid map.
    C(CachedLookup(p.data,&key)==newDef&&CachedLookup(q.data,&key)==oldDef);
    const void*verifiedInfo=s_nativeNamedInfoVtable;s_nativeNamedInfoVtable=NULL;hook_SetNamedConstants(p.data,a.Ptr());
    C(s_cache.DropMap(newMap)==0);s_nativeNamedInfoVtable=verifiedInfo;hook_SetNamedConstants(p.data,b.Ptr());
    C(s_cache.Find(oldMap,Key(key))==oldDef&&CachedLookup(p.data,&key)==newDef);
    // The same-pointer compatibility path still drops entries even though
    // native SharedPtr assignment itself is a no-op for equal control blocks.
    hook_SetNamedConstants(p.data,b.Ptr());C(s_cache.DropMap(newMap)==0);
    C(CachedLookup(p.data,&key)==newDef);
    b.Map()->map.clear();newDef=Definition(b,key,8);hook_SetNamedConstants(p.data,b.Ptr());
    C(s_cache.DropMap(newMap)==0&&CachedLookup(p.data,&key)==newDef);
    // Different-map replacement on a worker does not request global flush
    // while other real native owners keep its old map alive.
    s_flushPending=0;owner=false;hook_SetNamedConstants(p.data,a.Ptr());owner=true;
    C(s_flushPending==0&&NamedConstantsOf(p.data)==oldMap&&s_cache.Find(newMap,Key(key))==newDef);
    // Native final-reference destruction runs the real production delete hook
    // before freeing actual map nodes, including during a native setter.
    a.Fresh();LONG deletions=GpuNamedConstantsDeleted();
    hook_SetNamedConstants(p.data,b.Ptr());C(GpuNamedConstantsDeleted()==deletions);
    hook_SetNamedConstants(q.data,b.Ptr());C(GpuNamedConstantsDeleted()==deletions+1&&s_cache.DropMap(oldMap)==0);
    // Off-owner final release requests one flush; the next owner lookup clears
    // stale entries before dereferencing their now-deleted native map nodes.
    const void*priorB=newMap;b.Fresh();C(CachedLookup(p.data,&key)==newDef);
    owner=false;hook_SetNamedConstants(p.data,a.Ptr());C(s_flushPending==0);
    hook_SetNamedConstants(q.data,a.Ptr());owner=true;
    C(s_flushPending==1&&GpuNamedConstantsDeleted()==deletions+2);
    Definition(a,key,12);C(CachedLookup(p.data,&key)!=NULL&&s_flushPending==0&&s_cache.DropMap(priorB)==0);
    // Native's internal setter requires a nonempty map. Controlled SEH guards
    // prove invalid inputs retain the exact native fault/partial-state behavior
    // without taking down this standalone fixture or inventing null semantics.
    const void*live=a.Map();const void*liveDef=CachedLookup(p.data,&key);
    {NativeParams direct,wrapped;actualSet(direct.data,a.Ptr());actualSet(wrapped.data,a.Ptr());Ogre::GpuNamedConstantsPtr empty;
     before=forwarded;DWORD directFault=FaultSet(direct.data,&empty,false),wrappedFault=FaultSet(wrapped.data,&empty,true);
     C(directFault==EXCEPTION_ACCESS_VIOLATION&&wrappedFault==directFault&&forwarded==before+1&&seenNamed==&empty);
     C(NamedConstantsOf(direct.data)==NULL&&NamedConstantsOf(wrapped.data)==NULL&&s_cache.Find(live,Key(key))==liveDef);
     actualSet(direct.data,a.Ptr());actualSet(wrapped.data,a.Ptr());before=forwarded;
     directFault=FaultSet(direct.data,NULL,false);wrappedFault=FaultSet(wrapped.data,NULL,true);
     C(directFault==EXCEPTION_ACCESS_VIOLATION&&wrappedFault==directFault&&forwarded==before+1&&seenNamed==NULL);
     C(NamedConstantsOf(direct.data)==live&&NamedConstantsOf(wrapped.data)==live&&s_cache.Find(live,Key(key))==liveDef);
    }
    throwSet=true;bool caught=false;before=forwarded;
    try{hook_SetNamedConstants(p.data,b.Ptr());}catch(int e){caught=e==71;}throwSet=false;
    C(caught&&forwarded==before+1&&NamedConstantsOf(p.data)==live&&s_cache.Find(live,Key(key))==liveDef);
    // Actual setter still grows the real parameter buffers for its new map.
    b.Map()->floatBufferSize=16;b.Map()->doubleBufferSize=3;b.Map()->intBufferSize=5;b.Map()->uintBufferSize=7;hook_SetNamedConstants(p.data,b.Ptr());
    C(NamedConstantsOf(p.data)==b.Map());
    C(Values(p,0x58,0x60,4)==16&&Values(p,0x78,0x80,8)==3&&Values(p,0x98,0xa0,4)==5&&Values(p,0xb8,0xc0,4)==7);
    {NativeParams sdkParams;Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);
     Ogre::GpuConstantDefinition d;d.constType=Ogre::GCT_FLOAT4;d.physicalIndex=0;d.elementSize=4;d.arraySize=1;sdk->floatBufferSize=4;sdk->map[key]=d;
     actualSet(sdkParams.data,&sdk);const void*sdkMap=sdk.get();C(CachedLookup(sdkParams.data,&key)==&sdk->map.find(key)->second);
     sdk.setNull();LONG beforeSdk=GpuNamedConstantsDeleted();hook_SetNamedConstants(sdkParams.data,b.Ptr());
     C(GpuNamedConstantsDeleted()==beforeSdk&&s_cache.DropMap(sdkMap)==0); // SDK deleter bypasses DAC30: preserve explicit invalidation.
    }
    {NativeParams sdkParams;Ogre::GpuNamedConstantsPtr sdk(OGRE_NEW Ogre::GpuNamedConstants);
     sdk->floatBufferSize=16;sdk->map[key]=a.Map()->map.find(key)->second;actualSet(sdkParams.data,&sdk);const void*sdkMap=sdk.get();CachedLookup(sdkParams.data,&key);
     owner=false;hook_SetNamedConstants(sdkParams.data,b.Ptr());owner=true;C(s_flushPending==1);
     CachedLookup(q.data,&key);C(s_flushPending==0&&s_cache.DropMap(sdkMap)==0);
    }
    {NativeProgram manual("manual-mapping",3);NativeParams retained;const void*oldManual=manual.Map();const void*oldManualDef=Definition(manual,key,0);
     actualSet(retained.data,manual.Ptr());CachedLookup(retained.data,&key);
     typedef void(*ManualSet)(void*,const Ogre::GpuNamedConstants&);
     reinterpret_cast<ManualSet>(Symbol("?setManualNamedConstants@GpuProgram@Ogre@@UEAAXAEBUGpuNamedConstants@2@@Z"))(manual.data,*a.Map());
     C(manual.Map()!=oldManual&&NamedConstantsOf(retained.data)==oldManual&&s_cache.Find(oldManual,Key(key))==oldManualDef);
     hook_SetNamedConstants(retained.data,manual.Ptr());C(s_cache.DropMap(oldManual)==0);
     C(CachedLookup(retained.data,&key)==&manual.Map()->map.find(key)->second);
    }
    s_cache.Clear();std::printf("PASS %u actual Ogre parameter/map/rebind/deletion controls\n",checks);return 0;
}
