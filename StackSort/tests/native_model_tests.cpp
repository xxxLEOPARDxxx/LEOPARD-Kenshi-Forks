#include <vector>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include "MergeDriver.h"
struct Item;
struct hand
{
    int id;
    hand(int i=0):id(i){}
    bool operator==(const hand& h) const { return id==h.id; }
    Item* getItem() const;
};
template<class T> struct FakeVector { typedef std::vector<T> type; };
namespace Ogre { using ::FakeVector; template<class T> struct vector : FakeVector<T> {}; }
struct InventorySection;
struct Item
{
    hand handle;
    int quantity, maxStack, kind, quality, material, maker, owner;
    bool isEquipped, alive;
    std::string inventorySection;
    float originalFullChargeAmount, chargesLeft;
    Item(int id,int q):handle(id),quantity(q),maxStack(10),kind(1),quality(1),material(1),maker(1),owner(1),isEquipped(false),alive(true),inventorySection("main"),originalFullChargeAmount(1),chargesLeft(1){}
    int isStackable(InventorySection*) const { return maxStack; }
    // Charges left are deliberately absent, matching native isSameAs.
    bool canStackWith(Item* b) const { return kind==b->kind && quality==b->quality && material==b->material && maker==b->maker && owner==b->owner; }
};
static std::vector<Item*> handles;
Item* hand::getItem() const { for(size_t i=0;i<handles.size();++i) if(handles[i]->handle.id==id && handles[i]->alive) return handles[i]; return NULL; }
static int errors=0;
void ErrorLog(const char*) { ++errors; }
struct InventorySection
{
    struct SectionItem { Item* item; int x,y,w,h; };
    std::vector<SectionItem> items;
    int fault, addCalls, lastAmount;
    bool refuseRemove;
    int width,height,itemsLimit;
    bool isAnEquippedItemSection,enabled;
    struct Content { int nRows,nCols; } content;
    std::vector<int> veryLimitedSlot;
    InventorySection():fault(0),addCalls(0),lastAmount(0),refuseRemove(false),width(6),height(6),itemsLimit(0),isAnEquippedItemSection(false),enabled(true){content.nRows=6;content.nCols=6;}
    const std::vector<SectionItem>& getItems() const { return items; }
    bool hasItem(Item* item) const { for(size_t i=0;i<items.size();++i) if(items[i].item==item) return true; return false; }
    bool canItemGoHere(Item*,int x,int) const { for(size_t i=0;i<items.size();++i) if(items[i].x==x) return false; return true; }
    void _NV__addItem(Item* item,int x,int y) { SectionItem si={item,x,y,1,1};items.push_back(si);item->inventorySection="main"; }
    bool removeItem(Item* item)
    {
        if(refuseRemove) return false;
        for(size_t i=0;i<items.size();++i) if(items[i].item==item) { items.erase(items.begin()+i);item->inventorySection.clear();return true; }
        return false;
    }
    bool _NV_addItem(Item* source,int amount)
    {
        ++addCalls;lastAmount=amount;
        if(fault==1) return false;
        int remaining=source->quantity+amount-1; // audited native convention
        source->quantity=1;
        if(fault==4) throw std::runtime_error("normalized before allocation");
        for(size_t i=0;i<items.size();++i)
        {
            Item* target=items[i].item;
            if(!target->canStackWith(source)) continue;
            // Steam addQuantity 75D769-75D7FC: combine the open charges,
            // then the remaining full items. Exact-full uses >, not >=.
            const int extra=remaining-1;
            target->chargesLeft+=source->chargesLeft;
            if(target->chargesLeft>target->originalFullChargeAmount)
            {
                ++target->quantity;
                target->chargesLeft-=target->originalFullChargeAmount;
            }
            source->chargesLeft=0;
            target->quantity+=extra;
            remaining=0;
            if(target->quantity>target->maxStack)
            {
                source->chargesLeft=target->chargesLeft;
                target->chargesLeft=target->originalFullChargeAmount;
                remaining=target->quantity-target->maxStack;
                target->quantity=target->maxStack;
            }
            if(fault==5) throw std::runtime_error("credited before source update");
            source->quantity=remaining;
            if(remaining==0) { source->alive=false; if(fault==3) throw std::runtime_error("consumed");return true; }
            if(fault==2) return false; // partial native transfer, detached remainder
        }
        source->quantity=remaining;
        for(int x=0;x<20;++x) if(canItemGoHere(source,x,0)) { _NV__addItem(source,x,0);return true; }
        return false;
    }
    int Total() const { int n=0;for(size_t i=0;i<items.size();++i) n+=items[i].item->quantity;return n; }
};
#include "NativeMergeAdapter.h" // the production adapter, unchanged
#include "SectionEligibility.h" // the production native policy, unchanged
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL native %d: %s\n",__LINE__,#x);}}while(0)
static void Bind(InventorySection& s,Item& a,Item& b)
{
    handles.clear();handles.push_back(&a);handles.push_back(&b);s._NV__addItem(&a,0,0);s._NV__addItem(&b,1,0);
}
int main()
{
    { InventorySection s;Item a(1,3),b(2,4);Bind(s,a,b);
      CHECK(StackSort_EligibleSection(&s));s.width=1;s.height=1;CHECK(!StackSort_EligibleSection(&s));
      s.width=6;s.height=6;s.itemsLimit=1;CHECK(!StackSort_EligibleSection(&s));s.itemsLimit=0;
      s.content.nRows=1;s.content.nCols=1;CHECK(!StackSort_EligibleSection(&s));s.content.nRows=6;s.content.nCols=6;
      s.veryLimitedSlot.push_back(1);s.items.resize(1);s.items[0].w=6;s.items[0].h=6;CHECK(!StackSort_EligibleSection(&s));
      s.items[0].w=1;s.items[0].h=2;CHECK(StackSort_EligibleSection(&s));
    }
    { InventorySection s;Item a(1,3),b(2,4);Bind(s,a,b);NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(out.changed&&!out.fault);CHECK(s.Total()==7);CHECK(s.items.size()==1);CHECK(s.lastAmount==1);CHECK(s.addCalls==1); }
    for(int incompatible=0;incompatible<6;++incompatible)
    { InventorySection s;Item a(1,3),b(2,4);Bind(s,a,b);int* fields[]={&b.kind,&b.quality,&b.material,&b.maker,&b.owner};if(incompatible<5) *fields[incompatible]=2;else b.isEquipped=true;NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(!out.changed&&!out.fault);CHECK(s.Total()==7);CHECK(s.addCalls==0); }
    { InventorySection s;Item a(1,8),b(2,6);Bind(s,a,b);NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(out.changed&&!out.fault);CHECK(s.Total()==14);CHECK(s.items.size()==2);CHECK(a.quantity<=10&&b.quantity<=10); }
    { InventorySection s;Item a(1,3),b(2,4);Bind(s,a,b);s.refuseRemove=true;NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(!out.changed&&!out.fault);CHECK(s.Total()==7);CHECK(s.addCalls==0); }
    for(int fault=1;fault<=5;++fault)
    { InventorySection s;Item a(1,(fault==2||fault==5)?8:3),b(2,(fault==2||fault==5)?6:4);Bind(s,a,b);s.fault=fault;int total=s.Total();NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(out.fault);CHECK(s.Total()==total);CHECK(s.addCalls==1);for(size_t i=0;i<s.items.size();++i) CHECK(s.items[i].item->alive&&s.items[i].item->quantity>0); }
    // Two half-used kits at their physical stack limit become one full kit.
    // Physical quantity decreases legally; charge units must be conserved.
    { InventorySection s;Item a(1,1),b(2,1);Bind(s,a,b);a.maxStack=b.maxStack=1;
      a.originalFullChargeAmount=b.originalFullChargeAmount=100;a.chargesLeft=b.chargesLeft=50;
      NativeMergeAdapter adapter(&s);CHECK(a.canStackWith(&b));CHECK(adapter.HasMergeTarget(a.handle));
      StackMerge::Outcome out=StackMerge::Consolidate(adapter);CHECK(out.changed&&!out.fault);
      CHECK(s.items.size()==1&&s.Total()==1);CHECK(s.items[0].item->chargesLeft==100);
      CHECK(s.addCalls==1);CHECK(NativeMergeAdapter::Units(s.items[0].item,100)==100);
    }
    // Different partial charges are compatible too, but fully charged kits
    // at their physical limit must not be detached and reinserted fruitlessly.
    for(int full=0;full<=1;++full)
    { InventorySection s;Item a(1,1),b(2,1);Bind(s,a,b);a.maxStack=b.maxStack=1;
      a.originalFullChargeAmount=b.originalFullChargeAmount=100;a.chargesLeft=full?100:50;b.chargesLeft=full?100:75;
      NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);
      CHECK(out.changed==!full&&!out.fault);CHECK(s.Total()==2);CHECK(s.items.size()==2);
      CHECK(NativeMergeAdapter::Units(&a,100)+NativeMergeAdapter::Units(&b,100)==(full?200:125));
      if(full) CHECK(s.addCalls==0);else CHECK((a.chargesLeft==100&&b.chargesLeft==25)||(a.chargesLeft==25&&b.chargesLeft==100));
    }
    // All five audited fault phases, using native charge arithmetic through
    // the entire production driver and adapter, including consumed-source.
    for(int fault=1;fault<=5;++fault)
    { InventorySection s;Item a(1,1),b(2,1);Bind(s,a,b);a.maxStack=b.maxStack=1;
      a.originalFullChargeAmount=b.originalFullChargeAmount=100;a.chargesLeft=50;b.chargesLeft=fault==3?50:75;
      const double expected=fault==3?100:125;s.fault=fault;
      NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);
      CHECK(out.fault);CHECK(s.addCalls==1);double units=0;
      for(size_t i=0;i<s.items.size();++i){CHECK(s.items[i].item->alive&&s.items[i].item->quantity>0);units+=NativeMergeAdapter::Units(s.items[i].item,100);}
      CHECK(units==expected);
    }
    // A charged source with full items behind its open charge must also be
    // reconstructed after native normalizes its count before an allocation.
    for(int fault=2;fault<=5;++fault)
    { if(fault==3) continue;InventorySection s;Item a(1,2),b(2,2);Bind(s,a,b);a.maxStack=b.maxStack=2;
      a.originalFullChargeAmount=b.originalFullChargeAmount=100;a.chargesLeft=50;b.chargesLeft=75;s.fault=fault;
      NativeMergeAdapter adapter(&s);StackMerge::Outcome out=StackMerge::Consolidate(adapter);
      CHECK(out.fault);CHECK(s.items.size()==2);CHECK(s.addCalls==1);
      CHECK(NativeMergeAdapter::Units(&a,100)+NativeMergeAdapter::Units(&b,100)==325);
      CHECK(a.quantity>0&&a.quantity<=2&&b.quantity>0&&b.quantity<=2);
    }
    { InventorySection s;Item a(1,3),b(2,1);Bind(s,a,b);a.originalFullChargeAmount=b.originalFullChargeAmount=100;
      a.chargesLeft=50;b.chargesLeft=60;NativeMergeAdapter adapter(&s);std::vector<NativeMergeAdapter::Credit> credits(1);
      credits[0].key=b.handle;credits[0].units=60;b.quantity=2;b.chargesLeft=30;a.quantity=1;a.chargesLeft=0;
      CHECK(adapter.RestoreUncredited(&a,250,100,credits));CHECK(a.quantity==2&&a.chargesLeft==80);
      CHECK(NativeMergeAdapter::Units(&a,100)+NativeMergeAdapter::Units(&b,100)==310);
      b.quantity=5;CHECK(!adapter.RestoreUncredited(&a,250,100,credits));CHECK(a.quantity==2&&a.chargesLeft==80);
    }
    std::printf("%d native model checks, %d failures\n",checks,failures);return failures?1:0;
}
