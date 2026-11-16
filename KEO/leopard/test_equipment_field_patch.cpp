#define NOMINMAX
#ifndef OGRE_STATIC_LIB
#define OGRE_STATIC_LIB
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "fixes/near_page.h"
#include "fixes/world/equipment_field_lookup.h"
namespace Ogre{void*NedPoolingImpl::allocBytes(size_t n,const char*,int,const char*){void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void NedPoolingImpl::deallocBytes(void*p){free(p);}}
static DWORD ownerThread=GetCurrentThreadId();bool IsMainThread(){return GetCurrentThreadId()==ownerThread;}
namespace fixes{struct Config{bool equipmentFieldLookupEnabled;};Config g_fixesCfg={true};}
static void LogError(const std::string&){}
#define KEO_DETAIL(x) ((void)0)
#ifndef KEO_EQUIPMENT_PATCH_BASELINE
#define KEO_EQUIPMENT_PATCH_TEST
#include "../src/fixes/world/equipment_field_patch.cpp"
#endif
using namespace LeopardEquipmentField;
static volatile LONG calls=0;static bool throws=false;
static IntPair*__fastcall Native(IntMap*m,const std::string*k){InterlockedIncrement(&calls);if(throws)throw std::runtime_error("native field");return &*m->insert(IntPair(*k,0)).first;}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){unsigned char*code=AllocateNearPages((unsigned __int64)(uintptr_t)&Native,4096);C(code!=NULL);unsigned char*sites[3];unsigned char expected[3][5];Original functions[3];
 RUNTIME_FUNCTION unwind[3]={};for(unsigned i=0;i<3;++i){unsigned char*fn=code+i*64;const unsigned char body[]={0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3};memcpy(fn,body,sizeof(body));const __int64 delta=(__int64)(uintptr_t)&Native-(__int64)(uintptr_t)(fn+9);C(delta>=-2147483647LL-1&&delta<=2147483647LL);const int rel=(int)delta;memcpy(fn+5,&rel,4);sites[i]=fn+4;memcpy(expected[i],sites[i],5);functions[i]=(Original)fn;unwind[i].BeginAddress=i*64;unwind[i].EndAddress=i*64+14;unwind[i].UnwindData=0x300;}
 const unsigned char info[]={1,4,1,0,4,0x42,0,0};memcpy(code+0x300,info,sizeof(info));DWORD old=0;C(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)!=FALSE);FlushInstructionCache(GetCurrentProcess(),code,4096);C(RtlAddFunctionTable(unwind,3,(DWORD64)code)!=FALSE);
#ifndef KEO_EQUIPMENT_PATCH_BASELINE
 equipment_field_patch::original=Native;C(equipment_field_patch::Arm(sites,expected));equipment_field_patch::admitted=true;
#endif
 IntMap map;const std::string slot="slot";map[slot]=23;IntPair*pair=&*map.find(slot);calls=0;
 for(unsigned i=0;i<3;++i)C(functions[i](&map,&slot)==pair&&pair->second==23);
 printf("existing gear-field native fallbacks: %ld\n",calls);C(calls==0);
 const std::string other="other";for(unsigned i=0;i<3;++i){calls=0;IntPair*value=functions[i](&map,&other);C(value==&*map.find(other)&&calls==1);}
 map.erase(slot);calls=0;IntPair*created=functions[0](&map,&slot);C(calls==1&&created==&*map.find(slot)&&created->second==0);
 throws=true;bool caught=false;try{functions[2](&map,&other);}catch(const std::runtime_error&){caught=true;}C(caught);throws=false;
 RtlDeleteFunctionTable(unwind);VirtualFree(code,0,MEM_RELEASE);printf("PASS %u emitted equipment callsite checks\n",checks);return 0;
}
