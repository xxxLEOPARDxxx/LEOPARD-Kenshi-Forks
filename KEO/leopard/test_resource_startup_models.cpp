// Actual production startup scan/dispatch, deterministic metadata ordering.
// Reads only owned fixture files; never loads an Ogre resource or starts a game.
#include <windows.h>
#include <string>
#include <cstdio>
#include <vector>
#include "resources/resource_cache_store.h"
static DWORD fixtureNow=10000;
static DWORD Clock(){return fixtureNow;}
static void NoWait(DWORD){}
static bool pressure=false;
static BOOL Room(LPMEMORYSTATUSEX m){m->ullTotalPhys=16ULL<<30;m->ullAvailPhys=pressure?(512ULL<<20):(8ULL<<30);m->ullAvailPageFile=16ULL<<30;return TRUE;}
#define GetTickCount Clock
#define Sleep NoWait
#define GlobalMemoryStatusEx Room
#include "../src/resources/resource_cache_store.cpp"
#undef GetTickCount
#undef Sleep
#undef GlobalMemoryStatusEx
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
static bool Write(const std::string&p,unsigned size,char value){HANDLE h=CreateFileA(p.c_str(),GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);if(h==INVALID_HANDLE_VALUE)return false;std::vector<char>b(size,value);DWORD n=0;bool ok=WriteFile(h,&b[0],size,&n,NULL)&&n==size;CloseHandle(h);return ok;}
int main(){using namespace resource_cache;
 char full[MAX_PATH],suffix[64];sprintf_s(suffix,"build\\startup-models-%lu",GetCurrentProcessId());C(GetFullPathNameA(suffix,MAX_PATH,full,NULL)>0);std::string base=full;
 C(CreateDirectoryA(base.c_str(),NULL));std::string textures=base+"\\a-textures",models=base+"\\z-models";C(CreateDirectoryA(textures.c_str(),NULL)&&CreateDirectoryA(models.c_str(),NULL));
 std::string texture=textures+"\\first.dds",mesh=models+"\\body.MESH",skeleton=models+"\\body.Skeleton",ignored=models+"\\ignore.material";
 C(Write(texture,16,'t')&&Write(mesh,16,'m')&&Write(skeleton,16,'s')&&Write(ignored,16,'i'));
 InitializeCriticalSection(&lock);initialized=true;Configure(true,1024,128);ConfigureStartup(true);rootsChanged=fixtureNow-2000;
 StartupWork w;w.Reset(Load(startupGeneration));w.started=true;AddDirectory(w,textures,false);AddDirectory(w,models,false);
 while(!w.scanned)ScanStep(w);
 C(progress.startupFiles==3&&progress.startupTotalBytes==48); // RED before model/skeleton admission.
 // Both model types must be ready before the earlier-discovered texture.
 C(StartupStep(w,Load(generation)));C(progress.startupProcessed==1);C(!Find(texture));
 C(StartupStep(w,Load(generation)));C(progress.startupProcessed==2);C(Find(mesh)&&Find(skeleton)&&!Find(texture));
 C(StartupStep(w,Load(generation)));C(Find(texture)&&!Find(ignored));C(!StartupStep(w,Load(generation))&&progress.startupDone&&progress.startupLoaded==3);
 Clear();Configure(false,1024,128);C(Load64(allocated)==0);
 // A small cap fills with models first; texture saturation never evicts them
 // or retries forever. A retained native reader continues owning its bytes.
 Configure(true,32,128);ConfigureStartup(true);rootsChanged=fixtureNow-2000;
 StartupWork capped;capped.Reset(Load(startupGeneration));capped.started=true;AddDirectory(capped,textures,false);AddDirectory(capped,models,false);while(!capped.scanned)ScanStep(capped);
 C(StartupStep(capped,Load(generation))&&StartupStep(capped,Load(generation)));
 FilePtr reader=Find(mesh);C(reader&&Find(skeleton)&&!Find(texture));
 C(!StartupStep(capped,Load(generation)));C(progress.startupDone&&progress.startupCapped&&progress.startupProcessed==3&&progress.startupLoaded==2);
 C(capped.inventory.empty()&&capped.models.empty()&&capped.seenFiles.empty());
 const LONGLONG readBefore=Load64(readBytes);C(!StartupStep(capped,Load(generation))&&Load64(readBytes)==readBefore);
 Clear();C(Load64(allocated)==16&&reader->data.size()==16&&reader->data[0]=='m');reader.reset();C(Load64(allocated)==0);
 // Aggregate cap counts BOTH buckets. No disk manufacturing of262144files.
 StartupWork limit;limit.Reset(Load(startupGeneration));limit.started=true;AddDirectory(limit,textures,false);limit.models.resize(MaxStartupFiles);ScanStep(limit);
 C(limit.capped&&limit.scanned&&limit.inventory.empty()&&limit.models.size()==MaxStartupFiles&&limit.find==INVALID_HANDLE_VALUE);
 limit.Reset(Load(startupGeneration));C(limit.models.empty());
 // Deferred/epoch-cancelled model work never advances the inventory cursor.
 Configure(false,1024,128);Configure(true,1024,128);ConfigureStartup(true);rootsChanged=fixtureNow-2000;
 StartupWork paused;paused.Reset(Load(startupGeneration));paused.started=true;AddDirectory(paused,models,false);while(!paused.scanned)ScanStep(paused);
 SetBackgroundAllowed(false);C(StartupStep(paused,Load(generation))&&paused.next==0&&Load64(allocated)==0);SetBackgroundAllowed(true);
 const LONG oldEpoch=Load(generation);Configure(false,1024,128);C(!StartupStep(paused,oldEpoch)&&Load64(allocated)==0);
 C(CacheableFileName("x.MESH")&&CacheableFileName("x.Skeleton")&&!CacheableFileName("x.mesh.bak")&&!CacheableFileName("x.material"));
 Clear();Configure(false,1024,128);C(Load64(allocated)==0);
 DeleteFileA(texture.c_str());DeleteFileA(mesh.c_str());DeleteFileA(skeleton.c_str());DeleteFileA(ignored.c_str());RemoveDirectoryA(textures.c_str());RemoveDirectoryA(models.c_str());RemoveDirectoryA(base.c_str());
 printf("Startup model byte inventory actual production %u checks PASS\n",checks);return 0;
}
