#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "resources/resource_biome_manifest.h"
#include <cmath>

namespace resource_biome {
Status CellsForBounds(const Bounds& b,CellList& out){
    out.count=0;
    // Ordered comparisons also reject NaN and both infinities.
    if(!(b.minX>=-147456.0&&b.minZ>=-147456.0&&b.maxX<=147456.0&&b.maxZ<=147456.0
         &&b.minX<b.maxX&&b.minZ<b.maxZ))return BadBounds;
    unsigned x0=unsigned(std::floor((b.minX+147456.0)/9216.0));
    unsigned z0=unsigned(std::floor((b.minZ+147456.0)/9216.0));
    unsigned x1=unsigned(std::floor((b.maxX+147456.0)/9216.0));
    unsigned z1=unsigned(std::floor((b.maxZ+147456.0)/9216.0));
    if(x1==32)x1=31;if(z1==32)z1=31;
    if(x1-x0>1||z1-z0>1)return TooManyCells;
    for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x)out.index[out.count++]=z*32+x;
    return Ok;
}
int RoleSource(unsigned role,unsigned mask){
    if(role>=6)return -1;
    return (mask&(1u<<role))?int(role):(mask&1)?0:-1;
}
const char* StatusName(Status s){
    static const char* names[]={"ok","not initialized","wrong thread","getter contract",
        "biome metadata","world bounds","too many cells","biome record","texture type","path","read fault"};
    return unsigned(s)<sizeof(names)/sizeof(names[0])?names[s]:"unknown";
}
}

#ifndef RESOURCE_BIOME_PURE_TEST
#include <windows.h>
#include <cstring>
#include <stddef.h>
#include "game/klib_bindings.h"
#include "base/klib_include.h"
#include <kenshi/ZoneManager.h>
#include <kenshi/GameData.h>
#include "base/klib_include_end.h"

// Compile against the actual SDK; a changed layout must fail the native build.
static_assert(offsetof(ZoneManager,biomeMap)==0x168220,"biome map offset");
static_assert(offsetof(GameData,type)==0x50,"biome type offset");
static_assert(offsetof(GameData,stringID)==0x58,"biome SID offset");
static_assert(offsetof(GameData,filesdata)==0x1f8,"biome files offset");

