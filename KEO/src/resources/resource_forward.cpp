#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "resources/resource_forward.h"
#include "resources/resource_forward_policy.h"
#include "resources/resource_biome_manifest.h"
#include "zone/preload/preload.h"
#include "zone/transition.h"
#include "zone/zone_config.h"
#include "movement/tracking.h"
#include "base/log_admission.h"
#include "base/klib_include.h"
#include <ogre/OgreTextureManager.h>
#include "base/klib_include_end.h"
#include <sstream>
#include <cstring>
#include <float.h>
namespace {
unsigned __int64 world=1;
volatile LONG resetPending=0;
bool ready=false,gpuStarted=false,metadataReported=false;
double lastFrame=0,lastTick=0,nextCandidates=0,nextConfig=0,nextReport=0,blockedUntil=0;
struct Cell {int x,y;unsigned biome;};
Cell queue[32];unsigned count=0;
double seen[4096]={0};
int previousX=-1,previousY=-1;
unsigned zones=0,manifests=0,prefetches=0,metadataRefused=0,queueDropped=0;
std::string Absolute(const std::string&p){
    if(p.empty()||p.size()>=MAX_PATH)return std::string();
    char full[MAX_PATH];DWORD n=GetFullPathNameA(p.c_str(),MAX_PATH,full,NULL);
    if(!n||n>=MAX_PATH||n<3||full[1]!=':')return std::string();
    for(DWORD i=0;i<n;++i){if(full[i]=='/')full[i]='\\';if(full[i]>='A'&&full[i]<='Z')full[i]+=char('a'-'A');}
    return full;
}
void Add(int x,int y,double now){
    if(x<0||x>63||y<0||y>63)return;
    if(seen[x*64+y]>now)return;
    for(unsigned i=0;i<count;++i)if(queue[i].x==x&&queue[i].y==y)return;
    if(count>=32){++queueDropped;return;}Cell c={x,y,0};queue[count++]=c;
}
void AddAround(int x,int y,int dx,int dy,bool moving,double now){
    resource_forward::Cells cells;resource_forward::Candidates(x,y,dx,dy,moving,cells);
    for(unsigned i=0;i<cells.count;++i)Add(cells.x[i],cells.y[i],now);
}
bool CameraXY(const void*p,float*x,float*z){
    __try{if(!p)return false;*x=*reinterpret_cast<const float*>(p);*z=*reinterpret_cast<const float*>(static_cast<const char*>(p)+8);return _finite(*x)&&_finite(*z);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Candidates(double now,int x,int y){
    int dx=x+(x>previousX)-(x<previousX),dy=y+(y>previousY)-(y<previousY);
    AddAround(x,y,dx,dy,previousX>=0&&(x!=previousX||y!=previousY),now);previousX=x;previousY=y;
    unsigned used=0;
    for(int i=0;i<numWatched&&i<MAX_WATCHED&&used<4;++i){
        if(!watchedChars[i].hasMoveOrder)continue;
        int cx=watchedChars[i].currentZoneX,cy=watchedChars[i].currentZoneY;
        if(watchedChars[i].destZoneX<0||watchedChars[i].destZoneY<0)continue;
        if(abs(cx-x)>2||abs(cy-y)>2)continue;
        AddAround(cx,cy,watchedChars[i].destZoneX,watchedChars[i].destZoneY,true,now);++used;
    }
}
void DropHead(double now){
    if(!count)return;
    seen[queue[0].x*64+queue[0].y]=now+30;
    for(unsigned i=1;i<count;++i)queue[i-1]=queue[i];--count;++zones;
}
void OneBiome(void*manager,double now){
    if(!count||!gridCalibrated)return;
    Cell&c=queue[0];
    double x=zoneOriginX+c.x*zoneStepX,z=zoneOriginZ+c.y*zoneStepZ;
    resource_biome::Bounds bounds(x-zoneStepX*.5,z-zoneStepZ*.5,x+zoneStepX*.5,z+zoneStepZ*.5);
    resource_biome::BiomePointers biomes;resource_biome::Status status=resource_biome::CandidateBiomes(manager,bounds,biomes);
    if(status!=resource_biome::Ok){++metadataRefused;DropHead(now);return;}
    if(c.biome>=biomes.count){DropHead(now);return;}
    const void*biome=biomes.data[c.biome++]; // borrowed only within this bounded main tick
    for(int type=0;type<2;++type){
        resource_gpu::Job job;if(!ResourceBuildTerrainJob(biome,type,job)){++metadataRefused;continue;}
        ++manifests;
        for(unsigned i=0;i<6;++i){resource_cache::Prefetch(job.paths[i]);++prefetches;}
        if(zone::g_zoneCfg.cfg_resourceGpuWarm){
            // Do not duplicate GPU storage for arrays the engine already registered.
            bool exists=false;Ogre::TextureManager*textures=Ogre::TextureManager::getSingletonPtr();
            if(textures){Ogre::TexturePtr texture=textures->getByName(job.key.substr(0,job.key.find('|')),"Landscape");exists=!texture.isNull();}
            if(!exists)resource_gpu::Queue(job);
        }
    }
    if(c.biome>=biomes.count)DropHead(now);
}
}
unsigned __int64 ResourceWorldGeneration(){return world;}
bool ResourceBuildTerrainJob(const void*biome,int type,resource_gpu::Job&job){
    resource_biome::OwnedFiles files;
    if(resource_biome::CopyTerrainFiles(biome,type,files)!=resource_biome::Ok)return false;
    job=resource_gpu::Job();job.type=unsigned(type);job.worldGeneration=world;
    // Key includes ordered native layer paths, not only a re-used biome SID.
    if(files.sid.find('|')!=std::string::npos)return false;
    for(unsigned i=0;i<6;++i){job.paths[i]=Absolute(files.paths[i]);if(job.paths[i].empty())return false;}
    job.key=resource_forward::TerrainKey(files.sid,type,job.paths);
    return true;
}
void ResourceForwardReset(){InterlockedIncrement(&resetPending);}
void ResourceForwardInit(){
    resource_biome::Status s=resource_biome::Initialize(GetCurrentThreadId());ready=s==resource_biome::Ok;
    if(!ready){std::ostringstream out;out<<"Resource prefetch: metadata contract refused ("<<resource_biome::StatusName(s)<<"); normal loading retained";LogError(out.str());}
    gpuStarted=resource_gpu::Start();if(!gpuStarted)LogError("Resource GPU warmup: worker startup failed");
}
void ResourceForwardTick(void*manager,const void*cameraPosition,bool loading){
    double now=ElapsedSec(),frame=lastFrame?now-lastFrame:0;lastFrame=now;
    const zone::ZoneConfig&cfg=zone::g_zoneCfg;
    if(frame>=.020||loading||isTransitionActive)blockedUntil=now+1;
    resource_gpu::SetWorkAllowed(!loading&&!isTransitionActive&&frame<.020&&now>=blockedUntil);
    resource_cache::SetBackgroundAllowed(!cfg.cfg_resourcePrefetch||(!loading&&!isTransitionActive&&frame<.020&&now>=blockedUntil));
    if(InterlockedExchange(&resetPending,0)){
        ++world;count=0;previousX=previousY=-1;nextConfig=0;memset(seen,0,sizeof(seen));resource_gpu::Configure(false,0,world);
    }
    bool on=ready&&cfg.cfg_resourceRamCache&&cfg.cfg_resourcePrefetch;
    if(now>=nextConfig){
        nextConfig=now+1;
        bool gpuAdmitted=ResourceGpuBridgeTick(gpuStarted&&on&&cfg.cfg_resourceGpuWarm);
        resource_gpu::Configure(gpuAdmitted,unsigned __int64(cfg.cfg_resourceGpuGiB)*1024*1024*1024,world);
        std::string error;if(resource_gpu::TakeError(&error))LogError("Resource GPU warmup: "+error);
    }
    if(!on){count=0;return;}
    if(!loading&&manager&&resource_forward::CanWarm(now,lastTick,frame,blockedUntil)){
        lastTick=now;
        try{
            if(!gridCalibrated)CalibrateZoneGrid(manager);
            if(now>=nextCandidates){
                nextCandidates=now+1;float x,z;int gx,gy;
                if(CameraXY(cameraPosition,&x,&z)&&WorldToZoneGrid(x,z,&gx,&gy))Candidates(now,gx,gy);
            }
            OneBiome(manager,now);
        }catch(...){++metadataRefused;if(!metadataReported){LogError("Resource prefetch: metadata read refused; normal loading retained");metadataReported=true;}}
    }
    if(DetailedLogEnabled()&&now>=nextReport){
        nextReport=now+10;resource_gpu::Stats g=resource_gpu::Snapshot();std::ostringstream out;
        out<<"Resource forward: zones="<<zones<<" manifests="<<manifests<<" requests="<<prefetches<<" metadataRefused="<<metadataRefused<<" queue="<<count<<" queueDropped="<<queueDropped;
        out<<" gpuBytes="<<g.allocatedBytes<<" gpuReserved="<<g.reservedBytes<<" gpuFiles="<<g.entries<<" gpuQueue="<<g.pending<<" gpuCreated="<<g.created<<" gpuRejected="<<g.rejected<<" gpuRetries="<<g.retries<<" gpuDevice="<<g.deviceReady<<" gpuPressure="<<g.pressure;
        out<<ResourceGpuBridgeStats();LogDetail(out.str());
    }
}
