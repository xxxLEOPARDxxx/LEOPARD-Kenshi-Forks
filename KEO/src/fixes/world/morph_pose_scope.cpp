#include "fixes/world/morph_pose_scope.h"
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cstddef>
#include <new>
#include <ogre/OgreMesh.h>
#include <ogre/OgrePose.h>
#include <ogre/OgreEntity.h>
#include "fixes/world/morph_upload_batch.h"
#ifndef KEO_MORPH_POSE_SCOPE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace morph_pose_scope {
typedef Ogre::Pose*(*PoseFn)(const Ogre::Pose*);
typedef void(*RemoveFn)(Ogre::Mesh*);
static PoseFn originalPose=NULL;
static RemoveFn originalRemove=NULL;
static bool admitted=false,tried=false,cloneReady=false,createReady=false;
static uintptr_t ogreBase=0,gameBase=0;
struct MeshAccess:Ogre::Mesh {using Ogre::Mesh::mPoseList;};
static_assert(offsetof(MeshAccess,mPoseList)==0x348,"native Pose vector layout");
static_assert(sizeof(Ogre::PoseList)==32&&sizeof(Ogre::MeshPtr)==16,"native VC10 ABI");
// POD storage deliberately has no automatic C++ destructor. Explicit finally
// cleanup is required under /EHsc, including handled structured exceptions.
struct Context {
 Context*previous;
 __declspec(align(8)) unsigned char sourceStorage[sizeof(Ogre::MeshPtr)];
 __declspec(align(8)) unsigned char targetStorage[sizeof(Ogre::MeshPtr)];
 Ogre::Pose*borrowed[512];
 size_t count,expected,total;
 bool enabled,sourceLive,targetLive;
 morph_upload_batch::Batch upload;
 Ogre::MeshPtr&Source(){return *reinterpret_cast<Ogre::MeshPtr*>(sourceStorage);}
 Ogre::MeshPtr&Target(){return *reinterpret_cast<Ogre::MeshPtr*>(targetStorage);}
};
static __declspec(thread) Context*tracked=NULL;
static __declspec(thread) Context*current=NULL;
static __declspec(thread) Context*cloning=NULL;
static __declspec(thread) unsigned depth=0;
static bool Ready(){return admitted&&cloneReady&&createReady&&IsMainThread()&&fixes::g_fixesCfg.morphPoseReuseEnabled;}
static Ogre::PoseList&Poses(Ogre::Mesh*m){return const_cast<Ogre::PoseList&>(m->getPoseList());}
static void Error(const char*message){try{LogError(message);}catch(...){}}
static void Detach(Context*c,Ogre::Mesh*m){
 if(!c->targetLive||c->Target().get()!=m||!c->count)return;
 Ogre::PoseList&list=Poses(m);
 // The native clone normally contains exactly the recorded sequence. Verify
 // every pointer before taking the linear path; equal size alone is unsafe
 // for mixed/replaced lists. Other layouts retain the ownership fallback.
 bool complete=list.size()==c->count;
 for(size_t i=0;complete&&i<list.size();++i)complete=list[i]==c->borrowed[i];
 if(complete){list.clear();c->count=0;c->enabled=false;return;}
 size_t write=0;
 for(size_t i=0;i<list.size();++i){bool borrowed=false;
  for(size_t j=0;j<c->count;++j)if(list[i]==c->borrowed[j]){borrowed=true;break;}
  if(!borrowed)list[write++]=list[i];
 }
 // Shrinking a pointer vector does not allocate or destroy Pose objects.
 list.resize(write);c->count=0;c->enabled=false;
}
static void Finish(Context*c);
static void Remove(Ogre::Mesh*m){
 const uintptr_t caller=(uintptr_t)_ReturnAddress();
 // Cleanup is unconditional: live OFF, nested forwarding calls, unload and
 // partially cloned meshes must still protect every enclosing source lease.
 for(Context*c=tracked;c;c=c->previous)
  if(c->targetLive&&c->Target().get()==m)morph_upload_batch::End(&c->upload);
 for(Context*c=tracked;c;c=c->previous)Detach(c,m);
 originalRemove(m);
 // Native morph has finished its final read-only pose pass. Drop both added
 // references before switchBodyMesh, preserving native use-count decisions
 // when the old Entity is destroyed. Native owns its destination MeshPtr here.
 if(caller==gameBase+0x71ce2)
  for(Context*c=tracked;c;c=c->previous)
   if(c->targetLive&&c->Target().get()==m){c->enabled=false;Finish(c);}
}
static void Init(Context*c,void*entity){
 c->previous=tracked;c->count=c->expected=c->total=0;
 c->enabled=false;c->sourceLive=c->targetLive=false;
 morph_upload_batch::Init(&c->upload);
 if(!Ready()||!entity)return;
 try{
#ifndef KEO_MORPH_ENTITY_MESH
#define KEO_MORPH_ENTITY_MESH(entity) static_cast<Ogre::Entity*>(entity)->getMesh()
#endif
  const Ogre::MeshPtr&source=KEO_MORPH_ENTITY_MESH(entity);
  if(source.isNull()||!source->isLoaded())return;
  for(Context*p=tracked;p;p=p->previous)
   if(p->targetLive&&p->Target().get()==source.get())return;
  const Ogre::PoseList&poses=source->getPoseList();
  if(poses.empty()||poses.size()>512)return;
  for(size_t i=0;i<poses.size();++i)if(!poses[i])return;
  new(c->sourceStorage) Ogre::MeshPtr(source);c->sourceLive=true;
  c->total=poses.size();c->enabled=true;
 }catch(...){Error("Morph pose reuse: source lease unavailable; native cloning retained");}
}
static void Finish(Context*c){
 if(!morph_upload_batch::EndNoThrow(&c->upload))Error("Morph body upload: interrupted upload cleanup failed");
 __try{
  if(c->targetLive){Detach(c,c->Target().get());c->targetLive=false;c->Target().~SharedPtr();}
 }__finally{
  if(c->sourceLive){c->sourceLive=false;c->Source().~SharedPtr();}
 }
}
void*Create(CreateFn fn,const void*name,void*entity,void*data,bool shaved){
 Context context;Context*oldCurrent=current;Context*oldClone=cloning;void*result=NULL;
 // Limit retained lease frames; deeper calls mask admission while preserving
 // tracked parents for removeAllPoses sanitization.
 if(depth>=8){current=NULL;cloning=NULL;
  __try{result=fn(name,entity,data,shaved);}
  __finally{cloning=oldClone;current=oldCurrent;}
  return result;
 }
 Init(&context,entity);tracked=&context;current=&context;cloning=NULL;++depth;
 __try{result=fn(name,entity,data,shaved);}
 __finally{
  __try{if(context.targetLive)Detach(&context,context.Target().get());}
  __finally{
   --depth;tracked=context.previous;current=oldCurrent;cloning=oldClone;
   Finish(&context);
  }
 }
 return result;
}
static void BeginUpload(Context*c){
 try{morph_upload_batch::Begin(&c->upload,c->Source().get(),c->Target().get());}
 catch(...){Error("Morph body upload: batch unavailable; native uploads retained");}
}
void*Clone(CloneFn fn,void*self,void*out,const void*name,const void*group,uintptr_t caller){
 Context*previous=cloning;Context*c=current;void*result=NULL;
 const bool allow=Ready()&&c&&c->enabled&&c->sourceLive&&!c->targetLive
  &&caller==gameBase+0x71ae7&&self==c->Source().get();
 // Every nested clone masks the parent regardless of why it is ineligible.
 cloning=allow?c:NULL;
 __try{
  result=fn(self,out,name,group);
  if(allow&&c->enabled&&c->sourceLive&&c->targetLive&&c->expected==c->total&&c->count==c->total){
   BeginUpload(c);
  }
 }
 __finally{cloning=previous;}
 return result;
}
void CaptureCloneResult(void*result,void*out,uintptr_t caller){
 Context*c=cloning;
 if(!c||!c->enabled||!Ready()||c->targetLive||caller!=ogreBase+0x1a3fdd||!out||result!=out)return;
 try{
  const Ogre::MeshPtr&target=*static_cast<const Ogre::MeshPtr*>(out);
  if(target.isNull()||target.get()==c->Source().get()||!Poses(target.get()).empty()){c->enabled=false;return;}
  new(c->targetStorage) Ogre::MeshPtr(target);c->targetLive=true;
 }catch(...){c->enabled=false;Error("Morph pose reuse: destination lease unavailable; native cloning retained");}
}
static __declspec(noinline) Ogre::Pose*Pose(const Ogre::Pose*source){
 const uintptr_t caller=(uintptr_t)_ReturnAddress();Context*c=cloning;
 if(c&&c->enabled){
  const bool match=Ready()&&caller==ogreBase+0x1a43ff&&c->sourceLive&&c->targetLive
   &&c->expected<c->total&&c->count<512
   &&c->Source()->getPoseList().size()==c->total
   &&c->Source()->getPoseList()[c->expected]==source
   &&Poses(c->Target().get()).size()==c->expected;
  if(match){
   // Record before returning to native insertion; even its allocation failure
   // leaves a positively tracked pointer safe to detach during finally.
   c->borrowed[c->count++]=const_cast<Ogre::Pose*>(source);++c->expected;
   return const_cast<Ogre::Pose*>(source);
  }
  c->enabled=false;
 }
 return originalPose(source);
}
void SetCloneHooksReady(bool ready){cloneReady=ready;}
void SetCreateHookReady(bool ready){createReady=ready;}
struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char proof0[]={0x40,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x48,0x48,0x89,0x6c,0x24,0x50,0x48,0x89,0x74,0x24,0x58,0x48,0x8b};
static const unsigned char proof1[]={0x48,0x89,0x5c,0x24,0x10,0x56,0x48,0x83,0xec,0x20,0x48,0x8b,0x99,0x48,0x03,0x00,0x00,0x48,0x8b,0xf1,0x48,0x3b,0x99,0x50,0x03,0x00,0x00,0x74,0x22,0x0f,0x1f,0x00};
static const unsigned char proof2[]={0x48,0x8b,0x0b,0xe8,0xf1,0x57,0x08,0x00,0x4c,0x8b,0xe0};
static const unsigned char proof3[]={0x48,0x8b,0xc4,0x48,0x89,0x50,0x10,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x28,0xfe,0xff,0xff,0xff};
static const unsigned char proof4[]={0x48,0x8b,0x08,0xff,0x15,0xe9,0x45,0x1d,0x02,0x90};
static const unsigned char proof5[]={0x48,0x8b,0x4c,0x24,0x30,0xff,0x15,0xb6,0x43,0x1d,0x02};
static const unsigned char proof6[]={0x48,0x83,0xec,0x28,0x88,0x51,0x39,0x84,0xd2,0x75,0x06,0x48,0x8b,0x01,0xff,0x50,0x48,0x48,0x83,0xc4,0x28,0xc3};
static const unsigned char proof7[]={0x49,0x8b,0x49,0x10,0x48,0x8b,0x01,0x45,0x33,0xc0,0x41,0x8d,0x50,0x01,0xff,0x50,0x38,0x48,0x8b,0xf8,0x48,0x8b,0x4b,0x10,0x4c,0x8b,0x09,0x45,0x33,0xc0,0x41,0x8d,0x50,0x04,0x41,0xff,0x51,0x38,0x4c,0x8b,0xe0};
static const unsigned char proof8[]={0x4c,0x8b,0x77,0x50,0x49,0xc1,0xee,0x02,0x48,0x8b,0x07,0xc7,0x44,0x24,0x20,0x00,0x00,0x00,0x00,0x45,0x33,0xc9,0x4c,0x8b,0x47,0x08,0x33,0xd2,0x48,0x8b,0xcf,0xff,0x50,0x18};
static const unsigned char proof9[]={0x48,0x8b,0x07,0x48,0x8b,0xcf,0xff,0x50,0x20};
static const unsigned char proof10[]={0x33,0xd2,0x48,0x8b,0x4c,0x24,0x30,0xff,0x15,0xd3,0x45,0x1d,0x02,0x4c,0x8b,0x78,0x08};
static const Proof ogreProofs[]={{0x229bf0,proof0,sizeof(proof0)},{0x1ab3c0,proof1,sizeof(proof1)},{0x1a43f7,proof2,sizeof(proof2)},{0xcbff0,proof6,sizeof(proof6)},{0x1a9c1d,proof7,sizeof(proof7)},{0x1a9c93,proof8,sizeof(proof8)},{0x1a9e28,proof9,sizeof(proof9)}};
static const Proof gameProofs[]={{0x71a80,proof3,sizeof(proof3)},{0x71ade,proof4,sizeof(proof4)},{0x71cd7,proof5,sizeof(proof5)},{0x71ae8,proof10,sizeof(proof10)}};
static const char*const poseSymbol="?clone@Pose@Ogre@@QEBAPEAV12@XZ";
static const char*const removeSymbol="?removeAllPoses@Mesh@Ogre@@QEAAXXZ";
static bool Proofs(HMODULE m,const Proof*p,size_t n){for(size_t i=0;i<n;++i)if(memcmp((char*)m+p[i].rva,p[i].bytes,p[i].size))return false;return true;}
static bool Admit(HMODULE ogre,HMODULE game){
 __try{DWORD stamp=0,size=0;
  if(!ogre||!game||!ReadModuleImageId(ogre,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000)return false;
  if(!ReadModuleImageId(game,&stamp,&size)||stamp!=0x65d604d7||size!=0x232c000)return false;
  if((void*)GetProcAddress(ogre,poseSymbol)!=(char*)ogre+0x229bf0||(void*)GetProcAddress(ogre,removeSymbol)!=(char*)ogre+0x1ab3c0)return false;
  return Proofs(ogre,ogreProofs,sizeof(ogreProofs)/sizeof(ogreProofs[0]))&&Proofs(game,gameProofs,sizeof(gameProofs)/sizeof(gameProofs[0]));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#ifndef KEO_MORPH_OGRE_MODULE
#define KEO_MORPH_OGRE_MODULE() GetModuleHandleA("OgreMain_x64.dll")
#endif
#ifndef KEO_MORPH_GAME_MODULE
#define KEO_MORPH_GAME_MODULE() GetModuleHandleA(NULL)
#endif
void Install(int*,int*){
 if(tried)return;tried=true;HMODULE ogre=KEO_MORPH_OGRE_MODULE(),game=KEO_MORPH_GAME_MODULE();
 if(!Admit(ogre,game)){Error("Morph pose reuse: native contracts refused; native cloning retained");return;}
 ogreBase=(uintptr_t)ogre;gameBase=(uintptr_t)game;
 if(KenshiLib::AddHook((char*)ogre+0x229bf0,(void*)&Pose,(void**)&originalPose)!=KenshiLib::SUCCESS||!originalPose){Error("Morph pose reuse: pose hook failed; native cloning retained");return;}
 if(KenshiLib::AddHook((char*)ogre+0x1ab3c0,(void*)&Remove,(void**)&originalRemove)!=KenshiLib::SUCCESS||!originalRemove){Error("Morph pose reuse: cleanup hook failed; native cloning retained");return;}
 admitted=true;KEO_DETAIL("Morph pose reuse: bounded private-morph pose leases admitted");
}
}
