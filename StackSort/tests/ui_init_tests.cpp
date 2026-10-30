// Compile extracted production InitializeUI/OnClose/RemoveOurButtons against
// a bounded widget model. The planner itself is the unchanged production one.
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include "ControlStripPlan.h"
typedef unsigned long DWORD;static DWORD GetTickCount(){return 1;}static DWORD s_uiPollCount=0;
static void UnlinkModelWidget(void*);
namespace MyGUI
{
struct IntCoord { int left,top,width,height;IntCoord(int x=0,int y=0,int w=0,int h=0):left(x),top(y),width(w),height(h){} };
struct Size {int width,height;Size(int w=0,int h=0):width(w),height(h){} };
struct Align { enum {Default,Center}; };
struct MouseButton {};
struct Any {template<class T> Any(const T&){} };
struct Event {int count;Event():count(0){}template<class T>void operator+=(T){++count;} };
template<class F> int newDelegate(F){return 0;}
static int created=0,throwAt=0,destroyedForeign=0,fullCaptionPixels=136,throwPosition=0;
class Widget
{
public:
    Widget* parent;std::vector<Widget*> children;std::string name;IntCoord coord;bool visible;
    std::map<std::string,std::string> strings;
    Widget():parent(NULL),visible(true){}virtual ~Widget(){UnlinkModelWidget(this);}
    const std::string& getName(){return name;}Widget* getParent(){return parent;}
    const IntCoord& getCoord(){return coord;}void setCoord(const IntCoord& c){coord=c;}
    IntCoord getAbsoluteCoord(){IntCoord c=coord;for(Widget* p=parent;p;p=p->parent){c.left+=p->coord.left;c.top+=p->coord.top;}return c;}
    IntCoord getClientCoord(){return IntCoord(0,0,coord.width,coord.height);}
    void setPosition(int x,int y){int failure=throwPosition;throwPosition=0;if(failure==1)throw std::runtime_error("position before");coord.left=x;coord.top=y;if(failure==2)throw std::runtime_error("position after");}
    int getWidth(){return coord.width;}int getHeight(){return coord.height;}
    bool getVisible(){return visible;}void setVisible(bool value){visible=value;}
    const std::string& getUserString(const char* key){return strings[key];}
    void setUserString(const char* key,const char* value){strings[key]=value;}
    void clearUserString(const char* key){strings.erase(key);}
    size_t getChildCount(){return children.size();}Widget* getChildAt(size_t n){return children[n];}
    template<class T>T* createWidget(const char*,const IntCoord& c,int,const char* n)
    {if(++created==throwAt)throw std::runtime_error("allocation");T* w=new T;w->parent=this;w->coord=c;w->name=n;children.push_back(w);return w;}
};
class Button:public Widget
{
public:
    bool selected;Button():selected(false){}std::string caption;Event eventMouseButtonClick,eventMouseButtonPressed;
    void setCaption(const std::string& text){caption=text;}void setTextAlign(int){}void setUserData(const Any&){}
    IntCoord getTextRegion(){return IntCoord(3,3,coord.width-6,coord.height-6);}
    Size getTextSize(){return Size(caption.find("FULL")==0?fullCaptionPixels:caption.find("COMPACT")==0?88:caption.find("[")==0?42:caption=="H"||caption=="W"?14:8,18);}
};
class Gui
{
public:
    static Gui& getInstance(){static Gui value;return value;}static Gui* getInstancePtr(){return &getInstance();}
    void destroyWidget(Widget* w)
    {
        if(w->name.find("StackSort")!=0)++destroyedForeign;
        std::vector<Widget*>& list=w->parent->children;list.erase(std::find(list.begin(),list.end(),w));delete w;
    }
};
}
// Same unlink contract as shared WidgetRef: manager clears every weak reference
// before the widget memory is freed. No name lookup or tree walk on get().
class WidgetRef;
static std::vector<WidgetRef*> weakRefs;
static int weakThrowAt=0;
class WidgetRef
{
    MyGUI::Widget* ptr;bool registered;
public:
    WidgetRef():ptr(NULL),registered(false){}
    MyGUI::Widget* get(){return ptr;}
    void reset(){ptr=NULL;}
    void set(MyGUI::Widget* w){if(weakThrowAt&&!--weakThrowAt)throw std::runtime_error("unlink registration");if(w&&!registered){weakRefs.push_back(this);registered=true;}ptr=w;}
};
static void UnlinkModelWidget(void* w){for(size_t i=0;i<weakRefs.size();++i)if(weakRefs[i]->get()==w)weakRefs[i]->reset();}
class ToolTip
{
public:
    MyGUI::Widget* bound;std::string text;int cleared;
    ToolTip():bound(NULL),cleared(0){}
    void setup(MyGUI::Widget* w,const std::string& s){bound=w;text=s;}
    void clear(MyGUI::Widget* w){if(bound==w){bound=NULL;++cleared;}}
};
class ForgottenGUI {public:ToolTip tip;ToolTip* getToolTip(){return &tip;} };
static ForgottenGUI forgotten;static ForgottenGUI* gui=&forgotten;
class InventoryLayout {public:MyGUI::Widget* arrange;InventoryLayout():arrange(NULL){}MyGUI::Widget* getWidget(const char*){return arrange;} };
template<class T>class lektor:public std::vector<T>{};
namespace Ogre {template<class K,class V>struct map {typedef std::map<K,V> type;};}
struct InventorySection {std::string name;bool eligible;InventorySection():name("backpack"),eligible(true){} };
static bool StackSort_EligibleSection(const InventorySection* s){return s&&s->eligible;}
class Inventory {public:lektor<InventorySection*> sectionsInSearchOrder;};
class InventorySectionGUI {public:MyGUI::Widget* widget;};
class InventoryGUI {public:InventoryGUI* childInventory;InventoryLayout* layoutMgr;Inventory nativeInventory;Ogre::map<std::string,InventorySectionGUI*>::type inventorySections;
 InventoryGUI():childInventory(NULL),layoutMgr(NULL){}Inventory* _NV_getInventory(){return &nativeInventory;} };
