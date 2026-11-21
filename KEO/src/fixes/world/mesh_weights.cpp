#ifndef KEO_MESH_WEIGHT_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#endif
#include "fixes/world/mesh_weights.h"
#include "fixes/world/mesh_weights_linear.h"
#include "fixes/world/mesh_weights_contract.h"
#include <cstddef>
namespace mesh_weights_patch {
typedef Ogre::Mesh::VertexBoneAssignmentList List;
typedef unsigned short(*Original)(Ogre::Mesh*,size_t,List&);
static Original original=NULL;
static bool admitted=false,tried=false;
static_assert(sizeof(List)==40,"native VC10 Ogre allocator multimap ABI");
static_assert(sizeof(Ogre::VertexBoneAssignment)==12&&offsetof(Ogre::VertexBoneAssignment,weight)==8,"native bone weight record ABI");
static unsigned short Rationalise(Ogre::Mesh*self,size_t vertices,List&list){
 // Owner first: other native loader threads never read mutable live options.
 if(IsMainThread()&&admitted&&fixes::g_fixesCfg.meshWeightsEnabled){
  unsigned short maximum;
  if(mesh_weights::Try(vertices,list,&maximum))return maximum;
 }
 return original(self,vertices,list);
}
static bool Validate(const unsigned char*base,size_t size){
 if(!base||size!=0x9c9000)return false;
 for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i){
  const Proof&p=proofs[i];if(p.rva>size||p.size>size-p.rva||memcmp(base+p.rva,p.bytes,p.size))return false;
 }
 return true;
}
static void*Admit(HMODULE module){
 __try{
  DWORD stamp=0,size=0;
  if(!IsMainThread()||!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||!Validate((const unsigned char*)module,size))return NULL;
  void*target=(char*)module+0x1a5510;
  if((void*)GetProcAddress(module,"?_rationaliseBoneAssignments@Mesh@Ogre@@QEAAG_KAEAV?$multimap@_KUVertexBoneAssignment_s@Ogre@@U?$less@_K@std@@V?$STLAllocator@U?$pair@$$CB_KUVertexBoneAssignment_s@Ogre@@@std@@V?$CategorisedAllocPolicy@$0A@@Ogre@@@2@@std@@@Z")!=target)return NULL;
  return target;
 }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
#ifndef KEO_MESH_WEIGHT_TEST
static void Install(){
 if(tried)return;tried=true;
 void*target=Admit(GetModuleHandleA("OgreMain_x64.dll"));
 if(!target){LogError("Mesh weight preparation: native function/ABI contract refused; original handling retained");return;}
 if(KenshiLib::AddHook(target,(void*)&Rationalise,(void**)&original)!=KenshiLib::SUCCESS||!original){LogError("Mesh weight preparation: hook failed; original handling retained");return;}
 admitted=true;KEO_DETAIL("Mesh weight preparation: validated linear native weight traversal admitted");
}
#endif
}
#ifndef KEO_MESH_WEIGHT_TEST
void InstallMeshWeights(int*,int*){mesh_weights_patch::Install();}
#endif
