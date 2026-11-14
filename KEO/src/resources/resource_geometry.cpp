#include "resources/resource_geometry.h"
#include "resources/resource_gpu_store.h"
#ifndef RESOURCE_GEOMETRY_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <kenshi/ResourceLoader.h>
#endif
#include <ogre/OgreMesh.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreVertexIndexData.h>
#include <map>
#include <vector>
#include <algorithm>
#ifndef RESOURCE_GEOMETRY_TEST
#include "base/klib_include_end.h"
#endif
namespace resource_geometry {
struct Buffer {const Ogre::HardwareBuffer* id;U64 gpu,cpu;};
struct Description {std::vector<Buffer>buffers;U64 gpuBytes,cpuBytes;Description():gpuBytes(0),cpuBytes(0){}};
static const unsigned MaxPending=128,MaxModels=2048,MaxBuffers=65536,MaxMeshBuffers=512;
static const U64 GiB=1024ULL*1024*1024;
static bool BufferInfo(const Ogre::HardwareBuffer*b,Description*d){
    if(!b||b->isSystemMemory())return true;
    for(size_t i=0;i<d->buffers.size();++i)if(d->buffers[i].id==b)return true;
    if(d->buffers.size()>=MaxMeshBuffers)return false;
    Buffer v={b,U64(b->getSizeInBytes()),b->hasShadowBuffer()?U64(b->getSizeInBytes()):0};
    if(!v.gpu||v.gpu>8*GiB||d->gpuBytes>8*GiB-v.gpu)return false;
    d->buffers.push_back(v);d->gpuBytes+=v.gpu;d->cpuBytes+=v.cpu;return true;
}
static bool Vertices(const Ogre::VertexData*v,Description*d){
    if(!v)return true;
    if(v->vertexBufferBinding){
        const Ogre::VertexBufferBinding::VertexBufferBindingMap&b=v->vertexBufferBinding->getBindings();
        if(b.size()>64)return false;
        for(Ogre::VertexBufferBinding::VertexBufferBindingMap::const_iterator i=b.begin();i!=b.end();++i)
            if(!BufferInfo(i->second.get(),d))return false;
    }
    return BufferInfo(v->hardwareShadowVolWBuffer.get(),d);
}
static bool Indices(const Ogre::IndexData*i,Description*d){return !i||BufferInfo(i->indexBuffer.get(),d);}
bool Describe(const Ogre::MeshPtr&m,Description*d){
    if(!d||m.isNull()||!m->isLoaded())return false;
    *d=Description();
    // getLodLevel is NOT a passive accessor: actual Ogre1A733D calls load for
    // absent manual meshes. Do not invoke it or retain incompletely counted
    // manual/morph resources. Skeleton/entity skinning buffers are not pinned.
    if(m->hasManualLodLevel()||m->getNumAnimations()!=0||m->getPoseCount()!=0||m->getNumSubMeshes()>64)return false;
    if(!Vertices(m->sharedVertexData,d))return false;
    unsigned visits=0;
    for(unsigned i=0;i<m->getNumSubMeshes();++i){
        const Ogre::SubMesh*s=m->getSubMesh(static_cast<unsigned short>(i));
        if(!s||s->mLodFaceList.size()>64)return false;
        if(!Vertices(s->vertexData,d))return false;
        if(!Indices(s->indexData,d))return false;
        for(Ogre::SubMesh::LODFaceList::const_iterator j=s->mLodFaceList.begin();j!=s->mLodFaceList.end();++j){
            if(++visits>2048||!Indices(*j,d))return false;
        }
    }
    return true;
}
struct Count {unsigned refs;U64 gpu,cpu;};
struct Entry {Ogre::MeshPtr mesh;Description desc;U64 touch;};
// Weak manager identity ONLY. Pending observations must not pin loaded GPU
// buffers before byte admission. A purge before Tick can lose an optimization,
// never native correctness. The raw identity is compared, never dereferenced.
struct Candidate {Ogre::ResourceHandle handle;const Ogre::Mesh*id;DWORD time;};
typedef std::map<const Ogre::Mesh*,Entry> Models;
struct State {
    Stats stats;bool on;U64 cap,world,clock;unsigned device,read,count;DWORD lastTick;bool ticked;
    Candidate pending[MaxPending];Models models;
    std::map<const Ogre::HardwareBuffer*,Count>buffers;
    const Ogre::Mesh*refreshAfter;
    State():on(false),cap(GiB),world(0),clock(0),device(0),read(0),count(0),lastTick(0),ticked(false),refreshAfter(NULL){ZeroMemory(&stats,sizeof(stats));}
};
// Intentionally process-lifetime storage: a missed shutdown callback must never
// run an Ogre SharedPtr destructor inside DLL detach. Clear owns normal release.
static State*state=NULL;
static void Publish(){state->stats.models=unsigned(state->models.size());state->stats.pending=state->count;}
static void Release(Models::iterator i){
    Entry&e=i->second;
    for(size_t n=0;n<e.desc.buffers.size();++n){
        std::map<const Ogre::HardwareBuffer*,Count>::iterator b=state->buffers.find(e.desc.buffers[n].id);
        if(b!=state->buffers.end()&&--b->second.refs==0){state->stats.heldBytes-=b->second.gpu;state->stats.cpuShadowBytes-=b->second.cpu;state->buffers.erase(b);}
    }
    state->models.erase(i);++state->stats.evicted;
    resource_gpu::TryGeometryReservation(state->stats.heldBytes,state->cap);
}
static void Oldest(){
    Models::iterator oldest=state->models.begin();
    for(Models::iterator i=state->models.begin();i!=state->models.end();++i)if(i->second.touch<oldest->second.touch)oldest=i;
    if(oldest!=state->models.end())Release(oldest);
}
static bool Account(const Description&d,U64*extra,U64*cpu){
    *extra=0;*cpu=0;unsigned unique=0;
    for(size_t n=0;n<d.buffers.size();++n){
        const Buffer&v=d.buffers[n];std::map<const Ogre::HardwareBuffer*,Count>::const_iterator i=state->buffers.find(v.id);
        if(i==state->buffers.end()){++unique;*extra+=v.gpu;*cpu+=v.cpu;}
        else if(i->second.gpu!=v.gpu||i->second.cpu!=v.cpu)return false;
    }
    return state->buffers.size()+unique<=MaxBuffers;
}
static bool Promote(const Ogre::MeshPtr&m,const Description&d,U64 touch){
    U64 extra=0,cpu=0;if(!Account(d,&extra,&cpu)||state->models.size()>=MaxModels)return false;
    if(!d.gpuBytes||!resource_gpu::TryGeometryReservation(state->stats.heldBytes+extra,state->cap))return false;
    // Allocation exceptions are recovered by rebuilding all own bookkeeping.
    Entry e;e.mesh=m;e.desc=d;e.touch=touch;state->models.insert(std::make_pair(m.get(),e));
    for(size_t n=0;n<d.buffers.size();++n){const Buffer&v=d.buffers[n];
        std::map<const Ogre::HardwareBuffer*,Count>::iterator i=state->buffers.find(v.id);
        if(i!=state->buffers.end())++i->second.refs;
        else{Count c={1,v.gpu,v.cpu};state->buffers.insert(std::make_pair(v.id,c));}
    }
    state->stats.heldBytes+=extra;state->stats.cpuShadowBytes+=cpu;return true;
}
void Clear(){
    if(!state||!IsMainThread())return;
    state->on=false;state->stats.enabled=false;
    state->read=state->count=0;state->stats.evicted+=state->models.size();
    state->models.clear();state->buffers.clear();state->stats.heldBytes=state->stats.cpuShadowBytes=0;
    state->refreshAfter=NULL;resource_gpu::TryGeometryReservation(0,state->cap);Publish();
}
static void Observe(const Ogre::MeshPtr&m){
    if(!state||!state->on||!IsMainThread()||m.isNull())return;
    ++state->stats.observed;
    Models::iterator i=state->models.find(m.get());
    if(i!=state->models.end()){
        i->second.touch=++state->clock;
        if(m->isLoaded())++state->stats.reused; // actual native consumer requested this SAME retained mesh
        return;
    }
    const Ogre::ResourceHandle handle=m->getHandle();
    for(unsigned n=0;n<state->count;++n)if(state->pending[(state->read+n)%MaxPending].handle==handle)return;
    if(state->count>=MaxPending){++state->stats.rejected;++state->stats.queueFull;return;}
    Candidate&c=state->pending[(state->read+state->count)%MaxPending];c.handle=handle;c.id=m.get();c.time=GetTickCount();++state->count;
}
template<class Entity> static void ObserveEntity(Entity*entity,const Ogre::MeshPtr&requested){
    if(!state||!state->on||!IsMainThread()||!entity)return;
    // SceneManager::createEntity(MeshPtr) resolves name/group again. A collision
    // can leave requested as an unregistered disposable duplicate; retain the
    // actual Entity mesh, not that temporary. getMesh is a passive native getter.
    const Ogre::MeshPtr&actual=entity->getMesh();
    if(actual.get()!=requested.get())++state->stats.inputDifferent;
    Observe(actual);
}
static void DescribeRefusal(const Ogre::MeshPtr&m){
    if(m->hasManualLodLevel())++state->stats.unsupportedManual;
    else if(m->getNumAnimations()!=0||m->getPoseCount()!=0)++state->stats.unsupportedDeform;
    else ++state->stats.unsupportedOther;
}
void Tick(bool on,U64 cap,U64 world){
    if(!state||!IsMainThread())return;
    const unsigned device=resource_gpu::DeviceGeneration();
    if(!on||!state->stats.admitted||state->world!=world||state->device!=device){
        if(state->on||state->count||!state->models.empty())Clear();
        state->world=world;state->device=device;
    }
    state->cap=(std::max)(GiB,(std::min)(cap,8*GiB));state->on=on&&state->stats.admitted;state->stats.enabled=state->on;
    if(!state->on){Publish();return;}
    DWORD now=GetTickCount();if(state->ticked&&DWORD(now-state->lastTick)<250){Publish();return;}
    state->ticked=true;state->lastTick=now;
    try{
        state->stats.pressure=!resource_gpu::GeometryStandbyAllowed(state->cap);
        if(state->stats.pressure){
            // Release only our references; native remains free to consume them.
            // Up to32 meshes/heartbeat bounds destructor work under pressure.
            for(unsigned n=0;n<32&&!state->models.empty();++n)Oldest();
            state->read=state->count=0; // observations own no native references
            Publish();return;
        }
        // Refresh2 entries per heartbeat: native may explicitly reload or
        // rebuild a buffer. No iterator/container reference survives the tick.
        for(unsigned n=0;n<2&&!state->models.empty();++n){
            Models::iterator i=state->models.upper_bound(state->refreshAfter);if(i==state->models.end())i=state->models.begin();
            state->refreshAfter=i->first;Ogre::MeshPtr m=i->second.mesh;U64 touch=i->second.touch;Description d;
            if(!Describe(m,&d)){Release(i);continue;}
            const std::vector<Buffer>&old=i->second.desc.buffers;bool same=old.size()==d.buffers.size();
            for(size_t k=0;same&&k<old.size();++k)same=old[k].id==d.buffers[k].id&&old[k].gpu==d.buffers[k].gpu&&old[k].cpu==d.buffers[k].cpu;
            if(!same){Release(i);if(!Promote(m,d,touch))++state->stats.rejected;}
        }
        unsigned work=(std::min)(state->count,4u);
        for(unsigned n=0;n<work;++n){
            Candidate c=state->pending[state->read];
            state->read=(state->read+1)%MaxPending;--state->count;
            Ogre::MeshManager*manager=Ogre::MeshManager::getSingletonPtr();if(!manager){++state->stats.managerMissing;++state->stats.rejected;continue;}
            Ogre::ResourcePtr resource=manager->getByHandle(c.handle);
            if(resource.isNull()){++state->stats.managerMissing;++state->stats.rejected;continue;}
            if(resource.get()!=c.id){++state->stats.identityMismatch;++state->stats.rejected;continue;}
            Ogre::MeshPtr mesh=resource.staticCast<Ogre::Mesh>();
            if(!mesh->isLoaded()){
                if(DWORD(now-c.time)<30000){state->pending[(state->read+state->count)%MaxPending]=c;++state->count;}
                else {++state->stats.rejected;++state->stats.loadTimeout;}
                continue;
            }
            Description d;if(!Describe(mesh,&d)){++state->stats.rejected;DescribeRefusal(mesh);continue;}
            if(!d.gpuBytes){++state->stats.rejected;++state->stats.noGpuBuffers;continue;}
            if(state->models.size()>=MaxModels)Oldest();
            bool ok=Promote(mesh,d,++state->clock);
            // Bounded LRU reclamation on cap saturation; no full eviction loop.
            for(unsigned e=0;!ok&&e<2&&!state->models.empty();++e){Oldest();ok=Promote(mesh,d,++state->clock);}
            if(ok)++state->stats.retained;else {++state->stats.rejected;++state->stats.admissionRefused;}
        }
    }catch(...){
        Clear();
#ifndef RESOURCE_GEOMETRY_TEST
        LogError("Geometry retention: allocation/accounting exception; own references cleared");
#endif
    }
    Publish();
}
Stats Snapshot(){Stats s={};if(state&&IsMainThread()){Publish();s=state->stats;}return s;}
#ifndef RESOURCE_GEOMETRY_TEST
typedef Ogre::Entity*(*CreateFn)(ResourceLoader*,const Ogre::MeshPtr&,const boost::function<void(Ogre::ResourcePtr,void*)>&,const ResourceLoader::MeshLoadData&,Ogre::SceneMemoryMgrTypes);
static CreateFn originalCreate=NULL;
static Ogre::Entity*Create(ResourceLoader*self,const Ogre::MeshPtr&mesh,const boost::function<void(Ogre::ResourcePtr,void*)>&callback,const ResourceLoader::MeshLoadData&data,Ogre::SceneMemoryMgrTypes type){
    Ogre::Entity*result=originalCreate(self,mesh,callback,data,type);
    if(result)try{ObserveEntity(result,mesh);}catch(...){} // never replace native success/failure
    return result;
}
#endif
void Init(){
    if(!IsMainThread()||state)return;state=new State;
#ifdef RESOURCE_GEOMETRY_TEST
    state->stats.admitted=true;
#else
    if(NativeProfileIsGog()){KEO_DETAIL("Geometry retention: native Steam seam not admitted on GOG");return;}
    const uintptr_t base=reinterpret_cast<uintptr_t>(GetModuleHandleA(NULL));
    const IMAGE_NT_HEADERS64*nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    static const unsigned char bytes[32]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd8,0x48,0x81,0xec,0x28,0x01,0x00,0x00,0x48,0xc7,0x45,0x90,0xfe,0xff,0xff};
    void*target=reinterpret_cast<void*>(base+0x449620);
    if(nt->FileHeader.TimeDateStamp!=0x65d604d7||nt->OptionalHeader.SizeOfImage!=0x232c000||memcmp(target,bytes,sizeof(bytes))){
        LogError("Geometry retention: native mesh-request contract refused; ordinary native loading retained");return;
    }
    if(KenshiLib::AddHook(target,reinterpret_cast<void*>(&Create),reinterpret_cast<void**>(&originalCreate))!=KenshiLib::SUCCESS){
        originalCreate=NULL;LogError("Geometry retention: native mesh-request hook failed");return;
    }
    state->stats.admitted=true;KEO_DETAIL("Geometry retention: native MeshPtr request capture admitted (revisits only)");
#endif
}
}
