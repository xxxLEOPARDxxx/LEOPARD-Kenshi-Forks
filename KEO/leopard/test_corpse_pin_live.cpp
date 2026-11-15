// The actual detour is inserted by the runner; only native objects are fakes.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "fixes/world/corpse_pin_policy.h"
namespace Ogre { struct Vector3 { float x,y,z; }; }
static unsigned queries, deadQueries, originalCalls, resolves, positions, checks;
enum { CHARACTER=1 };
struct Character;
struct FakeHand { Character* p; Character* getCharacter(){++resolves;return p;} };
struct RootObject { void** vt; int kind; int getDataType(){++queries;return kind;} };
struct Character : RootObject {
 bool dead,_isBeingCarried; FakeHand carryingObject;
 bool isDead(){++deadQueries;return dead;}
};
struct ActivePlatoon { struct { RootObject** stuff; unsigned count; } things; };
namespace fixes { struct Cfg { bool corpsePinEnabled; } g_fixesCfg; }
static const int kMaxScanMembers=128;
static volatile LONG s_calls,s_noCarrierCount,s_overrideCount;
typedef Ogre::Vector3* (__fastcall *GetPositionFn)(void*,Ogre::Vector3*);
static const size_t kGetPositionVtableOffset=0x40;
static Ogre::Vector3 original={1,2,3};
static Ogre::Vector3* __fastcall Position(void*,Ogre::Vector3*out){++positions;out->x=4;out->y=5;out->z=6;return out;}
static Ogre::Vector3* Native(ActivePlatoon*,Ogre::Vector3*){++originalCalls;return &original;}
static Ogre::Vector3* (*orig_calculateCurrentPos)(ActivePlatoon*,Ogre::Vector3*)=Native;
// PRODUCTION_HOOK
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
static void Clear(){queries=deadQueries=originalCalls=resolves=positions=0;}
int main(){
 Character chars[128];RootObject* members[128];memset(chars,0,sizeof(chars));
 for(unsigned i=0;i<128;++i){chars[i].kind=CHARACTER;members[i]=&chars[i];}
 ActivePlatoon p;p.things.stuff=members;p.things.count=128;Ogre::Vector3 out;
 fixes::g_fixesCfg.corpsePinEnabled=true;Clear();
 C(hook_calculateCurrentPos(&p,&out)==&original);
 C(queries==1&&deadQueries==1&&originalCalls==1&&resolves==0);
 // A carried corpse before a live member must never resolve its carrier.
 chars[0].dead=true;chars[0]._isBeingCarried=true;Clear();
 C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==2&&resolves==0);
 // No live members: retain the original all-dead and missing-carrier paths.
 for(unsigned i=0;i<128;++i){chars[i].dead=true;chars[i]._isBeingCarried=false;}
 Clear();C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==128&&resolves==0);
 chars[7]._isBeingCarried=true;Clear();LONG old=s_noCarrierCount;
 C(hook_calculateCurrentPos(&p,&out)==&original);C(resolves==1&&s_noCarrierCount==old+1);
 Character carrier;void*vt[9]={0};vt[8]=(void*)&Position;carrier.vt=vt;chars[7].carryingObject.p=&carrier;Clear();old=s_overrideCount;
 C(hook_calculateCurrentPos(&p,&out)==&out);C(positions==1&&resolves==1&&s_overrideCount==old+1);C(out.x==4&&out.y==5&&out.z==6);
 fixes::g_fixesCfg.corpsePinEnabled=false;Clear();
 C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==0&&originalCalls==1);
 Clear();C(hook_calculateCurrentPos(NULL,&out)==&original);C(queries==0&&originalCalls==1);
 fixes::g_fixesCfg.corpsePinEnabled=true;Clear();p.things.stuff=NULL;
 C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==0&&originalCalls==1);
 p.things.stuff=members;p.things.count=0;Clear();
 C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==0&&originalCalls==1);
 p.things.count=129;chars[7]._isBeingCarried=false;Clear();
 C(hook_calculateCurrentPos(&p,&out)==&original);C(queries==128&&originalCalls==1);
 std::printf("PASS %u actual corpse detour controls\n",checks);return 0;
}
