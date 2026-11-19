#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "fixes/near_page.h"
#ifndef KEO_DIRTY_BASELINE
#include "fixes/world/animation_dirty_reuse.h"
namespace fixes{struct Config{bool animationDirtyReuseEnabled;};Config g_fixesCfg={true};}
#define KEO_DIRTY_PATCH_TEST
#include "../src/fixes/world/animation_dirty_patch.cpp"
static volatile LONG fallbackCalls=0;static bool fallbackThrows=false;
static void __fastcall ForwardNotify(void*){InterlockedIncrement(&fallbackCalls);if(fallbackThrows)throw std::runtime_error("native notify fallback");}
static void(__fastcall*workerNotify)(void*)=NULL;static unsigned*workerParent=NULL;
static DWORD WINAPI WorkerNotify(void*){workerParent[1]=GetCurrentThreadId();workerNotify(workerParent);return 0;}
namespace dirty_admission {
typedef animation_dirty_patch::Original Original;
static HMODULE gameImage=NULL;static bool owner=true;static void*world=NULL;
static animation_dirty_patch::Original original=NULL;static unsigned ownerThread=0;static uintptr_t expectedReturn=0;
static bool verifiedGog=false,hasWorldGlobal=true;
static bool NativeProfileIsGog(){return verifiedGog;}
static bool IsMainThread(){return owner;}
static HMODULE FakeGetModuleHandleA(const char*){return gameImage;}
static uintptr_t GameAddr(int){return hasWorldGlobal?(uintptr_t)&world:0;}
enum{RVA_GLOBAL_SECTION_MGR=1};
static bool ReadModuleImageId(HMODULE module,DWORD*stamp,DWORD*size){
 if(!module)return false;const IMAGE_DOS_HEADER*dos=(const IMAGE_DOS_HEADER*)module;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew!=0x100)return false;
 const IMAGE_NT_HEADERS64*nt=(const IMAGE_NT_HEADERS64*)((const char*)module+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;
 *stamp=nt->FileHeader.TimeDateStamp;*size=nt->OptionalHeader.SizeOfImage;return true;
}
#define GetModuleHandleA FakeGetModuleHandleA
// Generated verbatim from the production proof arrays and Admit body after
// checking those arrays against the pinned on-disk native image.
#include "../build/animation_dirty/admission.inc"
#undef GetModuleHandleA
static IMAGE_NT_HEADERS64*Header(HMODULE module){return (IMAGE_NT_HEADERS64*)((char*)module+0x100);}
static void Initialize(HMODULE module,DWORD stamp,DWORD size){
 IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)module;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
 IMAGE_NT_HEADERS64*nt=Header(module);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
 nt->FileHeader.TimeDateStamp=stamp;nt->OptionalHeader.SizeOfImage=size;
}
static bool Refused(HMODULE module){original=NULL;ownerThread=0;expectedReturn=0;return !Admit(module)&&!original&&!ownerThread&&!expectedReturn;}
}
#else
namespace animation_dirty_reuse{static bool TryOwned(void*,unsigned){return false;}}
#endif
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
#ifndef KEO_DIRTY_BASELINE
static int Admission(){using namespace dirty_admission;
 gameImage=(HMODULE)VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(gameImage!=NULL);
 HMODULE ogre=(HMODULE)VirtualAlloc(NULL,0x9c9000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(ogre!=NULL);
 Initialize(gameImage,0x65d604d7,0x232c000);Initialize(ogre,0x5ca5f929,0x9c9000);
 for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)memcpy((char*)ogre+proofs[i].rva,proofs[i].bytes,proofs[i].size);
 C(Admit(ogre)&&original==(animation_dirty_patch::Original)((char*)ogre+0x2ba20)&&ownerThread==GetCurrentThreadId()&&expectedReturn==(uintptr_t)ogre+0x2a35c);
 Initialize(gameImage,0x65d60519,0x232a000);C(Refused(ogre));
 verifiedGog=true;C(Admit(ogre));
 hasWorldGlobal=false;C(Refused(ogre));hasWorldGlobal=true;
 world=(void*)1;C(Refused(ogre));world=NULL;
 Initialize(gameImage,0x6602d5e3,0x232c000);C(Refused(ogre));
 verifiedGog=false;Initialize(gameImage,0x65d604d7,0x232c000);
 owner=false;C(Refused(ogre));owner=true;world=(void*)1;C(Refused(ogre));world=NULL;
 C(Refused(NULL));C(Refused((HMODULE)1));HMODULE savedGame=gameImage;gameImage=NULL;C(Refused(ogre));gameImage=savedGame;
 Header(gameImage)->FileHeader.TimeDateStamp^=1;C(Refused(ogre));Header(gameImage)->FileHeader.TimeDateStamp^=1;
 Header(gameImage)->OptionalHeader.SizeOfImage^=1;C(Refused(ogre));Header(gameImage)->OptionalHeader.SizeOfImage^=1;
 Header(ogre)->FileHeader.TimeDateStamp^=1;C(Refused(ogre));Header(ogre)->FileHeader.TimeDateStamp^=1;
 Header(ogre)->OptionalHeader.SizeOfImage^=1;C(Refused(ogre));Header(ogre)->OptionalHeader.SizeOfImage^=1;
 for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i){unsigned char*bytes=(unsigned char*)ogre+proofs[i].rva;bytes[0]^=1;C(Refused(ogre));bytes[0]^=1;bytes[proofs[i].size-1]^=1;C(Refused(ogre));bytes[proofs[i].size-1]^=1;}
 // The independent Append CALL may already be patched; these authenticating
 // windows intentionally do not overlap that CALL's five bytes.
 ((unsigned char*)ogre)[0x2b182]=0xe8;C(Admit(ogre));((unsigned char*)ogre)[0x2b183]^=1;C(Admit(ogre));
 VirtualFree(ogre,0,MEM_RELEASE);VirtualFree(gameImage,0,MEM_RELEASE);gameImage=NULL;original=NULL;return 0;
}
#endif
int main(){__declspec(align(16)) unsigned data[12]={1,GetCurrentThreadId(),0x80000000,0x87654321,0x12345678,0x10203040,9,0xaabbccdd};const unsigned tid=GetCurrentThreadId();
 C(animation_dirty_reuse::TryOwned(data,tid));C(data[0]==1&&data[1]==tid&&data[2]==0x80000000&&data[6]==10&&data[7]==0xaabbccdd);
 data[0]=2;C(animation_dirty_reuse::TryOwned(data,tid)&&data[0]==2&&data[6]==11);data[0]=0xffffffff;data[6]=0xffffffff;C(animation_dirty_reuse::TryOwned(data,tid)&&data[0]==0xffffffff&&data[6]==0);
 data[0]=0;C(!animation_dirty_reuse::TryOwned(data,tid)&&data[6]==0);data[0]=1;C(!animation_dirty_reuse::TryOwned(data,tid+1)&&data[6]==0);C(!animation_dirty_reuse::TryOwned(NULL,tid));
 typedef void(__fastcall*Notify)(void*);HMODULE dll=LoadLibraryExA("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll",NULL,LOAD_WITH_ALTERED_SEARCH_PATH);C(dll!=NULL);Notify native=(Notify)((char*)dll+0x2ba20);
 LARGE_INTEGER frequency,a,b,c;QueryPerformanceFrequency(&frequency);const unsigned count=1000000;
 for(unsigned trial=0;trial<3;++trial){data[0]=1;data[1]=tid;data[6]=0;QueryPerformanceCounter(&a);for(unsigned i=0;i<count;++i)native(data);QueryPerformanceCounter(&b);for(unsigned i=0;i<count;++i)animation_dirty_reuse::TryOwned(data,tid);QueryPerformanceCounter(&c);C(data[0]==1&&data[1]==tid&&data[6]==2*count);double stock=(b.QuadPart-a.QuadPart)*1e9/frequency.QuadPart/count,fast=(c.QuadPart-b.QuadPart)*1e9/frequency.QuadPart/count;printf("BENCH native_notify_ns=%.2f owned_notify_ns=%.2f\n",stock,fast);}
 #ifndef KEO_DIRTY_BASELINE
 C(Admission()==0);
 unsigned char*code=AllocateNearPages((unsigned __int64)(uintptr_t)native,4096);C(code!=NULL);const unsigned char body[]={0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3};memcpy(code,body,sizeof(body));const __int64 displacement=(__int64)(uintptr_t)native-(__int64)(uintptr_t)(code+9);C(displacement>=-2147483647LL-1&&displacement<=2147483647LL);int rel=(int)displacement;memcpy(code+5,&rel,4);unsigned char expected[5];memcpy(expected,code+4,5);const unsigned char ui[]={1,4,1,0,4,0x42,0,0};memcpy(code+0x300,ui,sizeof(ui));DWORD old=0;C(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)!=FALSE);FlushInstructionCache(GetCurrentProcess(),code,4096);RUNTIME_FUNCTION runtime={0,14,0x300};C(RtlAddFunctionTable(&runtime,1,(DWORD64)code)!=FALSE);Notify caller=(Notify)code;
 animation_dirty_patch::original=native;animation_dirty_patch::ownerThread=tid;animation_dirty_patch::expectedReturn=(uintptr_t)(code+9);animation_dirty_patch::admitted=true;
 for(unsigned trial=0;trial<3;++trial){data[6]=0;QueryPerformanceCounter(&a);for(unsigned i=0;i<count;++i)caller(data);QueryPerformanceCounter(&b);C(animation_dirty_patch::Arm(code+4,expected));QueryPerformanceCounter(&c);LARGE_INTEGER end;for(unsigned i=0;i<count;++i)caller(data);QueryPerformanceCounter(&end);double stock=(b.QuadPart-a.QuadPart)*1e9/frequency.QuadPart/count,fast=(end.QuadPart-c.QuadPart)*1e9/frequency.QuadPart/count;C(data[6]==2*count&&data[0]==1&&data[1]==tid);printf("BENCH full_guard_stock_ns=%.2f full_guard_fast_ns=%.2f ratio=%.3f\n",stock,fast,fast/stock);
 code_patch_batch::Edit undo={0};undo.address=code+4;undo.size=5;memcpy(undo.before,undo.address,5);memcpy(undo.after,expected,5);code_patch_threads::Set set={0};bool possible=false;C(code_patch_batch::Apply(&undo,1,&set,&possible));VirtualFree(animation_dirty_patch::page,0,MEM_RELEASE);animation_dirty_patch::page=NULL;}
 C(animation_dirty_patch::Arm(code+4,expected));animation_dirty_patch::original=ForwardNotify;
 data[0]=1;data[1]=tid;data[6]=400;fallbackCalls=0;
 animation_dirty_patch::expectedReturn=(uintptr_t)(code+10);caller(data);C(fallbackCalls==1&&data[6]==400&&data[0]==1);
 animation_dirty_patch::expectedReturn=(uintptr_t)(code+9);animation_dirty_patch::admitted=false;fallbackCalls=0;caller(data);C(fallbackCalls==1&&data[6]==400);animation_dirty_patch::admitted=true;
 fixes::g_fixesCfg.animationDirtyReuseEnabled=false;fallbackCalls=0;caller(data);C(fallbackCalls==1&&data[6]==400);fixes::g_fixesCfg.animationDirtyReuseEnabled=true;
 animation_dirty_patch::ownerThread=tid+1;fallbackCalls=0;caller(data);C(fallbackCalls==1&&data[6]==400);animation_dirty_patch::ownerThread=tid;
 workerNotify=caller;workerParent=data;fallbackCalls=0;HANDLE worker=CreateThread(NULL,0,WorkerNotify,NULL,0,NULL);C(worker!=NULL);C(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);DWORD workerCode=1;C(GetExitCodeThread(worker,&workerCode)&&workerCode==0);CloseHandle(worker);C(fallbackCalls==1&&data[6]==400&&data[0]==1);data[1]=tid;
 fallbackCalls=0;caller(NULL);C(fallbackCalls==1&&data[6]==400);
 data[0]=0;fallbackCalls=0;fallbackThrows=true;bool caught=false;try{caller(data);}catch(const std::runtime_error&){caught=true;}fallbackThrows=false;C(caught&&fallbackCalls==1&&data[6]==400&&data[0]==0);
 data[0]=1;animation_dirty_patch::original=native;
 code_patch_batch::Edit undo={0};undo.address=code+4;undo.size=5;memcpy(undo.before,undo.address,5);memcpy(undo.after,expected,5);code_patch_threads::Set set={0};bool possible=false;C(code_patch_batch::Apply(&undo,1,&set,&possible));VirtualFree(animation_dirty_patch::page,0,MEM_RELEASE);animation_dirty_patch::page=NULL;
 RtlDeleteFunctionTable(&runtime);VirtualFree(code,0,MEM_RELEASE);
 #endif
 FreeLibrary(dll);printf("PASS %u owned-mutex native dirty notification checks\n",checks);return 0;}
