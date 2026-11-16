#define NOMINMAX
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstring>
#include "fixes/world/equipment_animation_lookup.h"
static DWORD owner=GetCurrentThreadId();bool IsMainThread(){return GetCurrentThreadId()==owner;}
namespace fixes{struct Config{bool animationStateLookupEnabled;};Config g_fixesCfg={true};}
#define KEO_ANIMATION_PATCH_TEST
#include "../src/fixes/world/equipment_animation_patch.cpp"
using namespace equipment_animation_lookup;
static Original nativeOriginal=NULL;static unsigned originals=0;
static void**__fastcall CountOriginal(void*m,const unsigned*k){++originals;return nativeOriginal(m,k);}
static bool BoundState(void*set,unsigned key,void*state){return *nativeOriginal((char*)set+0x20,&key)==state;}
typedef void*(__fastcall*Ctor)(void*);
typedef void(__fastcall*Dtor)(void*);
typedef void*(__fastcall*Create)(void*,const std::string&,unsigned,float,float,float,bool);
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){HMODULE dll=LoadLibraryExA("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll",NULL,LOAD_WITH_ALTERED_SEARCH_PATH);C(dll!=NULL);unsigned char*base=(unsigned char*)dll;
 Ctor ctor=(Ctor)(base+0x2a8e0);Dtor dtor=(Dtor)(base+0x2aca0);Create create=(Create)(base+0x2af70);nativeOriginal=(Original)(base+0x2bd80);
 equipment_animation_patch::original=CountOriginal;equipment_animation_patch::allocate=(Allocate)(base+0x2df30);equipment_animation_patch::insert=(Insert)(base+0x2d750);equipment_animation_patch::expectedReturn=(uintptr_t)base+0x2b187;
 unsigned char expected[5];memcpy(expected,base+0x2b182,5);C(equipment_animation_patch::Arm(base+0x2b182,expected));equipment_animation_patch::admitted=true;
 __declspec(align(16)) unsigned char storage[0x68]={0};C(ctor(storage)==storage);const std::string name="native_state";
 fixes::g_fixesCfg.animationStateLookupEnabled=false;originals=0;C(create(storage,name,1,0,1,1,false)!=NULL&&originals==1);
 fixes::g_fixesCfg.animationStateLookupEnabled=true;originals=0;void*state=create(storage,name,10,0,1,1,false);C(state!=NULL&&originals==0&&BoundState(storage,10,state));
 equipment_animation_patch::expectedReturn++;originals=0;C(create(storage,name,20,0,1,1,false)!=NULL&&originals==1);equipment_animation_patch::expectedReturn--;
 equipment_animation_patch::admitted=false;originals=0;C(create(storage,name,30,0,1,1,false)!=NULL&&originals==1);equipment_animation_patch::admitted=true;
 owner++;originals=0;C(create(storage,name,40,0,1,1,false)!=NULL&&originals==1);owner--;
 originals=0;C(create(storage,name,5,0,1,1,false)!=NULL&&originals==1);
 originals=0;C(create(storage,name,0xffffffff,0,1,1,false)!=NULL&&originals==0);
 C(*(uint64_t*)(storage+0x30)==7);dtor(storage);
 code_patch_batch::Edit undo={0};undo.address=base+0x2b182;undo.size=5;memcpy(undo.before,undo.address,5);memcpy(undo.after,expected,5);code_patch_threads::Set threads={0};bool possible=false;C(code_patch_batch::Apply(&undo,1,&threads,&possible));C(memcmp(base+0x2b182,expected,5)==0);FreeLibrary(dll);
 printf("PASS %u real Ogre animation CALL/bridge/config/unwind lifecycle checks\n",checks);return 0;}