namespace resource_biome { namespace {
unsigned long mainId=0;
bool ready=false;
struct Keys {
    std::string values[2][6];
    Keys(){
        const char* roles[]={"base","slope","vertical","grass","dirt","road"};
        for(unsigned t=0;t<2;++t)for(unsigned i=0;i<6;++i)
            values[t][i]=std::string("texture ")+roles[i]+(t?" normal":"");
    }
};
Keys* keys=NULL; // Process lifetime: constructed once by main-thread Initialize.
struct MapHeader { unsigned fine,grid;const unsigned char* table; };
struct Slots { const void* data[5];const void* tree; };
struct RawFiles { char sid[256];char path[6][1024]; };
static_assert(sizeof(MapHeader)==16,"biome table header packing");
static_assert(sizeof(Slots)==48,"biome slot packing");

const void* ResolveGetter(){
    __try{return (const void*)KlibRealAddress(&ZoneManager::getBiomeMap);}
    __except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}

Status GuardGetter(const void* address){
    static const unsigned char bytes[]={0x48,0x8b,0x81,0x20,0x82,0x16,0x00,0xc3};
    __try {
        return address&&std::memcmp(address,bytes,sizeof(bytes))==0?Ok:BadGetter;
    } __except(EXCEPTION_EXECUTE_HANDLER){return BadGetter;}
}
Status ReadCandidatesBody(const void* manager,const CellList* cells,BiomePointers* out){
    if(!manager)return BadMetadata;
    const unsigned char* bm=*(const unsigned char* const*)((const unsigned char*)manager+0x168220);
    if(!bm)return BadMetadata;
    MapHeader h=*(const MapHeader*)(bm+0x50);
    if(h.fine!=32||h.grid!=32||!h.table)return BadMetadata;
    const unsigned char* image=*(const unsigned char* const*)bm;
    if(!image)return BadMetadata;
    const float* world=(const float*)(image+0xc0);
    if(world[0]!=-147456.0f||world[1]!=-147456.0f||world[2]!=294912.0f||world[3]!=294912.0f)return BadMetadata;
    Slots copied[4];
    for(unsigned c=0;c<cells->count;++c){
        copied[c]=*(const Slots*)(h.table+cells->index[c]*48);
        if(!copied[c].tree)return BadMetadata;
        for(unsigned s=0;s<5;++s){
            const void* data=copied[c].data[s];if(!data)continue;
            unsigned i=0;for(;i<out->count;++i)if(out->data[i]==data)break;
            if(i==out->count)out->data[out->count++]=data;
        }
    }
    const MapHeader* after=(const MapHeader*)(bm+0x50);
    if(after->fine!=h.fine||after->grid!=h.grid||after->table!=h.table
       ||*(const unsigned char* const*)((const unsigned char*)manager+0x168220)!=bm
       ||*(const unsigned char* const*)bm!=image)return BadMetadata;
    for(unsigned c=0;c<cells->count;++c)
        if(std::memcmp(&copied[c],h.table+cells->index[c]*48,sizeof(Slots)))return BadMetadata;
    return out->count?Ok:BadMetadata;
}
Status ReadCandidates(const void* manager,const CellList* cells,BiomePointers* out){
    __try{return ReadCandidatesBody(manager,cells,out);}
    __except(EXCEPTION_EXECUTE_HANDLER){return ReadFault;}
}
bool RawString(const std::string& value,char* out,unsigned capacity){
    size_t n=value.size();if(n>=capacity)return false;
    if(n)std::memcpy(out,value.data(),n);out[n]=0;
    return n==0||std::memchr(out,0,n)==NULL;
}
Status ReadFilesBody(const void* pointer,int type,RawFiles* out){
    const GameData* data=(const GameData*)pointer;
    if(!data||data->type!=BIOMES)return BadBiome;
    if(data->filesdata.size()>256||!RawString(data->stringID,out->sid,sizeof(out->sid))||!out->sid[0])return BadBiome;
    unsigned mask=0;
    for(unsigned i=0;i<6;++i){
        out->path[i][0]=0;
        const auto found=data->filesdata.find(keys->values[type][i]);
        if(found!=data->filesdata.end()){
            if(!RawString(found->second,out->path[i],sizeof(out->path[i])))return BadPath;
            if(out->path[i][0])mask|=1u<<i;
        }
    }
    // The engine substitutes base for unspecified roles. With no base the
    // metadata is incomplete; refusing is preferable to inventing a resource.
    if(!(mask&1))return BadPath;
    for(unsigned i=1;i<6;++i)if(RoleSource(i,mask)==0)
        std::memcpy(out->path[i],out->path[0],std::strlen(out->path[0])+1);
    return Ok;
}
Status ReadFiles(const void* pointer,int type,RawFiles* out){
    __try{return ReadFilesBody(pointer,type,out);}
    __except(EXCEPTION_EXECUTE_HANDLER){return ReadFault;}
}
Status MainThread(){return !ready?NotInitialized:GetCurrentThreadId()!=mainId?WrongThread:Ok;}
} // namespace

Status Initialize(unsigned long id){
    if(!id||GetCurrentThreadId()!=id)return WrongThread;
    if(ready)return mainId==id?Ok:WrongThread;
    const void* address=ResolveGetter();
    Status s=GuardGetter(address);if(s!=Ok)return s;
    static Keys ownedKeys;keys=&ownedKeys;mainId=id;ready=true;return Ok;
}
Status CandidateBiomes(const void* manager,const Bounds& b,BiomePointers& out){
    out.count=0;Status s=MainThread();if(s!=Ok)return s;
    CellList cells;s=CellsForBounds(b,cells);if(s!=Ok)return s;
    s=ReadCandidates(manager,&cells,&out);if(s!=Ok)out.count=0;return s;
}
Status CopyTerrainFiles(const void* biome,int type,OwnedFiles& out){
    out.Clear();Status s=MainThread();if(s!=Ok)return s;if(type<0||type>1)return BadType;
    RawFiles raw;s=ReadFiles(biome,type,&raw);if(s!=Ok)return s;
    out.sid=raw.sid;for(unsigned i=0;i<6;++i)out.paths[i]=raw.path[i];return Ok;
}
}
#endif
