#include <Windows.h>
#include <stdexcept>
#include <cstdio>
namespace MyGUI { struct Widget {}; }
class Inventory {};
class InventoryGUI
{
public:
    InventoryGUI* ownerInventory;
    Inventory inventory;
    InventoryGUI():ownerInventory(NULL){}
    Inventory* _NV_getInventory(){return &inventory;}
};
static volatile LONG s_worldWorkDepth=0;
static bool s_subsystemsReady=true,s_lifecycleReady=true,s_autoArrangeExceptionLogged=false;
static bool enabled=true,trader=false,customHandled=true,customThrows=false;
static int nativeCalls=0,customCalls=0,errorCalls=0;
static void Native(InventoryGUI*,MyGUI::Widget*){++nativeCalls;}
static void (*orig_autoArrangeButton)(InventoryGUI*,MyGUI::Widget*)=Native;
namespace StackSortMcm { bool Enabled(){return enabled;} }
namespace StackSortUI { bool OnForwardClick(InventoryGUI*,MyGUI::Widget*){++customCalls;if(customThrows)throw std::runtime_error("partial");return customHandled;} }
static void LearnBaseVtable(InventoryGUI*){}
static bool isTraderGUI(InventoryGUI*){return trader;}
static void WarnStartupIssuesOnce(){}
static void ReportHookException(const char*,bool&){++errorCalls;}
#include "ui_click_hook_under_test.h" // extracted actual production hook
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL click %d: %s\n",__LINE__,#x);}}while(0)
static void Reset(){s_worldWorkDepth=0;s_subsystemsReady=s_lifecycleReady=enabled=customHandled=true;trader=customThrows=false;nativeCalls=customCalls=errorCalls=0;}
int main()
{
    InventoryGUI body,bag;bag.ownerInventory=&body;MyGUI::Widget sender;
    Reset();hook_autoArrangeButton(&body,&sender);CHECK(customCalls==1&&nativeCalls==0);
    Reset();hook_autoArrangeButton(&bag,&sender);CHECK(customCalls==1&&nativeCalls==0);
    Reset();customHandled=false;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==1&&nativeCalls==1); // ineligible or narrow fallback
    Reset();customHandled=false;hook_autoArrangeButton(&bag,&sender);CHECK(customCalls==1&&nativeCalls==1);
    Reset();enabled=false;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==0&&nativeCalls==1);
    Reset();s_lifecycleReady=false;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==0&&nativeCalls==1);
    Reset();trader=true;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==0&&nativeCalls==1);
    Reset();s_worldWorkDepth=1;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==0&&nativeCalls==0);
    Reset();customThrows=true;hook_autoArrangeButton(&body,&sender);CHECK(customCalls==1&&nativeCalls==0&&errorCalls==1);
    std::printf("%d production click-hook checks, %d failures\n",checks,failures);return failures?1:0;
}
