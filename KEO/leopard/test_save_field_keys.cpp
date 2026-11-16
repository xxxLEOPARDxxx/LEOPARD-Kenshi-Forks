#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "fixes/near_page.h"
#ifndef KEO_FIELD_KEY_BASELINE
#include "fixes/world/save_field_key_leaf.h"
#endif
typedef void*(__fastcall*Assign)(void*,const char*,size_t);
typedef void*(__fastcall*Driver)(void*,const char*,size_t,void*);
typedef void*(__cdecl*NewFn)(size_t);
static Assign nativeAssign=NULL;static NewFn nativeNew=NULL;static unsigned originals=0,allocations=0;static bool failNew=false,failOriginal=false,raiseOriginal=false;static volatile bool enabled=true;
static const DWORD originalSeh=0xe04b4651;
static void*__fastcall CountAssign(void*p,const char*s,size_t n){++originals;if(failOriginal)throw std::runtime_error("native assign");if(raiseOriginal)RaiseException(originalSeh,0,0,NULL);return nativeAssign(p,s,n);}
static void*__cdecl CountNew(size_t n){++allocations;if(failNew)throw std::bad_alloc();return nativeNew(n);}
static void*__fastcall NoRead(void*p,const char*,size_t){return p;}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
static void Pointer(void**slot,void*value){DWORD old=0;VirtualProtect(slot,8,PAGE_READWRITE,&old);InterlockedExchangePointer(slot,value);DWORD ignored=0;VirtualProtect(slot,8,old,&ignored);}
static DWORD CaptureSEH(Driver call,void*self,const char*data,size_t count,void*permissions){
 __try{call(self,data,count,permissions);}__except(GetExceptionCode()==originalSeh?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return GetExceptionCode();}return 0;
}
int main(){HMODULE dll=LoadLibraryExA("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll",NULL,LOAD_WITH_ALTERED_SEARCH_PATH);C(dll!=NULL);unsigned char*module=(unsigned char*)dll;nativeAssign=(Assign)(module+0xdad0);void**newSlot=(void**)(module+0x5b7688);nativeNew=(NewFn)*newSlot;C((void*)nativeNew==(void*)GetProcAddress(GetModuleHandleA("MSVCR100.dll"),"??2@YAPEAX_K@Z"));Pointer(newSlot,(void*)&CountNew);
 unsigned char*code=AllocateNearPages((unsigned __int64)(uintptr_t)nativeAssign,8192);C(code!=NULL);Driver drivers[6],proven[6]={0};RUNTIME_FUNCTION functions[14]={0};const unsigned char body[]={0x56,0x48,0x83,0xec,0x20,0x4c,0x89,0xce,0xe8,0,0,0,0,0x48,0x83,0xc4,0x20,0x5e,0xc3};const unsigned char unwind[]={1,5,2,0,5,0x32,1,0x60};memcpy(code+0x200,unwind,8);
 code[0x300]=0xff;code[0x301]=0x25;memset(code+0x302,0,4);uintptr_t target=(uintptr_t)&CountAssign;memcpy(code+0x306,&target,8);
 for(unsigned i=0;i<6;++i){unsigned char*fn=code+i*32;memcpy(fn,body,sizeof(body));unsigned char*destination=code+0x300;
 #ifndef KEO_FIELD_KEY_BASELINE
 C(save_field_key_leaf::Build(code+0x400+i*256,256,&enabled,(void*)&CountAssign)>0);destination=code+0x400+i*256;
 #endif
 const int relative=(int)(destination-(fn+13));memcpy(fn+9,&relative,4);drivers[i]=(Driver)fn;functions[i].BeginAddress=i*32;functions[i].EndAddress=i*32+sizeof(body);functions[i].UnwindData=0x200;}
 Driver stock=NULL,fast=NULL;
 #ifndef KEO_FIELD_KEY_BASELINE
 C(save_field_key_leaf::Build(code+0xc00,256,&enabled,(void*)nativeAssign,true)>0);
 for(unsigned i=6;i<8;++i){unsigned offset=i==6?0xb00:0xb40;unsigned char*fn=code+offset;memcpy(fn,body,sizeof(body));unsigned char*target=i==6?(unsigned char*)nativeAssign:code+0xc00;const __int64 d=(__int64)(uintptr_t)target-(__int64)(uintptr_t)(fn+13);C(d>=-2147483647LL-1&&d<=2147483647LL);const int rel=(int)d;memcpy(fn+9,&rel,4);functions[i].BeginAddress=offset;functions[i].EndAddress=offset+sizeof(body);functions[i].UnwindData=0x200;}stock=(Driver)(code+0xb00);fast=(Driver)(code+0xb40);
 for(unsigned i=0;i<6;++i){const unsigned offset=0xd00+i*32;unsigned char*fn=code+offset;memcpy(fn,body,sizeof(body));unsigned char*leaf=code+0x1000+i*256;
  C(save_field_key_leaf::Build(leaf,256,&enabled,(void*)&CountAssign,true)>0);const int relative=(int)(leaf-(fn+13));memcpy(fn+9,&relative,4);proven[i]=(Driver)fn;
  functions[8+i].BeginAddress=offset;functions[8+i].EndAddress=offset+sizeof(body);functions[8+i].UnwindData=0x200;
 }
 #endif
 DWORD old=0;C(VirtualProtect(code,8192,PAGE_EXECUTE_READ,&old)!=FALSE);FlushInstructionCache(GetCurrentProcess(),code,8192);C(RtlAddFunctionTable(functions,stock?14:6,(DWORD64)code)!=FALSE);
 __declspec(align(16)) unsigned char permissions[64]={0};char source[96];memset(source,'k',95);source[95]=0;
 {std::string first;originals=allocations=0;C(drivers[0](&first,source,31,permissions)==&first);C(first.empty()&&originals==0&&allocations==0);}
 const unsigned counts[]={0,1,15,16,31,63,64};
 for(unsigned driver=0;driver<6;++driver)for(unsigned c=0;c<7;++c){unsigned n=counts[c];memset(source,'k',95);source[n]=0;originals=allocations=0;{std::string first;C(drivers[driver](&first,source,n,permissions)==&first);C(n<=63?first.empty():first.size()==n);}std::string second;C(nativeAssign(&second,source,n)==&second&&second.size()==n&&memcmp(second.data(),source,n)==0);C(originals==(n>63?1u:0u));C(allocations==(n>15?(n>63?2u:1u):0u));}
 *(uint64_t*)(permissions+0x20)=1;for(unsigned d=0;d<6;++d){std::string first;originals=allocations=0;C(drivers[d](&first,source,31,permissions)==&first&&first.size()==31&&originals==1&&allocations==1);}*(uint64_t*)(permissions+0x20)=0;
 enabled=false;{std::string first;originals=0;C(drivers[0](&first,source,31,permissions)==&first&&first.size()==31&&originals==1);}enabled=true;
 {std::string first="used";originals=0;drivers[0](&first,source,31,permissions);C(originals==1&&first.size()==31);}
 {std::string first;first.reserve(80);first.clear();originals=0;drivers[0](&first,source,31,permissions);C(originals==1&&first.size()==31);}
 {std::string first;Assign saved=nativeAssign;nativeAssign=NoRead;originals=0;C(drivers[0](&first,(char*)&first+35,16,permissions)==&first&&originals==1&&first.empty());originals=0;C(drivers[0](&first,(const char*)~(uintptr_t)3,16,permissions)==&first&&originals==1);nativeAssign=saved;}
 {std::string first;*(uint64_t*)(permissions+0x20)=1;failOriginal=true;bool caught=false;try{drivers[0](&first,source,31,permissions);}catch(const std::runtime_error&){caught=true;}failOriginal=false;C(caught&&first.empty());*(uint64_t*)(permissions+0x20)=0;}
 {std::string first;originals=allocations=0;C(drivers[0](&first,source,31,permissions)==&first);std::string second;failNew=true;bool caught=false;try{nativeAssign(&second,source,31);}catch(const std::bad_alloc&){caught=true;}failNew=false;C(caught&&originals==0&&allocations==2&&first.empty()&&second.empty());}
 *(uint64_t*)(permissions+0x20)=1;
 for(unsigned d=0;d<6;++d){std::string first;originals=allocations=0;raiseOriginal=true;DWORD caught=CaptureSEH(drivers[d],&first,source,31,permissions);raiseOriginal=false;C(caught==originalSeh&&originals==1&&allocations==0&&first.empty());}
 *(uint64_t*)(permissions+0x20)=0;
 #ifndef KEO_FIELD_KEY_BASELINE
 // Production provenFrame mode relies on the complete native helpers for
 // fresh/disjoint strings. Exercise those valid frames directly, independently
 // of the stricter generic-mode stale/overlap guards and timing-only driver.
 for(unsigned d=0;d<6;++d)for(unsigned c=0;c<7;++c){const unsigned n=counts[c];memset(source,'k',95);source[n]=0;originals=allocations=0;
  {std::string first;C(first.empty()&&first.capacity()==15&&first.data()[0]==0);C(proven[d](&first,source,n,permissions)==&first);C(n<=63?first.empty():first.size()==n);}
  std::string second;C(nativeAssign(&second,source,n)==&second&&second.size()==n&&memcmp(second.data(),source,n)==0);
  C(originals==(n>63?1u:0u)&&allocations==(n>15?(n>63?2u:1u):0u));
 }
 for(unsigned mode=0;mode<2;++mode){enabled=mode!=0;*(uint64_t*)(permissions+0x20)=mode==1?1:0;
  for(unsigned d=0;d<6;++d)for(unsigned c=0;c<7;++c){const unsigned n=counts[c];memset(source,'k',95);source[n]=0;std::string first;originals=allocations=0;
   C(proven[d](&first,source,n,permissions)==&first&&first.size()==n&&memcmp(first.data(),source,n)==0);C(originals==1&&allocations==(n>15?1u:0u));
  }
 }enabled=true;*(uint64_t*)(permissions+0x20)=1;
 for(unsigned d=0;d<6;++d){
  {std::string first;originals=0;failOriginal=true;bool caught=false;try{proven[d](&first,source,31,permissions);}catch(const std::runtime_error&x){caught=std::string(x.what())=="native assign";}failOriginal=false;C(caught&&originals==1&&first.empty());}
  {std::string first;originals=allocations=0;raiseOriginal=true;const DWORD caught=CaptureSEH(proven[d],&first,source,31,permissions);raiseOriginal=false;C(caught==originalSeh&&originals==1&&allocations==0&&first.empty());}
 }
 *(uint64_t*)(permissions+0x20)=0;
 for(unsigned d=0;d<6;++d){std::string first;originals=allocations=0;C(proven[d](&first,source,31,permissions)==&first&&first.empty());std::string second;failNew=true;bool caught=false;
  try{nativeAssign(&second,source,31);}catch(const std::bad_alloc&){caught=true;}failNew=false;C(caught&&originals==0&&allocations==2&&first.empty()&&second.empty());
 }
 Pointer(newSlot,(void*)nativeNew);LARGE_INTEGER frequency,a,b;QueryPerformanceFrequency(&frequency);volatile unsigned sink=0;const unsigned iterations=200000;
 for(unsigned mode=0;mode<4;++mode){*(uint64_t*)(permissions+0x20)=mode==3?1:0;double times[2]={0};for(unsigned path=0;path<2;++path){Driver call=path?fast:stock;QueryPerformanceCounter(&a);for(unsigned i=0;i<iterations;++i){unsigned n=mode==0?7:mode==1?31:(i%10?7:31);{std::string first;call(&first,source,n,permissions);if(mode==3)sink+=(unsigned)first.size();}std::string second;nativeAssign(&second,source,n);sink+=(unsigned)second.size();}QueryPerformanceCounter(&b);times[path]=(b.QuadPart-a.QuadPart)*1e9/frequency.QuadPart/iterations;}printf("BENCH mode=%u stock_field_ns=%.2f leaf_field_ns=%.2f ratio=%.3f\n",mode,times[0],times[1],times[1]/times[0]);}
 C(sink!=0);
 #endif
 Pointer(newSlot,(void*)nativeNew);RtlDeleteFunctionTable(functions);VirtualFree(code,0,MEM_RELEASE);FreeLibrary(dll);printf("PASS %u native string/RSI save-key leaf checks\n",checks);return 0;}
