// Missing weight accounting adapted from KenshiQOL, with bounded query-local
// traversal and thread-local native-baseline capture. No actor/cache survives a call.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "inventory/carried_weight.h"
#include "inventory/carried_weight_policy.h"
#include "inventory/inventory_config.h"
#include "game/game.h"
#include "game/native_profile.h"
#include "base/log_admission.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include <kenshi/Inventory.h>
#include <kenshi/Character.h>
#include <kenshi/Animation/AnimationClass.h>
#include <kenshi/CharStats.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/RaceData.h>
#include <kenshi/GameData.h>
#include "base/klib_include_end.h"
#include <string.h>
#include <string>
namespace keo_inventory {
namespace {
static float(*s_originalWeight)(void*)=NULL;
static volatile LONG s_admitted=0;
static bool s_installed=false,s_blocked=false;
static double s_nextCheck=0;
struct WeightScope {WeightScope*previous;bool raw,seen;uintptr_t target;float nativeInv;};
static __declspec(thread) WeightScope*t_weightScope=NULL;
static const std::string kHullX("hull size X"),kHullY("hull size Y"),kHullZ("hull size Z"),kHeight("Height"),kFrame("Frame");
static float GetNativeCarryWeight(uintptr_t actor){return ((Character*)actor)->getTotalCarryWeight();}
static bool ReadNativeBody(void*,uintptr_t actor,uintptr_t inventory,float*out){
 WeightScope local={};local.previous=t_weightScope;local.raw=true;local.target=inventory;
 t_weightScope=&local;float carry=0;
 __try {carry=GetNativeCarryWeight(actor);}
 __finally {t_weightScope=local.previous;}
 if(!local.seen)return false;
 *out=carry-local.nativeInv;return true;
}
static uintptr_t Owner(void*,uintptr_t inv){
 Inventory*i=(Inventory*)inv;if(!i||!i->callbackObject)return 0;
 Character*c=i->getCallbackCharacter();return c&&c->inventory==i?(uintptr_t)c:0;
}
static bool RootInventory(void*i){return Owner(NULL,(uintptr_t)i)!=0;}
static float NativeWeight(void*,uintptr_t i){return s_originalWeight((void*)i);}
static int ItemCount(void*,uintptr_t i){const lektor<Item*>&items=((Inventory*)i)->_allItems;
 return items.count>4096||items.count>items.maxSize||(items.count&&!items.stuff)?-1:(int)items.count;}
static uintptr_t ChildInventory(void*,uintptr_t i,int n){
 const lektor<Item*>&items=((Inventory*)i)->_allItems;Item*item=items.stuff[n];
 // Equipped bag contents already contribute through native sections/reductions.
 return item&&!item->isEquipped&&item->objectType==CONTAINER?(uintptr_t)item->getInventory():0;
}
static uintptr_t Carried(void*,uintptr_t a){Character*c=(Character*)a;return c->isCarryingSomething?(uintptr_t)c->carryingObject.getCharacter():0;}
static uintptr_t InventoryOf(void*,uintptr_t a){return a?(uintptr_t)((Character*)a)->inventory:0;}
static float FloatField(GameData*d,const std::string&key,float fallback){
 if(!d)return fallback;
 boost::unordered::unordered_map<std::string,float,boost::hash<std::string>,std::equal_to<std::string>,Ogre::STLAllocator<std::pair<std::string const,float>,Ogre::GeneralAllocPolicy> >::const_iterator i=d->fdata.find(key);
 return i==d->fdata.end()?fallback:i->second;
}
static bool Body(void*,uintptr_t a,WeightBody*out){
 Character*c=(Character*)a;if(!c)return false;RaceData*r=c->getRace();
 out->x=FloatField(r?r->data:NULL,kHullX,5);out->y=FloatField(r?r->data:NULL,kHullY,17);out->z=FloatField(r?r->data:NULL,kHullZ,5);
 out->scale=c->getAge();out->height=out->frame=100;
 GameDataCopyStandalone*appearance=c->animation&&c->animation->appearance?c->getAppearanceData():NULL;
 out->height=FloatField(appearance,kHeight,100);out->frame=FloatField(appearance,kFrame,100);
 out->robot=r&&r->robot?1:0;out->hunger=c->stats&&c->stats->medical?c->stats->medical->hunger:3;
 return true;
}
static WeightOps WeightOpsForGame(){WeightOps o={NULL,NativeWeight,Owner,ItemCount,ChildInventory,Carried,InventoryOf,Body,ReadNativeBody};return o;}
static float HookWeight(void* inventory){
 const float base=s_originalWeight(inventory);
 WeightScope*const outer=t_weightScope;
 if(outer&&outer->raw){if(outer->target==(uintptr_t)inventory){outer->seen=true;outer->nativeInv=base;}return base;}
 if(outer||!InterlockedCompareExchange(&s_admitted,0,0)||!inventory||!RootInventory(inventory))return base;
 WeightScope local={};local.previous=outer;t_weightScope=&local;float value=base;
 __try {value=CarriedWeightEvaluate(WeightOpsForGame(),(uintptr_t)inventory,base);}
 __finally {t_weightScope=local.previous;}
 return value;
}
static const unsigned char kWeightSteam[32]={0x33,0xC0,0x4C,0x8B,0xC1,0x89,0x81,0x90,0x00,0x00,0x00,0x48,0x39,0x41,0x48,0x74,0x13,0x48,0x8B,0x51,0x40,0x48,0x8B,0x41,0x60,0x48,0x8B,0x14,0xD0,0x48,0x89,0x54};
static const unsigned char kWeightGog[32]={0x33,0xC0,0x4C,0x8B,0xC1,0x89,0x81,0x90,0x00,0x00,0x00,0x48,0x39,0x41,0x48,0x74,0x13,0x48,0x8B,0x51,0x40,0x48,0x8B,0x41,0x60,0x48,0x8B,0x14,0xD0,0x48,0x89,0x54};
static const unsigned char kCarrySteam[44]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8B,0x89,0xE8,0x02,0x00,0x00,0xE8,0x73,0xC4,0xA7,0xFF,0x80,0xBB,0x48,0x03,0x00,0x00,0x00,0x74,0x08,0xF3,0x0F,0x58,0x05,0x3A,0xAF,0xB6,0x01,0x48,0x83,0xC4,0x20,0x5B,0xC3};
static const unsigned char kThunkSteam[5]={0xE9,0xC3,0x24,0x70,0x00};
static const unsigned char kCarryGog[44]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8B,0x89,0xE8,0x02,0x00,0x00,0xE8,0xD6,0xC1,0xA7,0xFF,0x80,0xBB,0x48,0x03,0x00,0x00,0x00,0x74,0x08,0xF3,0x0F,0x58,0x05,0x9A,0x8B,0xB6,0x01,0x48,0x83,0xC4,0x20,0x5B,0xC3};
static const unsigned char kThunkGog[5]={0xE9,0x80,0x1D,0x70,0x00};

