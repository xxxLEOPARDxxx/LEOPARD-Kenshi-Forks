#include "resources/resource_cache_store.h"
#include <windows.h>
#include <winioctl.h>
#include <cassert>
#include <fstream>
#include <iostream>
static volatile LONG pressure=0;
static BOOL MemoryProbe(LPMEMORYSTATUSEX m){if(!InterlockedCompareExchange(&pressure,0,0))return GlobalMemoryStatusEx(m);m->ullTotalPhys=16ULL<<30;m->ullAvailPhys=512ULL<<20;m->ullAvailPageFile=8ULL<<30;return TRUE;}
#define GlobalMemoryStatusEx MemoryProbe
#include "../src/resources/resource_cache_store.cpp"
#undef GlobalMemoryStatusEx
#undef assert
#define assert(x) do { if(!(x)){std::cerr<<"assert line "<<__LINE__<<": "<<#x<<"\n";resource_cache::Stop();exit(1);} } while(0)
using namespace resource_cache;
static std::string base;
static void write(const std::string&p,size_t bytes){std::ofstream f(p.c_str(),std::ios::binary);std::string block(1024,'x');while(bytes){size_t n=(std::min)(bytes,block.size());f.write(block.c_str(),n);bytes-=n;}}
static Stats waitDone(){Stats s;for(int i=0;i<600;++i){s=Snapshot();if(s.startupDone)return s;Sleep(10);}assert(!"startup did not finish");return s;}
static FilePtr waitFile(const std::string&p){for(int i=0;i<400;++i){FilePtr f=Find(p);if(f)return f;Sleep(10);}return FilePtr();}
static void off(){ConfigureStartup(false);Configure(false,1024*1024,128);for(int i=0;i<200&&Snapshot().allocatedBytes;++i)Sleep(10);assert(!Snapshot().allocatedBytes);}
static void junction(const std::string&link,const std::string&target){
 assert(CreateDirectoryA(link.c_str(),0));HANDLE h=CreateFileA(link.c_str(),GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,0);assert(h!=INVALID_HANDLE_VALUE);
 struct Mount {DWORD tag;WORD length,reserved,subOffset,subLength,printOffset,printLength;WCHAR path[1024];} r={0};
 r.tag=IO_REPARSE_TAG_MOUNT_POINT;std::string nt="\\??\\"+target;int n=MultiByteToWideChar(CP_ACP,0,nt.c_str(),-1,r.path,1024);assert(n>0);r.subLength=WORD((n-1)*sizeof(WCHAR));r.printOffset=WORD(n*sizeof(WCHAR));r.printLength=0;r.path[n]=0;r.length=WORD(8+(n+1)*sizeof(WCHAR));DWORD got;
 BOOL ok=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&r,r.length+8,0,0,&got,0);if(!ok)std::cerr<<"junction Win32 error "<<GetLastError()<<"\n";assert(ok);CloseHandle(h);
}
static void seedIndex(){
 Guard g;for(unsigned i=0;i<20000;++i){char suffix[40];sprintf_s(suffix,"\\synthetic-%u.dds",i);std::string p=base+suffix;std::shared_ptr<CachedFile> f(new CachedFile);f->data.resize(2048);f->charged=2048;InterlockedExchangeAdd64(&allocated,2048);lru.push_back(p);Entry e={f,--lru.end()};files.insert(std::make_pair(p,e));}
}
static DWORD heartbeatDrain(unsigned __int64 cap){DWORD worst=0;for(int i=0;i<1000&&Snapshot().allocatedBytes;++i){DWORD t=GetTickCount();Configure(true,cap,128);worst=(std::max)(worst,GetTickCount()-t);Sleep(1);}assert(!Snapshot().allocatedBytes&&worst<500);return worst;}
int main(){
 char full[MAX_PATH];GetFullPathNameA("build\\resource-startup\\",MAX_PATH,full,0);char pid[32];sprintf_s(pid,"fixture-%lu",GetCurrentProcessId());base=std::string(full)+pid;assert(CreateDirectoryA(base.c_str(),0));
 std::string sub=base+"\\child";assert(CreateDirectoryA(sub.c_str(),0));std::string outside=base+"-outside";assert(CreateDirectoryA(outside.c_str(),0));write(outside+"\\forbidden.dds",32);
 junction(base+"\\junction",outside);{std::string link=base+"\\linked.dds";write(link,0);HANDLE h=CreateFileA(link.c_str(),GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,0);assert(h!=INVALID_HANDLE_VALUE);struct GuidReparse{DWORD tag;WORD length,reserved;GUID guid;} r={0x42,0,0,{0x8022a7a1,0x33c1,0x4bca,{0x81,0x55,0x42,0x4a,0x82,0x11,0x25,0x27}}};DWORD got;assert(DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,&r,sizeof(r),0,0,&got,0));CloseHandle(h);}
 const char* names[]={"a.dds","b.PNG","c.tga","d.jpg","e.jpeg","f.bmp","ignore.mesh","ignore.skeleton","ignore.txt"};
 for(size_t i=0;i<9;++i)write(base+"\\"+names[i],16);write(sub+"\\nested.dds",16);
 assert(Start());std::string copied=base;RegisterTextureRoot(copied,false);copied="mutated";RegisterTextureRoot(base+"\\.\\",false);std::string alias=base;for(size_t i=0;i<alias.size();++i)if(alias[i]>='a'&&alias[i]<='z')alias[i]-='a'-'A';RegisterTextureRoot(alias,false);
 RegisterTextureRoot("\\\\server\\share",true);RegisterTextureRoot("relative",true);RegisterTextureRoot("C:relative",true);RegisterTextureRoot("1:\\definitely-not-local",true);RegisterTextureRoot(std::string(300,'x'),true);
 assert(Snapshot().startupRoots==1);Configure(true,1024*1024,128);Sleep(100);assert(!Snapshot().entries); // startup defaults off
 ConfigureStartup(true);Stats s=waitDone();assert(s.startupFiles==6&&s.startupLoaded==6&&s.startupProcessed==6&&s.startupSkipped==0&&s.startupReadBytes==96&&s.startupTotalBytes==96);
 assert(Find(base+"\\b.PNG"));assert(!Find(sub+"\\nested.dds"));assert(!Find(base+"\\ignore.mesh"));
 ConfigureStartup(true);Sleep(50);assert(Snapshot().startupLoaded==6); // unchanged heartbeat does not restart
 off();RegisterTextureRoot(base,true);RegisterTextureRoot(base+"\\junction",true);RegisterTextureRoot(base+"\\missing",true);Configure(true,1024*1024,128);ConfigureStartup(true);s=waitDone();assert(s.startupRoots==3&&s.startupFiles==7&&s.startupLoaded==7);assert(Find(sub+"\\nested.dds"));assert(!Find(outside+"\\forbidden.dds"));off();
 // No startup eviction of observed entries; terminal cap does not retry/spin.
 std::string observed=base+"\\ignore.mesh";Configure(true,32,128);Observe(observed);FilePtr old=waitFile(observed);assert(old);old.reset();ConfigureStartup(true);s=waitDone();assert(s.startupCapped&&s.startupDone&&s.startupLoaded==1&&s.startupProcessed==2);assert(Find(observed));assert(Snapshot().entries==2);Sleep(300);assert(Snapshot().startupReadBytes==s.startupReadBytes);off();
 // Sparse >256MiB and zero-byte candidates are processed/skipped, never allocated.
 std::string huge=base+"\\huge.dds",zero=base+"\\zero.dds";write(zero,0);HANDLE h=CreateFileA(huge.c_str(),GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);assert(h!=INVALID_HANDLE_VALUE);DWORD got;assert(DeviceIoControl(h,FSCTL_SET_SPARSE,0,0,0,0,&got,0));LARGE_INTEGER length;length.QuadPart=MaxFile+1;assert(SetFilePointerEx(h,length,0,FILE_BEGIN)&&SetEndOfFile(h));CloseHandle(h);
 Configure(true,1024*1024,128);ConfigureStartup(true);s=waitDone();assert(s.startupFiles==9&&s.startupProcessed==9&&s.startupLoaded==7&&s.startupSkipped==2&&!s.startupCapped&&s.startupReadBytes==112&&s.startupTotalBytes==MaxFile+113);off();DeleteFileA(huge.c_str());DeleteFileA(zero.c_str());
 // Memory pressure ends bounded work; re-enable can retry after pressure recovers.
 InterlockedExchange(&pressure,1);Configure(true,1024*1024,128);ConfigureStartup(true);s=waitDone();assert(s.startupDone&&s.startupCapped&&s.startupProcessed==0&&!s.startupLoaded&&!s.startupReadBytes);off();InterlockedExchange(&pressure,0);
 // Observed requests preempt startup even during paced large-file reads.
 std::string large=base+"\\large.dds";write(large,4*1024*1024);Configure(true,8*1024*1024,1);ConfigureStartup(true);
 for(int i=0;i<500&&Snapshot().startupReadBytes<256*1024;++i)Sleep(10);assert(Snapshot().startupReadBytes>=256*1024);Observe(observed);DWORD start=GetTickCount();old=waitFile(observed);assert(old&&GetTickCount()-start<1000);old.reset();
 ConfigureStartup(false);unsigned __int64 paused=Snapshot().startupReadBytes;Sleep(100);assert(Snapshot().startupReadBytes==paused&&!Snapshot().startupDone&&!Snapshot().startupScanning);off();
 Configure(true,8*1024*1024,128);ConfigureStartup(true);s=waitDone();assert(s.startupFiles==8&&s.startupLoaded==8);off();
 // Pressure and cap-drop retirement must not hold the callback lock while freeing buffers.
 Configure(true,64ULL*1024*1024,128);SetBackgroundAllowed(false);seedIndex();assert(Snapshot().entries==20000);InterlockedExchange(&pressure,1);DWORD worstPressure=heartbeatDrain(64ULL*1024*1024);InterlockedExchange(&pressure,0);seedIndex();assert(Snapshot().entries==20000);Configure(true,1024*1024,128);DWORD worstCap=heartbeatDrain(1024*1024);std::cout<<"20000-entry retirement heartbeat max ms: pressure="<<worstPressure<<" cap="<<worstCap<<"\n";off();SetBackgroundAllowed(true);
 // Late registered roots cause a fresh settled pass; already cached assets skip.
 Configure(true,8*1024*1024,128);ConfigureStartup(true);s=waitDone();RegisterTextureRoot(outside,false);s=waitDone();assert(s.startupRoots==4&&s.startupFiles==9&&s.startupLoaded==1&&s.startupSkipped==8);off();
 Configure(true,8*1024*1024,1);ConfigureStartup(true);for(int i=0;i<500&&Snapshot().startupReadBytes<256*1024;++i)Sleep(10);SetBackgroundAllowed(false);Sleep(50);start=GetTickCount();Stop();assert(GetTickCount()-start<1000&&!Find(large));
 // Actual production boundary branches, without manufacturing 262144 disk files.
 assert(MaxRoots==8192&&MaxDirectories==65536&&MaxStartupFiles==262144);
 {StartupWork w;for(unsigned i=0;i<MaxDirectories;++i){char suffix[32];sprintf_s(suffix,"\\d%u",i);AddDirectory(w,base+suffix,true);}AddDirectory(w,base+"\\overflow",true);assert(w.capped&&w.directories.size()==MaxDirectories&&w.visited.size()==MaxDirectories);}
 {StartupWork w;w.epoch=Load(startupGeneration);w.started=true;AddDirectory(w,base,false);w.inventory.resize(MaxStartupFiles);ScanStep(w);assert(w.capped&&w.scanned&&w.inventory.size()==MaxStartupFiles&&w.find==INVALID_HANDLE_VALUE);}
 for(unsigned i=unsigned(roots.size());i<MaxRoots+1;++i){char suffix[32];sprintf_s(suffix,"\\root%u",i);RegisterTextureRoot(base+suffix,false);}assert(roots.size()==MaxRoots&&rootsCapped);
 DeleteFileA(large.c_str());for(size_t i=0;i<9;++i)DeleteFileA((base+"\\"+names[i]).c_str());DeleteFileA((sub+"\\nested.dds").c_str());RemoveDirectoryA(sub.c_str());RemoveDirectoryA((base+"\\junction").c_str());DeleteFileA((base+"\\linked.dds").c_str());RemoveDirectoryA(base.c_str());DeleteFileA((outside+"\\forbidden.dds").c_str());RemoveDirectoryA(outside.c_str());
 std::cout<<"startup actual filesystem, junction, aliases, recursion, missing roots, oversize, pressure, cap/no-eviction, observed priority, off/re-enable, late roots, limits and stop tests passed\n";
}
