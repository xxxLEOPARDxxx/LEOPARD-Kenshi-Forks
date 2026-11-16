// Offline VC10 string ownership fixture. No game execution or process access.
#define NOMINMAX
#include <windows.h>
#include <string>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
static bool countAlloc=false,failAlloc=false;
static unsigned allocations=0,deallocations=0,checks=0,forwards=0;
void*operator new(size_t n){if(countAlloc){++allocations;if(failAlloc)throw std::bad_alloc();}void*p=malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p){if(p&&countAlloc)++deallocations;free(p);}
static bool owner=true;static uintptr_t caller=0;
static HMODULE module=NULL;static unsigned hooks=0,errors=0;static int hookStatus=0;static bool nullOriginal=false;
bool IsMainThread(){return owner;}
namespace fixes{struct Config{bool saveStringTransferEnabled;};Config g_fixesCfg={true};}
static void LogError(const std::string&){++errors;}
#define KEO_DETAIL(x) ((void)0)
namespace KenshiLib{enum Status{SUCCESS,FAILURE};Status AddHook(void*,void*,void**out){++hooks;*out=nullOriginal?NULL:(void*)0x12345;return (Status)hookStatus;}}
bool ReadModuleImageId(HMODULE m,DWORD*s,DWORD*z){if(!m)return false;*s=0x65d604d7;*z=0x232c000;return true;}
#define KEO_SAVE_STRING_TEST
#define KEO_SAVE_STRING_CALLER() caller
#define KEO_SAVE_STRING_MODULE() module
#ifdef KEO_SAVE_STRING_BASELINE
namespace save_string_transfer{
struct List{void*allocator;unsigned count,capacity;std::string*data;};
typedef void(*PushFn)(List*,std::string*);static PushFn original;
static uintptr_t gameBase=0;static bool admitted=true;
static void Push(List*list,std::string*value){original(list,value);}
}
#else
#ifndef KEO_SAVE_STRING_SOURCE
#define KEO_SAVE_STRING_SOURCE "../src/fixes/world/save_string_transfer.cpp"
#endif
#include KEO_SAVE_STRING_SOURCE
#endif
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
static bool nativeThrows=false;
static void Native(save_string_transfer::List*list,std::string*value){
 ++forwards;if(nativeThrows)throw std::runtime_error("native");
 if(!list||!value||list->count>=list->capacity)return;
 list->data[list->count]=*value;++list->count;std::string empty;value->swap(empty);
}
static void Reset(){allocations=deallocations=forwards=0;owner=true;nativeThrows=false;countAlloc=failAlloc=false;fixes::g_fixesCfg.saveStringTransferEnabled=true;save_string_transfer::admitted=true;save_string_transfer::gameBase=0x140000000;caller=save_string_transfer::gameBase+0x6cac21;save_string_transfer::original=Native;}
static void ByValue(save_string_transfer::List*list,std::string value){save_string_transfer::Push(list,&value);}
int main(){
 Reset();{std::string values[1],source(120,'x'),argument(source);save_string_transfer::List l={NULL,0,1,values};countAlloc=true;save_string_transfer::Push(&l,&argument);countAlloc=false;C(values[0]==source&&argument.empty());printf("long-state insertion: %u allocations, %u frees\n",allocations,deallocations);C(allocations==0);}
 for(unsigned length=0;length<260;++length)for(unsigned oldLength=0;oldLength<80;oldLength+=13){
  Reset();std::string slots[3];slots[0]="existing";slots[1].assign(oldLength,'z');
  std::string source(length,'x'),argument(source);const char*buffer=argument.c_str();
  save_string_transfer::List list={NULL,1,3,slots};
  countAlloc=true;save_string_transfer::Push(&list,&argument);countAlloc=false;
  C(list.count==2&&slots[0]=="existing"&&slots[1]==source&&argument.empty());
  const bool transfer=length>15&&oldLength==0;
  C(forwards==(transfer?0u:1u));if(transfer){C(allocations==0&&deallocations==0);C(slots[1].c_str()==buffer);}
  C(source==std::string(length,'x'));
 }
 Reset();std::string slots[2],arg(120,'a');save_string_transfer::List list={NULL,0,2,slots};
 failAlloc=countAlloc=true;bool threw=false;try{save_string_transfer::Push(&list,&arg);}catch(...){threw=true;}countAlloc=failAlloc=false;C(!threw&&list.count==1&&arg.empty()&&slots[0].size()==120&&allocations==0);
 // Refused paths leave argument untouched before forwarding once.
 for(unsigned mode=0;mode<8;++mode){Reset();std::string values[2];std::string value(100,'n');save_string_transfer::List l={NULL,0,2,values};
  if(mode==0)owner=false;if(mode==1)fixes::g_fixesCfg.saveStringTransferEnabled=false;if(mode==2)save_string_transfer::admitted=false;if(mode==3)++caller;if(mode==4)l.count=l.capacity;if(mode==5)l.capacity=65537;if(mode==6)l.data=NULL;if(mode==7)l.count=3;
  // Native throw is outside optimization catches, with original args intact.
  nativeThrows=true;threw=false;try{save_string_transfer::Push(&l,&value);}catch(const std::runtime_error&){threw=true;}C(threw&&forwards==1&&value==std::string(100,'n'));
 }
 Reset();std::string values[2];values[1].assign(100,'a');save_string_transfer::List l={NULL,0,2,values};nativeThrows=true;threw=false;try{save_string_transfer::Push(&l,&values[1]);}catch(const std::runtime_error&){threw=true;}C(threw&&forwards==1&&values[1]==std::string(100,'a'));
 Reset();{std::string values[1],value(80,'b');save_string_transfer::List l={NULL,0,1,values};ByValue(&l,value);C(value==std::string(80,'b')&&values[0]==value&&l.count==1&&forwards==0);}
 // Real x64 by-value call ABI: the compiler creates the temporary and expects
 // the callee to consume/destroy it, just like the pinned native helper.
 for(unsigned i=0;i<1000;++i){Reset();std::string source(16+i%200,'v');countAlloc=true;{
  std::string values[1];save_string_transfer::List l={NULL,0,1,values};
  typedef void(*ByValueFn)(save_string_transfer::List*,std::string);
  ByValueFn fn=reinterpret_cast<ByValueFn>(&save_string_transfer::Push);fn(&l,source);
  countAlloc=false;C(l.count==1&&values[0]==source&&forwards==0&&allocations==1&&deallocations==0);countAlloc=true;
 }countAlloc=false;C(deallocations==1&&source==std::string(16+i%200,'v'));}
 Reset();{std::string values[1],value(80,'c');save_string_transfer::List l={NULL,0,1,values};fixes::g_fixesCfg.saveStringTransferEnabled=false;ByValue(&l,value);C(value==std::string(80,'c')&&values[0]==value&&l.count==1&&forwards==1);}
 for(unsigned i=0;i<10000;++i){Reset();std::string value(16+i%200,'r');save_string_transfer::List l={NULL,0,2,slots};save_string_transfer::Push(&l,&value);C(l.count==1&&value.empty()&&slots[0].size()==16+i%200&&forwards==1);}
 Reset();{std::string values[1],value(80,'q');values[0].reserve(300);save_string_transfer::List l={NULL,0,1,values};save_string_transfer::Push(&l,&value);C(l.count==1&&forwards==1&&values[0]==std::string(80,'q')&&value.empty());}
 for(unsigned i=0;i<4;++i){Reset();static const uintptr_t returns[]={0x6c1ebc,0x6c1f47,0x6c2070,0x6cac21};caller=save_string_transfer::gameBase+returns[i];std::string values[1],value(30,'a');save_string_transfer::List l={NULL,0,1,values};save_string_transfer::Push(&l,&value);C(forwards==0&&l.count==1&&value.empty()&&values[0]==std::string(30,'a'));}
#ifndef KEO_SAVE_STRING_BASELINE
 module=(HMODULE)VirtualAlloc(NULL,0x232c000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);C(module!=NULL);C(!save_string_transfer::Admit(NULL));C(!save_string_transfer::Admit(module));
 for(size_t i=0;i<sizeof(save_string_transfer::proofs)/sizeof(save_string_transfer::proofs[0]);++i){const save_string_transfer::Proof&p=save_string_transfer::proofs[i];memcpy((char*)module+p.rva,p.bytes,p.size);}
 C(save_string_transfer::Admit(module));
 for(size_t i=0;i<sizeof(save_string_transfer::proofs)/sizeof(save_string_transfer::proofs[0]);++i){const save_string_transfer::Proof&p=save_string_transfer::proofs[i];((char*)module)[p.rva]^=1;C(!save_string_transfer::Admit(module));((char*)module)[p.rva]^=1;}
 for(unsigned mode=0;mode<3;++mode){save_string_transfer::tried=save_string_transfer::admitted=false;hookStatus=mode==1?1:0;nullOriginal=mode==2;hooks=errors=0;save_string_transfer::Install(NULL,NULL);C(hooks==1&&save_string_transfer::admitted==(mode==0)&&errors==(mode==0?0u:1u));save_string_transfer::Install(NULL,NULL);C(hooks==1);}
 VirtualFree(module,0,MEM_RELEASE);module=NULL;save_string_transfer::tried=save_string_transfer::admitted=false;hooks=errors=0;save_string_transfer::Install(NULL,NULL);C(hooks==0&&errors==1&&!save_string_transfer::admitted);
#endif
 printf("PASS %u native-CRT save string checks; no game execution\n",checks);return 0;
}