bool SdkAt(uintptr_t address,uintptr_t steam,uintptr_t gog){return address==gameBase+(NativeProfileIsGog()?gog:steam);}
bool Bindings(uintptr_t*weight){
 *weight=(uintptr_t)KlibRealAddress(&Inventory::getTotalWeight);
 const uintptr_t carry=(uintptr_t)KlibRealAddress(&Character::getTotalCarryWeight);
 return SdkAt(*weight,0x746890,0x7461C0)&&SdkAt(carry,0x5C7F40,0x5C8250)
  &&SdkAt((uintptr_t)KlibRealAddress(&Inventory::getCallbackCharacter),0x70C320,0x70BC80)
  &&SdkAt((uintptr_t)KlibRealAddress(&Character::getAppearanceData),0xCD330,0xCD330)
  &&SdkAt((uintptr_t)KlibRealAddress(&Character::_NV_getAge),0x5E4EF0,0x5E5200)
  &&SdkAt((uintptr_t)KlibRealAddress(&Character::_NV_getRace),0x5E1470,0x5E1780)
  &&memcmp((void*)*weight,NativeProfileIsGog()?kWeightGog:kWeightSteam,32)==0
  &&memcmp((void*)carry,NativeProfileIsGog()?kCarryGog:kCarrySteam,44)==0
  &&memcmp((void*)(gameBase+(NativeProfileIsGog()?0x4443B:0x443C8)),NativeProfileIsGog()?kThunkGog:kThunkSteam,5)==0;
}
}
void CarriedWeightTick(double now){
 if(!s_installed||s_blocked||now<s_nextCheck)return;s_nextCheck=now+1;
 if(GetModuleHandleW(L"KenshiQOL.dll")!=NULL){InterlockedExchange(&s_admitted,0);s_blocked=true;
  LogError("Carried weight: disabled because KenshiQOL.dll is loaded; only one weight implementation may run");return;}
 InterlockedExchange(&s_admitted,1);
}
void InstallCarriedWeight(int*installed,int*total){
 if(!g_inventoryCfg.carriedWeightEnabled)return;
 if(total)++*total;
 if(GetModuleHandleW(L"KenshiQOL.dll")!=NULL){s_blocked=true;LogError("Carried weight: KenshiQOL.dll is loaded; KEO weight hook refused to prevent double counting");return;}
 uintptr_t target=0;
 if(!Bindings(&target)){LogError("Carried weight: exact Steam/GOG SDK or native weight contract refused; native weight retained");return;}
 if(KenshiLib::AddHook((void*)target,HookWeight,&s_originalWeight)!=KenshiLib::SUCCESS){s_originalWeight=NULL;LogError("Carried weight: Inventory::getTotalWeight hook failed; native weight retained");return;}
 s_installed=true;if(installed)++*installed;
 // The first Main lifecycle check runs after normal startup DLL load order.
 // Until then the hook is a native-only pass-through.
 KEO_DETAIL("Carried weight: installed, waiting for post-startup compatibility check");
}
}
