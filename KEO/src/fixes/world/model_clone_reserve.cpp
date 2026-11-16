#include "fixes/world/model_clone_reserve.h"
#include "fixes/world/morph_pose_scope.h"
#include <windows.h>
#include <intrin.h>
#include <cstddef>
#include <cstring>
#include <ogre/OgreMesh.h>
#ifndef KEO_MODEL_CLONE_RESERVE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace model_clone_reserve {
struct MeshAccess:Ogre::Mesh {
 using Ogre::Mesh::mSubMeshList;using Ogre::Mesh::mPoseList;
 static SubMeshList&SubMeshes(Ogre::Mesh*p){SubMeshList Ogre::Mesh::*member=static_cast<SubMeshList Ogre::Mesh::*>(&MeshAccess::mSubMeshList);return p->*member;}
};
static_assert(offsetof(MeshAccess,mSubMeshList)==0x170,"native SubMesh vector layout");
static_assert(offsetof(MeshAccess,mPoseList)==0x348,"native Pose vector layout");
static_assert(sizeof(Ogre::Mesh::SubMeshList)==32&&sizeof(Ogre::PoseList)==32,"native VC10 vector ABI");
typedef void*(*CloneFn)(void*,void*,const void*,const void*);
typedef void*(*CreateFn)(void*,void*,const void*,const void*,void*);
static CloneFn originalClone=NULL;static CreateFn originalCreate=NULL;
static bool admitted=false,tried=false;static uintptr_t moduleBase=0;
static __declspec(thread) void*source=NULL;
#ifndef KEO_MODEL_CLONE_RESERVE_OPERATION
#define KEO_MODEL_CLONE_RESERVE_OPERATION(list,count) (list).reserve(count)
#endif
static void Prepare(void*input,void*output){
 if(!input||!output)return;
 try{
  Ogre::Mesh*src=static_cast<Ogre::Mesh*>(input);
  const Ogre::MeshPtr*view=static_cast<const Ogre::MeshPtr*>(output);
  Ogre::Mesh*dst=view->get();
  if(!dst||dst==src||!src->isLoaded())return;
  // Both are borrowed within native clone. No SharedPtr copy, callbacks,
  // manager lookup, model decoding or native object construction is added.
  const Ogre::Mesh::SubMeshList&subSource=MeshAccess::SubMeshes(src);
  Ogre::Mesh::SubMeshList&subTarget=MeshAccess::SubMeshes(dst);
  const size_t subCount=subSource.size();
  if(subTarget.empty()&&subCount>0&&subCount<=64&&subTarget.capacity()<subCount)
   KEO_MODEL_CLONE_RESERVE_OPERATION(subTarget,subCount);
  const Ogre::PoseList&poseSource=src->getPoseList();
  // The getter exposes a const view of an actually mutable destination Mesh.
  Ogre::PoseList&poseTarget=const_cast<Ogre::PoseList&>(dst->getPoseList());
  const size_t poseCount=poseSource.size();
  if(poseTarget.empty()&&poseCount>0&&poseCount<=512&&poseTarget.capacity()<poseCount)
   KEO_MODEL_CLONE_RESERVE_OPERATION(poseTarget,poseCount);
 }catch(...){
  try{LogError("Model clone preparation: storage reserve failed; native cloning retained");}catch(...){}
 }
}
static __declspec(noinline) void*Clone(void*self,void*out,const void*name,const void*group){
 const bool allow=IsMainThread()&&admitted&&fixes::g_fixesCfg.modelCloneReserveEnabled;
 void*previous=source;void*result=NULL;const uintptr_t caller=(uintptr_t)_ReturnAddress();
 source=allow?self:NULL;
 __try{result=morph_pose_scope::Clone(originalClone,self,out,name,group,caller);}
 __finally{source=previous;}
 return result;
}
static __declspec(noinline) void*CreateManual(void*self,void*out,const void*name,const void*group,void*loader){
 const uintptr_t caller=(uintptr_t)_ReturnAddress();
 void*result=originalCreate(self,out,name,group,loader);
 morph_pose_scope::CaptureCloneResult(result,out,caller);
 if(source&&IsMainThread()&&admitted&&fixes::g_fixesCfg.modelCloneReserveEnabled
    &&caller==moduleBase+0x1a3fdd&&result==out&&out)Prepare(source,out);
 return result;
}

struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char proof0[]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xe1,0x48,0x81,0xec,0xb8,0x00,0x00,0x00,0x48,0xc7,0x45,0xa7,0xfe,0xff,0xff};
static const unsigned char proof1[]={0x40,0x53,0x48,0x83,0xec,0x40,0x33,0xc0,0x48,0x8b,0xda,0x48,0x89,0x44,0x24,0x30,0x89,0x44,0x24,0x58,0x48,0x8b,0x44,0x24,0x70,0x48,0x89,0x44,0x24,0x28,0xc6,0x44};
static const unsigned char proof2[]={0x4c,0x89,0xad,0x70,0x01,0x00,0x00,0x4c,0x89,0xad,0x78,0x01,0x00,0x00,0x4c,0x89,0xad,0x80,0x01,0x00,0x00};
static const unsigned char proof3[]={0x4c,0x89,0xad,0x48,0x03,0x00,0x00,0x4c,0x89,0xad,0x50,0x03,0x00,0x00,0x4c,0x89,0xad,0x58,0x03,0x00,0x00};
static const unsigned char proof4[]={0xe8,0x93,0xa9,0x01,0x00};
static const unsigned char proof5[]={0x48,0x8d,0x81,0x48,0x03,0x00,0x00,0xc3};
static const Proof proofs[]={{0x1a3ee0,proof0,sizeof(proof0)},{0x1be970,proof1,sizeof(proof1)},{0x1a2983,proof2,sizeof(proof2)},{0x1a2b5f,proof3,sizeof(proof3)},{0x1a3fd8,proof4,sizeof(proof4)},{0x1ab490,proof5,sizeof(proof5)}};
static const char*const cloneSymbol="?clone@Mesh@Ogre@@QEAA?AV?$SharedPtr@VMesh@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z";
static const char*const createSymbol="?createManual@MeshManager@Ogre@@QEAA?AV?$SharedPtr@VMesh@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0PEAVManualResourceLoader@2@@Z";
#ifndef KEO_MODEL_CLONE_MODULE
#define KEO_MODEL_CLONE_MODULE() GetModuleHandleA("OgreMain_x64.dll")
#endif
#ifndef KEO_MODEL_CLONE_SYMBOL
#define KEO_MODEL_CLONE_SYMBOL(module,symbol) GetProcAddress(module,symbol)
#endif
static bool Admit(HMODULE module){
 __try{
  DWORD stamp=0,size=0;
  if(!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000)return false;
  if((void*)KEO_MODEL_CLONE_SYMBOL(module,cloneSymbol)!=(char*)module+0x1a3ee0
     ||(void*)KEO_MODEL_CLONE_SYMBOL(module,createSymbol)!=(char*)module+0x1be970)return false;
  for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)
   if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))return false;
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Install(){
 if(tried)return;tried=true;
 morph_pose_scope::SetCloneHooksReady(false);
 HMODULE module=KEO_MODEL_CLONE_MODULE();
 if(!Admit(module)){LogError("Model clone preparation: native contract refused; native loading retained");return;}
 moduleBase=(uintptr_t)module;
 // Do not publish readiness from a half-installed pair. Preserve every SDK
 // trampoline even on failure, so any armed wrapper remains pure forwarding.
 if(KenshiLib::AddHook((void*)(moduleBase+0x1a3ee0),(void*)&Clone,(void**)&originalClone)!=KenshiLib::SUCCESS||!originalClone){LogError("Model clone preparation: clone hook failed; native loading retained");return;}
 if(KenshiLib::AddHook((void*)(moduleBase+0x1be970),(void*)&CreateManual,(void**)&originalCreate)!=KenshiLib::SUCCESS||!originalCreate){LogError("Model clone preparation: manager hook failed; native loading retained");return;}
 admitted=true;morph_pose_scope::SetCloneHooksReady(true);KEO_DETAIL("Model clone preparation: bounded native pointer-vector preparation admitted");
}
}
void InstallModelCloneReserve(int*,int*){model_clone_reserve::Install();}
