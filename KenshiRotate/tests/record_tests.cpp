#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>
#include <float.h>
#include <limits>
#include "PairTracePolicy.h"
typedef unsigned long DWORD;typedef long LONG;typedef void* HMODULE;typedef const char* LPCSTR;
#define MAX_PATH 260
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
static DWORD thread=10,now=0;static bool debug=false;static int reads=0,getters=0,logs=0;static std::string last;
static DWORD GetCurrentThreadId(){return thread;}static DWORD GetTickCount(){return now;}
static LONG InterlockedCompareExchange(volatile LONG* p,LONG to,LONG from){LONG old=*p;if(old==from)*p=to;return old;}
static bool GetModuleHandleExA(int,LPCSTR,HMODULE*){return false;}static int GetModuleFileNameA(HMODULE,char*,int){return 0;}
static void ErrorLog(const char*){}
namespace PluginDiagnostics {bool Enabled(){return debug;}void Write(const std::string& s){++logs;last=s;}}
class InventoryItemBase {public:int key,quantity;float originalFullChargeAmount,chargesLeft;bool rotated;
 InventoryItemBase(int k=1):key(k),quantity(1),originalFullChargeAmount(0),chargesLeft(0),rotated(false){}
 int isStackable(void*){++getters;return 1;}};
class Item:public InventoryItemBase {public:Item(int k=1):InventoryItemBase(k){} };
class InventorySection {public:Item* target;bool hasItem(Item* p){++getters;return p==target;}};
class Inventory {public:InventorySection section;InventorySection* getSection(const std::string&){++getters;return &section;}};
class InventoryGUI {public:Inventory inv;Inventory* _NV_getInventory(){++getters;return &inv;}};
namespace CursorState {static Item* held=NULL;static Item* GetItem(){return held;}}
struct Snapshot {int key,quantity;float full,charges;bool rotated;};
static Snapshot Read(InventoryItemBase* item){++reads;Snapshot s;memset(&s,0,sizeof(s));s.key=item->key;s.quantity=item->quantity;s.full=item->originalFullChargeAmount;s.charges=item->chargesLeft;s.rotated=item->rotated;return s;}
static void Format(char* out,size_t n,const Snapshot&,InventoryItemBase*){++getters;strncpy_s(out,n,"state",_TRUNCATE);}
#include "pair_thread_under_test.h"
#include "pair_hash_under_test.h"
#include "pair_record_under_test.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL record %d: %s\n",__LINE__,#x);}}while(0)
static void Reset(){debug=false;thread=10;now=0;reads=getters=logs=0;last.clear();s_uiThread=0;s_gate=PairTrace::Gate();s_pairPlaceContext=PairTrace::Context();}
int main()
{
 Item a(1),b(2);CursorState::held=&b;
 Reset();PairTraceRememberUIThread();RecordStackPair(&a,&b,true,true,NULL);CHECK(reads==0&&getters==0&&logs==0);
 Reset();debug=true;PairTraceRememberUIThread();thread=11;RecordStackPair(&a,&b,true,true,NULL);CHECK(reads==0&&getters==0&&logs==0);
 thread=10;CursorState::held=NULL;RecordStackPair(&a,&b,true,true,NULL);CHECK(reads==0&&getters==0&&logs==0);
 CursorState::held=&b;RecordStackPair(&a,&b,false,false,NULL);CHECK(logs==1&&reads==2&&getters==2);CHECK(last.find("OriginalReject")!=std::string::npos&&last.find("capacity=-1")!=std::string::npos);
 int expensive=getters;now=20;RecordStackPair(&a,&b,false,false,NULL);CHECK(logs==1&&getters==expensive);
 now=250;b.quantity=2;RecordStackPair(&a,&b,true,false,NULL);CHECK(logs==2&&last.find("RotateReject")!=std::string::npos);
 now=500;InventoryGUI live;live.inv.section.target=&a;std::string name="bag";
 {PairTrace::ScopedContext scope(s_pairPlaceContext,&live,&name);RecordStackPair(&a,&b,true,true,NULL);}
 CHECK(logs==3&&last.find("TargetFull")!=std::string::npos&&last.find("native=1 result=1")!=std::string::npos);CHECK(s_pairPlaceContext.gui==NULL);
 now=750;a.chargesLeft=50;a.originalFullChargeAmount=100;RecordStackPair(&a,&b,true,true,NULL);CHECK(logs==4&&last.find("Accepted")!=std::string::npos);
 now=900;a.quantity=2;expensive=getters;RecordStackPair(&a,&b,true,true,NULL);CHECK(logs==4&&getters==expensive);
 now=1000;RecordStackPair(&a,&b,true,true,NULL);CHECK(logs==5);
 now=1500;expensive=getters;RecordStackPair(&a,&b,true,true,NULL);CHECK(logs==5&&getters==expensive);
 now=3000;RecordStackPair(&a,&b,true,true,NULL);CHECK(logs==6);
 now=3250;a.quantity=1;a.chargesLeft=25;
 {PairTrace::ScopedContext scope(s_pairPlaceContext,&live,&name);RecordStackPair(&a,&b,true,true,NULL);}
 CHECK(logs==7&&last.find("Accepted")!=std::string::npos&&last.find("targetFull=no")!=std::string::npos);
 CHECK(a.quantity==1&&a.chargesLeft==25&&a.originalFullChargeAmount==100&&!a.rotated&&b.quantity==2);
 now=3500;a.chargesLeft=std::numeric_limits<float>::quiet_NaN();
 {PairTrace::ScopedContext scope(s_pairPlaceContext,&live,&name);RecordStackPair(&a,&b,true,true,NULL);}
 CHECK(logs==8&&last.find("targetFull=unknown")!=std::string::npos);
 PairTrace::Context context;int one=1,two=2;
 {PairTrace::ScopedContext first(context,&one,&one);try{PairTrace::ScopedContext nested(context,&two,&two);CHECK(context.gui==&two);throw 1;}catch(...){}CHECK(context.gui==&one);}
 CHECK(context.gui==NULL);
 PairTrace::Gate wrap;CHECK(wrap.Admit(1,1,0xfffffff0UL));CHECK(!wrap.Admit(1,2,5));CHECK(wrap.Admit(1,2,300));
 printf("%d extracted diagnostic record checks, %d failures\n",checks,failures);return failures?1:0;
}
