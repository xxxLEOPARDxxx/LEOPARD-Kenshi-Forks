#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <ogre/OgreFileSystem.h>
#include "resources/resource_cache_stream.h"
using namespace resource_cache;
int main(){
    char dir[MAX_PATH];GetTempPathA(MAX_PATH,dir);
    char filename[64];sprintf_s(filename,"keo-native-stream-%lu.dds",GetCurrentProcessId());
    std::string path=std::string(dir)+filename;
    {std::ofstream f(path.c_str(),std::ios::binary);f<<"abcdefghijklmnop";}
    Ogre::FileSystemArchive archive(dir,"FileSystem",true);
    Ogre::String name(filename);
    Ogre::DataStreamPtr normal=archive.open(name,true);
    assert(normal->size()==16 && normal->getAsString()=="abcdefghijklmnop");normal.setNull();
    // Explicit hidden-return ABI used by the production hook, against actual Ogre DLL.
    typedef Ogre::DataStreamPtr* (*OpenFn)(Ogre::FileSystemArchive*,Ogre::DataStreamPtr*,const Ogre::String&,bool);
    OpenFn open=reinterpret_cast<OpenFn>(GetProcAddress(GetModuleHandleA("OgreMain_x64.dll"),"?open@FileSystemArchive@Ogre@@UEAA?AV?$SharedPtr@VDataStream@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z"));
    assert(open);
    std::aligned_storage<sizeof(Ogre::DataStreamPtr),__alignof(Ogre::DataStreamPtr)>::type storage;
    Ogre::DataStreamPtr* out=reinterpret_cast<Ogre::DataStreamPtr*>(&storage);
    assert(open(&archive,out,name,true)==out);
    assert((*out)->size()==16 && (*out)->getAsString()=="abcdefghijklmnop");out->~SharedPtr();
    assert(Start());Configure(true,1024*1024,128);Observe(path);
    FilePtr f;
    for(int i=0;i<100&&!f;++i){Sleep(20);f=Find(path);}assert(f);
    Ogre::DataStreamPtr a(new CachedStream(name,f)),b(new CachedStream(name,f));
    char bytes[9]={0};assert(a->read(bytes,8)==8 && std::string(bytes)=="abcdefgh");
    assert(b->tell()==0 && a->tell()==8 && !a->isWriteable());
    Configure(false,1024*1024,128);Sleep(300);f.reset();
    assert(Snapshot().entries==0 && Snapshot().allocatedBytes==16);
    a->seek(0);assert(a->getAsString()=="abcdefghijklmnop");
    a.setNull();b.setNull();assert(Snapshot().allocatedBytes==0);
    Stop();DeleteFileA(path.c_str());
    std::cout<<"native archive ABI and retained Ogre stream tests passed\n";
}
