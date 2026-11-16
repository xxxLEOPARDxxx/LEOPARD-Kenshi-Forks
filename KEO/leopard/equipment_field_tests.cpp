#ifndef OGRE_STATIC_LIB
#define OGRE_STATIC_LIB
#endif
#include "../src/fixes/world/equipment_field_lookup.h"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace Ogre{static size_t allocationCount=0;void*NedPoolingImpl::allocBytes(size_t n,const char*,int,const char*){++allocationCount;void*p=std::malloc(n);if(!p)throw std::bad_alloc();return p;}void NedPoolingImpl::deallocBytes(void*p){std::free(p);}}
using namespace LeopardEquipmentField;
static unsigned checks=0,originalCalls=0;static bool throws=false;
static void C(bool v,const char*s){++checks;if(!v){printf("FAIL %s\n",s);throw 1;}}
static IntPair*__fastcall NativeLike(IntMap*m,const std::string*k){++originalCalls;if(throws)throw std::runtime_error("native fixture");IntMap::iterator i=m->find(*k);if(i==m->end())i=m->insert(IntPair(*k,0)).first;return &*i;}
#ifdef BASELINE_FORWARD
namespace LeopardEquipmentField{IntPair*__fastcall Slot(IntMap*m,const std::string*k,Original original,bool,bool){return original(m,k);}}
#endif
static size_t Read(const void*p,size_t n){size_t x;std::memcpy(&x,(const char*)p+n,8);return x;}
static size_t Mix(size_t h){h=(~h)+(h<<21);h^=h>>24;h*=265;h^=h>>14;h*=21;h^=h>>28;return h+(h<<31);}
static IntMap*workerMap=NULL;static const std::string*workerKey=NULL;static DWORD WINAPI Worker(LPVOID){IntPair*p=Slot(workerMap,workerKey,NativeLike,true,false);return p&&p->second==17?0:1;}
static volatile size_t sink=0;
__declspec(noinline) static IntPair*__fastcall Stock(IntMap*m,const std::string*k){IntMap::iterator i=m->find(*k);return i==m->end()?NativeLike(m,k):&*i;}
__declspec(noinline) static IntPair*__fastcall Fast(IntMap*m,const std::string*k){return Slot(m,k,Stock,true,true);}
static volatile LONG live=1;static DWORD ownerThread=0;
__declspec(noinline) static IntPair*__fastcall Guarded(IntMap*m,const std::string*k){return Slot(m,k,Stock,live!=0,GetCurrentThreadId()==ownerThread);}
static double Bench(Original f,IntMap&m,const std::string&k){LARGE_INTEGER a,b,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&a);size_t n=0;for(unsigned i=0;i<2000000;++i)n+=size_t(f(&m,&k)->second);QueryPerformanceCounter(&b);sink=n;return double(b.QuadPart-a.QuadPart)*1e9/double(frequency.QuadPart)/2000000;}
int main(int argc,char**){try{const std::string key="slot";IntMap m;m[key]=17;IntPair*expected=&*m.find(key);size_t allocation=Ogre::allocationCount;originalCalls=0;
 C(Slot(&m,&key,NativeLike,true,true)==expected&&originalCalls==0,"constant slot hit avoids native character-hash query");
 C(Ogre::allocationCount==allocation,"existing-key fast hit allocates no nodes or buckets");
 C(sizeof(std::string)==40&&sizeof(IntMap)==64&&size_t((char*)&expected->second-(char*)expected)==40,"native string/map/pair ABI shape");
 C(Read(&m,0x18)==m.bucket_count()&&Read(&m,0x20)==m.size(),"native table bucket and size offsets match SDK type");
 C(boost::hash<std::string>()(key)==0x28253c9a37862ULL&&Read((char*)expected-16,8)==Mix(0x28253c9a37862ULL),"native slot character hash and cached mixed hash match SDK");
 std::string heapKey=key;heapKey.reserve(128);C(Slot(&m,&heapKey,NativeLike,true,true)==expected,"heap-backed exact key has the same pair result");
 workerMap=&m;workerKey=&key;unsigned oldWorker=originalCalls;HANDLE worker=CreateThread(NULL,0,Worker,NULL,0,NULL);C(worker!=NULL,"offline worker fixture begins");C(WaitForSingleObject(worker,INFINITE)==WAIT_OBJECT_0,"offline worker fixture joined");DWORD code=1;GetExitCodeThread(worker,&code);CloseHandle(worker);C(!code&&originalCalls==oldWorker+1,"actual non-owner thread follows original once");
 for(unsigned i=0;i<200;++i){m[key]=int(i)-70;C(Slot(&m,&key,NativeLike,true,true)->second==int(i)-70,"mutable mapped values observed without cache");}
 for(unsigned i=0;i<4;++i){bool enabled=(i&1)!=0,onThread=(i&2)!=0;unsigned old=originalCalls;C(Slot(&m,&key,NativeLike,enabled,onThread)==&*m.find(key)&&originalCalls-old==unsigned(!enabled||!onThread),"owner/enabled gates preserve exact original once");}
 std::string other="attach slot";unsigned old=originalCalls;C(Slot(&m,&other,NativeLike,true,true)->second==0&&originalCalls==old+1,"different key uses original insertion and defaults");
 m.erase(key);old=originalCalls;size_t before=m.size();IntPair*inserted=Slot(&m,&key,NativeLike,true,true);C(originalCalls==old+1&&m.size()==before+1&&inserted==&*m.find(key)&&inserted->second==0,"missing slot forwards original once and preserves inserted pair address");
 m.clear();m.rehash(4);unsigned collisionCount=0;for(unsigned i=0;collisionCount<20&&i<10000;++i){char text[30];sprintf_s(text,"field-%u",i);std::string collision=text;if((Mix(boost::hash<std::string>()(collision))&3)==(Mix(0x28253c9a37862ULL)&3)){m[collision]=int(i);++collisionCount;}}m[key]=31;
 C(collisionCount==20&&Slot(&m,&key,NativeLike,true,true)==&*m.find(key),"bucket collisions still compare actual strings");
 for(unsigned count=0;count<1000;++count){char text[30];sprintf_s(text,"gear-%u",count);m[text]=int(count);}C(Slot(&m,&key,NativeLike,true,true)->second==31,"rehash and other fields do not invalidate lookup");
 m.erase(key);throws=true;bool propagated=false;old=originalCalls;try{Slot(&m,&key,NativeLike,true,true);}catch(const std::runtime_error&){propagated=true;}throws=false;C(propagated&&originalCalls==old+1&&!m.count(key),"native exception is not swallowed or retried");
 m[key]=19;ownerThread=GetCurrentThreadId();if(argc>1){for(unsigned trial=0;trial<5;++trial){double stock=Bench(Stock,m,key),fast=Bench(Fast,m,key),guarded=Bench(Guarded,m,key);printf("BENCH stock_ns=%.3f fast_ns=%.3f guarded_ns=%.3f ratio=%.3f guarded_ratio=%.3f\n",stock,fast,guarded,fast/stock,guarded/stock);}}
 printf("PASS %u equipment constant-field controls\n",checks);return 0;}catch(...){return 1;}}
