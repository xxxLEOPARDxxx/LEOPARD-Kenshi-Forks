#ifndef KEO_BONE_INDEX_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#endif
#include "fixes/world/bone_index.h"
#include "plugin/hook_manifest.h"
#include <ogre/OgreMesh.h>
#include <cstring>
#include "fixes/world/bone_index_contract.h"
namespace bone_index_patch {
typedef void(*Original)(void*,const void*,void*,void*);
static Original original=NULL;static bone_index::Grow grow=NULL;
static bool tried=false,admitted=false;
static_assert(sizeof(Ogre::Mesh::VertexBoneAssignmentList)==40&&sizeof(Ogre::Mesh::IndexMap)==24&&sizeof(Ogre::VertexBoneAssignment)==12,"native bone-index ABI");
static __declspec(noinline) void Build(void*self,const void*list,void*forward,void*inverse){
 if(IsMainThread()&&admitted&&fixes::g_fixesCfg.meshWeightsEnabled&&bone_index::Try(list,forward,inverse,grow))return;
 original(self,list,forward,inverse);
}
static bool Validate(const unsigned char*base,size_t size){
 if(!base||size!=0x9c9000)return false;
 for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i){const Proof&p=proofs[i];if(p.rva>size||p.size>size-p.rva||memcmp(base+p.rva,p.bytes,p.size))return false;}
 return true;
}
static void*Admit(HMODULE module){
 __try{DWORD stamp=0,size=0;
  if(!IsMainThread()||!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||!Validate((const unsigned char*)module,size))return NULL;
  void*target=(char*)module+0x1a5e30;
  if((void*)GetProcAddress(module,"?buildIndexMap@Mesh@Ogre@@IEAAXAEBV?$multimap@_KUVertexBoneAssignment_s@Ogre@@U?$less@_K@std@@V?$STLAllocator@U?$pair@$$CB_KUVertexBoneAssignment_s@Ogre@@@std@@V?$CategorisedAllocPolicy@$0A@@Ogre@@@2@@std@@AEAV?$FastArray@G@2@1@Z")!=target)return NULL;
  return target;
 }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
#if !defined(KEO_BONE_INDEX_TEST) || defined(KEO_BONE_INDEX_INIT_TEST)
static void CompleteInstall(int,const char* why){
 if(why||!original){
  LogError(std::string("Bone index preparation: hook failed (")+(why?why:"original")+"); original tables retained");return;
 }
 admitted=true;KEO_DETAIL("Bone index preparation: bounded unsigned bone collection admitted");
}

static void Install(){
 if(tried)return;tried=true;HMODULE module=GetModuleHandleA("OgreMain_x64.dll");void*target=Admit(module);
 if(!target){LogError("Bone index preparation: native method/layout contract refused; original tables retained");return;}
 grow=(bone_index::Grow)((char*)module+0x1b0570);
 if(QueueStartupAuxHook(STARTUP_AUX_BONE_INDEX,target,(void*)&Build,(void**)&original,CompleteInstall))return;
 const bool installed=KenshiLib::AddHook(target,(void*)&Build,(void**)&original)==KenshiLib::SUCCESS;
 CompleteInstall(STARTUP_AUX_BONE_INDEX,installed?NULL:"hook");
}
#endif
}
#ifndef KEO_BONE_INDEX_TEST
void InstallBoneIndex(int*,int*){bone_index_patch::Install();}
#endif
