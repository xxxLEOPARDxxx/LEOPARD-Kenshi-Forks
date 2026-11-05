#include "jobs/steel_jobs_transfer.h"
#include <float.h>
#include <stddef.h>
#include <math.h>
namespace keo_jobs {
namespace {bool TransferFinite(double v){return v>=-DBL_MAX&&v<=DBL_MAX;}}
StockTransferResult TransferStockItemWithOps(const StockTransferOps&o,void*src,void*dst,void*item){
 StockTransferResult r={0,false,NULL,0};StockTransferSnapshot s={};
 if(!src||!dst||src==dst||!item||!o.snapshot||!o.valid||!o.detach||!o.offer||!o.restore||!o.measure)return r;
 if(!o.snapshot(o.ctx,src,dst,item,&s)||!o.valid(o.ctx,s))return r;r.epoch=s.epoch;
 if(s.quantity<=0||s.quantity>1000000||!TransferFinite(s.originalMeasure)||s.originalMeasure<=0||!TransferFinite(s.unit)||s.unit<=0)return r;
 void*owned=NULL;try{owned=o.detach(o.ctx,src,item,s.quantity);}catch(...){r.fault=true;r.quarantineItem=item;return r;}if(!owned){r.fault=true;return r;}
 if(owned==(void*)(uintptr_t)-1){r.fault=true;r.quarantineItem=item;return r;}
 if(owned!=item){r.fault=true;r.quarantineItem=owned;return r;}
 int outcome=-1;
 if(o.valid(o.ctx,s)){try{outcome=o.offer(o.ctx,dst,item);}catch(...){outcome=-1;}}
 if(outcome<=0){
  bool restored=false;if(o.valid(o.ctx,s)){try{restored=o.restore(o.ctx,src,item,s);}catch(...){restored=false;}}
  if(!restored){r.fault=true;r.quarantineItem=item;return r;}
 }
 if(outcome<0)r.fault=true;
 double source=0,dest=0;
 if(!o.valid(o.ctx,s)||!o.measure(o.ctx,src,dst,s,&source,&dest)||!TransferFinite(source)||!TransferFinite(dest)){r.fault=true;return r;}
 const double removed=s.sourceMeasure-source,added=dest-s.destMeasure;
 // Native float charge additions can round; integer/full-charge fixtures stay
 // exact. Reject large deltas, negative progress and any non-conservation.
 const double tolerance=8.0*FLT_EPSILON*s.unit*4098.0 + 8.0*DBL_EPSILON*(fabs(s.sourceMeasure)+fabs(s.destMeasure)+s.originalMeasure+1.0);
 if(removed < -tolerance||added < -tolerance||removed>s.originalMeasure+tolerance||fabs(removed-added)>tolerance){r.fault=true;return r;}
 if(removed>0)r.movedUnits=removed/s.unit;
 return r;
}
}
// Native adapter.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "jobs/steel_jobs.h"
#include "game/game.h"
#include "game/native_profile.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include "base/klib_include_end.h"
#include <string.h>
#include <wincrypt.h>
#pragma comment(lib,"advapi32.lib")
namespace keo_jobs {namespace {
typedef Item*(*RemoveFn)(Inventory*,Item*,int,bool);
typedef bool(*OfferFn)(Inventory*,Item*,int);
typedef void(*RestoreFn)(InventorySection*,Item*,int,int);
typedef bool(*CompatibleFn)(InventorySection*,Item*);
typedef bool(*StackFn)(const InventoryItemBase*,const InventoryItemBase*);
typedef int(*StackLimitFn)(const InventoryItemBase*,InventorySection*);
static RemoveFn s_remove=NULL;static OfferFn s_offer=NULL;static RestoreFn s_restore=NULL;
static CompatibleFn s_compatible=NULL;static StackFn s_canStack=NULL;static StackLimitFn s_stackLimit=NULL;
static uintptr_t s_sectionAdd=0,s_sourceCallback=0,s_destCallback=0;
static volatile LONG s_transferReady=0,s_transferFault=0,s_transferBusy=0,s_resetPending=0;
// Exactly one opaque native reference. No allocation, retry queue or cross-world
// dereference. The engine owns object destruction; world reset discards the ticket.
static void*s_quarantined=NULL;static uint64_t s_quarantineEpoch=0;
static bool NativeFinite(float v){return v>=-FLT_MAX&&v<=FLT_MAX;}
static bool NativeContractReady();
static bool InventoryShape(Inventory*i){return i&&i->_allItems.count<=4096&&i->_allItems.count<=i->_allItems.maxSize&&(!i->_allItems.count||i->_allItems.stuff);}
static bool SectionsShape(Inventory*i){return i->sectionsInSearchOrder.count<=64&&i->sectionsInSearchOrder.count<=i->sectionsInSearchOrder.maxSize&&(!i->sectionsInSearchOrder.count||i->sectionsInSearchOrder.stuff);}
static bool Grid(InventorySection*s){return s&&s->width>0&&s->height>0&&s->width<=64&&s->height<=64&&s->content.nRows==(unsigned)s->width&&s->content.nCols==(unsigned)s->height&&s->content.data.size()==(size_t)s->width*s->height&&s->items.size()<=4096;}
static bool Footprint(InventorySection*s,int x,int y,int w,int h,Item*expected){
 if(!Grid(s)||x<0||y<0||w<=0||h<=0||w>s->width-x||h>s->height-y)return false;
 for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)if(s->content.data[yy*s->width+xx]!=expected)return false;return true;
}
static bool MeasureOne(Item*i,const StockTransferSnapshot&s,double*out){
 if(i->quantity<=0||i->quantity>1000000||!NativeFinite(i->originalFullChargeAmount)||!NativeFinite(i->chargesLeft)||i->chargesLeft<0)return false;
 if(s.charged){if(i->originalFullChargeAmount!=(float)s.unit){*out=0;return true;}*out=(i->quantity-1)*s.unit+i->chargesLeft;}
 else {if(i->originalFullChargeAmount!=0||i->chargesLeft!=0){*out=0;return true;}*out=i->quantity;}
 return TransferFinite(*out)&&*out>=0;
}
static bool MeasureInventory(Inventory*i,const StockTransferSnapshot&s,double*out){
 if(!InventoryShape(i))return false;double sum=0;
 for(unsigned n=0;n<i->_allItems.count;++n){Item*v=i->_allItems.stuff[n];if(!v||v->getGameData()!=(GameData*)s.asset)continue;double q=0;if(!MeasureOne(v,s,&q))return false;sum+=q;}
 *out=sum;return TransferFinite(sum);
}
static bool NativeValid(void*,const StockTransferSnapshot&s){return SteelJobsAdmitted()&&SteelJobsEpoch()==s.epoch;}
static bool DestinationSpace(Inventory*d,Item*item,const StockTransferSnapshot&s){
 bool room=false;unsigned cells=0;
 for(unsigned n=0;n<d->sectionsInSearchOrder.count;++n){InventorySection*t=d->sectionsInSearchOrder.stuff[n];
  if(!Grid(t)||(uintptr_t)(*(void***)t)[2]!=s_sectionAdd)return false;
  cells+=(unsigned)(t->width*t->height);if(cells>65536)return false;
  if(!t->enabled||!s_compatible(t,item))continue;
  const int limit=s_stackLimit(item,t);if(limit<=0)continue;
  for(size_t k=0;k<t->items.size();++k){Item*v=t->items[k].item;if(!v||v->properOwner.type!=NULL_ITEM||!s_canStack(v,item))continue;
   // Different charge capacities cannot share our conservation dimension.
   if(s.charged?v->originalFullChargeAmount!=(float)s.unit:(v->originalFullChargeAmount!=0||v->chargesLeft!=0))return false;
   double used=0;if(!MeasureOne(v,s,&used))return false;
   if(used<(double)limit*s.unit)room=true;
  }
  for(int y=0;y<=t->height-item->itemHeight&&!room;++y)for(int x=0;x<=t->width-item->itemWidth&&!room;++x)
   if(Footprint(t,x,y,item->itemWidth,item->itemHeight,NULL))room=true;
 }
 return room;
}
static bool CaptureGame(void*,void*source,void*dest,void*what,StockTransferSnapshot*out){
 Inventory*src=(Inventory*)source;Inventory*dst=(Inventory*)dest;Item*item=(Item*)what;
 if(!SteelJobsAdmitted()||!NativeContractReady()||!InventoryShape(src)||!InventoryShape(dst)||!SectionsShape(src)||!SectionsShape(dst))return false;
 // Native addQuantity/addItem may clear provenance. Do not launder or rewrite ownership.
 if(src==dst||!item||item->properOwner.type!=NULL_ITEM||!item->isInInventory||item->isEquipped||item->quantity<=0||item->quantity>1000000||item->itemWidth<=0||item->itemHeight<=0)return false;
 if((uintptr_t)(*(void***)src)[5]!=(uintptr_t)s_remove||(uintptr_t)(*(void***)dst)[3]!=(uintptr_t)s_offer
  ||(uintptr_t)(*(void***)src)[10]!=s_sourceCallback||(uintptr_t)(*(void***)dst)[8]!=s_destCallback)return false;
 unsigned found=0;for(unsigned k=0;k<src->_allItems.count;++k)if(src->_allItems.stuff[k]==item)++found;if(found!=1)return false;
 for(unsigned k=0;k<dst->_allItems.count;++k)if(dst->_allItems.stuff[k]==item)return false;
 if(!NativeFinite(item->originalFullChargeAmount)||!NativeFinite(item->chargesLeft)||item->originalFullChargeAmount<0||item->originalFullChargeAmount>1e9f||item->chargesLeft<0||item->chargesLeft>1e9f)return false;
 StockTransferSnapshot s={};s.epoch=SteelJobsEpoch();s.asset=(uintptr_t)item->getGameData();s.quantity=item->quantity;
 s.charged=item->originalFullChargeAmount>0;s.unit=s.charged?item->originalFullChargeAmount:1;
 if(!s.asset||(!s.charged&&item->chargesLeft!=0)||!MeasureOne(item,s,&s.originalMeasure)||s.originalMeasure<=0||s.originalMeasure/s.unit>1000000)return false;
 InventorySection*section=NULL;
 for(unsigned n=0;n<src->sectionsInSearchOrder.count;++n){InventorySection*t=src->sectionsInSearchOrder.stuff[n];if(!Grid(t))return false;
  for(size_t k=0;k<t->items.size();++k)if(t->items[k].item==item){if(section)return false;section=t;s.x=t->items[k].x;s.y=t->items[k].y;}
 }
 if(!section||section->callbackObject||section->name!=item->inventorySection||(uintptr_t)(*(void***)section)[3]!=(uintptr_t)s_restore
  ||!Footprint(section,s.x,s.y,item->itemWidth,item->itemHeight,item))return false;
 s.section=(uintptr_t)section;
 if(!MeasureInventory(src,s,&s.sourceMeasure)||!MeasureInventory(dst,s,&s.destMeasure)||!DestinationSpace(dst,item,s)||!NativeValid(NULL,s))return false;
 *out=s;return true;
}
static bool CaptureSafe(void*c,void*a,void*b,void*i,StockTransferSnapshot*s){__try{return CaptureGame(c,a,b,i,s);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void*DetachSafe(void*,void*src,void*item,int qty){__try{return s_remove((Inventory*)src,(Item*)item,qty,false);}__except(EXCEPTION_EXECUTE_HANDLER){return (void*)(uintptr_t)-1;}}
static int OfferSafe(void*,void*dst,void*item){__try{return s_offer((Inventory*)dst,(Item*)item,1)?1:0;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}}
static bool RestoreGame(void*,void*source,void*what,const StockTransferSnapshot&s){
 Inventory*src=(Inventory*)source;Item*item=(Item*)what;InventorySection*t=(InventorySection*)s.section;
 if(!NativeValid(NULL,s)||!item||item->isInInventory||item->quantity<=0||item->quantity>s.quantity||!Grid(t)||t->parentInventory!=src
  ||t->items.size()>=t->items.capacity()||!InventoryShape(src)||src->_allItems.count>=src->_allItems.maxSize
  ||!Footprint(t,s.x,s.y,item->itemWidth,item->itemHeight,NULL))return false;
 s_restore(t,item,s.x,s.y);
 return item->isInInventory&&item->inventoryPos.x==s.x&&item->inventoryPos.y==s.y&&Footprint(t,s.x,s.y,item->itemWidth,item->itemHeight,item);
}
static bool RestoreSafe(void*c,void*a,void*i,const StockTransferSnapshot&s){__try{return RestoreGame(c,a,i,s);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool MeasureSafe(void*,void*a,void*b,const StockTransferSnapshot&s,double*x,double*y){__try{return MeasureInventory((Inventory*)a,s,x)&&MeasureInventory((Inventory*)b,s,y);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}

struct TransferContract {uintptr_t steam,gog;unsigned char a[32],b[32];};
static const TransferContract kTransferContracts[]={
 {0x749E10u,0x749800u,{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x41,0x54,0x48,0x83,0xEC,0x20,0x80,0xBA,0xD8,0x00,0x00,0x00,0x00,0x41,0x0F,0xB6,0xF1,0x45,0x8B,0xE0,0x48,0x8B,0xDA,0x48,0x8B},{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x41,0x54,0x48,0x83,0xEC,0x20,0x80,0xBA,0xD8,0x00,0x00,0x00,0x00,0x41,0x0F,0xB6,0xF1,0x45,0x8B,0xE0,0x48,0x8B,0xDA,0x48,0x8B}}, // remove
 {0x74B1E0u,0x74AB10u,{0x40,0x53,0x41,0x55,0x41,0x56,0x48,0x83,0xEC,0x20,0x45,0x8B,0xE8,0x4C,0x8B,0xF2,0x48,0x8B,0xD9,0x48,0x85,0xD2,0x75,0x0C,0x32,0xC0,0x48,0x83,0xC4,0x20,0x41,0x5E},{0x40,0x53,0x41,0x55,0x41,0x56,0x48,0x83,0xEC,0x20,0x45,0x8B,0xE8,0x4C,0x8B,0xF2,0x48,0x8B,0xD9,0x48,0x85,0xD2,0x75,0x0C,0x32,0xC0,0x48,0x83,0xC4,0x20,0x41,0x5E}}, // offer
 {0x748BE0u,0x748510u,{0x48,0x85,0xD2,0x0F,0x84,0xEA,0x02,0x00,0x00,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xA8,0x00,0x00,0x00,0x48,0xC7,0x44,0x24},{0x48,0x85,0xD2,0x0F,0x84,0xEA,0x02,0x00,0x00,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xA8,0x00,0x00,0x00,0x48,0xC7,0x44,0x24}}, // restore
 {0x74BD70u,0x74B6A0u,{0x48,0x8B,0xC4,0x44,0x89,0x40,0x18,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x40,0x48,0xC7,0x45,0xE0,0xFE,0xFF,0xFF,0xFF,0x48},{0x48,0x8B,0xC4,0x44,0x89,0x40,0x18,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x40,0x48,0xC7,0x45,0xE0,0xFE,0xFF,0xFF,0xFF,0x48}}, // sectionAdd
 {0x748EE0u,0x748810u,{0x48,0x89,0x5C,0x24,0x20,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xFA,0x48,0x8B,0xD9,0x48,0x85,0xD2,0x75,0x0D,0x32,0xC0,0x48,0x8B,0x5C,0x24,0x48,0x48,0x83,0xC4,0x20},{0x48,0x89,0x5C,0x24,0x20,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xFA,0x48,0x8B,0xD9,0x48,0x85,0xD2,0x75,0x0D,0x32,0xC0,0x48,0x8B,0x5C,0x24,0x48,0x48,0x83,0xC4,0x20}}, // sectionRemove
 {0x746AC0u,0x7463F0u,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x45,0x33,0xC0,0x48,0x8B,0xD9,0xFF,0x50,0x68,0x48,0x8B,0x4B,0x08,0x48,0x85,0xC9,0x74,0x05,0xE8,0x4B,0x98,0x90,0xFF},{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x45,0x33,0xC0,0x48,0x8B,0xD9,0xFF,0x50,0x68,0x48,0x8B,0x4B,0x08,0x48,0x85,0xC9,0x74,0x05,0xE8,0x93,0x9F,0x90,0xFF}}, // sourceCallback
 {0x746A70u,0x7463A0u,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x48,0x8B,0xD9,0xFF,0x50,0x60,0x48,0x8B,0x4B,0x08,0x48,0x85,0xC9,0x74,0x05,0xE8,0x9E,0x98,0x90,0xFF,0x48,0x83,0xC4},{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x48,0x8B,0xD9,0xFF,0x50,0x60,0x48,0x8B,0x4B,0x08,0x48,0x85,0xC9,0x74,0x05,0xE8,0xE6,0x9F,0x90,0xFF,0x48,0x83,0xC4}}, // destCallback
 {0x74B0F0u,0x74AA20u,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0x02,0x48,0x8B,0xD9,0x48,0x8B,0xCA,0x48,0x8B,0xF2,0xFF,0x50,0x18,0x44,0x8B},{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0x02,0x48,0x8B,0xD9,0x48,0x8B,0xCA,0x48,0x8B,0xF2,0xFF,0x50,0x18,0x44,0x8B}}, // compatible
 {0x75C930u,0x75C260u,{0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x48,0x8B,0xF2,0x48,0x8B,0xD9,0xFF,0x90,0xA0,0x02,0x00,0x00,0x84,0xC0,0x75,0x0B,0x48,0x8B,0x5C},{0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x20,0x48,0x8B,0x01,0x48,0x8B,0xF2,0x48,0x8B,0xD9,0xFF,0x90,0xA0,0x02,0x00,0x00,0x84,0xC0,0x75,0x0B,0x48,0x8B,0x5C}}, // canStack
 {0x75D550u,0x75CE80u,{0x40,0x57,0x48,0x83,0xEC,0x60,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48,0x89,0x9C,0x24,0x80,0x00,0x00,0x00,0x48,0x8B,0x05,0x4A,0x8E,0x9B,0x01,0x48,0x33},{0x40,0x57,0x48,0x83,0xEC,0x60,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48,0x89,0x9C,0x24,0x80,0x00,0x00,0x00,0x48,0x8B,0x05,0x2A,0x75,0x9B,0x01,0x48,0x33}}, // stackLimit
};

// A foreign detour is admitted only for these audited observer/predicate hooks.
// StackSort and Rotate are source-audited; KEP3FD50..3FEE4 is binary-audited.
// No trampoline is bypassed. Original inventory entries retain all callbacks.
struct TransferModulePin {const wchar_t*name;uint32_t stamp,size;const char*sha;};
static const TransferModulePin kTransferModules[]={
 {L"StackSort.dll",0x6AE727F7u,0x73000u,"185a8bcbffd610490d6cf78f1c59b09c3e49430ffc6670403359a5ca6c08a7e9"},
 {L"KenshiRotate.dll",0x6AE6FAA0u,0x29000u,"117061950e27d9ac586515dedc5517ad6c75c4ecf78fb340bfcc5c6bc934815a"},
 {L"KenshiExtensionPlugin.dll",0x6A9ACDF7u,0xF7000u,"74c028ae13c1d9584857e1f5c0945782c5c40989f9bbb4caa07541a1724c2e59"}
};
struct TransferHookPin {unsigned row,module;uintptr_t target;unsigned char bytes[32];};
static const TransferHookPin kTransferHooks[]={
 {5,0,0x2F8C0u,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xFF,0x15,0x29,0xB6,0x03,0x00,0x80,0x3D,0x6C,0xB5,0x03,0x00,0x00,0x75,0x08,0x48,0x8B,0xCB,0xE8,0xB0,0x9A,0xFF,0xFF}},
 {6,0,0x2F890u,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xFF,0x15,0x51,0xB6,0x03,0x00,0x80,0x3D,0x9C,0xB5,0x03,0x00,0x00,0x75,0x08,0x48,0x8B,0xCB,0xE8,0xE0,0x9A,0xFF,0xFF}},
 {7,2,0x3FD50u,{0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x20,0x80,0x3D,0x3A,0x99,0x0A,0x00,0x00,0x48,0x8B,0xF2,0x48,0x8B,0xD9,0x75,0x11,0x48,0x8B,0x5C,0x24,0x38,0x48,0x83}},
 {8,1,0x1C00u,{0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xEC,0x30,0x48,0x8B,0xFA,0x48,0x8B,0xF1,0x48,0x85,0xC9,0x0F,0x84,0x83,0x00,0x00,0x00,0x48,0x85,0xD2,0x74,0x7E,0x48,0x89}}
};
// Pure shape contract, extracted by the owned-buffer tests. Only E9 -> exact
// FF25[+0] relay -> one pinned module entry. No generic shared-site exemption.
static bool TransferSharedShape(uintptr_t site,const unsigned char*head,const unsigned char*expected,
 uintptr_t relayAddress,const unsigned char*relay,uintptr_t moduleBase,uintptr_t targetRva,
 const unsigned char*target,const unsigned char*expectedTarget){
 if(!head||!expected||!relay||!target||!expectedTarget||!moduleBase||head[0]!=0xE9||site>~uintptr_t(0)-5||targetRva>~uintptr_t(0)-moduleBase)return false;
 if(memcmp(head+5,expected+5,27)||relay[0]!=0xFF||relay[1]!=0x25||relay[2]||relay[3]||relay[4]||relay[5])return false;
 int32_t delta=0;memcpy(&delta,head+1,4);uintptr_t next=site+5,destination=0;
 if(delta<0){uintptr_t back=(uintptr_t)(-(int64_t)delta);if(back>next)return false;destination=next-back;}
 else{if((uintptr_t)delta>~uintptr_t(0)-next)return false;destination=next+(uintptr_t)delta;}
 if(destination!=relayAddress)return false;
 uintptr_t ownerTarget=0;memcpy(&ownerTarget,relay+6,8);
 return ownerTarget==moduleBase+targetRva&&ownerTarget==(uintptr_t)target&&memcmp(target,expectedTarget,32)==0;
}
struct TransferSharedTicket {const TransferHookPin*pin;uintptr_t module,relay;unsigned char head[32],relayBytes[14];};
static TransferSharedTicket s_sharedTransfer[10]={};
// Startup-only file/hash handles; bounded streaming, no heap and no hot I/O.
static bool TransferFileSha(const wchar_t*path,char out[65]){
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=false;BY_HANDLE_FILE_INFORMATION before={},after={};
 unsigned char buffer[4096],digest[32];DWORD size=sizeof(digest),count=0;unsigned __int64 total=0;
 if(GetFileInformationByHandle(file,&before)&&!before.nFileSizeHigh&&before.nFileSizeLow>0&&before.nFileSizeLow<16u*1024u*1024u
  &&CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
  ok=true;for(;;){if(!ReadFile(file,buffer,sizeof(buffer),&count,NULL)){ok=false;break;}if(!count)break;
   total+=count;if(total>before.nFileSizeLow||!CryptHashData(hash,buffer,count,0)){ok=false;break;}}
  ok=ok&&total==before.nFileSizeLow&&GetFileInformationByHandle(file,&after)
   &&before.dwVolumeSerialNumber==after.dwVolumeSerialNumber&&before.nFileIndexHigh==after.nFileIndexHigh&&before.nFileIndexLow==after.nFileIndexLow
   &&before.nFileSizeLow==after.nFileSizeLow&&CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)==0
   &&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32;
 }
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
 if(ok){const char*hex="0123456789abcdef";for(unsigned i=0;i<32;++i){out[2*i]=hex[digest[i]>>4];out[2*i+1]=hex[digest[i]&15];}out[64]=0;}
 return ok;
}
static bool TransferModuleReady(uintptr_t base,unsigned module){
 const TransferModulePin&p=kTransferModules[module];const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)base;
 if(!base||d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;
 const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)(base+d->e_lfanew);
 return n->Signature==IMAGE_NT_SIGNATURE&&n->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&n->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC
  &&n->FileHeader.TimeDateStamp==p.stamp&&n->OptionalHeader.SizeOfImage==p.size;
}
static bool CaptureTransferContracts(){
 __try{
  memset(s_sharedTransfer,0,sizeof(s_sharedTransfer));uintptr_t modules[3]={};bool checked[3]={};const bool gog=NativeProfileIsGog();
  for(unsigned row=0;row<10;++row){const TransferContract&r=kTransferContracts[row];const unsigned char*head=(const unsigned char*)(gameBase+(gog?r.gog:r.steam));const unsigned char*expected=gog?r.b:r.a;
   if(memcmp(head,expected,32)==0)continue;
   const TransferHookPin*pin=NULL;for(unsigned k=0;k<4;++k)if(kTransferHooks[k].row==row)pin=&kTransferHooks[k];if(!pin)return false;
   if(!checked[pin->module]){const TransferModulePin&m=kTransferModules[pin->module];HMODULE h=GetModuleHandleW(m.name);wchar_t path[1024];char sha[65]={};
    DWORD length=h?GetModuleFileNameW(h,path,1024):0;
    if(!length||length>=1024||!TransferModuleReady((uintptr_t)h,pin->module)||!TransferFileSha(path,sha)||strcmp(sha,m.sha))return false;
    modules[pin->module]=(uintptr_t)h;checked[pin->module]=true;
   }
   uintptr_t site=(uintptr_t)head;if(site>~uintptr_t(0)-5)return false;int32_t delta=0;memcpy(&delta,head+1,4);uintptr_t next=site+5,relay=0;
   if(delta<0){uintptr_t back=(uintptr_t)(-(int64_t)delta);if(back>next)return false;relay=next-back;}else{if((uintptr_t)delta>~uintptr_t(0)-next)return false;relay=next+(uintptr_t)delta;}
   uintptr_t module=modules[pin->module];if(pin->target>~uintptr_t(0)-module||pin->target>kTransferModules[pin->module].size-32)return false;
   if(!TransferSharedShape(site,head,expected,relay,(const unsigned char*)relay,module,pin->target,(const unsigned char*)(module+pin->target),pin->bytes))return false;
   TransferSharedTicket&t=s_sharedTransfer[row];t.pin=pin;t.module=module;t.relay=relay;memcpy(t.head,head,32);memcpy(t.relayBytes,(const void*)relay,14);
  }return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool NativeContractReady(){
 if(!InterlockedCompareExchange(&s_transferReady,0,0))return false;
 __try{
  const bool gog=NativeProfileIsGog();
  for(unsigned row=0;row<10;++row){const TransferContract&r=kTransferContracts[row];const unsigned char*head=(const unsigned char*)(gameBase+(gog?r.gog:r.steam));const unsigned char*expected=gog?r.b:r.a;
   const TransferSharedTicket&t=s_sharedTransfer[row];if(!t.pin){if(memcmp(head,expected,32))return false;continue;}
   if(memcmp(head,t.head,32)||memcmp((const void*)t.relay,t.relayBytes,14)||!TransferModuleReady(t.module,t.pin->module)
    ||!TransferSharedShape((uintptr_t)head,head,expected,t.relay,(const unsigned char*)t.relay,t.module,t.pin->target,(const unsigned char*)(t.module+t.pin->target),t.pin->bytes))return false;
  }return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

static bool BindTransfer(){
 uintptr_t found[]={
 (uintptr_t)KlibRealAddress(&Inventory::_NV_removeItemDontDestroy_returnsItem),
 (uintptr_t)KlibRealAddress(&Inventory::_NV_tryAddItem),
 (uintptr_t)KlibRealAddress(&InventorySection::_NV__addItem),
 (uintptr_t)KlibRealAddress(&InventorySection::_NV_addItem),
 (uintptr_t)KlibRealAddress(&InventorySection::removeItem),
 (uintptr_t)KlibRealAddress(&Inventory::_NV__sectionRemoveItemCallback),
 (uintptr_t)KlibRealAddress(&Inventory::_NV__sectionAddItemCallback),
 (uintptr_t)KlibRealAddress(static_cast<bool(InventorySection::*)(Item*)>(&InventorySection::isLimitedSlotCompatible)),
 (uintptr_t)KlibRealAddress(&InventoryItemBase::canStackWith),
 (uintptr_t)KlibRealAddress(&InventoryItemBase::isStackable)};
 const bool gog=NativeProfileIsGog();
 for(size_t i=0;i<sizeof(found)/sizeof(found[0]);++i)if(found[i]!=gameBase+(gog?kTransferContracts[i].gog:kTransferContracts[i].steam))return false;
 s_remove=(RemoveFn)found[0];s_offer=(OfferFn)found[1];s_restore=(RestoreFn)found[2];s_sectionAdd=found[3];s_sourceCallback=found[5];s_destCallback=found[6];s_compatible=(CompatibleFn)found[7];s_canStack=(StackFn)found[8];s_stackLimit=(StackLimitFn)found[9];
 if(!CaptureTransferContracts())return false;
 InterlockedExchange(&s_transferReady,1);if(NativeContractReady())return true;InterlockedExchange(&s_transferReady,0);return false;
}
static void ResetTransferTicket(){s_quarantined=NULL;s_quarantineEpoch=0;InterlockedExchange(&s_transferFault,0);InterlockedExchange(&s_resetPending,0);}
}
bool PrepareStockTransfer(){return InterlockedCompareExchange(&s_transferReady,0,0)?NativeContractReady():BindTransfer();}
bool CanTransferStockItem(void*src,void*dst,void*item){
 if(InterlockedCompareExchange(&s_transferFault,0,0)||InterlockedCompareExchange(&s_transferBusy,0,0))return false;
 StockTransferSnapshot s={};return CaptureSafe(NULL,src,dst,item,&s);
}
StockTransferResult TransferStockItem(void*src,void*dst,void*item){
 StockTransferResult r={0,false,NULL,0};
 if(InterlockedCompareExchange(&s_transferFault,0,0)||InterlockedCompareExchange(&s_transferBusy,1,0))return r;
 StockTransferOps o={NULL,CaptureSafe,NativeValid,DetachSafe,OfferSafe,RestoreSafe,MeasureSafe};
 try {r=TransferStockItemWithOps(o,src,dst,item);}catch(...){r.fault=true;}
 if(r.fault){s_quarantined=r.quarantineItem;s_quarantineEpoch=r.epoch;InterlockedExchange(&s_transferFault,1);}
 if(InterlockedCompareExchange(&s_resetPending,0,0))ResetTransferTicket();
 InterlockedExchange(&s_transferBusy,0);return r;
}
void ResetStockTransfer(){
 InterlockedExchange(&s_resetPending,1);if(!InterlockedCompareExchange(&s_transferBusy,1,0)){ResetTransferTicket();InterlockedExchange(&s_transferBusy,0);}
}
}
