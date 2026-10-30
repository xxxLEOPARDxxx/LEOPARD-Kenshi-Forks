#include <cstdio>
#include <string>
#include <stdexcept>
#include "PairTracePolicy.h"
namespace MyGUI {namespace types {template<class T>struct TPoint{T left,top;};}}
struct InventoryItemBase {bool rotated;InventoryItemBase():rotated(false){} };
struct Item:InventoryItemBase {};
struct InventoryGUI {};
static bool debug=false,ui=true,upstream=true,quick=false,placeThrows=false;
static int nativeCalls=0,rotationReads=0,logs=0,scopedCalls=0;
static bool recordedNative=false,recordedResult=false;
namespace PluginDiagnostics {static bool Enabled(){return debug;}}
namespace RotationState {static bool IsRotated(Item* i){++rotationReads;return i->rotated;}}
namespace CursorState {static Item* held=NULL;static Item* GetItem(){return held;}static void Clear(){held=NULL;}}
static bool g_inQuickTransfer=false;
static bool NativePair(InventoryItemBase*,InventoryItemBase*){++nativeCalls;return upstream;}
static bool (*canStackWith_orig)(InventoryItemBase*,InventoryItemBase*)=NativePair;
static void RecordStackPair(InventoryItemBase*,InventoryItemBase*,bool native,bool result,void*)
{if(!ui)return;++logs;recordedNative=native;recordedResult=result;}
static void* ReturnSite(){return NULL;}
#define _ReturnAddress ReturnSite
static PairTrace::Context s_pairPlaceContext;
static bool PairTraceOnUIThread(){return debug&&ui;}
static bool NativePlace(InventoryGUI* g,const std::string,const MyGUI::types::TPoint<int>&)
{if(s_pairPlaceContext.gui==g)++scopedCalls;if(placeThrows)throw std::runtime_error("place");return true;}
static bool (*placeItemFromMouse_orig)(InventoryGUI*,const std::string,const MyGUI::types::TPoint<int>&)=NativePlace;
#include "pair_hook_under_test.h"
#include "place_hook_under_test.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL hook %d: %s\n",__LINE__,#x);}}while(0)
static void Reset(){debug=false;ui=upstream=true;placeThrows=false;nativeCalls=rotationReads=logs=scopedCalls=0;g_inQuickTransfer=false;}
int main()
{
 Item a,b;b.rotated=true;
 Reset();CHECK(!canStackWith_hook(&a,&b));CHECK(nativeCalls==1&&rotationReads==2&&logs==0);
 Reset();upstream=false;CHECK(!canStackWith_hook(&a,&b));CHECK(nativeCalls==1&&rotationReads==0&&logs==0);
 Reset();debug=true;upstream=false;CHECK(!canStackWith_hook(&a,&b));CHECK(logs==1&&!recordedNative&&!recordedResult);
 Reset();debug=true;CHECK(!canStackWith_hook(&a,&b));CHECK(logs==1&&recordedNative&&!recordedResult);
 Reset();debug=true;g_inQuickTransfer=true;CHECK(canStackWith_hook(&a,&b));CHECK(logs==1&&recordedNative&&recordedResult&&rotationReads==0);
 Reset();debug=true;ui=false;CHECK(!canStackWith_hook(&a,&b));CHECK(logs==0&&nativeCalls==1);
 Reset();debug=true;CHECK(!canStackWith_hook(NULL,&b));CHECK(logs==0&&nativeCalls==0);
 InventoryGUI gui;MyGUI::types::TPoint<int> pos={0,0};CursorState::held=&a;
 Reset();CHECK(placeItemFromMouse_hook(&gui,"bag",pos));CHECK(scopedCalls==0&&s_pairPlaceContext.gui==NULL);
 Reset();debug=true;CHECK(placeItemFromMouse_hook(&gui,"bag",pos));CHECK(scopedCalls==1&&s_pairPlaceContext.gui==NULL);
 Reset();debug=true;placeThrows=true;try{placeItemFromMouse_hook(&gui,"bag",pos);}catch(...){}CHECK(scopedCalls==1&&s_pairPlaceContext.gui==NULL);
 printf("%d extracted production hook checks, %d failures\n",checks,failures);return failures?1:0;
}
