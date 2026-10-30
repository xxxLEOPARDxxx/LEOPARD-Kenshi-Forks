#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <float.h>
#include <PluginDiagnostics.h>
#include <kenshi/Item.h>
#include <kenshi/Inventory.h>
#include <kenshi/gui/InventoryGUI.h>
#include "CursorState.h"
#include "RotationState.h"
#include "PairDiagnostics.h"

PairTrace::Context s_pairPlaceContext;
static volatile LONG s_uiThread=0;
static PairTrace::Gate s_gate;
void PairTraceRememberUIThread(){InterlockedCompareExchange(&s_uiThread,(LONG)GetCurrentThreadId(),0);}
bool PairTraceOnUIThread()
{return PluginDiagnostics::Enabled() && (DWORD)InterlockedCompareExchange(&s_uiThread,0,0)==GetCurrentThreadId();}
class DiagnosticItemAccess:public Item
{public:using InventoryItemBase::properOwner;using InventoryItemBase::_whosInventoryWeAreIn;};
struct HandBits {unsigned type,container,containerSerial,index,serial;};
static HandBits Bits(const hand& h){HandBits x={h.type,h.container,h.containerSerial,h.index,h.serial};return x;}
struct Snapshot
{
    HandBits key,owner,current;
    void* item;void* data;void* manufacturer;void* material;
    int quantity,width,height;float full,charges,quality;bool trade,equipped,rotated;
};
static Snapshot Read(InventoryItemBase* item)
{
    Snapshot s;std::memset(&s,0,sizeof(s));
    DiagnosticItemAccess* p=(DiagnosticItemAccess*)item;
    s.key=Bits(p->handle);s.owner=Bits(p->properOwner);s.current=Bits(p->_whosInventoryWeAreIn);
    s.item=p;s.data=p->data;s.manufacturer=p->manufacturerData;s.material=p->materialData;
    s.quantity=p->quantity;s.width=p->itemWidth;s.height=p->itemHeight;
    s.full=p->originalFullChargeAmount;s.charges=p->chargesLeft;s.quality=p->quality;
    s.trade=p->isTradeItem;s.equipped=p->isEquipped;s.rotated=RotationState::IsRotated(p);return s;
}
static PairTrace::Key Hash(const void* data,size_t count,PairTrace::Key h=1469598103934665603ULL)
{const unsigned char* b=(const unsigned char*)data;for(size_t i=0;i<count;++i){h^=b[i];h*=1099511628211ULL;}return h;}
static const char* Sid(void* data){return data?((GameData*)data)->stringID.c_str():"unknown";}
static void Format(char* out,size_t size,const Snapshot& s,InventoryItemBase* item)
{
    // These native getters run once per admitted GUI-thread event only.
    const HandBits proper=Bits(item->getProperOwner());const int stolen=item->isStolen(true)?1:0;
    _snprintf_s(out,size,_TRUNCATE,
      "item=%p hand=%u/%u/%u/%u/%u data=%p sid=%.64s maker=%p material=%p qty=%d full=%.7g charges=%.7g quality=%.7g trade=%d equipped=%d dims=%dx%d rotated=%d stolen=%d proper=%u/%u/%u/%u/%u storedProper=%u/%u/%u/%u/%u current=%u/%u/%u/%u/%u section=%.64s",
      s.item,s.key.type,s.key.container,s.key.containerSerial,s.key.index,s.key.serial,s.data,Sid(s.data),s.manufacturer,s.material,
      s.quantity,s.full,s.charges,s.quality,s.trade?1:0,s.equipped?1:0,s.width,s.height,s.rotated?1:0,stolen,
      proper.type,proper.container,proper.containerSerial,proper.index,proper.serial,
      s.owner.type,s.owner.container,s.owner.containerSerial,s.owner.index,s.owner.serial,
      s.current.type,s.current.container,s.current.containerSerial,s.current.index,s.current.serial,item->inventorySection.c_str());
}
void RecordStackPair(InventoryItemBase* a,InventoryItemBase* b,bool nativeResult,bool finalResult,void* caller)
{
    // Pointer equality only: the cursor's cached pointer is never dereferenced.
    if(!PairTraceOnUIThread() || !a || !b) return;
    Item* held=CursorState::GetItem();if(held!=(Item*)a && held!=(Item*)b)return;
    try
    {
        const Snapshot x=Read(a),y=Read(b);
        PairTrace::Key pair=Hash(&x.key,sizeof(x.key));pair=Hash(&y.key,sizeof(y.key),pair);
        PairTrace::Key state=Hash(&x,sizeof(x));state=Hash(&y,sizeof(y),state);
        state=Hash(&nativeResult,sizeof(nativeResult),state);state=Hash(&finalResult,sizeof(finalResult),state);
        if(!s_gate.Admit(pair,state,GetTickCount()))return;
        int capacity=-1;const char* target="unknown";bool full=false,fullKnown=false;
        if(s_pairPlaceContext.gui && s_pairPlaceContext.section)
        {
            InventoryGUI* live=(InventoryGUI*)s_pairPlaceContext.gui;
            Inventory* inventory=live->_NV_getInventory();
            const std::string& name=*(const std::string*)s_pairPlaceContext.section;
            InventorySection* section=inventory?inventory->getSection(name):NULL;
            InventoryItemBase* item=NULL;
            if(section && section->hasItem((Item*)a)){item=a;target="a";}
            else if(section && section->hasItem((Item*)b)){item=b;target="b";}
            if(item)
            {
                capacity=item->isStackable(section);
                if(capacity>0 && _finite(item->originalFullChargeAmount) && _finite(item->chargesLeft))
                {
                    fullKnown=true;
                    full=item->quantity>=capacity && (item->originalFullChargeAmount<=0 || item->chargesLeft>=item->originalFullChargeAmount);
                }
            }
        }
        char module[MAX_PATH]="unknown";HMODULE owner=NULL;size_t rva=0;
        if(caller && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)caller,&owner))
        {
            char path[MAX_PATH]={0};if(GetModuleFileNameA(owner,path,MAX_PATH))
            {path[MAX_PATH-1]=0;const char* slash=strrchr(path,'\\');strncpy_s(module,slash?slash+1:path,_TRUNCATE);}
            rva=(size_t)caller-(size_t)owner;
        }
        char left[1400],right[1400],line[3200];Format(left,sizeof(left),x,a);Format(right,sizeof(right),y,b);
        _snprintf_s(line,sizeof(line),_TRUNCATE,
          "KenshiRotate: STACK_PAIR reason=%s native=%d result=%d rotationMismatch=%d target=%s capacity=%d targetFull=%s caller=%s+0x%Ix A{%s} B{%s}",
          PairTrace::Reason(nativeResult,finalResult,full),nativeResult?1:0,finalResult?1:0,x.rotated!=y.rotated?1:0,target,capacity,!fullKnown?"unknown":full?"yes":"no",module,rva,left,right);
        PluginDiagnostics::Write(line);
    }
    catch(...)
    {
        // Main-thread-only, once per process; even diagnostic errors must not
        // escape into the native predicate or suppress its original result.
        static bool reported=false;
        if(!reported){reported=true;try{ErrorLog("KenshiRotate: stack-pair diagnostics failed; rotation and stacking results are unchanged");}catch(...) {}}
    }
}
