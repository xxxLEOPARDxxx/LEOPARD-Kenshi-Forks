// Exact Init and request wrapper bodies with controlled SDK installation.
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
static bool owner=true,gog=false;static unsigned calls=0,errors=0,nativeCalls=0;
static int status=0;static void*provided=NULL;static unsigned char*image=NULL;
bool IsMainThread(){return owner;}bool NativeProfileIsGog(){return gog;}
#define GetModuleHandleA FixtureModule
void*GetModuleHandleA(const char*){return image;}
static void LogError(const char*){++errors;}
#define KEO_DETAIL(x) ((void)0)
namespace Ogre {struct Entity{};struct MeshPtr{};struct ResourcePtr{};enum SceneMemoryMgrTypes{One,Two};}
namespace boost {template<class T>struct function{};}
struct ResourceLoader {struct MeshLoadData{};};
namespace KenshiLib {enum{SUCCESS=0};int AddHook(void*,void*,void**out){++calls;*out=provided;return status;}}
struct State {struct Stats{bool admitted;Stats():admitted(false){}}stats;};static State*state=NULL;
template<class T>static void ObserveEntity(T*,const Ogre::MeshPtr&){}
static Ogre::Entity entity;static bool failNative=false;
static Ogre::Entity*Native(ResourceLoader*,const Ogre::MeshPtr&,const boost::function<void(Ogre::ResourcePtr,void*)>&,const ResourceLoader::MeshLoadData&,Ogre::SceneMemoryMgrTypes){++nativeCalls;if(failNative)throw std::runtime_error("native");return &entity;}
#include "geometry_hook_actual.inc"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 image=(unsigned char*)VirtualAlloc(NULL,0x450000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(image!=NULL);
 IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)image;dos->e_lfanew=0x100;
 IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)(image+0x100);nt->FileHeader.TimeDateStamp=0x65d604d7;nt->OptionalHeader.SizeOfImage=0x232c000;
 const unsigned char prefix[]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd8,0x48,0x81,0xec,0x28,0x01,0x00,0x00,0x48,0xc7,0x45,0x90,0xfe,0xff,0xff};
 memcpy(image+0x449620,prefix,sizeof(prefix));
 for(unsigned mode=0;mode<4;++mode){
  delete state;state=NULL;originalCreate=NULL;calls=errors=nativeCalls=0;
  status=mode&1;provided=mode&2?NULL:(void*)&Native;Init();
  CHECK(calls==1);CHECK(state!=NULL);CHECK((void*)originalCreate==provided);
  bool ready=status==KenshiLib::SUCCESS&&provided!=NULL;
  CHECK(state->stats.admitted==ready);CHECK(errors==(ready?0u:1u));
  Init();CHECK(calls==1);
  if(originalCreate){ResourceLoader r;Ogre::MeshPtr p;ResourceLoader::MeshLoadData d;boost::function<void(Ogre::ResourcePtr,void*)> cb;
   CHECK(Create(&r,p,cb,d,Ogre::Two)==&entity);CHECK(nativeCalls==1);
   failNative=true;bool caught=false;try{Create(&r,p,cb,d,Ogre::One);}catch(const std::runtime_error&){caught=true;}failNative=false;CHECK(caught&&nativeCalls==2);
  }
 }
 delete state;state=NULL;owner=false;calls=0;Init();CHECK(state==NULL&&calls==0);owner=true;
 gog=true;Init();CHECK(state!=NULL&&!state->stats.admitted&&calls==0);gog=false;
 delete state;state=NULL;image[0x449620]^=1;Init();CHECK(!state->stats.admitted&&calls==0);
 delete state;state=NULL;VirtualFree(image,0,MEM_RELEASE);
 printf("PASS %u geometry hook install/forward checks\n",checks);return 0;
}
