// Actual FirstClickInit plus the real NativeMergeAdapter/driver. Reuse the
// native fault model; its standalone test main is not executed here.
#define main NativeModelStandaloneMain
#include "native_model_tests.cpp"
#undef main

template<class T> struct lektor : std::vector<T> {};
class Inventory {public:lektor<InventorySection*> sections;lektor<InventorySection*>& getAllSections(){return sections;}};
class InventoryGUI {public:Inventory* inventory;InventoryGUI(Inventory* i=0):inventory(i){}Inventory* _NV_getInventory(){return inventory;}};
static bool StackSort_RotateAvailable(){return true;}static bool StackSort_IsRotated(Item* i){return i->rotated;}
#include "NativeMergeContentStamp.h"
struct UIState {InventoryGUI* gui;InventoryGUI* ownerGUI;bool stacksConsolidated;MergeContent::Value mergeStamp;UIState():gui(0),ownerGUI(0),stacksConsolidated(false){}};
static UIState s_uiStates[1];
static bool mergeEnabled=true;static int mergeCalls=0,refreshes=0,mutations=0,captures=0,binds=0;
static void StackSort_InitRotateAPI(){++binds;}
namespace StackSortMcm {static bool MergeStacks(){return mergeEnabled;}}
class SortWorker {public:static void OnMutation(InventoryGUI*){++mutations;}};
static void LogInfo(const char*){}static void LogVerbose(const char*){}
class StackSortUI {public:static void FirstClickInit(int);static bool ConsolidateStacks(InventorySection* s){++mergeCalls;NativeMergeAdapter native(s);StackMerge::Outcome o=StackMerge::Consolidate(native);if(o.fault)throw std::runtime_error("native fault");return o.changed;}static void ForceRefreshAllSections(InventoryGUI*){++refreshes;}static void CaptureOriginalPositions(InventoryGUI*){++captures;}};
#include "ui_merge_under_test.h"
int main(){
 InventorySection section;Inventory inv;inv.sections.push_back(&section);InventoryGUI gui(&inv);s_uiStates[0].gui=s_uiStates[0].ownerGUI=&gui;
 Item a(1,1),b(2,1);a.nativeRules=b.nativeRules=true;a.maxStack=b.maxStack=1;a.originalFullChargeAmount=b.originalFullChargeAmount=100;a.chargesLeft=60;b.chargesLeft=40;Bind(section,a,b);
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==1&&section.items.size()==1);CHECK(mutations==1&&refreshes==1&&captures==1);
 // Quiet healing/repair consumption and a manually split compatible kit,
 // while the same inventory window remains open.
 Item c(3,1);c.nativeRules=true;c.maxStack=1;c.originalFullChargeAmount=100;c.chargesLeft=30;handles.push_back(&c);section.items[0].item->chargesLeft=70;section._NV__addItem(&c,2,0);
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==2&&section.items.size()==1);CHECK(section.items[0].item->chargesLeft==100);
 // An unchanged repeat and native packing that only reorders/relocates
 // items do not run the quadratic merge driver again.
 int calls=mergeCalls,adds=section.addCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls&&section.addCalls==adds);
 for(size_t i=0;i<section.items.size();++i){section.items[i].x+=3;section.items[i].y+=2;}std::reverse(section.items.begin(),section.items.end());
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls);
 // Quiet charge consumption alone creates capacity in two existing stacks.
 InventorySection quiet;Inventory quietInv;quietInv.sections.push_back(&quiet);gui.inventory=&quietInv;
 Item q1(10,1),q2(11,1);q1.nativeRules=q2.nativeRules=true;q1.maxStack=q2.maxStack=1;q1.originalFullChargeAmount=q2.originalFullChargeAmount=100;q1.chargesLeft=q2.chargesLeft=100;Bind(quiet,q1,q2);
 StackSortUI::FirstClickInit(0);calls=mergeCalls;CHECK(quiet.items.size()==2);
 q1.chargesLeft=60;q2.chargesLeft=40;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&quiet.items.size()==1&&quiet.items[0].item->chargesLeft==100);
 // Ammunition uses actual native data/material/manufacturer/charges. Raw
 // Item.quality is deliberately irrelevant to native base isSameAs.
 InventorySection arrows;Inventory arrowsInv;arrowsInv.sections.push_back(&arrows);gui.inventory=&arrowsInv;
 Item ar1(20,1),ar2(21,1);ar1.nativeRules=ar2.nativeRules=true;ar1.kind=ar2.kind=2;ar1.quality=7;ar2.quality=99;ar1.originalFullChargeAmount=ar2.originalFullChargeAmount=20;ar1.chargesLeft=8;ar2.chargesLeft=12;Bind(arrows,ar1,ar2);
 StackSortUI::FirstClickInit(0);CHECK(arrows.items.size()==1&&arrows.items[0].item->chargesLeft==20);
 Item ar3(22,1);ar3.nativeRules=true;ar3.kind=2;ar3.quality=38;ar3.originalFullChargeAmount=20;ar3.chargesLeft=13;handles.push_back(&ar3);arrows.items[0].item->chargesLeft=7;arrows._NV__addItem(&ar3,3,0);
 calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&arrows.items.size()==1&&arrows.items[0].item->chargesLeft==20);
 arrows.items[0].item->quality=3;calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls);
 // A new item stays split if the actual Native predicate rejects it.
 Item stolen(23,1);stolen.nativeRules=true;stolen.kind=2;stolen.stolen=true;stolen.owner=2;stolen.originalFullChargeAmount=20;stolen.chargesLeft=5;handles.push_back(&stolen);arrows._NV__addItem(&stolen,4,0);
 arrows.items[0].item->chargesLeft=15;StackSortUI::FirstClickInit(0);CHECK(arrows.items.size()==2&&stolen.chargesLeft==5);
 calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls);
 // Quiet ownership compatibility changes are inspected on the next click.
 stolen.stolen=false;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&arrows.items.size()==1&&arrows.items[0].item->chargesLeft==20);
 // Relative rotation, not a global partition flip, changes compatibility.
 InventorySection rotations;Inventory rotationInv;rotationInv.sections.push_back(&rotations);gui.inventory=&rotationInv;
 Item r1(30,1),r2(31,1);r1.nativeRules=r2.nativeRules=true;r1.maxStack=r2.maxStack=1;r1.originalFullChargeAmount=r2.originalFullChargeAmount=100;r1.chargesLeft=r2.chargesLeft=100;r2.rotated=true;r2.itemWidth=2;r2.itemHeight=1;Bind(rotations,r1,r2);
 StackSortUI::FirstClickInit(0);calls=mergeCalls;
 r1.rotated=true;r1.itemWidth=2;r1.itemHeight=1;r2.rotated=false;r2.itemWidth=1;r2.itemHeight=2;
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls); // same native pair compatibility after own pack/Rotate
 r2.rotated=true;r2.itemWidth=2;r2.itemHeight=1;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1);calls=mergeCalls;
 for(int i=0;i<1000;++i){std::reverse(rotations.items.begin(),rotations.items.end());StackSortUI::FirstClickInit(0);}CHECK(mergeCalls==calls);
 // MCM can enable merging after sorting once with it disabled.
 mergeEnabled=false;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls);
 mergeEnabled=true;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1);
 // Existing undo invalidates the merge-success flag; next sort is allowed to
 // inspect again, while the capture still occurs only after native merging.
 s_uiStates[0].stacksConsolidated=false;calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1);
 // An unreadable semantic input stops before native mutation/capture; repair
 // then retries rather than keeping a successful stamp for the fault.
 int captured=captures;calls=mergeCalls;float old=r1.chargesLeft;unsigned int nan=0x7fc00000;std::memcpy(&r1.chargesLeft,&nan,4);bool threw=false;
 try{StackSortUI::FirstClickInit(0);}catch(...){threw=true;}CHECK(threw&&mergeCalls==calls&&captures==captured&&!s_uiStates[0].stacksConsolidated);r1.chargesLeft=old;
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&s_uiStates[0].stacksConsolidated);
 // Bounds fail before consulting live per-item methods or native merging.
 InventorySection excessive;Inventory tooMany;tooMany.sections.push_back(&excessive);excessive.items.resize(MergeContent::Builder::MaxItems+1);gui.inventory=&tooMany;calls=mergeCalls;threw=false;
 try{StackSortUI::FirstClickInit(0);}catch(...){threw=true;}CHECK(threw&&mergeCalls==calls&&!s_uiStates[0].stacksConsolidated);
 // A native partial-credit fault stops this click before capture and leaves
 // the gate uncached. The real adapter conserves units; the next click retries.
 InventorySection broken;Inventory brokenInv;brokenInv.sections.push_back(&broken);gui.inventory=&brokenInv;
 Item f1(40,3),f2(41,4);f1.nativeRules=f2.nativeRules=true;Bind(broken,f1,f2);broken.fault=5;captured=captures;threw=false;
 try{StackSortUI::FirstClickInit(0);}catch(...){threw=true;}CHECK(threw&&!s_uiStates[0].stacksConsolidated&&captures==captured&&broken.Total()==7);
 broken.fault=0;calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&s_uiStates[0].stacksConsolidated&&broken.Total()==7);
 // Full item identity includes serial, even if an allocator reuses a pointer.
 broken.items[0].item->handle.serial++;calls=mergeCalls;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1);
 // Native manufacturer/material changes without callbacks alter grouping.
 InventorySection materials;Inventory matInv;matInv.sections.push_back(&materials);gui.inventory=&matInv;
 Item m1(50,1),m2(51,1);m1.nativeRules=m2.nativeRules=true;m2.materialData=(void*)2;m1.maxStack=m2.maxStack=1;m1.originalFullChargeAmount=m2.originalFullChargeAmount=100;m1.chargesLeft=60;m2.chargesLeft=40;Bind(materials,m1,m2);
 StackSortUI::FirstClickInit(0);CHECK(materials.items.size()==2);calls=mergeCalls;m2.materialData=m1.materialData;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&materials.items.size()==1);
 InventorySection owners;Inventory ownInv;ownInv.sections.push_back(&owners);gui.inventory=&ownInv;
 Item o1(60,1),o2(61,1);o1.nativeRules=o2.nativeRules=o1.stolen=o2.stolen=true;o1.owner=3;o2.owner=4;o1.maxStack=o2.maxStack=1;o1.originalFullChargeAmount=o2.originalFullChargeAmount=100;o1.chargesLeft=60;o2.chargesLeft=40;Bind(owners,o1,o2);
 StackSortUI::FirstClickInit(0);CHECK(owners.items.size()==2);calls=mergeCalls;o2.owner=3;StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&owners.items.size()==1&&owners.items[0].item->chargesLeft==100);
 // At the positive bound, the actual native reader remains order independent
 // and handles square items' relative rotation (dimensions alone cannot).
 InventorySection large;large.width=large.height=64;large.content.nRows=large.content.nCols=64;Inventory largeInv;largeInv.sections.push_back(&large);std::vector<Item> live;live.reserve(MergeContent::Builder::MaxItems);
 for(int i=0;i<MergeContent::Builder::MaxItems;++i){live.push_back(Item(1000+i,1));live.back().nativeRules=true;live.back().itemWidth=live.back().itemHeight=1;large._NV__addItem(&live.back(),i%64,i/64);}
 MergeContent::Value before,after;calls=mergeCalls;CHECK(ReadMergeContentStamp(&largeInv,before)&&before.items==MergeContent::Builder::MaxItems);
 std::reverse(large.items.begin(),large.items.end());for(size_t i=0;i<live.size();++i)live[i].rotated=true;
 CHECK(ReadMergeContentStamp(&largeInv,after)&&before==after&&mergeCalls==calls);
 live[0].rotated=false;CHECK(ReadMergeContentStamp(&largeInv,after)&&before!=after&&mergeCalls==calls);
 // Native does not require equal stolen states. Either stolen -> compare
 // BOTH proper owners. Singleton stolen/clean groups must not hide a relative
 // rotation change across those groups.
 InventorySection mixed;Inventory mixInv;mixInv.sections.push_back(&mixed);gui.inventory=&mixInv;
 Item x1(70,1),x2(71,1);x1.nativeRules=x2.nativeRules=true;x1.owner=x2.owner=7;x2.stolen=true;x2.rotated=true;x1.maxStack=x2.maxStack=1;x1.originalFullChargeAmount=x2.originalFullChargeAmount=100;x1.chargesLeft=60;x2.chargesLeft=40;Bind(mixed,x1,x2);
 StackSortUI::FirstClickInit(0);CHECK(mixed.items.size()==2);calls=mergeCalls;CHECK(ReadMergeContentStamp(&mixInv,before));
 x2.rotated=false;CHECK(ReadMergeContentStamp(&mixInv,after)&&before!=after);
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&mixed.items.size()==1&&mixed.items[0].item->chargesLeft==100);
 // The CLEAN item's owner matters when a stolen candidate exists, even if
 // the clean state itself and all quantities/charges remain unchanged.
 InventorySection quietOwner;Inventory ownerInv;ownerInv.sections.push_back(&quietOwner);gui.inventory=&ownerInv;
 Item y1(80,1),y2(81,1);y1.nativeRules=y2.nativeRules=true;y1.owner=7;y2.owner=8;y2.stolen=true;y1.maxStack=y2.maxStack=1;y1.originalFullChargeAmount=y2.originalFullChargeAmount=100;y1.chargesLeft=60;y2.chargesLeft=40;Bind(quietOwner,y1,y2);
 StackSortUI::FirstClickInit(0);CHECK(quietOwner.items.size()==2);calls=mergeCalls;CHECK(ReadMergeContentStamp(&ownerInv,before));
 y1.owner=8;CHECK(ReadMergeContentStamp(&ownerInv,after)&&before!=after);
 StackSortUI::FirstClickInit(0);CHECK(mergeCalls==calls+1&&quietOwner.items.size()==1&&quietOwner.items[0].item->chargesLeft==100);
 printf("%d actual first-click merge checks, %d failures\n",checks,failures);return failures?1:0;
}
