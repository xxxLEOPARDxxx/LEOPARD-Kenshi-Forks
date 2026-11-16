// The fixture uses real native Ogre resources/SharedPtr and the production
// collector/state. Only the GPU pressure/reservation boundary is substituted.
#define RESOURCE_GEOMETRY_TEST
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <iostream>
#include "../leopard/test_retention_alloc.h"
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
    void Metadata(size_t size,bool shadow=false,bool system=false){mSizeInBytes=size;mUseShadowBuffer=shadow;mSystemMemory=system;}
    void*lockImpl(size_t,size_t,LockOptions){return NULL;}void unlockImpl(){}
    void readData(size_t,size_t,void*){}void writeData(size_t,size_t,const void*,bool){}
};
struct TestIB:Ogre::HardwareIndexBuffer {
    TestIB(size_t n):HardwareIndexBuffer(NULL,IT_16BIT,n/2,HBU_STATIC,false,false){}
    void*lockImpl(size_t,size_t,LockOptions){return NULL;}void unlockImpl(){}
    void readData(size_t,size_t,void*){}void writeData(size_t,size_t,const void*,bool){}
};
struct Loader:Ogre::ManualResourceLoader {void loadResource(Ogre::Resource*){}};
// Genuine native Mesh object; controlled passive virtual getters isolate
// creator/handle identity branches while the real manager owns its maps.
struct IdentityMesh:Ogre::Mesh {
    bool wrongCreator;Ogre::ResourceHandle alternateHandle;
    IdentityMesh(Ogre::MeshManager*m,Loader*l):Ogre::Mesh(m,"identity-mesh",0xfed00001,"General",true,l),wrongCreator(false),alternateHandle(0){}
    Ogre::ResourceManager*getCreator(){return wrongCreator?NULL:Ogre::Mesh::getCreator();}
    Ogre::ResourceHandle getHandle()const{return alternateHandle?alternateHandle:Ogre::Mesh::getHandle();}
    // SDK String animation virtuals are not exported by this Ogre build.
    // Identity cases have no animations; trap any accidental invocation.
    Ogre::Animation*createAnimation(const Ogre::String&,Ogre::Real){abort();return NULL;}
    Ogre::Animation*getAnimation(const Ogre::String&)const{abort();return NULL;}
    Ogre::Animation*_getAnimationImpl(const Ogre::String&)const{abort();return NULL;}
    bool hasAnimation(const Ogre::String&)const{abort();return false;}
    void removeAnimation(const Ogre::String&){abort();}
    // This game's createAnimation export always throws unsupported. Seed only
    // its native map for the eligibility boundary, then erase before teardown.
    void FixtureAnimation(bool on){if(on)mAnimationsList["fixture-animation"]=NULL;else mAnimationsList.clear();}
};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static void TickNow(bool on=true,resource_geometry::U64 world=1){
    resource_geometry::state->ticked=false;resource_geometry::Tick(on,1ULL<<30,world);
}
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){std::cerr<<"crash RIP="<<std::hex<<p->ContextRecord->Rip<<" image="<<(void*)GetModuleHandle(NULL)<<" Ogre="<<(void*)GetModuleHandleA("OgreMain_x64.dll")<<"\n";return EXCEPTION_EXECUTE_HANDLER;}
static resource_geometry::Models::iterator LegacyGeometryOldest(){
    resource_geometry::Models&models=resource_geometry::state->models;
    resource_geometry::Models::iterator o=models.begin();for(resource_geometry::Models::iterator i=models.begin();i!=models.end();++i)if(i->second.touch<o->second.touch)o=i;return o;
}
static bool failedAfterGeometryIndex=false;
static void GeometryAllocationProbe(){if(resource_geometry::state->victims.Size())failedAfterGeometryIndex=true;}
struct CacheCleanup{~CacheCleanup(){mainOwner=true;resource_geometry::Clear();Ogre::MeshManager::getSingleton().removeAll();}};
int main(){
    SetUnhandledExceptionFilter(Crash);
    Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;
    Loader loader;Ogre::MeshManager&m=Ogre::MeshManager::getSingleton();CacheCleanup cleanup;
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
    // The native background request can pass a disposable duplicate while
    // the returned Entity resolves the registered source by name/group.
    // Exercise the real selection helper with actual native MeshPtr resources.
    struct ReturnedEntity {Ogre::MeshPtr actual;mutable unsigned reads;ReturnedEntity():reads(0){}const Ogre::MeshPtr&getMesh()const{++reads;return actual;}} entity;
    entity.actual=a;Ogre::MeshPtr temporary=m.createManual("temporary-input","General",&loader);m.remove(temporary->getHandle());
    resource_geometry::ObserveEntity(&entity,temporary);TickNow();
    CHECK(resource_geometry::Snapshot().models==1&&reserved==152);
    CHECK(resource_geometry::Snapshot().inputDifferent==1);
    resource_geometry::ObserveEntity(&entity,temporary);
    CHECK(resource_geometry::Snapshot().reused==1&&resource_geometry::Snapshot().inputDifferent==2);
    resource_geometry::Clear();TickNow();entity.actual.setNull();temporary.setNull();
    unsigned before=a.useCount();resource_geometry::Observe(a);CHECK(a.useCount()==before);
    resource_geometry::Observe(a);CHECK(resource_geometry::Snapshot().pending==1);
    TickNow();CHECK(resource_geometry::Snapshot().models==1&&reserved==152);
    CHECK(a.useCount()==before+1); // ONE actual native controlling reference
    resource_geometry::Observe(b);TickNow();CHECK(resource_geometry::Snapshot().models==2&&reserved==152);
    CHECK(resource_geometry::Snapshot().cpuShadowBytes==120); // globally unique buffer, two meshes
    resource_geometry::Observe(a);CHECK(resource_geometry::Snapshot().reused==2);
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
    // Real Ogre remove(handle) unregisters without unloading an externally
    // owned mesh. A cache pin must not preserve that unreachable resource.
    a=m.createManual("orphan-reused-name","General",&loader);a->load();
    a->sharedVertexData=OGRE_NEW Ogre::VertexData;
    a->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(61)));
    Ogre::MeshPtr alias=a;const Ogre::ResourceHandle oldHandle=a->getHandle();
    CHECK(a->getCreator()==&m);
    resource_geometry::Observe(a);TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==1&&reserved==61);
    m.remove(oldHandle);
    CHECK(m.getByHandle(oldHandle).isNull()&&a->isLoaded()&&alias.get()==a.get());
    unsigned orphanRefs=a.useCount();
    // Same native name now identifies a DIFFERENT live Mesh; releasing the
    // orphan must neither unload nor uncharge this independent replacement.
    Ogre::MeshPtr replacement=m.createManual("orphan-reused-name","General",&loader);replacement->load();
    replacement->sharedVertexData=OGRE_NEW Ogre::VertexData;
    replacement->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(67)));
    CHECK(replacement.get()!=a.get()&&replacement->getHandle()!=oldHandle);
    resource_geometry::Observe(replacement);
    TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==1&&reserved==67);
    CHECK(a.useCount()+1==orphanRefs&&a->isLoaded()&&alias->isLoaded());
    CHECK(m.getByHandle(replacement->getHandle()).get()==replacement.get()&&replacement->isLoaded());
    resource_geometry::Observe(alias);TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==1&&reserved==67);
    // Native unload remains authoritative, and surviving aliases stay alive.
    replacement->unload();TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    CHECK(a->isLoaded()&&alias->isLoaded());
    a.setNull();alias.setNull();replacement.setNull();
    IdentityMesh*identityRaw=new IdentityMesh(&m,&loader);
    Ogre::MeshPtr identity(identityRaw);
    Ogre::ResourcePtr identityResource=identity.staticCast<Ogre::Resource>();
    // Native protected addImpl (verified vtable+100), as used by createResource.
    typedef void(*Add)(Ogre::ResourceManager*,Ogre::ResourcePtr&);
    Add add=reinterpret_cast<Add>((*reinterpret_cast<void***>(&m))[0x100/8]);add(&m,identityResource);
    identity->load();identity->sharedVertexData=OGRE_NEW Ogre::VertexData;
    identity->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(73)));
    CHECK(m.getByHandle(identity->getHandle()).get()==identity.get());
    resource_geometry::Observe(identity);TickNow(true,3);CHECK(reserved==73);
    const resource_geometry::U64 mismatchBefore=resource_geometry::Snapshot().identityMismatch;
    unsigned identityRefs=identity.useCount();identityRaw->wrongCreator=true;
    TickNow(true,3);CHECK(reserved==0&&resource_geometry::Snapshot().models==0);
    CHECK(identity.useCount()+1==identityRefs&&identity->isLoaded());
    CHECK(resource_geometry::Snapshot().identityMismatch==mismatchBefore+1);
    identityRaw->wrongCreator=false;
    resource_geometry::Observe(identity);TickNow(true,3);CHECK(reserved==73);
    Ogre::MeshPtr other=m.createManual("other-handle-target","General",&loader);other->load();
    identityRefs=identity.useCount();identityRaw->alternateHandle=other->getHandle();
    CHECK(m.getByHandle(identity->getHandle()).get()==other.get());
    TickNow(true,3);CHECK(reserved==0&&resource_geometry::Snapshot().models==0);
    CHECK(identity.useCount()+1==identityRefs&&identity->isLoaded()&&other->isLoaded());
    CHECK(resource_geometry::Snapshot().identityMismatch==mismatchBefore+2);
    identityRaw->alternateHandle=0;
    m.remove(identity->getHandle());identityResource.setNull();identity.setNull();other.setNull();
    Ogre::MeshPtr orphans[5];
    for(unsigned n=0;n<5;++n){
        char name[50];sprintf_s(name,"bounded-orphan-%u",n);orphans[n]=m.createManual(name,"General",&loader);orphans[n]->load();
        orphans[n]->sharedVertexData=OGRE_NEW Ogre::VertexData;
        orphans[n]->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(11)));
        resource_geometry::Observe(orphans[n]);
    }
    TickNow(true,3);TickNow(true,3);CHECK(resource_geometry::Snapshot().models==5&&reserved==55);
    for(unsigned n=0;n<5;++n)m.remove(orphans[n]->getHandle());
    TickNow(true,3);CHECK(resource_geometry::Snapshot().models==3&&reserved==33);
    TickNow(true,3);CHECK(resource_geometry::Snapshot().models==1&&reserved==11);
    TickNow(true,3);CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    for(unsigned n=0;n<5;++n){CHECK(orphans[n]->isLoaded());orphans[n].setNull();}
    resource_geometry::Clear();a.setNull();m.removeAll();
    // Successful final-switch capture uses a borrowed returned Entity mesh.
    // Adapter substitutes only the passive Entity getter; all resources,
    // manager membership, control blocks, collector and purge are native.
    TickNow(true,3);
    resource_geometry::ObserveFinalEntity(NULL);CHECK(resource_geometry::Snapshot().pending==0);
    void*unreadable=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);CHECK(unreadable!=NULL);
    resource_geometry::Clear();resource_geometry::ObserveFinalEntity(unreadable);
    unsigned reads=entity.reads;resource_geometry::ObserveFinalEntityImpl(&entity);CHECK(entity.reads==reads);
    CHECK(resource_geometry::Snapshot().pending==0);
    TickNow(true,3);mainOwner=false;
    resource_geometry::State*savedState=resource_geometry::state;
    resource_geometry::state=static_cast<resource_geometry::State*>(unreadable);
    resource_geometry::ObserveFinalEntity(unreadable); // gate BEFORE even reading state
    resource_geometry::state=savedState;mainOwner=true;VirtualFree(unreadable,0,MEM_RELEASE);
    CHECK(resource_geometry::Snapshot().pending==0);
    Ogre::MeshPtr final=m.createManual("native-final-morph","General",&loader);final->load();
    final->sharedVertexData=OGRE_NEW Ogre::VertexData;
    final->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(91)));
    final->createPose(0,"native-base-pose");entity.actual=final;
    resource_geometry::ObserveFinalEntityImpl(&entity);TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==0&&reserved==0); // pose-bearing base rejected
    final->removeAllPoses();CHECK(final->getPoseCount()==0&&m.getByHandle(final->getHandle()).get()==final.get());
    unsigned finalRefs=final.useCount();resource_geometry::ObserveFinalEntityImpl(&entity);
    CHECK(final.useCount()==finalRefs&&resource_geometry::Snapshot().pending==1); // weak acquisition
    TickNow(true,3);CHECK(resource_geometry::Snapshot().models==1&&reserved==91&&final.useCount()==finalRefs+1);
    Ogre::Mesh*finalRaw=final.get();entity.actual.setNull();final.setNull();purge(&m,false);
    CHECK(finalRaw->isLoaded()&&m.getByName("native-final-morph").get()==finalRaw);
    resource_geometry::Clear();purge(&m,false);CHECK(!finalRaw->isLoaded());
    TickNow(true,3);
    // Native vertex-animation exclusion survives even after pose removal.
    IdentityMesh*animatedRaw=new IdentityMesh(&m,&loader);final=Ogre::MeshPtr(animatedRaw);
    Ogre::ResourcePtr animatedResource=final.staticCast<Ogre::Resource>();add(&m,animatedResource);final->load();
    final->sharedVertexData=OGRE_NEW Ogre::VertexData;
    final->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(93)));
    animatedRaw->FixtureAnimation(true);CHECK(final->getNumAnimations()==1&&final->getPoseCount()==0);
    entity.actual=final;resource_geometry::ObserveFinalEntityImpl(&entity);TickNow(true,3);
    CHECK(resource_geometry::Snapshot().models==0&&reserved==0);
    animatedRaw->FixtureAnimation(false);CHECK(final->getNumAnimations()==0&&resource_geometry::Describe(final,&d));
    animatedResource.setNull();
    // Native alwaysUnique player names remain distinct; observation never
    // aliases their pointers, handles, or names into a shared model.
    Ogre::MeshPtr players[2];
    for(unsigned n=0;n<2;++n){
        players[n]=m.createManual(n?"player_morph_102":"player_morph_101","General",&loader);players[n]->load();
        players[n]->sharedVertexData=OGRE_NEW Ogre::VertexData;
        players[n]->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(17)));
        entity.actual=players[n];resource_geometry::ObserveFinalEntityImpl(&entity);
    }
    TickNow(true,3);CHECK(resource_geometry::Snapshot().models==2&&reserved==34);
    CHECK(players[0].get()!=players[1].get()&&players[0]->getHandle()!=players[1]->getHandle());
    CHECK(m.getByName("player_morph_101").get()==players[0].get()&&m.getByName("player_morph_102").get()==players[1].get());
    resource_geometry::Clear();entity.actual.setNull();final.setNull();players[0].setNull();players[1].setNull();m.removeAll();
    // Full production capacity with real native MeshPtr ownership. Direct
    // Promote bypasses only scheduling, never native lifetime or accounting.
    TickNow(true,3);Ogre::MeshPtr indexed[resource_geometry::MaxModels];
    for(unsigned n=0;n<resource_geometry::MaxModels;++n){
        char name[48];sprintf_s(name,"victim-index-%u",n);indexed[n]=m.createManual(name,"General",&loader);indexed[n]->load();
        indexed[n]->sharedVertexData=OGRE_NEW Ogre::VertexData;
        indexed[n]->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(1)));
        CHECK(resource_geometry::Describe(indexed[n],&d));CHECK(resource_geometry::Promote(indexed[n],d,n%7));
        CHECK(resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());
        if(n==1){
            resource_geometry::U64 oldTouch=resource_geometry::state->models.find(indexed[0].get())->second.touch;
            Ogre::HardwareVertexBufferSharedPtr old=indexed[0]->sharedVertexData->vertexBufferBinding->getBuffer(0);
            indexed[0]->sharedVertexData->vertexBufferBinding->setBinding(0,Ogre::HardwareVertexBufferSharedPtr(new TestVB(3)));TickNow(true,3);
            CHECK(resource_geometry::state->models.find(indexed[0].get())->second.touch==oldTouch);CHECK(resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());
            indexed[0]->sharedVertexData->vertexBufferBinding->setBinding(0,old);TickNow(true,3);CHECK(reserved==2);
            CHECK(!resource_geometry::Promote(indexed[0],d,0));CHECK(reserved==2&&resource_geometry::state->victims.Size()==2);
        }
    }
    CHECK(resource_geometry::state->victims.Size()==resource_geometry::MaxModels&&reserved==resource_geometry::MaxModels);
    resource_geometry::U64 held=reserved;CHECK(!resource_geometry::Promote(indexed[0],d,0));CHECK(reserved==held);
    resource_geometry::state->clock=1024;resource_geometry::Observe(resource_geometry::state->victims.Oldest()->second.mesh);
    CHECK(resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());
    resource_geometry::state->clock=~0ULL-1;resource_geometry::Observe(indexed[0]);resource_geometry::Observe(indexed[1]);
    CHECK(resource_geometry::state->clock==0&&resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());
    for(unsigned n=0;n<16;++n){
        resource_geometry::Models::iterator i=resource_geometry::state->models.find(indexed[n*11].get());resource_geometry::U64 touch=i->second.touch;
        resource_geometry::Release(i);CHECK(resource_geometry::Describe(indexed[n*11],&d));CHECK(resource_geometry::Promote(indexed[n*11],d,touch));
        CHECK(resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());
    }
    Ogre::MeshPtr overflow=m.createManual("victim-index-overflow","General",&loader);overflow->load();
    CHECK(!resource_geometry::Promote(overflow,d,0));CHECK(reserved==held&&resource_geometry::state->victims.Size()==resource_geometry::MaxModels);overflow.setNull();
    const Ogre::Mesh*victims[32];
    // Deterministic oracle scenario before the real pressure Tick.
    for(unsigned n=0;n<resource_geometry::MaxModels;++n){resource_geometry::Models::iterator i=resource_geometry::state->models.find(indexed[n].get());i->second.touch=n%7;resource_geometry::state->victims.Changed(i);}
    for(unsigned n=0;n<32;++n){resource_geometry::Models::iterator i=LegacyGeometryOldest();victims[n]=i->first;resource_geometry::Oldest();CHECK(resource_geometry::state->models.count(victims[n])==0);CHECK(resource_geometry::state->victims.Oldest()==LegacyGeometryOldest());}
    budgetAllowed=false;TickNow(true,3);CHECK(resource_geometry::state->victims.Size()==resource_geometry::MaxModels-64);budgetAllowed=true;
    resource_geometry::Clear();CHECK(resource_geometry::state->victims.Size()==0&&reserved==0);
    TickNow(true,3);CHECK(resource_geometry::Describe(indexed[0],&d));
    budgetAllowed=false;CHECK(!resource_geometry::Promote(indexed[0],d,0));CHECK(resource_geometry::state->victims.Size()==0&&reserved==0);budgetAllowed=true;
    // Failed Account must leave every index/accounting member unchanged.
    resource_geometry::Count conflict={1,2,0};resource_geometry::state->buffers.insert(std::make_pair(d.buffers[0].id,conflict));
    CHECK(!resource_geometry::Promote(indexed[0],d,0));CHECK(resource_geometry::state->victims.Size()==0&&reserved==0);resource_geometry::state->buffers.clear();
    static char bufferKeys[resource_geometry::MaxBuffers];resource_geometry::Count dummy={1,1,0};
    for(unsigned n=0;n<resource_geometry::MaxBuffers;++n)resource_geometry::state->buffers.insert(std::make_pair(reinterpret_cast<const Ogre::HardwareBuffer*>(&bufferKeys[n]),dummy));
    CHECK(!resource_geometry::Promote(indexed[0],d,0));CHECK(resource_geometry::state->victims.Size()==0&&reserved==0);resource_geometry::state->buffers.clear();
    allocationFailureProbe=&GeometryAllocationProbe;
    unsigned failedBefore=allocationFailures;
    for(int fail=0;fail<12;++fail){
        resource_geometry::Clear();TickNow(true,3);unsigned refs=indexed[0].useCount();resource_geometry::Observe(indexed[0]);
        unsigned prior=allocationFailures;allocationFailAfter=fail;TickNow(true,3);allocationFailAfter=-1;
        CHECK(resource_geometry::state->models.size()==resource_geometry::state->victims.Size());
        if(allocationFailures!=prior){CHECK(reserved==0&&resource_geometry::state->models.empty()&&resource_geometry::state->buffers.empty());CHECK(indexed[0].useCount()==refs);}
        else CHECK(resource_geometry::state->victims.Size()==1&&reserved==1&&indexed[0].useCount()==refs+1);
    }
    allocationFailureProbe=NULL;CHECK(allocationFailures>failedBefore&&failedAfterGeometryIndex);
    resource_geometry::Clear();for(unsigned n=0;n<resource_geometry::MaxModels;++n)indexed[n].setNull();m.removeAll();
    // The collector has already built one owned description. Admission must
    // transfer that storage, not allocate copies in temporary map entries.
    TickNow(true,4);
    Ogre::MeshPtr transfer=m.createManual("description-transfer","General",&loader);transfer->load();
    for(unsigned part=0;part<8;++part){
        Ogre::SubMesh*s=transfer->createSubMesh();s->vertexData=OGRE_NEW Ogre::VertexData;
        for(unsigned binding=0;binding<64;++binding)
            s->vertexData->vertexBufferBinding->setBinding((unsigned short)binding,Ogre::HardwareVertexBufferSharedPtr(new TestVB(1)));
    }
    CHECK(resource_geometry::Describe(transfer,&d));CHECK(d.buffers.size()==resource_geometry::MaxMeshBuffers);
    const resource_geometry::Buffer*collectedStorage=&d.buffers[0];
    unsigned originalRefs=transfer.useCount();
    budgetAllowed=false;CHECK(!resource_geometry::Promote(transfer,d,77));budgetAllowed=true;
    CHECK(d.buffers.size()==512&&&d.buffers[0]==collectedStorage&&d.gpuBytes==512&&d.cpuBytes==0);
    CHECK(transfer.useCount()==originalRefs&&reserved==0);
    const unsigned failureSites[]={0,1,2,3,7,63,255,511,512};
    for(unsigned site=0;site<sizeof(failureSites)/sizeof(failureSites[0]);++site){
        bool threw=false;allocationFailAfter=(int)failureSites[site];
        try{resource_geometry::Promote(transfer,d,77);}catch(const std::bad_alloc&){threw=true;}
        allocationFailAfter=-1;
        bool retainedInput=d.buffers.size()==512&&&d.buffers[0]==collectedStorage&&d.gpuBytes==512&&d.cpuBytes==0;
        resource_geometry::Clear();bool clean=reserved==0&&resource_geometry::state->models.empty()&&resource_geometry::state->buffers.empty()&&transfer.useCount()==originalRefs;
        TickNow(true,4);CHECK(threw);CHECK(retainedInput);CHECK(clean);
    }
    watchedAllocationSize=d.buffers.size()*sizeof(resource_geometry::Buffer);watchedAllocations=0;
    bool promoted=resource_geometry::Promote(transfer,d,77);
    watchedAllocationSize=0;unsigned descriptionCopies=watchedAllocations;
    resource_geometry::Models::iterator admitted=resource_geometry::state->models.find(transfer.get());
    bool sameStorage=admitted!=resource_geometry::state->models.end()&&!admitted->second.desc.buffers.empty()&& &admitted->second.desc.buffers[0]==collectedStorage;
    bool consumed=d.buffers.empty()&&d.gpuBytes==0&&d.cpuBytes==0;
    bool accounted=reserved==512&&resource_geometry::state->buffers.size()==512&&transfer.useCount()==originalRefs+1;
    unsigned failuresBeforeRefresh=allocationFailures;
    allocationFailAfter=0;for(unsigned repeat=0;repeat<16;++repeat)TickNow(true,4);allocationFailAfter=-1;
    resource_geometry::Models::iterator stable=resource_geometry::state->models.find(transfer.get());
    bool stableWithoutAllocation=allocationFailures==failuresBeforeRefresh&&stable!=resource_geometry::state->models.end()&&reserved==512&&transfer.useCount()==originalRefs+1;
    bool stableIdentity=stable!=resource_geometry::state->models.end()&&stable->second.touch==77&&stable->second.desc.buffers.size()==512&&&stable->second.desc.buffers[0]==collectedStorage;
    if(!stableWithoutAllocation||!stableIdentity){resource_geometry::Clear();transfer.setNull();m.removeAll();CHECK(stableWithoutAllocation);CHECK(stableIdentity);}
    resource_geometry::Description baseline;CHECK(resource_geometry::Describe(transfer,&baseline));CHECK(resource_geometry::Matches(transfer,baseline));
    Ogre::VertexBufferBinding*binding=transfer->getSubMesh(0)->vertexData->vertexBufferBinding;
    Ogre::HardwareVertexBufferSharedPtr first=binding->getBuffer(0),second=binding->getBuffer(1),refreshReplacement(new TestVB(1));
    binding->setBinding(0,second);binding->setBinding(1,first);CHECK(!resource_geometry::Matches(transfer,baseline));
    binding->setBinding(0,first);binding->setBinding(1,second);CHECK(resource_geometry::Matches(transfer,baseline));
    binding->setBinding(0,refreshReplacement);CHECK(!resource_geometry::Matches(transfer,baseline));binding->setBinding(0,first);
    binding->unsetBinding(1);CHECK(!resource_geometry::Matches(transfer,baseline));binding->setBinding(1,second);
    binding->setBinding(1,first);CHECK(!resource_geometry::Matches(transfer,baseline));binding->setBinding(1,second);
    Ogre::VertexBufferBinding*tailBinding=transfer->getSubMesh(7)->vertexData->vertexBufferBinding;Ogre::HardwareVertexBufferSharedPtr tail=tailBinding->getBuffer(63);
    tailBinding->unsetBinding(63);CHECK(!resource_geometry::Matches(transfer,baseline));tailBinding->setBinding(63,tail);
    TestVB*metadata=static_cast<TestVB*>(first.get());metadata->Metadata(3,true);CHECK(!resource_geometry::Matches(transfer,baseline));metadata->Metadata(1);
    metadata->Metadata(1,true);CHECK(!resource_geometry::Matches(transfer,baseline));metadata->Metadata(1);
    metadata->Metadata(1,false,true);CHECK(!resource_geometry::Matches(transfer,baseline));metadata->Metadata(1);
    resource_geometry::Buffer decoded;metadata->Metadata(0);CHECK(!resource_geometry::ReadBuffer(first.get(),0,&decoded));CHECK(!resource_geometry::Matches(transfer,baseline));CHECK(!resource_geometry::Describe(transfer,&d));metadata->Metadata(1);
    metadata->Metadata(size_t((8ULL<<30)+1));CHECK(!resource_geometry::ReadBuffer(first.get(),0,&decoded));CHECK(!resource_geometry::Matches(transfer,baseline));CHECK(!resource_geometry::Describe(transfer,&d));metadata->Metadata(1);
    metadata->Metadata(size_t(8ULL<<30));CHECK(!resource_geometry::Describe(transfer,&d));metadata->Metadata(1);
    // Equal totals cannot hide changed per-buffer accounting.
    TestVB*secondMetadata=static_cast<TestVB*>(second.get());resource_geometry::Description balanced;
    metadata->Metadata(2);secondMetadata->Metadata(4);CHECK(resource_geometry::Describe(transfer,&balanced));
    metadata->Metadata(3);secondMetadata->Metadata(3);CHECK(!resource_geometry::Matches(transfer,balanced));metadata->Metadata(1);secondMetadata->Metadata(1);
    metadata->Metadata(1,true);CHECK(resource_geometry::Describe(transfer,&balanced));metadata->Metadata(1);secondMetadata->Metadata(1,true);CHECK(!resource_geometry::Matches(transfer,balanced));secondMetadata->Metadata(1);
    // Duplicates across shared/dedicated vertices and shadow buffers preserve
    // the exact first-encounter inventory rather than requiring unique slots.
    transfer->sharedVertexData=OGRE_NEW Ogre::VertexData;transfer->sharedVertexData->vertexBufferBinding->setBinding(0,first);
    transfer->sharedVertexData->hardwareShadowVolWBuffer=first;CHECK(resource_geometry::Matches(transfer,baseline));
    Ogre::HardwareIndexBufferSharedPtr repeatedIndex(new TestIB(2));
    Ogre::VertexBufferBinding*lastBinding=transfer->getSubMesh(7)->vertexData->vertexBufferBinding;
    Ogre::HardwareVertexBufferSharedPtr last=lastBinding->getBuffer(63);
    transfer->getSubMesh(0)->indexData->indexBuffer=repeatedIndex;
    CHECK(!resource_geometry::Describe(transfer,&d));CHECK(!resource_geometry::Matches(transfer,baseline)); // 513 distinct buffers.
    lastBinding->unsetBinding(63);
    Ogre::IndexData*repeatedLod=OGRE_NEW Ogre::IndexData;repeatedLod->indexBuffer=repeatedIndex;transfer->getSubMesh(0)->mLodFaceList.push_back(repeatedLod);
    resource_geometry::Description duplicated;CHECK(resource_geometry::Describe(transfer,&duplicated));CHECK(duplicated.buffers.size()==512&&duplicated.gpuBytes==513);CHECK(resource_geometry::Matches(transfer,duplicated));CHECK(!resource_geometry::Matches(transfer,baseline));
    // A mismatch must still take ordinary collecting failure recovery.
    unsigned beforeMismatch=allocationFailures;allocationFailAfter=0;TickNow(true,4);allocationFailAfter=-1;
    CHECK(allocationFailures==beforeMismatch+1);CHECK(resource_geometry::state->models.empty()&&reserved==0&&transfer.useCount()==originalRefs);
    TickNow(true,4);resource_geometry::Observe(transfer);TickNow(true,4);CHECK(resource_geometry::state->models.size()==1&&reserved==513);
    CHECK(resource_geometry::Matches(transfer,resource_geometry::state->models.find(transfer.get())->second.desc));
    // Shared traversal limits must still be examined after all expected buffers
    // have been encountered; matching must never terminate the walk early.
    Ogre::MeshPtr limits=m.createManual("matcher-limits","General",&loader);limits->load();limits->sharedVertexData=OGRE_NEW Ogre::VertexData;
    for(unsigned n=0;n<64;++n)limits->sharedVertexData->vertexBufferBinding->setBinding((unsigned short)n,first);
    for(unsigned n=0;n<64;++n)limits->createSubMesh();
    resource_geometry::Description limitDescription;CHECK(resource_geometry::Describe(limits,&limitDescription));CHECK(resource_geometry::Matches(limits,limitDescription));
    limits->sharedVertexData->vertexBufferBinding->setBinding(64,first);CHECK(!resource_geometry::Describe(limits,&d));CHECK(!resource_geometry::Matches(limits,limitDescription));limits->sharedVertexData->vertexBufferBinding->unsetBinding(64);
    for(unsigned part=0;part<32;++part)for(unsigned n=0;n<64;++n)limits->getSubMesh((unsigned short)part)->mLodFaceList.push_back(OGRE_NEW Ogre::IndexData);
    CHECK(resource_geometry::Describe(limits,&d));CHECK(resource_geometry::Matches(limits,limitDescription));
    limits->getSubMesh(32)->mLodFaceList.push_back(OGRE_NEW Ogre::IndexData);CHECK(!resource_geometry::Describe(limits,&d));CHECK(!resource_geometry::Matches(limits,limitDescription));
    for(unsigned part=0;part<33;++part){Ogre::SubMesh*s=limits->getSubMesh((unsigned short)part);while(!s->mLodFaceList.empty()){Ogre::IndexData*old=s->mLodFaceList.back();s->mLodFaceList.pop_back();OGRE_DELETE old;}}
    for(unsigned n=0;n<64;++n)limits->getSubMesh(0)->mLodFaceList.push_back(OGRE_NEW Ogre::IndexData);
    CHECK(resource_geometry::Describe(limits,&d));CHECK(resource_geometry::Matches(limits,limitDescription));
    limits->getSubMesh(0)->mLodFaceList.push_back(OGRE_NEW Ogre::IndexData);CHECK(!resource_geometry::Describe(limits,&d));CHECK(!resource_geometry::Matches(limits,limitDescription));
    Ogre::IndexData*lastLod=limits->getSubMesh(0)->mLodFaceList.back();limits->getSubMesh(0)->mLodFaceList.pop_back();OGRE_DELETE lastLod;
    limits->createSubMesh();CHECK(!resource_geometry::Describe(limits,&d));CHECK(!resource_geometry::Matches(limits,limitDescription));
    transfer->createPose(0,"matcher-pose");CHECK(!resource_geometry::Matches(transfer,duplicated));transfer->removeAllPoses();
    CHECK(!resource_geometry::Matches(Ogre::MeshPtr(),duplicated));
    limits.setNull();
    resource_geometry::Clear();bool released=reserved==0&&transfer.useCount()==originalRefs;
    transfer.setNull();m.removeAll();
    std::cout<<"description transfer: copies="<<descriptionCopies<<" same storage="<<sameStorage<<" consumed="<<consumed<<"\n";
    CHECK(promoted);CHECK(descriptionCopies==0);CHECK(sameStorage);CHECK(consumed);CHECK(accounted);CHECK(released);CHECK(stableWithoutAllocation);CHECK(stableIdentity);
    struct LegacyEntry {Ogre::MeshPtr mesh;resource_geometry::Description desc;resource_geometry::U64 touch;};
    std::cout<<"geometry entry bytes="<<sizeof(resource_geometry::Entry)<<" legacy entry bytes="<<sizeof(LegacyEntry)<<" victim index bytes="<<sizeof(resource_geometry::state->victims)<<" allocation failures="<<allocationFailures-failedBefore<<"\n";
    std::cout<<"geometry actual Ogre collector/lifetime/purge checks="<<checks<<"\n";return 0;
}
