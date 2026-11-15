#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <memory>
#include <type_traits>
#include <stdio.h>
#include <stdlib.h>
#include <ogre/OgreFileSystem.h>
#include "resources/resource_cache_stream.h"
#include "resources/resource_cache_admission.h"
static unsigned paths=0,finds=0,observes=0,calls=0,checks=0;
static bool throwsNative=false,throwsLookup=false;static volatile LONG active=1;
static void Check(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
namespace resource_cache {
FilePtr Find(const std::string&){++finds;if(throwsLookup)throw 21;return FilePtr();}
void Observe(const std::string&){++observes;}
}
static DWORD WINAPI PathProbe(LPCSTR p,DWORD n,LPSTR out,LPSTR*part){++paths;return GetFullPathNameA(p,n,out,part);}
typedef Ogre::DataStreamPtr*(*OpenFn)(Ogre::FileSystemArchive*,Ogre::DataStreamPtr*,const Ogre::String&,bool);
static Ogre::FileSystemArchive*seenArchive=NULL;static Ogre::String seenName;static bool seenReadOnly;
static Ogre::DataStreamPtr*Native(Ogre::FileSystemArchive*a,Ogre::DataStreamPtr*out,const Ogre::String&name,bool readOnly){
 ++calls;seenArchive=a;seenName=name;seenReadOnly=readOnly;if(throwsNative)throw 17;
 new(out)Ogre::DataStreamPtr(new Ogre::MemoryDataStream(16));return out;
}
static OpenFn original=&Native;
#define GetFullPathNameA PathProbe
#include "archive_open_production.inc"
#undef GetFullPathNameA
int main(){
 Ogre::FileSystemArchive archive("D:/keo-archive-fixture","FileSystem",true);
 const char*unsupported[]={"boot.material","npc.xml","layout.LAYOUT","shader.program","gui.fontdef","settings.cfg","foo",".dds/filename","a.mesh.xml",""};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(*unsupported);++i){
  paths=finds=observes=calls=0;Ogre::String name(unsupported[i]);
  std::aligned_storage<sizeof(Ogre::DataStreamPtr),__alignof(Ogre::DataStreamPtr)>::type storage;
  Ogre::DataStreamPtr*out=reinterpret_cast<Ogre::DataStreamPtr*>(&storage);
  Check(Open(&archive,out,name,true)==out&&calls==1&&seenArchive==&archive&&seenName==name&&seenReadOnly,"unsupported forwarding keeps native output/args/exact-once");
  Check(paths==0&&finds==0&&observes==0,"unsupported names bypass all path/cache work");out->~SharedPtr();
 }
 const char*supported[]={"npc.mesh","anim.SKELETON","body.DdS","a.png","a.tga","a.jpg","a.JPEG","a.bmp"};
 for(unsigned i=0;i<sizeof(supported)/sizeof(*supported);++i){
  paths=finds=observes=calls=0;Ogre::String name(supported[i]);
  std::aligned_storage<sizeof(Ogre::DataStreamPtr),__alignof(Ogre::DataStreamPtr)>::type storage;
  Ogre::DataStreamPtr*out=reinterpret_cast<Ogre::DataStreamPtr*>(&storage);
  Check(Open(&archive,out,name,true)==out&&calls==1&&paths==1&&finds==1&&observes==1,"supported miss keeps original admission and observation");out->~SharedPtr();
 }
 Ogre::String name("gui.xml");std::aligned_storage<sizeof(Ogre::DataStreamPtr),__alignof(Ogre::DataStreamPtr)>::type storage;Ogre::DataStreamPtr*out=reinterpret_cast<Ogre::DataStreamPtr*>(&storage);
 throwsNative=true;calls=paths=finds=observes=0;bool thrown=false;try{Open(&archive,out,name,true);}catch(int code){thrown=code==17;}
 Check(thrown&&calls==1&&paths==0&&observes==0,"unsupported native exception remains exact");throwsNative=false;
 name="npc.mesh";active=0;calls=paths=finds=observes=0;Check(Open(&archive,out,name,true)==out&&calls==1&&paths==0,"disabled cache performs no path work");out->~SharedPtr();active=1;
 calls=paths=finds=observes=0;Check(Open(&archive,out,name,false)==out&&calls==1&&paths==0&&!seenReadOnly,"write open forwards unchanged");out->~SharedPtr();
 throwsLookup=true;calls=paths=finds=observes=0;Check(Open(&archive,out,name,true)==out&&calls==1&&observes==0,"optional lookup failure preserves native result");out->~SharedPtr();
 printf("PASS %u actual archive-wrapper controls\n",checks);return 0;
}
