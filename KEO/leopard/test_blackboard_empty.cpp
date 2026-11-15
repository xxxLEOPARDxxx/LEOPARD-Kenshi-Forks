#define KEO_BLACKBOARD_EMPTY_TEST
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include "game/klib_member_contract.h"
static __declspec(thread) void*caller=NULL;
#define _ReturnAddress() caller
static bool mainOwner=true;
static DWORD ownerId=GetCurrentThreadId();
bool IsMainThread(){return mainOwner&&GetCurrentThreadId()==ownerId;}
static unsigned errors=0,details=0,hookCalls=0;
static bool sdk=true,gog=false,identity=true;
static int hookFailure=-1;
static unsigned char*image=NULL;
static void NativeBoard(void*,float);
static void NativeFaction(void*,float);
HMODULE FixtureModule(LPCSTR){return (HMODULE)image;}
#define GetModuleHandleA FixtureModule
bool NativeProfileIsGog(){return gog;}
bool NativeProfileVerifySdk(){return sdk;}
bool ReadModuleImageId(HMODULE,DWORD*t,DWORD*s){*t=identity?0x65d604d7u:0;*s=0x232c000;return true;}
void LogError(const char*){++errors;}
#define KEO_DETAIL(x) (++details)
namespace KenshiLib{enum HookStatus{SUCCESS,FAIL};HookStatus AddHook(void*,void*,void**o){unsigned n=hookCalls++;*o=n? (void*)&NativeFaction:(void*)&NativeBoard;return int(n)==hookFailure?FAIL:SUCCESS;}}
namespace fixes {struct Config{bool blackboardEmptyEnabled;};static Config g_fixesCfg={true};}
#include "../src/fixes/world/blackboard_empty.cpp"
static unsigned checks=0,nativeCalls=0,factions=0;
static bool throwNative=false,wrongInner=false,nested=false,nestedThree=false,disableInside=false,throwFaction=false;
static unsigned nestedDepth=0;
static unsigned char board[0x200];
static float expectedDt=0;
static void*expectedSelf=board;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);throw 99;}}while(0)
static void NativeBoard(void*self,float dt){C(self==expectedSelf&&memcmp(&dt,&expectedDt,4)==0);++nativeCalls;if(throwNative)throw 42;}
static void NativeFaction(void*self,float dt){
 C(self==board&&memcmp(&dt,&expectedDt,4)==0);++factions;if(throwFaction)throw 43;if(disableInside)fixes::g_fixesCfg.blackboardEmptyEnabled=false;
 if(nested&&nestedDepth<(nestedThree?2u:1u)){++nestedDepth;void*old=caller;caller=nestedDepth==2?(void*)(blackboard_empty::imageBase+0x78848a):(void*)1;blackboard_empty::FactionUpdate(board,dt);caller=old;--nestedDepth;return;}
 void*old=caller;caller=(void*)(blackboard_empty::imageBase+(wrongInner?0x7eb1be:0x7eb1bd));
 blackboard_empty::BlackboardUpdate(expectedSelf,dt);caller=old;
}
static void Setup(){nativeCalls=factions=0;mainOwner=true;throwNative=wrongInner=nested=nestedThree=disableInside=throwFaction=false;nestedDepth=0;fixes::g_fixesCfg.blackboardEmptyEnabled=true;blackboard_empty::ready=1;blackboard_empty::phase=blackboard_empty::ownerEntered=false;blackboard_empty::imageBase=0x140000000ULL;blackboard_empty::originalFaction=NativeFaction;blackboard_empty::originalBlackboard=NativeBoard;ZeroMemory(board,sizeof(board));expectedSelf=board;expectedDt=1.25f;caller=(void*)(blackboard_empty::imageBase+0x78848a);}
static DWORD WINAPI Worker(void*){caller=(void*)(blackboard_empty::imageBase+0x7eb1bd);blackboard_empty::BlackboardUpdate(board,expectedDt);return 0;}
int main(){try{
 Setup();blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==0&&factions==1&&!blackboard_empty::phase&&!blackboard_empty::ownerEntered);
 for(unsigned kind=0;kind<7;++kind){Setup();if(kind==0)blackboard_empty::ready=0;if(kind==1)mainOwner=false;if(kind==2)caller=(void*)1;if(kind==3)fixes::g_fixesCfg.blackboardEmptyEnabled=false;if(kind==4)wrongInner=true;if(kind==5)*(size_t*)(board+0x178)=1;if(kind==6)expectedSelf=NULL;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1&&!blackboard_empty::phase&&!blackboard_empty::ownerEntered);}
 Setup();caller=(void*)(blackboard_empty::imageBase+0x7eb1bd);blackboard_empty::BlackboardUpdate(board,expectedDt);C(nativeCalls==1);
 Setup();*(size_t*)(board+0x178)=3;throwNative=true;try{blackboard_empty::FactionUpdate(board,expectedDt);C(false);}catch(int n){C(n==42);}C(!blackboard_empty::phase&&nativeCalls==1);
 Setup();nested=true;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1&&factions==2&&!blackboard_empty::phase&&!blackboard_empty::ownerEntered);
 Setup();nested=nestedThree=true;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1&&factions==3&&!blackboard_empty::phase&&!blackboard_empty::ownerEntered);
 Setup();expectedDt=std::numeric_limits<float>::quiet_NaN();*(size_t*)(board+0x178)=5;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1);
 Setup();blackboard_empty::ready=1;fixes::g_fixesCfg.blackboardEmptyEnabled=false;blackboard_empty::FactionUpdate(board,expectedDt);fixes::g_fixesCfg.blackboardEmptyEnabled=true;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1&&factions==2);
 Setup();disableInside=true;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1&&!blackboard_empty::phase&&!blackboard_empty::ownerEntered);
 Setup();throwFaction=true;try{blackboard_empty::FactionUpdate(board,expectedDt);C(false);}catch(int n){C(n==43);}C(!blackboard_empty::phase&&!blackboard_empty::ownerEntered&&nativeCalls==0);
 Setup();blackboard_empty::FactionUpdate(board,expectedDt);*(size_t*)(board+0x178)=~size_t(0);blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1);
 Setup();void*inaccessible=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);C(inaccessible);expectedSelf=inaccessible;blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==1);VirtualFree(inaccessible,0,MEM_RELEASE);
 Setup();blackboard_empty::phase=true;HANDLE worker=CreateThread(NULL,0,Worker,NULL,0,NULL);C(worker&&WaitForSingleObject(worker,10000)==WAIT_OBJECT_0);CloseHandle(worker);C(nativeCalls==1&&blackboard_empty::phase);blackboard_empty::phase=false;
 Setup();void*bad=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);C(bad&&!blackboard_empty::ReadableEmpty(bad));VirtualFree(bad,0,MEM_RELEASE);
 image=(unsigned char*)VirtualAlloc(NULL,0x232c000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(image!=NULL);
 for(int mode=0;mode<13;++mode){
  Setup();sdk=identity=true;gog=false;hookFailure=-1;errors=details=hookCalls=0;blackboard_empty::tried=false;blackboard_empty::ready=0;blackboard_empty::originalBlackboard=blackboard_empty::originalFaction=NULL;
  for(unsigned i=0;i<sizeof(blackboard_empty::proofs)/sizeof(blackboard_empty::proofs[0]);++i){const blackboard_empty::Proof&p=blackboard_empty::proofs[i];memcpy(image+p.rva,p.bytes,p.size);}
  if(mode==1)gog=true;if(mode==2)sdk=false;if(mode==3)identity=false;if(mode>=4&&mode<=10)image[blackboard_empty::proofs[mode-4].rva]^=1;if(mode==11)hookFailure=0;if(mode==12)hookFailure=1;
  InstallBlackboardEmpty(NULL,NULL);unsigned calls=hookCalls;InstallBlackboardEmpty(NULL,NULL);C(hookCalls==calls);
  C(blackboard_empty::ready==(mode==0?1:0));C(hookCalls==(mode==0||mode==12?2:mode==11?1:0));C(errors==(mode==0||mode==1?0:1));
  if(hookCalls){C(blackboard_empty::originalBlackboard==NativeBoard);if(hookCalls==2){C(blackboard_empty::originalFaction==NativeFaction);caller=(void*)(blackboard_empty::imageBase+0x78848a);blackboard_empty::FactionUpdate(board,expectedDt);C(nativeCalls==(mode==0?0:1));}else{caller=(void*)(blackboard_empty::imageBase+0x7eb1bd);blackboard_empty::BlackboardUpdate(board,expectedDt);C(nativeCalls==1);}}
 }
 VirtualFree(image,0,MEM_RELEASE);
 std::printf("PASS %u actual Blackboard wrapper controls\n",checks);return 0;
}catch(...){return 1;}}
