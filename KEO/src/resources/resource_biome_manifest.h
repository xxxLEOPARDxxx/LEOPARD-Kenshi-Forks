#pragma once
#include <string>

// Main-thread metadata only. Returned biome pointers are borrowed for the
// current tick; never retain them or send them to a worker. OwnedFiles contains
// copied strings and is the only result suitable for a worker queue.
namespace resource_biome {
enum Status { Ok, NotInitialized, WrongThread, BadGetter, BadMetadata,
              BadBounds, TooManyCells, BadBiome, BadType, BadPath, ReadFault };
struct Bounds {
    double minX,minZ,maxX,maxZ;
    Bounds(double a=0,double b=0,double c=0,double d=0):minX(a),minZ(b),maxX(c),maxZ(d){}
};
struct CellList { unsigned index[4],count; CellList():count(0){} };
struct BiomePointers {
    const void* data[20]; unsigned count;
    BiomePointers():count(0){}
};
struct OwnedFiles {
    std::string sid,paths[6];
    void Clear(){sid.clear();for(unsigned i=0;i<6;++i)paths[i].clear();}
};
// Fixed profiled geography only; rejects other world layouts. Conservative
// inclusive upper bounds may include the adjacent coarse cell at a seam.
Status CellsForBounds(const Bounds&,CellList&);
// Pure native role-fallback rule: own nonempty role, then base, else unavailable.
int RoleSource(unsigned role,unsigned populatedMask);
const char* StatusName(Status);
// Caller supplies the game's actual main-thread ID; initialization on another
// thread refuses. Resolves/guards a getter but never calls any engine function.
Status Initialize(unsigned long mainThreadId);
Status CandidateBiomes(const void* zoneManager,const Bounds&,BiomePointers&);
// type0=diffuse, type1=normal; base/slope/vertical/grass/dirt/road order.
// Paths remain the raw relative metadata values; caller handles safe resolution.
Status CopyTerrainFiles(const void* biome,int type,OwnedFiles&);
}
