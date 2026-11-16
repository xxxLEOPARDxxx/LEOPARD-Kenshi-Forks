#define NOMINMAX
#include <windows.h>
#include <map>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <stdint.h>
#include "fixes/near_page.h"
static DWORD owner=GetCurrentThreadId();bool IsMainThread(){return owner==GetCurrentThreadId();}
namespace fixes{struct Config{bool saveInstanceLookupEnabled;};Config g_fixesCfg={true};}
static unsigned descents=0,comparisons=0;static bool failDefault=false;static unsigned born=0,dead=0;
static bool failLower=false,sehLower=false;
static void(*onDefault)()=NULL;
struct Value{int value;Value():value(0){if(failDefault)throw std::runtime_error("default");if(onDefault)onDefault();++born;}Value(const Value&o):value(o.value){++born;}~Value(){++dead;}};
struct Less{bool operator()(const std::string&a,const std::string&b)const{++comparisons;return a<b;}};
typedef std::map<std::string,Value,Less> Map;
extern "C" void*KEO_TestNativeLower(void*m,const void*k) throw(...){++descents;if(descents==2&&failLower)throw std::runtime_error("lookup");if(descents==2&&sehLower)RaiseException(0xe1234567,0,0,NULL);Map*map=(Map*)m;return map->lower_bound(*(const std::string*)k)._Mynode();}
extern "C" Value*KEO_TestCompleteIndex(Map*map,const std::string*key,void*node) throw(...){Map::iterator at((Map::iterator::_Nodeptr)node,map);if(at==map->end()||Less()(*key,at->first))at=map->insert(at,Map::value_type(*key,Value()));return &at->second;}
extern "C" Value*KEO_TestSaveDriver(Map*,const std::string*) throw(...);
extern "C" Value*KEO_TestOtherDriver(Map*,const std::string*) throw(...);
extern "C" char KEO_TestParserAfter,KEO_TestOuterAfter,KEO_TestLowerPivot,KEO_TestLowerAfter;
#ifndef KEO_INSTANCE_BASELINE
#define KEO_INSTANCE_TEST
#define KEO_INSTANCE_ADMIT_TEST
static DWORD profileStamp=0x65d604d7,profileSize=0x232c000;static void*world=NULL;
static bool ReadModuleImageId(HMODULE,DWORD*stamp,DWORD*size){*stamp=profileStamp;*size=profileSize;return true;}
static uintptr_t GameAddr(int){return (uintptr_t)&world;}
#define RVA_GLOBAL_SECTION_MGR 0
static std::string GetDLLDirectory(){return std::string();}
static const char*OPTIMIZER_LOG_NAME="fixture.log";
static void LogError(const std::string&){}
#define KEO_DETAIL(x) ((void)0)
#include "../src/fixes/world/save_instance_lookup.cpp"
#endif
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
static DWORD WINAPI Other(void*p){Map*map=(Map*)p;const std::string key="worker";Value*v=KEO_TestSaveDriver(map,&key);return v?0:1;}
static Map*nestMap=NULL;static void Nested(){onDefault=NULL;const std::string key="nested";KEO_TestSaveDriver(nestMap,&key);onDefault=Nested;}
static bool SehProbe(Map*map,const std::string*key){__try{KEO_TestOtherDriver(map,key);return false;}__except(GetExceptionCode()==0xe1234567?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
int main(){
#ifndef KEO_INSTANCE_BASELINE
 unsigned char*image=(unsigned char*)VirtualAlloc(NULL,0x232c000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);C(image!=NULL);
 for(unsigned i=0;i<sizeof(save_instance_lookup::proofs)/sizeof(save_instance_lookup::proofs[0]);++i){const save_instance_lookup::Proof&p=save_instance_lookup::proofs[i];memcpy(image+p.rva,p.bytes,p.size);}
 *(void**)(image+0x2244b08)=(void*)GetProcAddress(GetModuleHandleA("MSVCR100.dll"),"memcmp");C(save_instance_lookup::Admit((HMODULE)image));
 C(!save_instance_lookup::Admit(NULL));++profileStamp;C(!save_instance_lookup::Admit((HMODULE)image));--profileStamp;++profileSize;C(!save_instance_lookup::Admit((HMODULE)image));--profileSize;world=image;C(!save_instance_lookup::Admit((HMODULE)image));world=NULL;
 ++owner;C(!save_instance_lookup::Admit((HMODULE)image));--owner;
 for(unsigned i=0;i<sizeof(save_instance_lookup::proofs)/sizeof(save_instance_lookup::proofs[0]);++i){const save_instance_lookup::Proof&p=save_instance_lookup::proofs[i];image[p.rva+p.size-1]^=1;C(!save_instance_lookup::Admit((HMODULE)image));image[p.rva+p.size-1]^=1;}
 *(void**)(image+0x2244b08)=image;C(!save_instance_lookup::Admit((HMODULE)image));VirtualFree(image,0,MEM_RELEASE);
 save_instance_lookup::original=&KEO_TestNativeLower;save_instance_lookup::innerReturn=(uintptr_t)&KEO_TestLowerAfter;save_instance_lookup::outerReturn=(uintptr_t)&KEO_TestOuterAfter;save_instance_lookup::parserReturn=(uintptr_t)&KEO_TestParserAfter;
 unsigned char expected[5];memcpy(expected,&KEO_TestLowerPivot,5);C(save_instance_lookup::Arm((unsigned char*)&KEO_TestLowerPivot,expected));save_instance_lookup::admitted=true;
#endif
 {Map map;const std::string key="initial_long_saved_object_identity";descents=0;Value*v=KEO_TestSaveDriver(&map,&key);C(v&&v->value==0&&map.size()==1);printf("native descents per insertion: %u\n",descents);C(descents==1);
 v->value=71;descents=0;C(KEO_TestSaveDriver(&map,&key)==v&&v->value==71&&descents==1);
 const char*names[]={"a","z","middle","initial_long_saved_object_identitx","","zzzz"};for(unsigned i=0;i<6;++i){const std::string k=names[i];descents=0;Value*p=KEO_TestSaveDriver(&map,&k);C(p==&map.find(k)->second&&descents==1);}
 descents=0;C(KEO_TestOtherDriver(&map,&key)==v&&descents==2);
#ifndef KEO_INSTANCE_BASELINE
 fixes::g_fixesCfg.saveInstanceLookupEnabled=false;descents=0;C(KEO_TestSaveDriver(&map,&key)==v&&descents==2);fixes::g_fixesCfg.saveInstanceLookupEnabled=true;
 save_instance_lookup::admitted=false;descents=0;C(KEO_TestSaveDriver(&map,&key)==v&&descents==2);save_instance_lookup::admitted=true;
 uintptr_t saved=save_instance_lookup::innerReturn;save_instance_lookup::innerReturn++;descents=0;C(KEO_TestSaveDriver(&map,&key)==v&&descents==2);save_instance_lookup::innerReturn=saved;
 saved=save_instance_lookup::outerReturn;save_instance_lookup::outerReturn++;descents=0;C(KEO_TestSaveDriver(&map,&key)==v&&descents==2);save_instance_lookup::outerReturn=saved;
#endif
 descents=0;HANDLE thread=CreateThread(NULL,0,Other,&map,0,NULL);C(thread!=NULL);C(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CloseHandle(thread);C(descents==2&&map.count("worker")==1);
 const std::string fail="new_that_throws";const size_t size=map.size();failDefault=true;bool caught=false;try{KEO_TestSaveDriver(&map,&fail);}catch(const std::runtime_error&){caught=true;}failDefault=false;C(caught&&map.size()==size&&map.find(fail)==map.end());
 failLower=true;descents=0;caught=false;try{KEO_TestOtherDriver(&map,&key);}catch(const std::runtime_error&){caught=true;}failLower=false;C(caught&&descents==2&&map.size()==size&&v->value==71);
 sehLower=true;descents=0;C(SehProbe(&map,&key)&&descents==2&&map.size()==size&&v->value==71);sehLower=false;
 for(unsigned i=0;i<2048;++i){char text[48];sprintf_s(text,"saved_object_%06u",(i*1093)%2048);const std::string k=text;descents=0;Value*p=KEO_TestSaveDriver(&map,&k);C(p&&descents==1);p->value=(int)i;}
 #ifndef KEO_INSTANCE_BASELINE
 comparisons=0;for(unsigned i=0;i<128;++i)KEO_TestSaveDriver(&map,&key);const unsigned fast= comparisons;
 fixes::g_fixesCfg.saveInstanceLookupEnabled=false;comparisons=0;for(unsigned i=0;i<128;++i)KEO_TestSaveDriver(&map,&key);const unsigned stock=comparisons;fixes::g_fixesCfg.saveInstanceLookupEnabled=true;
 printf("existing-object tree comparisons: %u -> %u\n",stock,fast);C(fast<stock);
 const std::string outside="direct";descents=0;C(save_instance_lookup::Lookup(&map,&outside)!=NULL&&descents==1);
 // Synthetic authenticated frames exercise each production bound without
 // touching any process other than this isolated fixture.
 uintptr_t frame[64]={0};frame[0]=save_instance_lookup::innerReturn;frame[0x160/8]=save_instance_lookup::outerReturn;frame[0x1d0/8]=save_instance_lookup::parserReturn;frame[0x150/8]=(uintptr_t)&map;frame[0x148/8]=(uintptr_t)&key;frame[0x1d8/8]=(uintptr_t)map.begin()._Mynode();
 C(save_instance_lookup::Candidate(&map,&key,(unsigned char*)frame)==map.begin()._Mynode());
 const unsigned offsets[]={0,0x160/8,0x1d0/8,0x150/8,0x148/8,0x1d8/8};for(unsigned i=0;i<6;++i){const uintptr_t saved=frame[offsets[i]];frame[offsets[i]]=0;C(save_instance_lookup::Candidate(&map,&key,(unsigned char*)frame)==NULL);frame[offsets[i]]=saved;}
 frame[0x1d8/8]|=1;C(save_instance_lookup::Candidate(&map,&key,(unsigned char*)frame)==NULL);
 C(save_instance_lookup::Candidate(NULL,&key,(unsigned char*)frame)==NULL);C(save_instance_lookup::Candidate(&map,NULL,(unsigned char*)frame)==NULL);C(save_instance_lookup::Candidate(&map,&key,NULL)==NULL);
 #endif
 Map nested;nestMap=&nested;onDefault=Nested;const std::string nest="outer_new";Value*n=KEO_TestSaveDriver(&map,&nest);onDefault=NULL;C(n&&nested.count("nested")==1&&map.count(nest)==1);
 }C(born==dead);printf("PASS %u actual Win64 saved-object lookup checks\n",checks);return 0;}
