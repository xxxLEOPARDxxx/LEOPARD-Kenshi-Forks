#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include "resources/resource_forward.h"
#include "resources/resource_adoption_policy.h"
#include "resources/resource_biome_manifest.h"
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#include "game/klib_bindings.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreRenderSystem.h>
#include <ogre/OgreTexture.h>
#include <kenshi/ResourceLoader.h>
#include "base/klib_include_end.h"
#include <sstream>
namespace {
typedef HRESULT (STDMETHODCALLTYPE *CreateFn)(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**);
typedef void (STDMETHODCALLTYPE *UpdateFn)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
// Member hidden-return ABI: RCX=request, RDX=TexturePtr output.
typedef Ogre::TexturePtr* (*ArrayFn)(void*,Ogre::TexturePtr*);
ArrayFn originalArray=NULL;
struct Slot {void**table;void*original;};
Slot creates[8],updates[8];volatile LONG createCount=0,updateCount=0,enabled=0;
ID3D11Device* currentDevice=NULL;
bool arrayInstalled=false;
struct Context {resource_gpu::ReadyPtr ready;ID3D11Device*device;ID3D11Resource*adopted;Context():device(NULL),adopted(NULL){}};
__declspec(thread) Context* context=NULL;
volatile LONG adopted=0,skipped=0,mismatches=0,descRefused=0;
volatile LONG consumerCalls=0,consumerOff=0,consumerNonMain=0,requestRefused=0,readyHits=0,readyMisses=0;
__declspec(align(8)) volatile LONGLONG verifyUs=0;
bool EnsureContext(ID3D11Device*);
HRESULT CreateImpl(CreateFn original,ID3D11Device*d,const D3D11_TEXTURE2D_DESC*desc,const D3D11_SUBRESOURCE_DATA*data,ID3D11Texture2D**out){
    Context*c=context;
    if(c&&c->ready&&!c->adopted&&d==c->device&&!data&&desc&&out){
        if(resource_adoption::SameDesc(*desc,c->ready->desc)&&EnsureContext(d)){
            ID3D11Texture2D*t=c->ready->AddRefTexture();
            if(t){*out=t;c->adopted=t;resource_gpu::MarkAdopted(c->ready);InterlockedIncrement(&adopted);return S_OK;}
        }else InterlockedIncrement(&descRefused);
    }
    return original?original(d,desc,data,out):E_FAIL;
}
void UpdateImpl(UpdateFn original,ID3D11DeviceContext*c,ID3D11Resource*dst,UINT sub,const D3D11_BOX*box,const void*data,UINT row,UINT depth){
    Context*ctx=context;
    if(ctx&&ctx->ready&&dst==ctx->adopted&&sub<ctx->ready->subresources.size()){
        LONGLONG begin=QpcNow();bool equal=resource_adoption::Matches(ctx->ready->subresources[sub],box,data,row,depth);
        InterlockedExchangeAdd64(&verifyUs,QpcDeltaUs(begin,QpcNow()));
        if(equal){InterlockedIncrement(&skipped);return;}
        InterlockedIncrement(&mismatches);
        // A changed source invalidates the remainder of this array's upload
        // shortcut. Native uploads finish it; do not add repeated comparisons
        // or fight the driver's fast-dispatch changes on this fallback path.
        ctx->ready.reset();
    }
    original(c,dst,sub,box,data,row,depth);
}
// Each installed slot has a distinct thunk carrying its original target. A
// later overlay copying a vtable still chains correctly, without a hot lookup.
template<int N> HRESULT STDMETHODCALLTYPE CreateThunk(ID3D11Device*d,const D3D11_TEXTURE2D_DESC*s,const D3D11_SUBRESOURCE_DATA*p,ID3D11Texture2D**out){return CreateImpl(reinterpret_cast<CreateFn>(creates[N].original),d,s,p,out);}
template<int N> void STDMETHODCALLTYPE UpdateThunk(ID3D11DeviceContext*c,ID3D11Resource*d,UINT sub,const D3D11_BOX*b,const void*p,UINT row,UINT depth){UpdateImpl(reinterpret_cast<UpdateFn>(updates[N].original),c,d,sub,b,p,row,depth);}
void*createHooks[8]={reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<0>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<1>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<2>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<3>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<4>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<5>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<6>)),reinterpret_cast<void*>(static_cast<CreateFn>(&CreateThunk<7>))};
void*updateHooks[8]={reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<0>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<1>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<2>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<3>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<4>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<5>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<6>)),reinterpret_cast<void*>(static_cast<UpdateFn>(&UpdateThunk<7>))};
bool Patch(void*object,unsigned index,void**replacements,Slot*slots,volatile LONG&count){
    void**table=*reinterpret_cast<void***>(object);LONG n=InterlockedCompareExchange(&count,0,0);
    void*old=table[index];LONG selected=-1;
    for(LONG i=0;i<n;++i){if(old==replacements[i])return true;if(old==slots[i].original)selected=i;}
    if(!old||(selected<0&&n>=8))return false;
    DWORD oldProtect=0;if(!VirtualProtect(table+index,sizeof(void*),PAGE_READWRITE,&oldProtect))return false;
    if(selected<0){selected=n;slots[n].table=table;slots[n].original=old;InterlockedIncrement(&count);}
    // The D3D runtime may replace its fast-dispatch entry after Map/Unmap.
    // Re-admit known original targets without exhausting slots or breaking chains.
    bool ok=InterlockedCompareExchangePointer(table+index,replacements[selected],old)==old;
    DWORD unused;VirtualProtect(table+index,sizeof(void*),oldProtect,&unused);return ok;
}
bool EnsureContext(ID3D11Device*d){
    ID3D11DeviceContext*c=NULL;d->GetImmediateContext(&c);if(!c)return false;
    bool ok=Patch(c,48,updateHooks,updates,updateCount);c->Release();return ok;
}
struct Scope {Context*old;Scope(Context*c):old(context){context=c;}~Scope(){context=old;}};
Ogre::TexturePtr* LoadedArray(void*request,Ogre::TexturePtr*out){
    InterlockedIncrement(&consumerCalls);
    if(!IsMainThread()){InterlockedIncrement(&consumerNonMain);return originalArray(request,out);}
    if(!InterlockedCompareExchange(&enabled,0,0)||!currentDevice){InterlockedIncrement(&consumerOff);return originalArray(request,out);}
    Context c;resource_gpu::Job job;
    try{if(ResourceBuildArrayJob(request,job))c.ready=resource_gpu::TakeReady(job.key,job.worldGeneration);else InterlockedIncrement(&requestRefused);}catch(...){InterlockedIncrement(&requestRefused);}
    if(!c.ready){InterlockedIncrement(&readyMisses);return originalArray(request,out);}
    InterlockedIncrement(&readyHits);
    c.device=currentDevice;Scope scope(&c);
    // Native Image loading, material objects, SRVs and one manager notification
    // all remain native. Only exact-descriptor allocation and byte-equal uploads change.
    return originalArray(request,out);
}
}
#ifndef RESOURCE_GPU_BRIDGE_TEST
void ResourceGpuBridgeInit(){
    if(arrayInstalled)return;
    // Materials enqueue TextureArrayLoadData. The old ZoneMap array builder
    // A10080 has no active caller in this build; use the actual loader.
    if(NativeProfileIsGog()) {KEO_DETAIL("Resource GPU warmup: native array seam not yet admitted for GOG");return;}
    unsigned char expected[64]={0x4c,0x8b,0xdc,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x40,0x01,0x00,0x00,0x48,0xc7,0x84,0x24,0xb0,0x00,0x00,0x00,0xfe,0xff,0xff,0xff,0x49,0x89,0x5b,0x18,0x49,0x89,0x73,0x20,0x48,0x8b,0x05,0xca,0xd7,0xcc,0x01,0x48,0x33,0xc4,0x48,0x89,0x84,0x24,0x30,0x01,0x00,0x00,0x48,0x8b,0xda,0x4c,0x8b,0xe1,0x48};
    void*target=reinterpret_cast<void*>(KlibRealAddress(&ResourceLoader::TextureArrayLoadData::_NV_getTexture));
    if(memcmp(target,expected,sizeof(expected))){LogError("Resource GPU warmup: native array contract refused; normal loading retained");return;}
    arrayInstalled=KenshiLib::AddHook(target,(void*)&LoadedArray,(void**)&originalArray)==KenshiLib::SUCCESS;
    if(!arrayInstalled)LogError("Resource GPU warmup: array hook failed; normal loading retained");
}
namespace {bool RefuseDevice(){
    if(currentDevice){resource_gpu::SetDevice(NULL);ID3D11Device*old=currentDevice;currentDevice=NULL;old->Release();}
    return false;
}}
bool ResourceGpuBridgeTick(bool on){
    InterlockedExchange(&enabled,0);
    if(on&&!arrayInstalled){static bool tried=false;if(!tried){tried=true;ResourceGpuBridgeInit();}}
    if(!on||!arrayInstalled)return RefuseDevice();
    try{
        Ogre::Root*root=Ogre::Root::getSingletonPtr();if(!root||!root->getRenderSystem())return RefuseDevice();
        Ogre::RenderSystem*render=root->getRenderSystem();
        HMODULE module=GetModuleHandleA("RenderSystem_Direct3D11_x64.dll");DWORD stamp=0,size=0;
        if(!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f9e9||size!=0xbf000)return RefuseDevice();
        // Direct verified void-return method avoids an unverified SDK virtual-slot index.
        unsigned char expected[32]={0x40,0x55,0x56,0x57,0x48,0x81,0xec,0xb0,0x01,0x00,0x00,0x48,0xc7,0x44,0x24,0x30,0xfe,0xff,0xff,0xff,0x48,0x89,0x9c,0x24,0xd0,0x01,0x00,0x00,0x48,0x8b,0x05,0xed};
        void*attr=(char*)module+0x38A20;
        MEMORY_BASIC_INFORMATION tableInfo;
        if(VirtualQuery(*reinterpret_cast<void***>(render),&tableInfo,sizeof(tableInfo))!=sizeof(tableInfo)||tableInfo.AllocationBase!=module||memcmp(attr,expected,sizeof(expected)))return RefuseDevice();
        typedef void(*GetAttr)(void*,const Ogre::String&,void*);
        ID3D11Device*device=NULL;reinterpret_cast<GetAttr>(attr)(render,"D3DDEVICE",&device);
        if(!device||!resource_gpu::DeviceCanCreateConcurrently(device))return RefuseDevice();
        ID3D11DeviceContext*immediate=NULL;device->GetImmediateContext(&immediate);if(!immediate)return RefuseDevice();
        bool hooks=Patch(device,5,createHooks,creates,createCount)&&Patch(immediate,48,updateHooks,updates,updateCount);
        immediate->Release();if(!hooks)return RefuseDevice();
        if(device!=currentDevice){
            if(!resource_gpu::SetDevice(device))return RefuseDevice();
            device->AddRef();ID3D11Device*old=currentDevice;currentDevice=device;if(old)old->Release();
        }
        InterlockedExchange(&enabled,1);
        return true;
    }catch(...){LogError("Resource GPU warmup: device admission failed; normal loading retained");return RefuseDevice();}
}
void ResourceGpuBridgeStop(){InterlockedExchange(&enabled,0);ID3D11Device*old=currentDevice;currentDevice=NULL;if(old)old->Release();}
#endif
std::string ResourceGpuBridgeStats(){
    std::ostringstream s;s<<" bridge="<<InterlockedCompareExchange(&enabled,0,0)
        <<" consumerCalls="<<InterlockedCompareExchange(&consumerCalls,0,0)<<" consumerOff="<<InterlockedCompareExchange(&consumerOff,0,0)<<" consumerNonMain="<<InterlockedCompareExchange(&consumerNonMain,0,0)
        <<" requestRefused="<<InterlockedCompareExchange(&requestRefused,0,0)<<" readyHit="<<InterlockedCompareExchange(&readyHits,0,0)<<" readyMiss="<<InterlockedCompareExchange(&readyMisses,0,0)
        <<" adopted="<<InterlockedCompareExchange(&adopted,0,0)<<" skipUploads="<<InterlockedCompareExchange(&skipped,0,0)
        <<" mismatch="<<InterlockedCompareExchange(&mismatches,0,0)<<" descRefused="<<InterlockedCompareExchange(&descRefused,0,0)
        <<" verifyUs="<<InterlockedCompareExchange64(&verifyUs,0,0);return s.str();
}
ResourceGpuProgress ResourceGpuBridgeProgress(){
    ResourceGpuProgress s={static_cast<unsigned long>(InterlockedCompareExchange(&adopted,0,0)),static_cast<unsigned long>(InterlockedCompareExchange(&skipped,0,0)),InterlockedCompareExchange(&enabled,0,0)!=0};return s;
}
