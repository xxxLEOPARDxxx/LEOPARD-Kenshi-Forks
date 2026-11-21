#include "fixes/world/material_reserve.h"
#include <windows.h>
#include <intrin.h>
#include <cstddef>
#include <cstring>
#include <ogre/OgrePass.h>
#ifndef KEO_MATERIAL_RESERVE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace material_reserve {
struct PassAccess:Ogre::Pass {
 using Ogre::Pass::mTextureUnitStates;using Ogre::Pass::TextureUnitStates;
 static TextureUnitStates&Units(Ogre::Pass*p){TextureUnitStates Ogre::Pass::*member=static_cast<TextureUnitStates Ogre::Pass::*>(&PassAccess::mTextureUnitStates);return p->*member;}
};
static_assert(offsetof(PassAccess,mTextureUnitStates)==0x118,"native material pointer vector");
static_assert(sizeof(PassAccess::TextureUnitStates)==32&&sizeof(Ogre::Pass)==0x210,"native VC10 material ABI");
typedef void*(*AssignFn)(void*,const void*);
static AssignFn original=NULL;
static bool tried=false,admitted=false;static uintptr_t moduleBase=0;
#ifndef KEO_MATERIAL_RESERVE_OPERATION
#define KEO_MATERIAL_RESERVE_OPERATION(list,count) (list).reserve(count)
#endif
static void Prepare(void*self,const void*other){
 if(!self||!other||self==other)return;
 try{
  Ogre::Pass*dst=static_cast<Ogre::Pass*>(self);
  Ogre::Pass*src=const_cast<Ogre::Pass*>(static_cast<const Ogre::Pass*>(other));
  PassAccess::TextureUnitStates&target=PassAccess::Units(dst);
  const size_t count=PassAccess::Units(src).size();
  // Admitted copy ctor: destination is initialized and unpublished. Native
  // assignment retains this storage and constructs independent texture units.
  if(target.empty()&&count>=2&&count<=64&&target.capacity()<count)
   KEO_MATERIAL_RESERVE_OPERATION(target,count);
 }catch(...){try{LogError("Material preparation: storage reserve failed; native copying retained");}catch(...){}}
}
static __declspec(noinline) void*Assign(void*self,const void*other){
 const uintptr_t caller=(uintptr_t)_ReturnAddress();
 if(IsMainThread()&&admitted&&fixes::g_fixesCfg.materialReserveEnabled&&caller==moduleBase+0x211e50)
  Prepare(self,other);
 return original(self,other);
}
struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char proof0[]={0x40,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x68,0x48,0x89,0x6c,0x24};
static const unsigned char proof1[]={0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x48,0x48,0x89,0x74,0x24,0x50,0x41,0x0f,0xb7};
static const unsigned char proof2[]={0x48,0x89,0x8e,0x18,0x01,0x00,0x00,0x48,0x89,0x8e,0x20,0x01,0x00,0x00,0x48,0x89,0x8e,0x28,0x01,0x00,0x00};
static const unsigned char proof3[]={0xe8,0xd0,0x01,0x00,0x00};
static const unsigned char proof4[]={0xf8,0x03,0x48,0x8d,0x3c,0xc5,0x00,0x00,0x00,0x00,0x4c,0x8b,0xc7,0x48,0x8b,0xcb,0xff,0x15,0xd9,0x4c,0x3a,0x00,0x4c,0x8d,0x1c,0x3b,0x4c,0x89,0x9d,0x20,0x01,0x00,0x00};
static const Proof proofs[]={{0x212020,proof0,sizeof(proof0)},{0x211c90,proof1,sizeof(proof1)},{0x211d87,proof2,sizeof(proof2)},{0x211e4b,proof3,sizeof(proof3)},{0x2129a1,proof4,sizeof(proof4)}};
static const char*const assignSymbol="??4Pass@Ogre@@QEAAAEAV01@AEBV01@@Z";
static const char*const copySymbol="??0Pass@Ogre@@QEAA@PEAVTechnique@1@GAEBV01@@Z";
#ifndef KEO_MATERIAL_MODULE
#define KEO_MATERIAL_MODULE() GetModuleHandleA("OgreMain_x64.dll")
#endif
#ifndef KEO_MATERIAL_SYMBOL
#define KEO_MATERIAL_SYMBOL(module,symbol) GetProcAddress(module,symbol)
#endif
static void*Admit(HMODULE module){
 __try{DWORD stamp=0,size=0;
  if(!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000)return NULL;
  void*target=(void*)KEO_MATERIAL_SYMBOL(module,assignSymbol);
  if(target!=(char*)module+0x212020||(void*)KEO_MATERIAL_SYMBOL(module,copySymbol)!=(char*)module+0x211c90)return NULL;
  for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)
   if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))return NULL;
  return target;
 }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
void Install(int*,int*){
 if(tried)return;tried=true;HMODULE module=KEO_MATERIAL_MODULE();void*target=Admit(module);
 if(!target){LogError("Material preparation: native copy contract refused; native copying retained");return;}
 moduleBase=(uintptr_t)module;
 // A failed enable may still publish a trampoline. Keep it for forwarding;
 // no preparation is admitted unless installation and original both succeed.
 if(KenshiLib::AddHook(target,(void*)&Assign,(void**)&original)!=KenshiLib::SUCCESS||!original){
  LogError("Material preparation: hook failed; native copying retained");return;
 }
 admitted=true;KEO_DETAIL("Material preparation: bounded native copy-constructor storage admitted");
}
}
void InstallMaterialReserve(int*installed,int*total){material_reserve::Install(installed,total);}
