#include "fixes/world/blackboard_empty.h"
#ifndef KEO_BLACKBOARD_EMPTY_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#include "game/klib_member_contract.h"
#include "render/module_hooks.h"
#include "fixes/fixes_config.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#include <intrin.h>
#include <cstring>
#endif

namespace blackboard_empty {
typedef void (*UpdateFn)(void*,float);
static UpdateFn originalFaction=NULL,originalBlackboard=NULL;
static volatile LONG ready=0;
static bool tried=false;
static uintptr_t imageBase=0;
static __declspec(thread) bool phase=false,ownerEntered=false;

// Only this exact fenced native phase may move the empty-map read ahead of
// the native temporary allocation. Unknown/nested owners mask any outer scope.
struct PhaseScope {
 bool previous,entered;
 explicit PhaseScope(bool active):previous(phase),entered(ownerEntered){ownerEntered=true;phase=active;}
 ~PhaseScope(){phase=previous;ownerEntered=entered;}
private:PhaseScope(const PhaseScope&);PhaseScope&operator=(const PhaseScope&);
};
KLIB_ASSERT_OFFSET(Blackboard_requests_size,0x178);
static_assert(KLIB_WIDTH_Blackboard_requests_size==sizeof(size_t),"request count width");
static bool ReadableEmpty(void*self){
 if(!self)return false;
 __try{return *(const size_t*)((const char*)self+KLIB_OFF_Blackboard_requests_size)==0;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static __declspec(noinline) void FactionUpdate(void*self,float dt){
 const bool allowed=!ownerEntered&&InterlockedCompareExchange(&ready,0,0)!=0&&IsMainThread()
   &&(uintptr_t)_ReturnAddress()==imageBase+0x78848a&&fixes::g_fixesCfg.blackboardEmptyEnabled;
 PhaseScope scope(allowed);originalFaction(self,dt);
}
static __declspec(noinline) void BlackboardUpdate(void*self,float dt){
 if(phase&&(uintptr_t)_ReturnAddress()==imageBase+0x7eb1bd
   &&fixes::g_fixesCfg.blackboardEmptyEnabled&&ReadableEmpty(self))return;
 originalBlackboard(self,dt);
}

struct Proof {uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char blackboardBytes[]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x60,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x9c,0x24,0xa8,0x00,0x00};
static const unsigned char factionBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x48,0x83,0xec,0x30,0x33,0xed,0x0f,0x29,0x74,0x24,0x20,0x48,0x8b,0xd9,0x0f,0x28,0xf1,0x8b,0xf5,0x39,0x69};
static const unsigned char worldFenceBytes[]={0x48,0x8d,0x4b,0x68,0xe8,0xdf,0x79,0x8c,0xff,0x48,0x8d,0x4b,0x68,0xe8,0x6b,0x69,0x8a,0xff};
static const unsigned char factionPhaseBytes[]={0x48,0x8d,0x4b,0x68,0xe8,0x5c,0x74,0x8c,0xff,0x48,0x8d,0x4b,0x68,0xe8,0xe8,0x63,0x8a,0xff,0x48,0x8b,0xce,0x80,0xbe,0xb9,0x08,0x00,0x00,0x00,0x75,0x0f,0xe8,0xbc,0x31,0x8c,0xff,0xf3,0x0f,0x10,0x0d,0xc0,0xa2,0x9a,0x01,0xeb,0x08,0xe8,0x49,0x30,0x88,0xff,0x0f,0x28,0xcf,0x48,0x8b,0x8e,0xa8,0x04,0x00,0x00,0xe8,0x1d,0xb0,0x89,0xff,0xe8,0x6a,0x12,0x8a,0xff,0xe8,0xbe,0xf4,0x8a,0xff,0x48,0x85,0xc0,0x74,0x15,0xe8,0xb4,0xf4,0x8a,0xff,0x48,0x8b,0xc8,0xf3,0x0f,0x10,0x0d,0x87,0xa2,0x9a,0x01,0xe8,0xc3,0x0b,0x8c,0xff};
static const unsigned char blackboardCallerBytes[]={0xe8,0x21,0xbb,0x82,0xff};
static const unsigned char blackboardThunkBytes[]={0xe9,0xfd,0x35,0x25,0x00};
static const unsigned char factionThunkBytes[]={0xe9,0x04,0x40,0x2c,0x00};
static const Proof proofs[]={
 {0x26a2e0,blackboardBytes,sizeof(blackboardBytes)},
 {0x2e74b0,factionBytes,sizeof(factionBytes)},
 {0x787ec6,worldFenceBytes,sizeof(worldFenceBytes)},
 {0x788449,factionPhaseBytes,sizeof(factionPhaseBytes)},
 {0x7eb1b8,blackboardCallerBytes,sizeof(blackboardCallerBytes)},
 {0x16cde,blackboardThunkBytes,sizeof(blackboardThunkBytes)},
 {0x234a7,factionThunkBytes,sizeof(factionThunkBytes)},
};
static bool AdmitNative(){
 if(NativeProfileIsGog()||!NativeProfileVerifySdk())return false;
 HMODULE module=GetModuleHandleA(NULL);DWORD stamp=0,size=0;
 if(!ReadModuleImageId(module,&stamp,&size)||stamp!=0x65d604d7u||size!=0x232c000u)return false;
 imageBase=(uintptr_t)module;
 for(unsigned i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i){
  const Proof&p=proofs[i];if(p.size>size||p.rva>size-p.size||memcmp((const char*)module+p.rva,p.bytes,p.size))return false;
 }
 return true;
}
static void Install(){
 if(tried)return;tried=true;
 if(NativeProfileIsGog()){KEO_DETAIL("Empty squad requests: Steam phase contract unavailable on GOG; native behavior retained");return;}
 if(!AdmitNative()){LogError("Empty squad requests: native phase/SDK contract refused; native behavior retained");return;}
 // Coupled pair: never publish readiness from creation of only one hook.
 // Retain all originals on failure: an earlier hook may already be active.
 if(KenshiLib::AddHook((void*)(imageBase+0x26a2e0),(void*)&BlackboardUpdate,(void**)&originalBlackboard)!=KenshiLib::SUCCESS){LogError("Empty squad requests: Blackboard hook failed; forwarding only");return;}
 if(KenshiLib::AddHook((void*)(imageBase+0x2e74b0),(void*)&FactionUpdate,(void**)&originalFaction)!=KenshiLib::SUCCESS){LogError("Empty squad requests: phase hook failed; forwarding only");return;}
 InterlockedExchange(&ready,1);KEO_DETAIL("Empty squad requests: verified main-phase allocation fast path installed");
}
}
void InstallBlackboardEmpty(int*,int*){blackboard_empty::Install();}
