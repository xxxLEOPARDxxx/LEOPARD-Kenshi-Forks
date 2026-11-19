#include "fixes/world/save_string_transfer.h"
#include "fixes/world/save_gog_native.h"
#include <windows.h>
#include <intrin.h>
#include <string>
#include <cstring>
#include <cstddef>
#ifndef KEO_SAVE_STRING_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace save_string_transfer {
// Only a view. Ownership of the array and its constructed strings stays native.
struct List{void*allocator;unsigned count,capacity;std::string*data;};
static_assert(_ITERATOR_DEBUG_LEVEL==0&&sizeof(std::string)==40,"native release VC10 strings");
static_assert(sizeof(List)==24&&offsetof(List,count)==8&&offsetof(List,capacity)==12&&offsetof(List,data)==16,"native lektor string ABI");
typedef void(*PushFn)(List*,std::string*);
static PushFn original=NULL;
static bool tried=false,admitted=false;
static uintptr_t gameBase=0;
static const uintptr_t steamCallers[]={0x6c1ebc,0x6c1f47,0x6c2070,0x6cac21};
static const uintptr_t gogCallers[]={0x6c17cc,0x6c1857,0x6c1980,0x6ca531};
static const uintptr_t* parserCallers=steamCallers;
static bool ParserCaller(uintptr_t caller){
 const uintptr_t rva=caller-gameBase;
 return rva==parserCallers[0]||rva==parserCallers[1]||rva==parserCallers[2]||rva==parserCallers[3];
}
static bool SpareSlot(const List*list,const std::string*value){
 if(!list||!value||!list->data||list->count>=list->capacity||list->capacity>65536||value->size()<=15)return false;
 const uintptr_t begin=(uintptr_t)list->data,argument=(uintptr_t)value;
 const size_t bytes=(size_t)list->capacity*sizeof(std::string);
 if(begin>~(uintptr_t)0-bytes||argument>~(uintptr_t)0-sizeof(std::string))return false;
 // Do not consume a source overlapping any live/inactive array slot.
 if(!(argument+sizeof(std::string)<=begin||argument>=begin+bytes))return false;
 const std::string&slot=list->data[list->count];
 return slot.empty()&&slot.capacity()==15;
}
#ifndef KEO_SAVE_STRING_CALLER
#define KEO_SAVE_STRING_CALLER() (uintptr_t)_ReturnAddress()
#endif
static __declspec(noinline) void Push(List*list,std::string*value){
 if(IsMainThread()&&admitted&&fixes::g_fixesCfg.saveStringTransferEnabled&&ParserCaller(KEO_SAVE_STRING_CALLER())&&SpareSlot(list,value)){
  // Native takes this already-owned by-value temporary, copies it once more
  // and then empties it. The admitted spare slot is a constructed empty SSO
  // string. A single swap consumes the argument without allocating/freeing
  // memory in this DLL; native list cleanup remains the sole buffer owner.
  list->data[list->count].swap(*value);
  ++list->count;
  return;
 }
 // Includes growth and every non-parser caller. Exceptions remain native.
 original(list,value);
}
struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char proof0[]={0x40,0x57,0x48,0x83,0xec,0x40,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x60,0x48,0x8b,0x05,0xed,0x1b,0x07,0x02,0x48,0x33,0xc4,0x48,0x89,0x44,0x24,0x30,0x48,0x8b,0xda,0x48,0x8b,0xf9,0x48,0x89,0x54,0x24,0x28,0x8b,0x51,0x0c,0x39,0x51,0x08,0x72,0x07,0x03,0xd2,0xe8,0xc7,0x38,0xf9,0xff,0x8b,0x47,0x08,0x48,0x8d,0x0c,0x80,0x48,0x8b,0x47,0x10,0x48,0x8d,0x0c,0xc8,0x49,0x83,0xc9,0xff,0x45,0x33,0xc0,0x48,0x8b,0xd3,0xe8,0x82,0x38,0xf8,0xff,0xff,0x47,0x08,0x48,0x83,0x7b,0x18,0x10,0x72,0x08,0x48,0x8b,0x0b,0xe8,0x0b,0x0e,0xe3,0x00,0x48,0xc7,0x43,0x18,0x0f,0x00,0x00,0x00,0x48,0xc7,0x43,0x10,0x00,0x00,0x00,0x00,0xc6,0x03,0x00,0x48,0x8b,0x4c,0x24,0x30,0x48,0x33,0xcc,0xe8,0x73,0x15,0xe3,0x00,0x48,0x8b,0x5c,0x24,0x60,0x48,0x83,0xc4,0x40,0x5f,0xc3};
static const unsigned char proof1[]={0xe9,0xf4,0x02,0x05,0x00};
static const unsigned char proof2[]={0xff,0x48,0x8d,0x95,0xb8,0x01,0x00,0x00,0x48,0x8d,0x8d,0x90,0x01,0x00,0x00,0xe8,0xfb,0x25,0x99,0xff};
static const unsigned char proof3[]={0xff,0x48,0x8d,0x95,0xb8,0x01,0x00,0x00,0x48,0x8d,0x8d,0x90,0x01,0x00,0x00,0xe8,0x70,0x25,0x99,0xff};
static const unsigned char proof4[]={0xff,0x48,0x8d,0x95,0xb8,0x01,0x00,0x00,0x48,0x8d,0x8d,0x90,0x01,0x00,0x00,0xe8,0x47,0x24,0x99,0xff};
static const unsigned char proof5[]={0xe8,0x7a,0xd4,0x95,0xff,0x48,0x8d,0x54,0x24,0x28,0x48,0x8d,0x4f,0x50,0xe8,0x96,0x98,0x98,0xff,0xff,0xc3,0x3b,0x5e,0x08};
static const unsigned char proof6[]={0xff,0x25,0x32,0x3f,0x37,0x01};
static const Proof proofs[]={{0xa47b0,proof0,sizeof(proof0)},{0x544b7,proof1,sizeof(proof1)},{0x6c1ea8,proof2,sizeof(proof2)},{0x6c1f33,proof3,sizeof(proof3)},{0x6c205c,proof4,sizeof(proof4)},{0x6cac0e,proof5,sizeof(proof5)},{0xed5628,proof6,sizeof(proof6)}};
static const save_gog_native::Window gogProofs[]={
 {0xa47b0,152,0x7e1bc6faa99bbdebULL}, // Steam 0xa47b0
 {0x1d51b,5,0x9e97f2ea4ab0f472ULL}, // Steam 0x544b7
 {0x6c17b8,20,0xdd224d0196970050ULL}, // Steam 0x6c1ea8
 {0x6c1843,20,0xedc0a7d064b295e0ULL}, // Steam 0x6c1f33
 {0x6c196c,20,0x292181a74f5da2faULL}, // Steam 0x6c205c
 {0x6ca51e,24,0x518b613246dff04bULL}, // Steam 0x6cac0e
 {0xed4d2a,6,0x037a1cc79bd8a610ULL}, // Steam 0xed5628
};
static bool Admit(HMODULE module){
 __try{DWORD stamp=0,size=0;
  if(!module||!ReadModuleImageId(module,&stamp,&size))return false;
  const bool gog=save_gog_native::IsGogImage(stamp,size);
  if(gog){
   if(!save_gog_native::Match(module,gogProofs))return false;
  }else{
   if(stamp!=0x65d604d7||size!=0x232c000)return false;
   for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)
    if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))return false;
  }
  parserCallers=gog?gogCallers:steamCallers;
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Install(int*,int*){
 if(tried)return;tried=true;
#ifndef KEO_SAVE_STRING_MODULE
#define KEO_SAVE_STRING_MODULE() GetModuleHandleA(NULL)
#endif
 HMODULE game=KEO_SAVE_STRING_MODULE();
 if(!Admit(game)){LogError("Save string transfer: native parser contract refused; normal parsing retained");return;}
 gameBase=(uintptr_t)game;
 if(KenshiLib::AddHook((char*)game+0xa47b0,(void*)&Push,(void**)&original)!=KenshiLib::SUCCESS||!original){LogError("Save string transfer: hook failed; normal parsing retained");return;}
 admitted=true;KEO_DETAIL("Save string transfer: owned parser temporaries admitted");
}
}
void InstallSaveStringTransfer(int*installed,int*total){save_string_transfer::Install(installed,total);}
