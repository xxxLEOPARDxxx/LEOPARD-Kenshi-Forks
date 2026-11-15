#pragma once
#include <string>
#include <string.h>
namespace resource_cache {
// Pure extension admission, before constructing a path or consulting the cache.
// Keep both archive forwarding and absolute-path admission on the same list.
inline bool TextureFileName(const std::string& path){
    const size_t dot=path.find_last_of('.');if(dot==std::string::npos)return false;
    const char*ext=path.c_str()+dot;
    return !_stricmp(ext,".dds")||!_stricmp(ext,".png")||!_stricmp(ext,".tga")||
           !_stricmp(ext,".jpg")||!_stricmp(ext,".jpeg")||!_stricmp(ext,".bmp");
}
inline bool CacheableFileName(const std::string& path){
    const size_t dot=path.find_last_of('.');if(dot==std::string::npos)return false;
    const char*ext=path.c_str()+dot;
    return TextureFileName(path)||!_stricmp(ext,".mesh")||!_stricmp(ext,".skeleton");
}
}
