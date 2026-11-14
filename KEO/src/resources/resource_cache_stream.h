#pragma once
#include "resources/resource_cache_store.h"
#include <ogre/OgreDataStream.h>
namespace resource_cache {
// Independent native stream positions, immutable shared backing storage. The
// inherited stream never owns/free()s the bytes; the shared file owns them.
class CachedStream:public Ogre::MemoryDataStream {
    FilePtr owner;
public:
    CachedStream(const Ogre::String&name,const FilePtr&f)
        :Ogre::MemoryDataStream(name,const_cast<unsigned char*>(&f->data[0]),f->data.size(),false,true),owner(f){}
};
}
