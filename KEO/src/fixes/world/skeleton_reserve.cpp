#include "fixes/world/skeleton_reserve.h"
#include <ogre/OgreSkeleton.h>
#include <cstring>
#include <cstddef>
#ifndef KEO_SKELETON_RESERVE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace skeleton_reserve {
struct BonesAccess:Ogre::Skeleton {
    using Ogre::Skeleton::mBoneList;
    static BoneList& Bones(Ogre::Skeleton* p){BoneList Ogre::Skeleton::*member=static_cast<BoneList Ogre::Skeleton::*>(&BonesAccess::mBoneList);return p->*member;}
};
static_assert(offsetof(BonesAccess,mBoneList)==0x178,"native bone vector layout");
static_assert(sizeof(Ogre::Skeleton::BoneList)==32,"native VC10 bone vector ABI");

typedef void(*LoadFn)(void*);
static LoadFn original=NULL;
static bool admitted=false,tried=false;
#ifndef KEO_SKELETON_RESERVE_OPERATION
#define KEO_SKELETON_RESERVE_OPERATION(list,span) (list).reserve(span)
#endif
static void Prepare(void*self){
    if(!self)return;
    try{
        // This game's OldSkeletonInstance tail differs from the SDK. Do NOT
        // access its declared mSkeleton, construct that class, or copy its
        // SharedPtr. The admitted native loadImpl itself reads master at290.
        Ogre::Skeleton*master=NULL;
        memcpy(&master,static_cast<const char*>(self)+0x290,sizeof(master));
        Ogre::Skeleton*destination=static_cast<Ogre::Skeleton*>(self);
        if(!master||master==destination||!master->isLoaded())return;
        Ogre::Skeleton::BoneList&bones=BonesAccess::Bones(destination);
        const size_t span=BonesAccess::Bones(master).size();
        // Native createBone handles are limited to0..255. Size is the full
        // handle span, including holes; reserve changes no logical element.
        if(bones.empty()&&span>0&&span<=256&&bones.capacity()<span)
            KEO_SKELETON_RESERVE_OPERATION(bones,span);
    }catch(...){
        try{LogError("Skeleton preparation: storage reserve failed; native loading retained");}catch(...){} // diagnostics cannot prevent native fallback
    }
}
static void Load(void*self){
    if(IsMainThread()&&admitted&&fixes::g_fixesCfg.skeletonReserveEnabled)Prepare(self);
    original(self);
}

struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
static const unsigned char proof0[]={0x40,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xf9,0x48,0x8b,0x89,0x90,0x02,0x00,0x00,0x48,0x8d,0x54,0x24,0x20,0x0f,0xb7,0x81,0xe0,0x01,0x00,0x00,0x66,0x89,0x87,0xe0};
static const unsigned char proof1[]={0x48,0x89,0xb3,0x78,0x01,0x00,0x00,0x48,0x89,0xb3,0x80,0x01,0x00,0x00,0x48,0x89,0xb3,0x88,0x01,0x00,0x00};
static const unsigned char proof2[]={0x49,0x8b,0x85,0x80,0x01,0x00,0x00,0x49,0x2b,0x85,0x78,0x01,0x00,0x00,0x48,0xc1,0xf8,0x03,0x49,0x3b,0xc4,0x77,0x14,0x0f,0xb7,0xc6,0xff,0xc0,0x48,0x63,0xd0,0x49,0x8d,0x8d,0x78,0x01,0x00,0x00,0xe8,0xe7,0x3b,0x00,0x00};
static const unsigned char proof3[]={0x48,0x89,0x87,0x90,0x02,0x00,0x00,0x48,0x8b,0x43,0x08,0x48,0x89,0x87,0x98,0x02,0x00,0x00};
static const Proof proofs[]={{0x1e6db0,proof0,sizeof(proof0)},{0x342081,proof1,sizeof(proof1)},{0x342dce,proof2,sizeof(proof2)},{0x1e6851,proof3,sizeof(proof3)}};
static const char*const loadSymbol="?loadImpl@OldSkeletonInstance@Ogre@@MEAAXXZ";
#ifndef KEO_SKELETON_MODULE
#define KEO_SKELETON_MODULE() GetModuleHandleA("OgreMain_x64.dll")
#endif
#ifndef KEO_SKELETON_SYMBOL
#define KEO_SKELETON_SYMBOL(module) GetProcAddress(module,loadSymbol)
#endif
static void*Admit(HMODULE module){
    __try{
        DWORD stamp=0,size=0;
        if(!module||!ReadModuleImageId(module,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000)return NULL;
        void*target=(void*)KEO_SKELETON_SYMBOL(module);
        if(target!=(char*)module+0x1e6db0)return NULL;
        for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)
            if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))return NULL;
        return target;
    }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
void Install(){
    if(tried)return;tried=true;
    void*target=Admit(KEO_SKELETON_MODULE());
    if(!target){LogError("Skeleton preparation: native layout/function contract refused; native loading retained");return;}
    // Keep any published trampoline on failure: the SDK can fail after creating
    // one. An unadmitted wrapper must remain pure forwarding, never retry.
    if(KenshiLib::AddHook(target,(void*)&Load,(void**)&original)!=KenshiLib::SUCCESS||!original){
        LogError("Skeleton preparation: hook failed; native loading retained");return;
    }
    admitted=true;KEO_DETAIL("Skeleton preparation: bounded native bone-storage reservation admitted");
}
}
void InstallSkeletonReserve(int*,int*){skeleton_reserve::Install();}
