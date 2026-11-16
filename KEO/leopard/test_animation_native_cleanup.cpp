#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "fixes/world/equipment_animation_lookup.h"
#include "fixes/near_page.h"
#include "fixes/code_patch_batch.h"
using namespace equipment_animation_lookup;
typedef void*(__fastcall*Ctor)(void*);typedef void(__fastcall*Dtor)(void*);
typedef void(__fastcall*Release)(void*,void*,uintptr_t);
static Original original=NULL;static Allocate allocate=NULL;static Insert insert=NULL;static Release release=NULL;
static unsigned mode=0,freed=0;static void*captured=NULL;static void**reentrantSlot=NULL;
static void __fastcall CountRelease(void*pool,void*block,uintptr_t arg){if(block==captured)++freed;release(pool,block,arg);}
static void*__fastcall ReentrantAllocate(void*map,const void*value){
 if(mode==1)reentrantSlot=original(map,(const unsigned*)value);
 if(mode==2)*(uint64_t*)((char*)map+0x10)=~(uint64_t)0-1;
 captured=allocate(map,value);return captured;
}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){HMODULE dll=LoadLibraryExA("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll",NULL,LOAD_WITH_ALTERED_SEARCH_PATH);C(dll!=NULL);unsigned char*base=(unsigned char*)dll;
 Ctor ctor=(Ctor)(base+0x2a8e0);Dtor dtor=(Dtor)(base+0x2aca0);original=(Original)(base+0x2bd80);allocate=(Allocate)(base+0x2df30);insert=(Insert)(base+0x2d750);release=(Release)(base+0x1a1340);
 unsigned char*island=AllocateNearPages((unsigned __int64)(uintptr_t)(base+0x2cc46),4096);C(island!=NULL);island[0]=0xff;island[1]=0x25;memset(island+2,0,4);const uintptr_t fn=(uintptr_t)&CountRelease;memcpy(island+6,&fn,8);DWORD old=0;C(VirtualProtect(island,4096,PAGE_EXECUTE_READ,&old)!=FALSE);FlushInstructionCache(GetCurrentProcess(),island,14);
 code_patch_batch::Edit edits[2]={0};const unsigned sites[]={0x2cc46,0x2db0c};
 for(unsigned i=0;i<2;++i){edits[i].address=base+sites[i];edits[i].size=5;memcpy(edits[i].before,edits[i].address,5);C(edits[i].before[0]==0xe8);int actual=0;memcpy(&actual,edits[i].before+1,4);C(edits[i].address+5+actual==base+0x1a1340);memcpy(edits[i].after,edits[i].before,5);const __int64 delta=(__int64)(uintptr_t)island-(__int64)(uintptr_t)(edits[i].address+5);C(delta>=-2147483647LL-1&&delta<=2147483647LL);const int relative=(int)delta;memcpy(edits[i].after+1,&relative,4);}
 code_patch_threads::Set threads={0};bool possible=false;C(code_patch_batch::Apply(edits,2,&threads,&possible));
 {__declspec(align(16)) unsigned char set[0x68]={0};C(ctor(set)==set);void*map=set+0x20;const unsigned key=77;mode=1;freed=0;captured=NULL;reentrantSlot=NULL;void**slot=Append(map,&key,original,ReentrantAllocate,insert,true,true);
  C(slot==reentrantSlot&&slot==original(map,&key));C(*slot==NULL&&*(uint64_t*)((char*)map+0x10)==1);C(freed==1);dtor(set);}
 {__declspec(align(16)) unsigned char set[0x68]={0};C(ctor(set)==set);void*map=set+0x20;const unsigned key=88;mode=2;freed=0;captured=NULL;bool threw=false;try{Append(map,&key,original,ReentrantAllocate,insert,true,true);}catch(const std::length_error&){threw=true;}
  C(threw&&freed==1);C(*(uint64_t*)((char*)map+0x10)==~(uint64_t)0-1);*(uint64_t*)((char*)map+0x10)=0;dtor(set);}
 for(unsigned i=0;i<2;++i){unsigned char temp[5];memcpy(temp,edits[i].before,5);memcpy(edits[i].before,edits[i].after,5);memcpy(edits[i].after,temp,5);}C(code_patch_batch::Apply(edits,2,&threads,&possible));VirtualFree(island,0,MEM_RELEASE);FreeLibrary(dll);
 printf("PASS %u actual Ogre duplicate/reentrant allocation/size-error native-free checks\n",checks);return 0;}
