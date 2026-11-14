#pragma once
#include "resources/resource_biome_manifest.h"
#include "resources/resource_forward_policy.h"
#include "resources/resource_gpu_store.h"
namespace resource_forward {
// Read the engine's actual deferred request on main, keep owned names only.
inline bool CopyArrayJob(const void*request,unsigned __int64 generation,resource_gpu::Job&job){
    resource_biome::OwnedArrayFiles files;
    if(resource_biome::CopyArrayFiles(request,files)!=resource_biome::Ok)return false;
    job=resource_gpu::Job();job.type=unsigned(files.name[7]-'0');job.worldGeneration=generation;
    for(unsigned i=0;i<6;++i)job.paths[i]=files.paths[i];
    job.key=TerrainKey(files.name.substr(9),int(job.type),job.paths);
    return true;
}
}
