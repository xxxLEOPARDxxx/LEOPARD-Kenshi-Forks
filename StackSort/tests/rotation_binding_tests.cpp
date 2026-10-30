// Actual production binder plus opening/click entry prefixes: native calls
// and worker execution are replaced with a deterministic DLL-loading model.
#include <cstdio>
#include <string>
typedef void* HMODULE;
class Item {};
class InventoryGUI {};
static bool loaded=false,ready=true,inputEnabled=true,missing=false,missingRefresh=false;
static int version=1,moduleLookups=0,symbolLookups=0,invalidations=0,snapshotRotatable=0,errors=0;
static int ApiVersion(){return version;}
static int CanRotate(void*){return ready?1:0;} // Rotate's C API gates HooksReady, not input toggle
static int IsRotated(void*){return 0;}
static int SetRotated(void*,int,void*){return ready?1:0;}
static void RefreshVisuals(void*){}
static HMODULE GetModuleHandleA(const char*){++moduleLookups;return loaded?(HMODULE)1:NULL;}
static void* GetProcAddress(HMODULE,const char* name)
{
    ++symbolLookups;
    if(missing && std::string(name)=="KenshiRotate_CanRotate")return NULL;
    if(std::string(name)=="KenshiRotate_ApiVersion")return (void*)&ApiVersion;
    if(std::string(name)=="KenshiRotate_CanRotate")return (void*)&CanRotate;
    if(std::string(name)=="KenshiRotate_IsRotated")return (void*)&IsRotated;
    if(std::string(name)=="KenshiRotate_SetRotated")return (void*)&SetRotated;
    if(missingRefresh)return NULL;
    return (void*)&RefreshVisuals;
}
static void LogInfo(const std::string&){}
static void LogError(const std::string&){++errors;}
static std::string IntToStr(int v){char s[32];sprintf_s(s,"%d",v);return s;}
static Item item;
static int s_guiTracking[1];
static int FindTracking(InventoryGUI*){return -1;}
static void StampSlot(int&){}
bool StackSort_CanRotate(Item*);
class SortWorker
{
public:
    static void OnRotationAPIAvailable(){++invalidations;}
    static void OnInventoryOpened(InventoryGUI*);
    static int AllocTracking(InventoryGUI*){return 0;}
    static void SnapshotSections(int,InventoryGUI*,bool*){snapshotRotatable=StackSort_CanRotate(&item)?1:0;}
};
struct UIState {};
static UIState s_uiStates[1];
class StackSortUI {public:static void FirstClickInit(int);};
#include "rotation_types_under_test.h"
#include "rotation_api_under_test.h"
#include "rotation_open_under_test.h"
#include "rotation_click_under_test.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL rotate %d: %s\n",__LINE__,#x);}}while(0)
static void Reset()
{
    loaded=missing=missingRefresh=false;ready=inputEnabled=true;version=1;
    moduleLookups=symbolLookups=invalidations=snapshotRotatable=errors=0;
    s_krAvailable=false;s_krApiVersion=NULL;s_krCanRotate=NULL;s_krIsRotated=NULL;s_krSetRotated=NULL;s_krRefreshVisuals=NULL;
    s_krRejectedModule=NULL;s_krAbsentLogged=false;
}
int main()
{
    InventoryGUI gui;
    Reset();StackSort_InitRotateAPI();CHECK(!StackSort_RotateAvailable());loaded=true;
    SortWorker::OnInventoryOpened(&gui);CHECK(StackSort_RotateAvailable()&&snapshotRotatable==1); // Stack-first
    Reset();loaded=true;StackSort_InitRotateAPI();SortWorker::OnInventoryOpened(&gui);
    CHECK(StackSort_RotateAvailable()&&snapshotRotatable==1);CHECK(moduleLookups==1); // Rotate-first, cached binding
    int lookups=moduleLookups;for(int i=0;i<50;++i){StackSortUI::FirstClickInit(0);StackSort_CanRotate(&item);}
    CHECK(moduleLookups==lookups);CHECK(invalidations==1);
    Reset();StackSort_InitRotateAPI();SortWorker::OnInventoryOpened(&gui);CHECK(!StackSort_RotateAvailable()&&snapshotRotatable==0);
    loaded=true;StackSortUI::FirstClickInit(0);CHECK(StackSort_RotateAvailable()&&StackSort_CanRotate(&item));CHECK(invalidations==1); // late load, open panel
    Reset();StackSort_InitRotateAPI();loaded=true; // StackSort disabled during startup/open: no GUI calls until enabled
    SortWorker::OnInventoryOpened(&gui);CHECK(snapshotRotatable==1);
    Reset();loaded=true;ready=false;StackSort_InitRotateAPI();SortWorker::OnInventoryOpened(&gui);
    CHECK(StackSort_RotateAvailable()&&!StackSort_CanRotate(&item));ready=true;StackSortUI::FirstClickInit(0);CHECK(StackSort_CanRotate(&item));
    inputEnabled=false;CHECK(StackSort_CanRotate(&item)); // preserve Rotate's existing input-only switch semantics
    Reset();loaded=true;version=0;StackSort_InitRotateAPI();SortWorker::OnInventoryOpened(&gui);StackSortUI::FirstClickInit(0);
    CHECK(!StackSort_RotateAvailable()&&!StackSort_CanRotate(&item));CHECK(invalidations==0);CHECK(errors==1);
    Reset();loaded=true;missingRefresh=true;SortWorker::OnInventoryOpened(&gui);StackSort_RefreshVisuals(&gui);
    CHECK(StackSort_RotateAvailable()&&StackSort_CanRotate(&item)&&invalidations==1&&errors==0);
    Reset();loaded=true;missing=true;StackSort_InitRotateAPI();SortWorker::OnInventoryOpened(&gui);StackSortUI::FirstClickInit(0);
    CHECK(!StackSort_RotateAvailable()&&!StackSort_CanRotate(&item));CHECK(invalidations==0);CHECK(errors==1);
    std::printf("%d production rotation-binding checks, %d failures\n",checks,failures);return failures?1:0;
}
