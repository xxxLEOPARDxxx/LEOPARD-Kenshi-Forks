// Reuse only the independent DDS/WARP fixture generator; all adoption hooks,
// descriptor matching, cache accounting and COM dispatch are production code.
#define RESOURCE_GPU_INCLUDE_STORE
#define main ExistingGpuFixtureMain
#include "test_resource_gpu.cpp"
#undef main
#define RESOURCE_GPU_BRIDGE_TEST
#include "../src/resources/resource_gpu_bridge.cpp"
#include "../src/resources/resource_biome_manifest.cpp"
#include "resources/resource_array_job.h"
intptr_t KlibRealAddressOf(void*){return 0;}
const BiomeMap* ZoneManager::getBiomeMap() const{return NULL;}
LARGE_INTEGER qpcFrequency,pluginStartTime;
bool IsMainThread(){return true;}
static resource_gpu::Job actualJob;
bool ResourceBuildArrayJob(const void*request,resource_gpu::Job&j){return resource_forward::CopyArrayJob(request,7,j);}
unsigned __int64 ResourceWorldGeneration(){return 7;}
static ID3D11Texture2D*nativeResult=NULL;
static ID3D11DeviceContext*immediate=NULL;
static resource_gpu::ReadyPtr expected;
static bool changeFirst=false;
struct RequestFixture {
    __declspec(align(16)) unsigned char bytes[sizeof(ResourceLoader::TextureArrayLoadData)];
    typedef decltype(((ResourceLoader::TextureArrayLoadData*)0)->filesNames) Paths;
    std::string*name,*group;Paths*paths;
    RequestFixture(){memset(bytes,0,sizeof(bytes));name=new(bytes+8)std::string("Terrain0_"+std::string(255,'s'));group=new(bytes+0x30)std::string("Landscape");paths=new(bytes+0xa0)Paths(6,"BASE.DDS");}
    ~RequestFixture(){paths->~Paths();group->~basic_string();name->~basic_string();}
};
static Ogre::TexturePtr* FakeNative(void*,Ogre::TexturePtr*out){
    D3D11_TEXTURE2D_DESC desc=expected->desc;
    Check(SUCCEEDED(currentDevice->CreateTexture2D(&desc,NULL,&nativeResult)),"actual intercepted device create");
    for(unsigned sub=0;sub<expected->subresources.size();++sub){
        const resource_gpu::Span&sp=expected->subresources[sub];
        D3D11_BOX box={0,0,0,sp.width,sp.height,1}; // actual native D3D writer's full-slice box
        if(changeFirst&&sub==0){std::vector<unsigned char>changed(sp.data,sp.data+sp.bytes);changed[0]^=1;immediate->UpdateSubresource(nativeResult,sub,&box,&changed[0],sp.rowPitch,0);}
        else immediate->UpdateSubresource(nativeResult,sub,&box,sp.data,sp.rowPitch,0);
    }
    new(out)Ogre::TexturePtr();return out;
}
int main(){
    QueryPerformanceFrequency(&qpcFrequency);QueryPerformanceCounter(&pluginStartTime);
    ID3D11Device*d=NULL;D3D_FEATURE_LEVEL level;
    Check(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&d,&level,&immediate)),"bridge WARP device");
    Check(Patch(d,5,createHooks,creates,createCount)&&Patch(immediate,48,updateHooks,updates,updateCount),"real COM vtable admission");
    currentDevice=d;InterlockedExchange(&enabled,1);originalArray=&FakeNative;
    resource_biome::ready=true;resource_biome::mainId=GetCurrentThreadId();
    RequestFixture request;
    if(!resource_gpu::initialized){InitializeCriticalSection(&resource_gpu::lock);resource_gpu::initialized=true;}
    InterlockedExchange(&resource_gpu::enabled,1);resource_gpu::world=7;resource_gpu::ceiling=1ULL<<30;
    for(unsigned pass=0;pass<2;++pass){
        actualJob.worldGeneration=7;
        for(unsigned i=0;i<6;++i)actualJob.paths[i]="D:\\Kenshi\\data\\land\\base.dds";
        actualJob.key=resource_forward::TerrainKey(std::string(255,'s'),0,actualJob.paths);
        resource_cache::FilePtr sources[6];for(unsigned i=0;i<6;++i)sources[i]=Source(0,i);
        std::string error;Check(resource_gpu::BuildTexture(d,actualJob,sources,&expected,&error),"ready native byte fixture");
        ID3D11Texture2D*identity=expected->AddRefTexture();
        resource_gpu::StoreAccess::Charge(expected);InterlockedExchangeAdd64(&resource_gpu::allocated,LONGLONG(expected->gpuBytes));
        resource_gpu::Entry e={expected,0,resource_gpu::Read(resource_gpu::epoch)};resource_gpu::entries[actualJob.key]=e;
        LONG oldSkipped=skipped,oldMismatch=mismatches;
        changeFirst=pass==1;
        std::aligned_storage<sizeof(Ogre::TexturePtr),__alignof(Ogre::TexturePtr)>::type storage;
        Ogre::TexturePtr*out=reinterpret_cast<Ogre::TexturePtr*>(&storage);
        Check(LoadedArray(request.bytes,out)==out,"actual SDK request, basename key and member hidden-return consumer");out->~SharedPtr();
        Check(nativeResult==identity,"native received actual precreated texture, no duplicate GPU object");
        Check(resource_gpu::Read64(resource_gpu::allocated)==0,"adopted graphics memory detached exactly once");
        Check(skipped-oldSkipped==LONG(pass?0:expected->subresources.size()),"byte-equal arrays skipped; changed source abandons shortcut");
        Check(mismatches-oldMismatch==LONG(pass),"changed source delegated to actual native update");
        D3D11_TEXTURE2D_DESC staging=expected->desc;staging.Usage=D3D11_USAGE_STAGING;staging.BindFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D*readback=NULL;Check(SUCCEEDED(d->CreateTexture2D(&staging,NULL,&readback)),"readback texture");immediate->CopyResource(readback,nativeResult);
        D3D11_MAPPED_SUBRESOURCE map={};Check(SUCCEEDED(immediate->Map(readback,0,D3D11_MAP_READ,0,&map)),"native adoption readback");
        Check(*static_cast<unsigned char*>(map.pData)==(pass?1:0),"fallback changed pixel actually reached GPU");immediate->Unmap(readback,0);
        readback->Release();nativeResult->Release();nativeResult=NULL;identity->Release();expected.reset();
    }
    InterlockedExchange(&enabled,0);currentDevice=NULL;immediate->Release();d->Release();
    std::printf("native bridge TLS/COM/adoption/upload-fallback tests PASS\n");
}
