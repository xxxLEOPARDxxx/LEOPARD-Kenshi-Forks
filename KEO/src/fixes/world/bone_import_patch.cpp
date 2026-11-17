#ifndef KEO_BONE_IMPORT_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#endif
#include "fixes/world/bone_import.h"
#include <ogre/OgreSubMesh.h>
#include <cstddef>
#include <cstring>
#include "fixes/world/bone_import_contract.h"
namespace bone_import_patch {
struct Access:Ogre::SubMesh{using Ogre::SubMesh::mBoneAssignments;using Ogre::SubMesh::mBoneAssignmentsOutOfDate;};
static_assert(offsetof(Access,mBoneAssignments)==0xd0&&offsetof(Access,mBoneAssignmentsOutOfDate)==0xf8&&offsetof(Access,useSharedVertices)==0,"native SubMesh assignment layout");
static_assert(sizeof(Ogre::SubMesh::VertexBoneAssignmentList)==40&&sizeof(Ogre::VertexBoneAssignment)==12,"native VC10 Ogre assignment ABI");
static bone_import::Original original=NULL;
static bone_import::Allocate allocate=NULL;
static bone_import::Link link=NULL;
static bool tried=false,admitted=false;
static __declspec(noinline) void Import(void*self,const void*assignment){
 const bool owner=IsMainThread();
 bone_import::Add(self,assignment,original,allocate,link,
                  owner&&admitted&&fixes::g_fixesCfg.meshWeightsEnabled,owner);
}
static bool Validate(const unsigned char*base,size_t size){
 if(!base||size!=0x9c9000)return false;
 for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i){const Proof&p=proofs[i];if(p.rva>size||p.size>size-p.rva||memcmp(base+p.rva,p.bytes,p.size))return false;}
 return true;
}
static void*Admit(HMODULE module){
 __try{DWORD stamp=0,size=0;
  if(!IsMainThread()||!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||!Validate((const unsigned char*)module,size))return NULL;
  void*target=(char*)module+0x369330;
  if((void*)GetProcAddress(module,"?addBoneAssignment@SubMesh@Ogre@@QEAAXAEBUVertexBoneAssignment_s@2@@Z")!=target)return NULL;
  return target;
 }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
#ifndef KEO_BONE_IMPORT_TEST
static void Install(){
 if(tried)return;tried=true;HMODULE module=GetModuleHandleA("OgreMain_x64.dll");void*target=Admit(module);
 if(!target){LogError("Bone assignment import: native method/layout contract refused; original import retained");return;}
 allocate=(bone_import::Allocate)((char*)module+0x1bb050);link=(bone_import::Link)((char*)module+0x1ba270);
 // Preserve a published trampoline even if the SDK reports an install failure.
 if(KenshiLib::AddHook(target,(void*)&Import,(void**)&original)!=KenshiLib::SUCCESS||!original){LogError("Bone assignment import: hook failed; original import retained");return;}
 admitted=true;KEO_DETAIL("Bone assignment import: current-end native insertion admitted for ordered records");
}
#endif
}
#ifndef KEO_BONE_IMPORT_TEST
void InstallBoneImport(int*,int*){bone_import_patch::Install();}
#endif
