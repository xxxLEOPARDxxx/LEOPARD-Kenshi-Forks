// The fixture uses real native Ogre resources/SharedPtr and the production
// collector/state. Only the GPU pressure/reservation boundary is substituted.
#define RESOURCE_GEOMETRY_TEST
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <iostream>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreHardwareBufferManager.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreVertexIndexData.h>
#include "resources/resource_geometry.h"
#include "resources/resource_gpu_store.h"
static bool mainOwner=true,budgetAllowed=true;
static resource_gpu::U64 reserved=0;
static unsigned deviceGeneration=1;
bool IsMainThread(){return mainOwner;}
namespace resource_gpu {
bool TryGeometryReservation(U64 n,U64 cap){if(n&&(!budgetAllowed||n>cap))return false;reserved=n;return true;}
bool GeometryStandbyAllowed(U64 cap){return budgetAllowed&&reserved<=cap;}
unsigned DeviceGeneration(){return deviceGeneration;}
}
#include "../src/resources/resource_geometry.cpp"
struct TestVB:Ogre::HardwareVertexBuffer {
    TestVB(size_t n,bool system=false,bool shadow=false):HardwareVertexBuffer(NULL,1,n,HBU_STATIC,system,shadow){}
    void*lockImpl(size_t,size_t,LockOptions){return NULL;}void unlockImpl(){}
    void readData(size_t,size_t,void*){}void writeData(size_t,size_t,const void*,bool){}
};
struct TestIB:Ogre::HardwareIndexBuffer {
    TestIB(size_t n):HardwareIndexBuffer(NULL,IT_16BIT,n/2,HBU_STATIC,false,false){}
    void*lockImpl(size_t,size_t,LockOptions){return NULL;}void unlockImpl(){}
    void readData(size_t,size_t,void*){}void writeData(size_t,size_t,const void*,bool){}
};
struct Loader:Ogre::ManualResourceLoader {void loadResource(Ogre::Resource*){}};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static void TickNow(bool on=true,resource_geometry::U64 world=1){
    resource_geometry::state->ticked=false;resource_geometry::Tick(on,1ULL<<30,world);
}
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){std::cerr<<"crash RIP="<<std::hex<<p->ContextRecord->Rip<<" image="<<(void*)GetModuleHandle(NULL)<<" Ogre="<<(void*)GetModuleHandleA("OgreMain_x64.dll")<<"\n";return EXCEPTION_EXECUTE_HANDLER;}
int main(){
    SetUnhandledExceptionFilter(Crash);
    Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;
    Loader loader;Ogre::MeshManager&m=Ogre::MeshManager::getSingleton();
    typedef void(*Purge)(Ogre::ResourceManager*,bool);
    Purge purge=reinterpret_cast<Purge>((*reinterpret_cast<void***>(&m))[0x68/8]);
    Ogre::MeshPtr a=m.createManual("geometry-A","General",&loader);a->load();
    Ogre::HardwareVertexBufferSharedPtr shared(new TestVB(100,true));
    a->sharedVertexData=OGRE_NEW Ogre::VertexData;
    a->sharedVertexData->vertexBufferBinding->setBinding(0,shared);
    resource_geometry::Description d;
    CHECK(resource_geometry::Describe(a,&d));CHECK(d.buffers.empty());
    shared=Ogre::HardwareVertexBufferSharedPtr(new TestVB(120,false,true));
    a->sharedVertexData->vertexBufferBinding->setBinding(0,shared);
    a->sharedVertexData->vertexBufferBinding->setBinding(1,shared);
    Ogre::SubMesh*sub=a->createSubMesh();sub->useSharedVertices=true;
    sub->indexData->indexBuffer=Ogre::HardwareIndexBufferSharedPtr(new TestIB(24));
    CHECK(resource_geometry::Describe(a,&d));CHECK(d.buffers.size()==2);
    CHECK(d.gpuBytes==144&&d.cpuBytes==120);
    // A nonnull dedicated VertexData remains mesh-owned even when the native
    // draw currently selects shared vertices; retaining it still pins buffers.
    sub->vertexData=OGRE_NEW Ogre::VertexData;
    sub->vertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(10)));
    CHECK(resource_geometry::Describe(a,&d));CHECK(d.gpuBytes==154);
    OGRE_DELETE sub->vertexData;sub->vertexData=NULL;
    a->createPose(0,"retained-pose");CHECK(!resource_geometry::Describe(a,&d));a->removeAllPoses();
    a->sharedVertexData->hardwareShadowVolWBuffer=Ogre::HardwareVertexBufferSharedPtr(new TestVB(8));
    Ogre::IndexData*lod=OGRE_NEW Ogre::IndexData;lod->indexBuffer=sub->indexData->indexBuffer;
    sub->mLodFaceList.push_back(lod);
    CHECK(resource_geometry::Describe(a,&d));CHECK(d.gpuBytes==152&&d.buffers.size()==3);
    Ogre::MeshPtr b=m.createManual("geometry-B","General",&loader);b->load();
    b->sharedVertexData=OGRE_NEW Ogre::VertexData;b->sharedVertexData->vertexBufferBinding->setBinding(0,shared);
    resource_geometry::Init();TickNow();
    Ogre::MeshPtr pendingOnly=m.createManual("pending-not-pinned","General",&loader);pendingOnly->load();
    pendingOnly->sharedVertexData=OGRE_NEW Ogre::VertexData;
    pendingOnly->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(size_t(2ULL<<30))));
    resource_geometry::Observe(pendingOnly);Ogre::Mesh*pendingRaw=pendingOnly.get();pendingOnly.setNull();
    purge(&m,false);CHECK(!pendingRaw->isLoaded());
    CHECK(reserved==0&&resource_geometry::Snapshot().models==0&&resource_geometry::Snapshot().pending==1);
    TickNow(true,2);TickNow(); // invalidate that weak observation by world generation
    unsigned before=a.useCount();resource_geometry::Observe(a);CHECK(a.useCount()==before);
    resource_geometry::Observe(a);CHECK(resource_geometry::Snapshot().pending==1);
    TickNow();CHECK(resource_geometry::Snapshot().models==1&&reserved==152);
    CHECK(a.useCount()==before+1); // ONE actual native controlling reference
    resource_geometry::Observe(b);TickNow();CHECK(resource_geometry::Snapshot().models==2&&reserved==152);
    CHECK(resource_geometry::Snapshot().cpuShadowBytes==120); // globally unique buffer, two meshes
    resource_geometry::Observe(a);CHECK(resource_geometry::Snapshot().reused==1);
    mainOwner=false;resource_geometry::Observe(b);resource_geometry::Clear();mainOwner=true;
    CHECK(resource_geometry::Snapshot().models==2);
    // Actual native purge vtable+68: the retained MeshPtr excludes otherwise
    // unused resources. This verifies native control-block ABI, not a mock ref.
    Ogre::Mesh*raw=a.get();a.setNull();b.setNull();shared.setNull();
    purge(&m,false);CHECK(raw->isLoaded());
    budgetAllowed=false;TickNow();CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    purge(&m,false);CHECK(!raw->isLoaded());
    budgetAllowed=true;
    // Native explicit reload/unload remains authoritative and is discovered
    // by the bounded refresh; the cache never calls unload itself.
    a=m.createManual("geometry-C","General",&loader);a->load();
    a->sharedVertexData=OGRE_NEW Ogre::VertexData;
    a->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(33)));
    resource_geometry::Observe(a);TickNow();CHECK(resource_geometry::Snapshot().models==1&&reserved==33);
    a->unload();TickNow();CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    a->load();a->sharedVertexData=OGRE_NEW Ogre::VertexData;
    a->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(50)));
    resource_geometry::Observe(a);TickNow();CHECK(reserved==50);
    a->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(80)));
    TickNow();CHECK(reserved==80&&resource_geometry::Snapshot().heldBytes==80);
    ++deviceGeneration;TickNow();CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    resource_geometry::Observe(a);TickNow();CHECK(reserved==80);
    TickNow(true,2);CHECK(reserved==0&&resource_geometry::Snapshot().models==0);
    resource_geometry::Observe(a);TickNow(true,2);CHECK(reserved==80);
    TickNow(false,2);CHECK(reserved==0&&resource_geometry::Snapshot().models==0);
    resource_geometry::Observe(a);CHECK(resource_geometry::Snapshot().pending==0);
    TickNow(true,2);
    a->_setLodInfo(2);Ogre::MeshLodUsage usage;usage.manualName="must-not-be-loaded";a->_setLodUsage(1,usage);
    CHECK(!resource_geometry::Describe(a,&d));CHECK(!m.resourceExists("must-not-be-loaded"));
    a->_setLodInfo(1);
    // Fixed pending queue: main-hook acquisition does no heap allocation and
    // cannot retain an unbounded stream of unknown/unloaded resources.
    for(unsigned n=0;n<140;++n){char name[50];sprintf_s(name,"pending-%u",n);Ogre::MeshPtr p=m.createManual(name,"General",&loader);resource_geometry::Observe(p);}
    CHECK(resource_geometry::Snapshot().pending==128);
    TickNow(true,3);CHECK(resource_geometry::Snapshot().pending==0);
    CHECK(resource_geometry::Snapshot().rejected>=12);
    resource_geometry::Clear();a.setNull();m.removeAll();
    std::cout<<"geometry actual Ogre collector/lifetime/purge checks="<<checks<<"\n";return 0;
}