struct TargetKey {};
struct UIState
{
    InventoryGUI* gui;InventoryGUI* ownerGUI;InventoryLayout* layout;
    MyGUI::Button* backButton;MyGUI::Button* sortButton;MyGUI::Button* dimButton;MyGUI::Button* forwardButton;
    bool initialized,nativeFallback,restoreVanilla,hasMemKey;int generation,captionMode,parentWidth,parentHeight,currentTarget;
    bool updateStamped,recallPending,captionDirty;DWORD lastSeenTick,lastSeenPoll;
    MyGUI::Widget* controlsParent;TargetKey memKey;std::string ownerLabel;const char* slotName;
    UIState():gui(NULL),ownerGUI(NULL),layout(NULL),backButton(NULL),sortButton(NULL),dimButton(NULL),forwardButton(NULL),initialized(false),nativeFallback(false),restoreVanilla(false),hasMemKey(false),generation(1),captionMode(0),parentWidth(0),parentHeight(0),currentTarget(0),updateStamped(false),recallPending(false),captionDirty(false),lastSeenTick(0),lastSeenPoll(0),controlsParent(NULL),slotName(NULL){}
};
static const int MAX_GUIS=1;static UIState s_uiStates[MAX_GUIS];
static const char* const BUTTON_NAMES[4]={"StackSortBack","StackSortSort","StackSortDim","StackSortForward"};
static const char* const ARRANGE_VISIBILITY_KEY="StackSort_OriginalArrangeVisible_v1";
struct ButtonTag {int idx,generation;};
struct StackSortSettings {int buttonGap,arrowButtonWidth,sortButtonWidth,dimButtonWidth,bodyXOffset;StackSortSettings():buttonGap(2),arrowButtonWidth(30),sortButtonWidth(180),dimButtonWidth(40),bodyXOffset(-40){} };
namespace StackSortSettingsStore {const StackSortSettings& Get(){static StackSortSettings settings;return settings;} }
namespace Packer {static const int MAX_GRID_DIM=126;}
enum {TR_BACK_ARROW,TR_FORWARD_ARROW,TR_DIM_H,TR_DIM_W,TR_SORT_TOOLTIP};
static const char* Tr(int key){const char* text[]={"<",">","H","W","LOCALIZED Sort tooltip"};return text[key];}
static std::string MakeCaption(int target,int mode=0){char n[20];sprintf_s(n,"[%d]",target);return std::string(mode==0?"FULL":mode==1?"COMPACT":"")+n;}
static bool MakeKey(InventoryGUI*,InventoryGUI*,TargetKey&,std::string&){return false;}
static int logs=0,errors=0;
static void LogInfo(const char*){++logs;}static void ReportUIException(const char*){++errors;}
static void LogVerbose(const std::string&){}static std::string IntToStr(int i){char s[20];sprintf_s(s,"%d",i);return s;}static void ReclaimStaleUIStates(DWORD){}
static void ResetSlot(UIState& state){state=UIState();}
static bool IsOurButtonName(const std::string& name){for(int i=0;i<4;++i)if(name==BUTTON_NAMES[i])return true;return false;}
static bool IsSlotButton(MyGUI::Widget* w){const UIState& s=s_uiStates[0];return s.initialized&&(w==s.backButton||w==s.sortButton||w==s.dimButton||w==s.forwardButton);}
#include "ui_remove_under_test.h"
static void TeardownWidgets(InventoryGUI* live,bool restore){if(!live||!live->layoutMgr)return;MyGUI::Widget* arrange=live->layoutMgr->arrange;RemoveOurButtons(arrange->getParent());arrange->setVisible(restore);arrange->clearUserString(ARRANGE_VISIBILITY_KEY);}
class StackSortUI
{
public:
    static int FindUIState(InventoryGUI* g){UIState& s=s_uiStates[0];return(s.initialized||s.nativeFallback)&&s.gui==g?0:-1;}
    static int AllocUIState(InventoryGUI* g){UIState& s=s_uiStates[0];if(s.initialized||s.nativeFallback)return -1;ResetSlot(s);s.gui=g;return 0;}
    static void ReKeyAndRecall(int,bool){}
    static void UpdateCaption(int){UIState& s=s_uiStates[0];s.sortButton->setCaption(MakeCaption(s.currentTarget,s.captionMode));s.dimButton->setCaption("H");}
    static void OnBackClick(MyGUI::Widget*){}static void OnSortCycleClick(MyGUI::Widget*){}static void OnDimClick(MyGUI::Widget*){}static void OnForwardArrowClick(MyGUI::Widget*){}
    static void OnSortRightClick(MyGUI::Widget*,int,int,MyGUI::MouseButton){}
    static void InitializeUI(InventoryGUI*,MyGUI::Widget*,InventoryGUI*);
    static void OnClose(InventoryGUI*);
    static void MaintainSlots(InventoryGUI*);
};
#include "ui_init_under_test.h"
#include "ui_close_under_test.h"
#include "ui_maintain_under_test.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL init %d: %s\n",__LINE__,#x);}}while(0)
struct Fixture
{
    MyGUI::Widget parent;MyGUI::Button arrange,keo;InventoryLayout layout;InventoryGUI inventory;
    MyGUI::Widget grid;InventorySection section;InventorySectionGUI sectionGUI;
    Fixture(int width,int boxLeft,bool shown=true)
    {
        ResetSlot(s_uiStates[0]);MyGUI::created=MyGUI::throwAt=0;forgotten.tip=ToolTip();gui=&forgotten;
        parent.coord=MyGUI::IntCoord(0,0,width,100);arrange.parent=keo.parent=&parent;
        arrange.name="native_ArrangeButton";arrange.coord=MyGUI::IntCoord(width/3,60,width/3,24);arrange.visible=shown;
        arrange.eventMouseButtonClick.count=1;keo.name="native_keo_backpackFirst";keo.coord=MyGUI::IntCoord(boxLeft,60,24,24);
        parent.children.push_back(&arrange);parent.children.push_back(&keo);layout.arrange=&arrange;inventory.layoutMgr=&layout;
    }
    ~Fixture(){StackSortUI::OnClose(&inventory);}
    void AddGrid(int left,int width)
    {grid.parent=&parent;grid.coord=MyGUI::IntCoord(left,0,width,50);sectionGUI.widget=&grid;
     inventory.nativeInventory.sectionsInSearchOrder.push_back(&section);inventory.inventorySections[section.name]=&sectionGUI;}
    void CheckAnchor(){CHECK(arrange.name=="native_ArrangeButton");CHECK(arrange.coord.left==parent.coord.width/3&&arrange.coord.width==parent.coord.width/3);CHECK(arrange.eventMouseButtonClick.count==1);CHECK(keo.visible&&keo.name=="native_keo_backpackFirst");CHECK(MyGUI::destroyedForeign==0);}
};
int main()
{
    const int localePixels[]={136,89,85}; // measured RU/EN/ZH [126], see check_caption.py
    for(int lang=0;lang<3;++lang)for(int selected=0;selected<2;++selected){Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     MyGUI::fullCaptionPixels=localePixels[lang];
     f.AddGrid(20,350);f.parent.coord.left=1000;f.parent.coord.top=200;
     f.arrange.coord=MyGUI::IntCoord(61,60,206,24);f.keo.selected=selected!=0;f.keo.strings["KEO_owner"]="123";f.keo.eventMouseButtonClick.count=1;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);UIState& s=s_uiStates[0];CHECK(s.initialized&&s.captionMode==0);
     CHECK(abs((s.backButton->coord.left+f.keo.coord.left+f.keo.coord.width)-390)<=1); // Content center, not off-center Arrange164
     CHECK(f.keo.coord.left==s.forwardButton->coord.left+s.forwardButton->coord.width+2);
     CHECK(f.keo.selected==(selected!=0)&&f.keo.strings["KEO_owner"]=="123"&&f.keo.eventMouseButtonClick.count==1);
     StackSortUI::OnClose(&f.inventory);CHECK(f.keo.coord.left==271&&f.keo.coord.top==60);
     CHECK(f.keo.selected==(selected!=0)&&f.keo.strings["KEO_owner"]=="123"&&f.keo.eventMouseButtonClick.count==1);}
    MyGUI::fullCaptionPixels=136;
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;f.arrange.coord=MyGUI::IntCoord(61,60,206,24);f.AddGrid(80,290);
     ControlStrip::Rect grid=BackpackGridAnchor(&f.inventory,&f.parent,f.arrange.coord);CHECK(grid.x==80&&grid.w==290);
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);UIState& s=s_uiStates[0];
     CHECK(abs(s.backButton->coord.left+f.keo.coord.left+f.keo.coord.width-450)<=1);CHECK(f.arrange.coord.left==61&&f.arrange.coord.width==206);}
    {Fixture f(390,271);f.AddGrid(80,290);f.section.eligible=false;
     ControlStrip::Rect grid=BackpackGridAnchor(&f.inventory,&f.parent,f.arrange.coord);CHECK(grid.x==0&&grid.w==390);
     f.section.eligible=true;f.grid.visible=false;grid=BackpackGridAnchor(&f.inventory,&f.parent,f.arrange.coord);CHECK(grid.x==0&&grid.w==390);
     f.grid.visible=true;for(int i=0;i<65;++i)f.inventory.nativeInventory.sectionsInSearchOrder.push_back(&f.section);
     grid=BackpackGridAnchor(&f.inventory,&f.parent,f.arrange.coord);CHECK(grid.x==0&&grid.w==390);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);int moved=f.keo.coord.left;
     f.keo.coord.left+=17;f.keo.coord.top+=3;f.keo.coord.width=27; // foreign move/resize
     StackSortUI::OnClose(&owner);CHECK(f.keo.coord.left==288&&f.keo.coord.top==63&&f.keo.coord.width==27);CHECK(moved!=271);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);ResetSlot(s_uiStates[0]); // world-reset/lost slot
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);StackSortUI::OnClose(&owner);
     CHECK(f.keo.coord.left==271&&f.arrange.visible&&f.parent.children.size()==2);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);ResetSlot(s_uiStates[0]);
     StackSortUI::OnClose(&owner);CHECK(f.keo.coord.left==271);CHECK(f.parent.children.size()==2&&f.arrange.visible);}
    {Fixture f(390,271,false);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);ResetSlot(s_uiStates[0]);
     StackSortUI::OnClose(&owner);CHECK(f.keo.coord.left==271&&!f.arrange.visible&&f.parent.children.size()==2);
     CHECK(f.arrange.getUserString(ARRANGE_VISIBILITY_KEY).empty()&&forgotten.tip.bound==NULL);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::OnClose(&owner);CHECK(f.parent.children.size()==2&&f.arrange.visible&&f.keo.coord.left==271);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;MyGUI::Widget other;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);int moved=f.keo.coord.left;
     f.keo.parent=&other;StackSortUI::OnClose(&owner);CHECK(f.keo.coord.left==moved);CHECK(s_backpackBoxLeases[0].widget.get()==NULL);f.keo.parent=&f.parent;}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);UnlinkModelWidget(&f.keo); // foreign destruction callback
     CHECK(s_backpackBoxLeases[0].widget.get()==NULL);StackSortUI::OnClose(&owner);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);
     f.parent.coord.width+=40;f.arrange.coord.left+=40;f.keo.coord.left+=40; // Right alignment
     StackSortUI::MaintainSlots(&owner);CHECK(f.keo.coord.left==311&&f.arrange.visible);
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(s_uiStates[0].initialized);
     StackSortUI::OnClose(&owner);CHECK(f.keo.coord.left==311);}
    for(int fail=1;fail<=4;++fail){Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;MyGUI::throwAt=fail;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(s_uiStates[0].nativeFallback&&f.keo.coord.left==271&&f.arrange.visible);}
    for(int fail=1;fail<=2;++fail){Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;weakThrowAt=fail;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(s_uiStates[0].nativeFallback&&f.keo.coord.left==271&&f.arrange.visible);CHECK(s_backpackBoxLeases[0].widget.get()==NULL);}
    for(int fail=1;fail<=2;++fail){Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;MyGUI::throwPosition=fail;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(s_uiStates[0].nativeFallback&&f.keo.coord.left==271&&f.arrange.visible);CHECK(s_backpackBoxLeases[0].widget.get()==NULL);}
    {Fixture f(80,54);InventoryGUI owner;owner.childInventory=&f.inventory;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(s_uiStates[0].nativeFallback&&f.keo.coord.left==54);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;f.keo.visible=false;
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);UIState& s=s_uiStates[0];
     CHECK(s.initialized&&abs(s.backButton->coord.left+s.forwardButton->coord.left+s.forwardButton->coord.width-f.parent.coord.width)<=1);
     CHECK(f.keo.coord.left==271&&!f.keo.visible);}
    {Fixture f(390,271);InventoryGUI owner;owner.childInventory=&f.inventory;f.keo.name="other_keo_backpackFirst";
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&owner);CHECK(f.keo.coord.left==271);}
    {Fixture f(650,424);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].initialized&&!f.arrange.visible);CHECK(f.parent.children.size()==6);f.CheckAnchor();
     UIState& s=s_uiStates[0];CHECK(s.backButton->eventMouseButtonClick.count==1&&s.sortButton->eventMouseButtonClick.count==1&&s.dimButton->eventMouseButtonClick.count==1&&s.forwardButton->eventMouseButtonClick.count==1);CHECK(s.sortButton->eventMouseButtonPressed.count==1);CHECK(forgotten.tip.bound==s.sortButton);
     StackSortUI::OnClose(&f.inventory);CHECK(f.arrange.visible&&f.parent.children.size()==2);CHECK(forgotten.tip.bound==NULL&&forgotten.tip.cleared==1);f.CheckAnchor();}
    {Fixture f(260,198);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].initialized&&s_uiStates[0].captionMode==1);f.CheckAnchor();}
    {Fixture f(180,140);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].initialized&&s_uiStates[0].captionMode==2);CHECK(forgotten.tip.bound!=NULL&&!forgotten.tip.text.empty());f.CheckAnchor();}
    {Fixture f(80,54);int priorLogs=logs;StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].nativeFallback&&f.arrange.visible);CHECK(f.parent.children.size()==2);int allocated=MyGUI::created;StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(MyGUI::created==allocated&&logs==priorLogs+1);f.CheckAnchor();}
    {Fixture f(650,424,false);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);StackSortUI::OnClose(&f.inventory);CHECK(!f.arrange.visible);f.CheckAnchor();}
    {Fixture f(650,424,false);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);ResetSlot(s_uiStates[0]);
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);StackSortUI::OnClose(&f.inventory);
     CHECK(!f.arrange.visible);CHECK(f.parent.children.size()==2);f.CheckAnchor();}
    for(int fail=1;fail<=4;++fail){Fixture f(650,424);MyGUI::throwAt=fail;StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].nativeFallback&&f.arrange.visible);CHECK(f.parent.children.size()==2);CHECK(forgotten.tip.bound==NULL);f.CheckAnchor();}
    {Fixture f(650,424);f.parent.createWidget<MyGUI::Button>("",MyGUI::IntCoord(0,60,30,24),0,BUTTON_NAMES[0]);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].initialized&&f.parent.children.size()==6);f.CheckAnchor();}
    {Fixture f(180,140);gui=NULL;StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].nativeFallback&&f.arrange.visible);CHECK(f.parent.children.size()==2);gui=&forgotten;}
    {Fixture f(80,54);StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);int allocated=MyGUI::created;
     StackSortUI::MaintainSlots(&f.inventory);CHECK(MyGUI::created==allocated&&s_uiStates[0].nativeFallback);
     f.parent.coord.width=650;f.arrange.coord.left=f.arrange.coord.width=650/3;f.keo.coord.left=424;
     StackSortUI::MaintainSlots(&f.inventory);CHECK(StackSortUI::FindUIState(&f.inventory)==-1);
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].initialized&&!f.arrange.visible);f.CheckAnchor();
     allocated=MyGUI::created;StackSortUI::MaintainSlots(&f.inventory);CHECK(MyGUI::created==allocated);
     f.parent.coord.width=80;f.arrange.coord.left=f.arrange.coord.width=80/3;f.keo.coord.left=54;
     StackSortUI::MaintainSlots(&f.inventory);CHECK(f.arrange.visible&&f.parent.children.size()==2&&forgotten.tip.bound==NULL);
     StackSortUI::InitializeUI(&f.inventory,&f.arrange,&f.inventory);CHECK(s_uiStates[0].nativeFallback&&f.arrange.visible);f.CheckAnchor();}
    std::printf("%d production UI-init checks, %d failures\n",checks,failures);return failures?1:0;
}
