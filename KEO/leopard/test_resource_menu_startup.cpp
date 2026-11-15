#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <cstdio>
#include <cstdlib>
#include "resources/resource_cache_store.h"
static unsigned __int64 slept=0;static unsigned yields=0;
static unsigned sleepAction=0;static bool pressure=false;
static void SleepSpy(DWORD ms){
 slept+=ms;if(!ms)++yields;
 if(ms&&sleepAction){unsigned action=sleepAction;sleepAction=0;
  if(action==1)resource_cache::SetBackgroundAllowed(false);
  else if(action==2)resource_cache::Configure(false,8ULL<<20,1);
  else if(action==3)pressure=true;
 }
}
static BOOL Room(LPMEMORYSTATUSEX m){m->ullTotalPhys=16ULL<<30;m->ullAvailPhys=pressure?(512ULL<<20):(8ULL<<30);m->ullAvailPageFile=16ULL<<30;return TRUE;}
#define Sleep SleepSpy
#define GlobalMemoryStatusEx Room
#include "../src/resources/resource_cache_store.cpp"
#undef Sleep
#undef GlobalMemoryStatusEx
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
int main(){using namespace resource_cache;
 InitializeCriticalSection(&lock);initialized=true;Configure(true,8ULL<<20,1);ConfigureStartup(true);
 C(StartupEnabled());SetStartupMenu(false);C(!StartupUnpaced(true,GetTickCount()));
 SetStartupMenu(true);DWORD at=DWORD(Load(startupMenuAt));C(StartupUnpaced(true,at));C(!StartupUnpaced(false,at));
 C(StartupUnpaced(true,at+250)&&!StartupUnpaced(true,at+251));
 SetBackgroundAllowed(false);C(!StartupUnpaced(true,at));SetBackgroundAllowed(true);
 InterlockedExchange(&startupMenuAt,LONG(0xfffffff0));C(StartupUnpaced(true,0x20)&&!StartupUnpaced(true,0x200));
 ConfigureStartup(false);C(!StartupEnabled()&&!StartupUnpaced(true,at));ConfigureStartup(true);
 char relative[100],absolute[MAX_PATH];sprintf_s(relative,"build/menu-warm-%lu.dds",GetCurrentProcessId());
 C(GetFullPathNameA(relative,MAX_PATH,absolute,NULL)>0);C(GetFileAttributesA(absolute)==INVALID_FILE_ATTRIBUTES);
 HANDLE f=CreateFileA(absolute,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);C(f!=INVALID_HANDLE_VALUE);
 std::vector<unsigned char> bytes(4*1024*1024,42);DWORD written=0;C(WriteFile(f,&bytes[0],DWORD(bytes.size()),&written,NULL)&&written==bytes.size());CloseHandle(f);
 LONG epoch=Load(generation),se=Load(startupGeneration);SetStartupMenu(false);slept=0;
 C(ReadOne(absolute,epoch,se)==ReadLoaded);C(slept>=3900);unsigned __int64 paced=slept;
 SetStartupMenu(true);slept=0;yields=0;C(ReadOne(absolute,epoch,se)==ReadLoaded);C(slept==0&&yields>0);
 C(Snapshot().startupFastReadBytes==bytes.size());
 SetStartupMenu(true);slept=0;C(ReadOne(absolute,epoch)==ReadLoaded);C(slept>=3900); // observed requests never accelerated
 SetStartupMenu(false);slept=0;C(ReadOne(absolute,epoch,se)==ReadLoaded);C(slept>=3900);
 C(Snapshot().startupFastReadBytes==bytes.size());
 // Loading/frame pauses suppress speculative work only. Exercise actual
 // production ReadOne across16chunks; the Sleep input is deterministic.
 Clear();SetBackgroundAllowed(false);unsigned __int64 beforeRead=Snapshot().readBytes;
 C(ReadOne(absolute,epoch,se)==ReadDeferred&&ReadOne(absolute,epoch,-1,false)==ReadDeferred);
 C(Snapshot().readBytes==beforeRead&&Snapshot().allocatedBytes==0);
 SetBackgroundAllowed(true);SetStartupMenu(false);sleepAction=1;slept=0;
 C(ReadOne(absolute,epoch)==ReadLoaded);C(!BackgroundAllowed()&&slept>=3900);
 C(Find(absolute)&&Snapshot().allocatedBytes==bytes.size()); // completed cache survives pause
 SetBackgroundAllowed(true);sleepAction=1;slept=0;beforeRead=Snapshot().readBytes;
 C(ReadOne(absolute,epoch,-1,false)==ReadDeferred);
 C(Snapshot().readBytes-beforeRead==Chunk&&Snapshot().allocatedBytes==bytes.size()&&slept<=10);
 SetBackgroundAllowed(true);sleepAction=1;slept=0;beforeRead=Snapshot().readBytes;
 C(ReadOne(absolute,epoch,se)==ReadDeferred);
 C(Snapshot().readBytes-beforeRead==Chunk&&Snapshot().allocatedBytes==bytes.size()&&Find(absolute));
 Clear();SetBackgroundAllowed(false);sleepAction=2;slept=0;beforeRead=Snapshot().readBytes;
 C(ReadOne(absolute,epoch)==ReadCancelledResult);
 C(Snapshot().readBytes-beforeRead==Chunk&&Snapshot().allocatedBytes==0&&!Find(absolute));
 Configure(true,8ULL<<20,1);epoch=Load(generation);se=Load(startupGeneration);sleepAction=3;beforeRead=Snapshot().readBytes;
 C(ReadOne(absolute,epoch)==ReadCapped);
 C(Snapshot().readBytes-beforeRead==Chunk&&Snapshot().allocatedBytes==0&&!Find(absolute));pressure=false;
 Configure(true,8ULL<<20,128);SetStartupMenu(true);slept=0;
 C(ReadOne(absolute,epoch)==ReadLoaded);C(slept>0); // demand never bypasses the configured rate
 Clear();SetBackgroundAllowed(false);
 for(unsigned i=0;i<MaxQueue;++i){char suffix[32];sprintf_s(suffix,"-%u.dds",i);Prefetch(std::string(absolute)+suffix);}
 C(Snapshot().pending==MaxQueue&&!HasDemandRequests());
 {Guard g;C(!RunnableLocked());} // paused speculative queue cannot self-wake the worker
 unsigned __int64 queuedBefore=Snapshot().queued;Observe(std::string(absolute)+"-0.dds");
 C(Snapshot().pending==MaxQueue&&Snapshot().queued==queuedBefore&&HasDemandRequests());
 Observe(std::string(absolute)+"-native.dds");
 unsigned demand=0;{Guard g;for(size_t i=0;i<requests.size();++i)if(requests[i].observed)++demand;C(RunnableLocked());}
 C(Snapshot().pending==MaxQueue&&demand==2); // demand displaces a speculative tail, never grows queue
 for(unsigned i=0;i<MaxQueue;++i){char suffix[32];sprintf_s(suffix,"-%u.dds",i);Observe(std::string(absolute)+suffix);}
 C(Snapshot().pending==MaxQueue);
 Observe(std::string(absolute)+"-all-demand-overflow.dds");C(Snapshot().pending==MaxQueue);
 SignalStop();C(!StartupEnabled()&&!StartupUnpaced(true,GetTickCount()));Clear();C(DeleteFileA(absolute));
 printf("PASS %u actual store menu lease/pacing checks; normal=%llu ms, fast=0 pacing ms\n",checks,paced);return 0;
}
