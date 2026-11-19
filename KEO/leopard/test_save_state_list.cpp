// Native Win64 callsite fixture; does not launch or inspect Kenshi.
#define NOMINMAX
#include <windows.h>
#include "save_gog_test_fixture.h"
#include <intrin.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <stdint.h>
#include <cstddef>
#include <cstdlib>
#include <new>
static bool failAlloc=false,watchAlloc=false;
static unsigned allocations=0;
void*operator new(size_t n){if(watchAlloc)++allocations;if(failAlloc)throw std::bad_alloc();void*p=malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p){free(p);}
static unsigned checks=0,copies=0,assignCalls=0,errors=0;
static bool owner=true;static const DWORD fixtureMain=GetCurrentThreadId();bool IsMainThread(){return owner&&GetCurrentThreadId()==fixtureMain;}
namespace fixes{struct Config{bool saveStringTransferEnabled;};Config g_fixesCfg={true};}
static void LogError(const std::string&){++errors;}
#define KEO_DETAIL(x) ((void)0)
static HMODULE fakeGame=NULL;
static unsigned protectCount=0,flushCount=0;
static unsigned __int64 protectFailures=0,flushFailures=0;
static bool allocationFailure=false,farAllocation=false;
static int exchangeFault=0;static bool exchangeFailure=false;static DWORD fixtureCodeProtection=0;
static __int64 Exchange(volatile __int64*p,__int64 value,__int64 expected){if(exchangeFailure)return expected^1;__int64 old=_InterlockedCompareExchange64(p,value,expected);if(exchangeFault&&old==expected){const int kind=exchangeFault;exchangeFault=0;if(kind==1)RaiseException(0xe9876543,0,0,NULL);else throw std::runtime_error("after CAS");}return old;}
bool ReadModuleImageId(HMODULE m,DWORD*s,DWORD*z){if(!m)return false;const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>0x100000)return false;const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((const char*)m+d->e_lfanew);if(n->Signature!=IMAGE_NT_SIGNATURE)return false;*s=n->FileHeader.TimeDateStamp;*z=n->OptionalHeader.SizeOfImage;return true;}
static BOOL Protect(void*p,size_t n,DWORD flags,DWORD*old){const unsigned i=protectCount++;return i<64&&(protectFailures&(1ULL<<i))?FALSE:VirtualProtect(p,n,flags,old);}
static BOOL Flush(void*p,size_t n){const unsigned i=flushCount++;return i<64&&(flushFailures&(1ULL<<i))?FALSE:FlushInstructionCache(GetCurrentProcess(),p,n);}
#include "fixes/near_page.h"
static unsigned char*Allocate(unsigned __int64 p,size_t n){return allocationFailure?NULL:farAllocation?(unsigned char*)VirtualAlloc((void*)0x600000000000ULL,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE):AllocateNearPages(p,n);}
#ifdef KEO_STATE_LIST_BASELINE
namespace save_state_list {struct List{uintptr_t prefix;unsigned count,capacity;std::string*data;};}
#else
#define KEO_STATE_LIST_TEST
#define KEO_STATE_PROTECT(p,n,f,old) Protect(p,n,f,old)
#define KEO_STATE_FLUSH(p,n) Flush(p,n)
#define KEO_STATE_ALLOCATE(p,n) Allocate(p,n)
#define KEO_STATE_EXCHANGE(p,v,e) Exchange(p,v,e)
#define KEO_STATE_WORLD_POINTER(game) ((char*)game+0x2133560)
#define KEO_STATE_GAME_MODULE() fakeGame
#ifndef KEO_STATE_LIST_SOURCE
#define KEO_STATE_LIST_SOURCE "../src/fixes/world/save_state_list_transfer.cpp"
#endif
#include KEO_STATE_LIST_SOURCE
#endif
using save_state_list::List;
struct Vec{float x,y,z;};struct Quat{float w,x,y,z;};
struct Object{Vec pos;Quat rot;char remaining[0x50-sizeof(Vec)-sizeof(Quat)];List states;Object(){memset(this,0,sizeof(*this));states.prefix=0x2222;}};
static_assert(offsetof(Object,rot)==0xc&&offsetof(Object,states)==0x50&&sizeof(List)==24,"native fixture frame fields");
typedef void*(*AssignFn)(void*,const void*);static AssignFn nativeAssign=NULL;
static bool throwAssign=false,raiseAssign=false,throwAfterCallback=false;static void*alternate=NULL;
static void(*beforeAssign)()=NULL;
extern "C" void* KEO_TestAssignSlot=NULL;
extern "C" void* KEO_TestAssignmentResult=NULL;
extern "C" void* KEO_TestAltSlot=NULL;
extern "C" unsigned char KEO_TestUseAlt=0;
extern "C" void KEO_TestSavedDriver(Object*,const std::string&,const std::string&,int,const Vec&,const Quat&,const List&);
extern "C" void KEO_TestSavedStage(Object*,const std::string&,const std::string&,int,const Vec&,const Quat&,const List&);
extern "C" unsigned char KEO_TestParserReturn,KEO_TestSavedPivot,KEO_TestSavedAfter;
static void*Assign(void*self,const void*other){InterlockedIncrement((volatile LONG*)&assignCalls);if(throwAssign)throw std::runtime_error("native assignment");if(raiseAssign)RaiseException(0xe1234567,0,0,NULL);if(beforeAssign){void(*callback)()=beforeAssign;beforeAssign=NULL;callback();if(throwAfterCallback)throw std::runtime_error("after nested assignment");}nativeAssign(self,other);return alternate?alternate:self;}
extern "C" void KEO_TestCopyStates(List*dst,const List*src){
 const unsigned count=src->count;
 if(!count)return;
 if(dst->capacity<dst->count+count){const unsigned cap=dst->count+count;std::string*data=new std::string[cap];for(unsigned i=0;i<dst->count;++i)data[i]=dst->data[i];delete[]dst->data;dst->data=data;dst->capacity=cap;}
 for(unsigned i=0;i<count;++i){dst->data[dst->count++]=src->data[i];InterlockedIncrement((volatile LONG*)&copies);}
}
struct Owned:List{Owned(unsigned n=0,unsigned spare=0){prefix=0x1111;count=n;capacity=n+spare;data=capacity?new std::string[capacity]:NULL;for(unsigned i=0;i<n;++i)data[i]=std::string(35+i,'a'+i%20);}~Owned(){delete[]data;}};
struct Cleanup{Object&obj;Cleanup(Object&o):obj(o){}~Cleanup(){delete[]obj.states.data;}};
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
static Vec pos={1,2,3};static Quat rot={0.5f,0.25f,0.125f,0.0625f};static const std::string key="object",ref="reference";
static void Run(Object&obj,const List&source){KEO_TestSavedDriver(&obj,key,ref,7,pos,rot,source);}
static Object*nestedObject=NULL;static const List*nestedSource=NULL;static void Nested(){Run(*nestedObject,*nestedSource);}
static volatile LONG workerStop=0,workerReady=0,workerFault=0;
static DWORD WINAPI ParseWorker(void*){Owned source(3);InterlockedIncrement(&workerReady);while(!InterlockedCompareExchange(&workerStop,0,0)){Object obj;Cleanup cleanup(obj);Run(obj,source);if(source.count!=3||obj.states.count!=3||obj.states.data[0]!=source.data[0])InterlockedIncrement(&workerFault);}return 0;}
static DWORD CaptureSEH(Object*obj,const List*source){__try{Run(*obj,*source);}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
#ifndef KEO_STATE_LIST_BASELINE
static void Unarm(unsigned char*site,const unsigned char*before){DWORD old=0,ignored=0;VirtualProtect(site,9,PAGE_EXECUTE_READWRITE,&old);memcpy(site,before,9);FlushInstructionCache(GetCurrentProcess(),site,9);VirtualProtect(site,9,site==&KEO_TestSavedPivot?fixtureCodeProtection:old,&ignored);if(save_state_list::page)VirtualFree(save_state_list::page,0,MEM_RELEASE);save_state_list::page=NULL;save_state_list::admitted=false;}
static void Reset(){owner=true;fixes::g_fixesCfg.saveStringTransferEnabled=true;save_state_list::admitted=true;copies=assignCalls=0;throwAssign=raiseAssign=throwAfterCallback=false;beforeAssign=NULL;alternate=NULL;protectCount=flushCount=0;protectFailures=flushFailures=0;allocationFailure=farAllocation=false;failAlloc=watchAlloc=false;allocations=0;KEO_TestUseAlt=0;exchangeFault=0;exchangeFailure=false;}
static DWORD CaptureArmSEH(unsigned char*site,const unsigned char*expected){__try{save_state_list::Arm(site,expected);}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;}
#endif
int main(){
 HMODULE ogre=LoadLibraryA("OgreMain_x64.dll");C(ogre!=NULL);nativeAssign=(AssignFn)GetProcAddress(ogre,"??4Quaternion@Ogre@@QEAAAEAV01@AEBV01@@Z");C(nativeAssign!=NULL);KEO_TestAssignSlot=(void*)&Assign;
 C((uintptr_t)&KEO_TestSavedAfter-(uintptr_t)&KEO_TestParserReturn==0x8a06);
#ifndef KEO_STATE_LIST_BASELINE
 save_state_list::gameBase=(uintptr_t)&KEO_TestParserReturn-0x6c21c7;
 save_state_list::original=Assign;
 unsigned char before[9];memcpy(before,&KEO_TestSavedPivot,9);
 MEMORY_BASIC_INFORMATION initialCode;C(VirtualQuery(&KEO_TestSavedPivot,&initialCode,sizeof(initialCode))==sizeof(initialCode));fixtureCodeProtection=initialCode.Protect;
 C(save_state_list::Arm(&KEO_TestSavedPivot,before));save_state_list::admitted=true;
 KEO_TestAltSlot=(void*)&save_state_list::Assign;
#endif
 {Object obj;Cleanup cleanup(obj);Owned source(4,6);std::string*array=source.data;const char*buffer=array[0].c_str();Run(obj,source);
  C(assignCalls==1&&memcmp(&obj.rot,&rot,sizeof(rot))==0&&KEO_TestAssignmentResult==&obj.rot);
  printf("final state-list copies: %u\n",copies);C(copies==0);C(obj.states.data==array&&obj.states.data[0].c_str()==buffer&&obj.states.count==4&&obj.states.capacity==10);
  C(source.count==0&&source.capacity==0&&source.data==NULL&&source.prefix==0x1111&&obj.states.prefix==0x2222);
  C(obj.states.data[0]==std::string(35,'a')&&obj.states.data[3]==std::string(38,'d'));
 }
#ifndef KEO_STATE_LIST_BASELINE
 for(unsigned count=0;count<40;++count){Reset();Object obj;Cleanup cleanup(obj);Owned source(count,3);std::string*array=source.data;const unsigned nativeCap=count<=10?10:count<=20?20:40;const bool move=count>=2&&count+3<=nativeCap;Run(obj,source);C(assignCalls==1&&copies==(move?0u:count)&&obj.states.count==count);C(move?(obj.states.data==array&&source.data==NULL):(source.data==array&&source.count==count));for(unsigned i=0;i<count;++i)C(obj.states.data[i]==std::string(35+i,'a'+i%20));}
 for(unsigned mode=0;mode<8;++mode){Reset();Object obj;Cleanup cleanup(obj);Owned source(3);std::string*array=source.data;
  if(mode==0)owner=false;if(mode==1)fixes::g_fixesCfg.saveStringTransferEnabled=false;if(mode==2)save_state_list::admitted=false;
  if(mode==3){obj.states.data=new std::string[1];obj.states.capacity=1;}
  if(mode==4){obj.states.data=new std::string[1];obj.states.capacity=obj.states.count=1;obj.states.data[0]="old";}
  if(mode==5){obj.states.data=new std::string[1];}
  if(mode==7)KEO_TestUseAlt=1;
  if(mode==6)KEO_TestSavedStage(&obj,key,ref,7,pos,rot,source);else Run(obj,source);
  C(assignCalls==1&&source.data==array&&source.count==3&&copies==3&&obj.states.count==(mode==4?4u:3u));C(obj.states.data[mode==4?1:0]==std::string(35,'a'));if(mode==4)C(obj.states.data[0]=="old");
 }
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(4);throwAssign=true;bool caught=false;try{Run(obj,source);}catch(const std::runtime_error&){caught=true;}C(caught&&assignCalls==1&&copies==0&&source.count==4&&obj.states.data==NULL);}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(4);raiseAssign=true;C(CaptureSEH(&obj,&source)==0xe1234567&&assignCalls==1&&copies==0&&source.count==4&&obj.states.data==NULL);}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(3);alternate=(void*)0x12345;Run(obj,source);C(KEO_TestAssignmentResult==alternate&&obj.states.count==3&&copies==0&&memcmp(&obj.rot,&rot,sizeof(rot))==0);}
 for(unsigned same=0;same<2;++same){Reset();Object obj,other;Cleanup clean(obj),cleanOther(other);Owned source(3),inner(2);nestedObject=same?&obj:&other;nestedSource=&inner;beforeAssign=Nested;Run(obj,source);C(assignCalls==2&&inner.count==0&&obj.states.count==(same?5u:3u));C(same?(source.count==3&&copies==3&&obj.states.data[0]==std::string(35,'a')&&obj.states.data[2]==std::string(35,'a')):(source.count==0&&copies==0&&other.states.count==2));}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(3),inner(2);nestedObject=&obj;nestedSource=&inner;beforeAssign=Nested;throwAfterCallback=true;bool caught=false;try{Run(obj,source);}catch(const std::runtime_error&){caught=true;}C(caught&&assignCalls==2&&source.count==3&&inner.count==0&&obj.states.count==2&&copies==0);}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(10);failAlloc=watchAlloc=true;bool threw=false;try{Run(obj,source);}catch(...){threw=true;}failAlloc=watchAlloc=false;C(!threw&&allocations==0&&source.count==0&&obj.states.count==10);}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(2);source.data[0]="";source.data[1]="short";Run(obj,source);C(source.count==0&&obj.states.count==2&&obj.states.data[0].empty()&&obj.states.data[1]=="short");}
 Reset();{Object obj;Cleanup cleanup(obj);Owned source(2,4094);Run(obj,source);C(source.count==2&&obj.states.count==2&&obj.states.capacity==2&&copies==2);}
 Reset();{Object obj;Owned source(2,4095);C(!save_state_list::Transfer(&obj.states,&source)&&source.count==2&&obj.states.count==0);}
 Reset();{Object obj;Owned source(2,4095);source.count=4097;C(!save_state_list::Transfer(&obj.states,&source)&&source.count==4097&&obj.states.count==0);}
 for(unsigned mode=0;mode<5;++mode){Reset();Object obj;Owned source(3);std::string*data=source.data;const unsigned cap=source.capacity;unsigned cookie=0;memcpy(&cookie,(char*)data-8,4);
  if(mode==0)source.count=4;if(mode==1)source.capacity=4097;if(mode==2)source.capacity=2;if(mode==3){unsigned wrong=7;memcpy((char*)data-8,&wrong,4);}if(mode==4)source.data=(std::string*)((char*)data+1);
  C(!save_state_list::Transfer(&obj.states,&source)&&obj.states.count==0&&obj.states.data==NULL);source.data=data;source.capacity=cap;source.count=3;memcpy((char*)data-8,&cookie,4);
 }
 Reset();{Owned source(3);C(!save_state_list::Transfer(&source,&source)&&source.count==3);C(!save_state_list::Transfer(NULL,&source));Object obj;C(!save_state_list::Transfer(&obj.states,NULL));}
 {__declspec(align(8)) unsigned char bytes[256]={0};*(unsigned*)bytes=5;List*dst=(List*)(bytes+80);List src={0x1111,2,5,(std::string*)(bytes+8)};C(!save_state_list::Transfer(dst,&src)&&src.count==2&&dst->count==0);}
 Unarm(&KEO_TestSavedPivot,before);
 // Each failed patch leaves native bytes or a callable retained forwarding
 // branch. Exercise both forms through the real assembly frame afterward.
 for(unsigned mode=0;mode<11;++mode){Reset();save_state_list::admitted=false;save_state_list::original=Assign;
  if(mode==0)allocationFailure=true;if(mode==1)protectFailures=1;if(mode==2)protectFailures=2;if(mode==3)protectFailures=4;if(mode==4)flushFailures=1;if(mode==5)flushFailures=1;if(mode==6){flushFailures=1;protectFailures=4;}if(mode==7){flushFailures=1;protectFailures=8;}if(mode==8)flushFailures=3;if(mode==9)farAllocation=true;
  if(mode==10)exchangeFailure=true;
  C(!save_state_list::Arm(&KEO_TestSavedPivot,before));{Object obj;Cleanup cleanup(obj);Owned source(3);Run(obj,source);C(assignCalls==1&&copies==3&&source.count==3&&obj.states.count==3);}protectFailures=flushFailures=0;Unarm(&KEO_TestSavedPivot,before);C(memcmp(&KEO_TestSavedPivot,before,9)==0);
 }
 for(int kind=1;kind<=2;++kind){Reset();save_state_list::admitted=false;exchangeFault=kind;bool caught=false;if(kind==1)caught=CaptureArmSEH(&KEO_TestSavedPivot,before)==0xe9876543;else{try{save_state_list::Arm(&KEO_TestSavedPivot,before);}catch(const std::runtime_error&){caught=true;}}
  MEMORY_BASIC_INFORMATION mbi={0};VirtualQuery(&KEO_TestSavedPivot,&mbi,sizeof(mbi));C(caught&&save_state_list::patchThreads.count==0&&memcmp(&KEO_TestSavedPivot,before,9)==0&&mbi.Protect==fixtureCodeProtection&&save_state_list::page!=NULL);
  {Object obj;Cleanup cleanup(obj);Owned source(3);Run(obj,source);C(assignCalls==1&&copies==3&&source.count==3&&obj.states.count==3);}Unarm(&KEO_TestSavedPivot,before);
 }
 Reset();{unsigned char wrong[9];memcpy(wrong,before,9);wrong[0]^=1;C(!save_state_list::Arm(&KEO_TestSavedPivot,wrong)&&save_state_list::page==NULL&&protectCount==0);save_state_list::original=NULL;C(!save_state_list::Arm(&KEO_TestSavedPivot,before)&&save_state_list::page==NULL);}
 Reset();save_state_list::original=Assign;save_state_list::admitted=false;
 {HANDLE workers[4]={0};for(unsigned i=0;i<4;++i)workers[i]=CreateThread(NULL,0,ParseWorker,NULL,0,NULL);while(InterlockedCompareExchange(&workerReady,0,0)!=4)Sleep(1);
  const bool armed=save_state_list::Arm(&KEO_TestSavedPivot,before);save_state_list::admitted=armed;Sleep(10);InterlockedExchange(&workerStop,1);WaitForMultipleObjects(4,workers,TRUE,10000);for(unsigned i=0;i<4;++i)CloseHandle(workers[i]);C(armed&&workerFault==0&&workerReady==4);Unarm(&KEO_TestSavedPivot,before);
 }
 // Production image gate against a controlled mapped image, actual Ogre
 // assignment export and exact pinned instruction proofs.
 Reset();fakeGame=(HMODULE)VirtualAlloc(NULL,0x232c000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(fakeGame!=NULL);
 IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)fakeGame;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)((char*)fakeGame+0x80);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.TimeDateStamp=0x65d604d7;nt->OptionalHeader.SizeOfImage=0x232c000;
 for(size_t i=0;i<sizeof(save_state_list::proofs)/sizeof(save_state_list::proofs[0]);++i){const save_state_list::Proof&p=save_state_list::proofs[i];memcpy((char*)fakeGame+p.rva,p.bytes,p.size);}
 *(void**)((char*)fakeGame+0x2247000)=(void*)nativeAssign;C(save_state_list::Admit(fakeGame,ogre));
 for(size_t i=0;i<sizeof(save_state_list::proofs)/sizeof(save_state_list::proofs[0]);++i){const save_state_list::Proof&p=save_state_list::proofs[i];((char*)fakeGame)[p.rva]^=1;C(!save_state_list::Admit(fakeGame,ogre));((char*)fakeGame)[p.rva]^=1;}
 owner=false;C(!save_state_list::Admit(fakeGame,ogre));owner=true;*(void**)((char*)fakeGame+0x2133560)=(void*)1;C(!save_state_list::Admit(fakeGame,ogre));*(void**)((char*)fakeGame+0x2133560)=NULL;
 *(void**)((char*)fakeGame+0x2247000)=(void*)Assign;C(!save_state_list::Admit(fakeGame,ogre));*(void**)((char*)fakeGame+0x2247000)=(void*)nativeAssign;
 save_state_list::tried=save_state_list::admitted=false;save_state_list::Install(NULL,NULL);C(save_state_list::admitted&&save_state_list::page!=NULL);unsigned char nativeBefore[9];memcpy(nativeBefore,save_state_list::proof0+(0x6cabc7-0x6cab10),9);Unarm((unsigned char*)fakeGame+0x6cabc7,nativeBefore);
 VirtualFree(fakeGame,0,MEM_RELEASE);fakeGame=NULL;
 SaveGogTestImage gog;
 if(gog.requested){
  C(gog.bytes);fakeGame=gog.Module();*(void**)(gog.bytes+0x2244f78)=(void*)nativeAssign;
  C(save_state_list::Admit(fakeGame,ogre)&&save_state_list::gogProfile&&save_state_list::assignmentReturn==0x6ca4dd&&save_state_list::parserReturn==0x6c1ad7);
  for(size_t i=0;i<sizeof(save_state_list::gogProofs)/sizeof(save_state_list::gogProofs[0]);++i){
   unsigned char&byte=gog.bytes[save_state_list::gogProofs[i].rva];byte^=1;C(!save_state_list::Admit(fakeGame,ogre));byte^=1;
  }
  *(void**)(gog.bytes+0x2244f78)=(void*)Assign;C(!save_state_list::Admit(fakeGame,ogre));
  *(void**)(gog.bytes+0x2244f78)=(void*)nativeAssign;
  C(memcmp(gog.bytes+0x6ca4d7,save_state_list::gogCallBytes,9)==0);
  save_state_list::tried=save_state_list::admitted=false;save_state_list::Install(NULL,NULL);
  C(save_state_list::admitted&&save_state_list::page!=NULL);
  Unarm(gog.bytes+0x6ca4d7,save_state_list::gogCallBytes);fakeGame=NULL;
 }
#endif
 printf("PASS %u native save-list checks\n",checks);return 0;
}
