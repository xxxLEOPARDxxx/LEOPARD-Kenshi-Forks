#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <cstdio>
#include <cstdlib>
static unsigned __int64 slept=0;static unsigned yields=0;
static void SleepSpy(DWORD ms){slept+=ms;if(!ms)++yields;}
static BOOL Room(LPMEMORYSTATUSEX m){m->ullTotalPhys=16ULL<<30;m->ullAvailPhys=8ULL<<30;m->ullAvailPageFile=16ULL<<30;return TRUE;}
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
 SignalStop();C(!StartupEnabled()&&!StartupUnpaced(true,GetTickCount()));Clear();C(DeleteFileA(absolute));
 printf("PASS %u actual store menu lease/pacing checks; normal=%llu ms, fast=0 pacing ms\n",checks,paced);return 0;
}
