#define NOMINMAX
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstring>
static DWORD mainThread=GetCurrentThreadId();bool IsMainThread(){return GetCurrentThreadId()==mainThread;}
namespace fixes{struct Config{bool animationStateLookupEnabled,animationDirtyReuseEnabled;};Config g_fixesCfg={true,true};}
static HANDLE go=NULL,attempt=NULL,done=NULL;static bool pauseOnce=false,blocked=false;
// Test-only pause while the real native outer lock is held; return the real id.
static DWORD WINAPI FixtureThreadId(){const DWORD tid=GetCurrentThreadId();if(tid==mainThread&&pauseOnce){pauseOnce=false;SetEvent(go);WaitForSingleObject(attempt,5000);Sleep(15);blocked=WaitForSingleObject(done,0)==WAIT_TIMEOUT;}return tid;}
#define KEO_ANIMATION_PATCH_TEST
#include "../src/fixes/world/equipment_animation_patch.cpp"
#define KEO_DIRTY_PATCH_TEST
#define GetCurrentThreadId FixtureThreadId
#include "../src/fixes/world/animation_dirty_patch.cpp"
#undef GetCurrentThreadId
using namespace equipment_animation_lookup;
typedef void*(__fastcall*Ctor)(void*);typedef void(__fastcall*Dtor)(void*);
typedef void*(__fastcall*Create)(void*,const std::string&,unsigned,float,float,float,bool);
typedef void*(__fastcall*StateCtor)(void*,const std::string&,unsigned,void*,float,float,float,bool);
static animation_dirty_patch::Original nativeNotify=NULL;static volatile LONG nativeCalls=0;
static void __fastcall CountNotify(void*p){InterlockedIncrement(&nativeCalls);nativeNotify(p);}
static Create create=NULL;static void*shared=NULL;
static DWORD WINAPI Worker(void*){WaitForSingleObject(go,5000);SetEvent(attempt);const std::string name="worker";create(shared,name,501,0,1,1,false);SetEvent(done);return 0;}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){HMODULE dll=LoadLibraryExA("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll",NULL,LOAD_WITH_ALTERED_SEARCH_PATH);C(dll!=NULL);unsigned char*b=(unsigned char*)dll;Ctor ctor=(Ctor)(b+0x2a8e0);Dtor dtor=(Dtor)(b+0x2aca0);create=(Create)(b+0x2af70);nativeNotify=(animation_dirty_patch::Original)(b+0x2ba20);
 unsigned char beforeAppend[5],beforeDirty[5];memcpy(beforeAppend,b+0x2b182,5);memcpy(beforeDirty,b+0x2a357,5);
 equipment_animation_patch::original=(Original)(b+0x2bd80);equipment_animation_patch::allocate=(Allocate)(b+0x2df30);equipment_animation_patch::insert=(Insert)(b+0x2d750);equipment_animation_patch::expectedReturn=(uintptr_t)b+0x2b187;C(equipment_animation_patch::Arm(b+0x2b182,beforeAppend));equipment_animation_patch::admitted=true;
 animation_dirty_patch::original=CountNotify;animation_dirty_patch::ownerThread=mainThread;animation_dirty_patch::expectedReturn=(uintptr_t)b+0x2a35c;C(animation_dirty_patch::Arm(b+0x2a357,beforeDirty));animation_dirty_patch::admitted=true;
 __declspec(align(16)) unsigned char set[0x68]={0};C(ctor(set)==set);const std::string name="state";
 for(unsigned mask=0;mask<4;++mask){fixes::g_fixesCfg.animationStateLookupEnabled=(mask&1)!=0;fixes::g_fixesCfg.animationDirtyReuseEnabled=(mask&2)!=0;nativeCalls=0;const unsigned old=*(unsigned*)(set+0x18);C(create(set,name,mask,0,1,1,false)!=NULL);C(*(unsigned*)set==0&&*(unsigned*)(set+4)==0&&*(unsigned*)(set+0x18)==old+1);C(nativeCalls==((mask&2)?0:1));}
 fixes::g_fixesCfg.animationStateLookupEnabled=fixes::g_fixesCfg.animationDirtyReuseEnabled=true;
 __declspec(align(16)) unsigned char state[0x60]={0};StateCtor stateCtor=(StateCtor)(b+0x2a2d0);Dtor stateDtor=(Dtor)(b+0x2a290);nativeCalls=0;const unsigned old=*(unsigned*)(set+0x18);C(stateCtor(state,name,900,set,0,1,1,false)==state);C(nativeCalls==1&&*(unsigned*)set==0&&*(unsigned*)(set+0x18)==old+1);stateDtor(state);
 go=CreateEventA(NULL,TRUE,FALSE,NULL);attempt=CreateEventA(NULL,TRUE,FALSE,NULL);done=CreateEventA(NULL,TRUE,FALSE,NULL);C(go&&attempt&&done);shared=set;HANDLE worker=CreateThread(NULL,0,Worker,NULL,0,NULL);C(worker!=NULL);nativeCalls=0;pauseOnce=true;C(create(set,name,500,0,1,1,false)!=NULL);C(blocked);C(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);C(nativeCalls==1&&*(unsigned*)set==0&&*(unsigned*)(set+4)==0&&*(uint64_t*)(set+0x30)==6);CloseHandle(worker);CloseHandle(go);CloseHandle(attempt);CloseHandle(done);dtor(set);
 code_patch_batch::Edit undo[2]={0};const unsigned sites[]={0x2b182,0x2a357};const unsigned char*bytes[]={beforeAppend,beforeDirty};for(unsigned i=0;i<2;++i){undo[i].address=b+sites[i];undo[i].size=5;memcpy(undo[i].before,undo[i].address,5);memcpy(undo[i].after,bytes[i],5);}code_patch_threads::Set threads={0};bool possible=false;C(code_patch_batch::Apply(undo,2,&threads,&possible));FreeLibrary(dll);printf("PASS %u actual native combined animation/owned-lock/contention checks\n",checks);return 0;}
