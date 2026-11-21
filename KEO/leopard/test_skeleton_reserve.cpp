#define KEO_SKELETON_RESERVE_TEST
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreOldSkeletonInstance.h>
#include <ogre/OgreOldBone.h>
#include <cstdio>
#include <stdexcept>
#include <cmath>
static bool owner=true;
bool IsMainThread(){return owner;}
namespace fixes {struct Config{bool skeletonReserveEnabled;};Config g_fixesCfg={true};}
static unsigned errors=0,reserves=0,hookCalls=0;
static bool reserveFault=false,hookFailure=false,fakeSymbolWrong=false,throwLogger=false;
static HMODULE moduleOverride=NULL;
static bool overrideModule=false;
void LogError(const std::string&){++errors;if(throwLogger)throw std::bad_alloc();}
#define KEO_DETAIL(x) ((void)0)
static void Native(void*);
namespace KenshiLib {enum Status{SUCCESS,FAILURE};Status AddHook(void*,void*,void**p){++hookCalls;*p=(void*)&Native;return hookFailure?FAILURE:SUCCESS;}}
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){
 const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;
 const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((const char*)m+d->e_lfanew);
 if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;
 *stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;
}
static void ReserveOperation(Ogre::Skeleton::BoneList&list,size_t span){++reserves;if(reserveFault)throw std::bad_alloc();list.reserve(span);}
#define KEO_SKELETON_RESERVE_OPERATION(list,span) ReserveOperation(list,span)
#define KEO_SKELETON_MODULE() (overrideModule?moduleOverride:GetModuleHandleA("OgreMain_x64.dll"))
#define KEO_SKELETON_SYMBOL(module) (moduleOverride==module?(FARPROC)((char*)module+(fakeSymbolWrong?0x1e6db1:0x1e6db0)):GetProcAddress(module,loadSymbol))
#ifndef KEO_SKELETON_SOURCE
#define KEO_SKELETON_SOURCE "../src/fixes/world/skeleton_reserve.cpp"
#endif
#include KEO_SKELETON_SOURCE
static unsigned growth=0;
typedef Ogre::OldBone*(*Unnamed)(Ogre::Skeleton*,unsigned short);
typedef Ogre::OldBone*(*Named)(Ogre::Skeleton*,const Ogre::String&,unsigned short);
static Unnamed unnamed=NULL;static Named named=NULL;
static Ogre::OldBone* UnnamedProbe(Ogre::Skeleton*s,unsigned short h){size_t before=skeleton_reserve::BonesAccess::Bones(s).capacity();Ogre::OldBone*b=unnamed(s,h);growth+=before!=skeleton_reserve::BonesAccess::Bones(s).capacity();return b;}
static Ogre::OldBone* NamedProbe(Ogre::Skeleton*s,const Ogre::String&n,unsigned short h){size_t before=skeleton_reserve::BonesAccess::Bones(s).capacity();Ogre::OldBone*b=named(s,n,h);growth+=before!=skeleton_reserve::BonesAccess::Bones(s).capacity();return b;}
// SDK OldSkeletonInstance member order differs from this game's fork. Invoke
// the actual constructor in sufficiently large owned placement storage; never
// instantiate an SDK-derived skeleton instance with that incompatible layout.
struct Probe {
 Ogre::OldSkeletonInstance*p;void**originalVT;void*vt[128];unsigned grows;
 Probe(const Ogre::SkeletonPtr&s):p(NULL),grows(0){
  p=(Ogre::OldSkeletonInstance*)Ogre::NedPoolingPolicy::allocateBytes(0x2f8+64,__FILE__,__LINE__,__FUNCTION__);
  memset((char*)p+0x2f8,0x5a,64);
  typedef void*(*Ctor)(void*,const Ogre::SkeletonPtr&);
  Ctor ctor=(Ctor)GetProcAddress(GetModuleHandleA("OgreMain_x64.dll"),"??0OldSkeletonInstance@Ogre@@QEAA@AEBV?$SharedPtr@VSkeleton@Ogre@@@1@@Z");ctor(p,s);
  originalVT=*(void***)p;memcpy(vt,originalVT,sizeof(vt));unnamed=(Unnamed)vt[0x178/8];named=(Named)vt[0x168/8];vt[0x178/8]=(void*)UnnamedProbe;vt[0x168/8]=(void*)NamedProbe;*(void***)p=vt;
 }
 bool Canary()const{for(unsigned i=0;i<64;++i)if(((const unsigned char*)p)[0x2f8+i]!=0x5a)return false;return true;}
 ~Probe(){*(void***)p=originalVT;p->~OldSkeletonInstance();Ogre::NedPoolingPolicy::deallocateBytes(p);}
};
static unsigned calls=0;
static skeleton_reserve::LoadFn nativeLoad=NULL;
static void Native(void*p){++calls;nativeLoad(p);}
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::printf("FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static bool throwNative=false;
static void*expectedSelf=NULL,*lastSelf=NULL;
static void Spy(void*p){++calls;lastSelf=p;if(throwNative)throw std::runtime_error("native failure");}
typedef void*(*MasterCtor)(void*,Ogre::ResourceManager*,const Ogre::String&,Ogre::ResourceHandle,const Ogre::String&,bool,Ogre::ManualResourceLoader*);
static MasterCtor masterCtor=NULL;
struct Loader:Ogre::ManualResourceLoader{void loadResource(Ogre::Resource*){}};
static Loader loader;
static Ogre::SkeletonPtr Master(){
 void*space=Ogre::NedPoolingPolicy::allocateBytes(4096,__FILE__,__LINE__,__FUNCTION__);
 masterCtor(space,NULL,Ogre::String("reserve-master"),1,Ogre::String("General"),true,&loader);
 Ogre::SkeletonPtr result((Ogre::Skeleton*)space);result->load();return result;
}
static int Case(unsigned count,unsigned names,bool reverse){
 Ogre::SkeletonPtr master=Master();std::vector<Ogre::OldBone*>created;
 for(unsigned i=0;i<count;++i){unsigned short h=(unsigned short)(reverse?count-1-i:i);char name[32];sprintf_s(name,"bone%u",h);
  Ogre::OldBone*b=(names==1||(names==2&&h%2==0))?master->createBone(name,h):master->createBone(h);
  b->setPosition(Ogre::Vector3(float(h)/10,1,2));b->setScale(Ogre::Vector3(1,1.01f,1.02f));
  b->setOrientation(Ogre::Quaternion(.9238795f,0,.3826834f,0));
  if(!created.empty())created[(i-1)/2]->addChild(b);created.push_back(b);
 }
 master->setBindingPose();
 fixes::g_fixesCfg.skeletonReserveEnabled=false;Probe baseline(master);growth=reserves=0;skeleton_reserve::Load(baseline.p);baseline.grows=growth;
 fixes::g_fixesCfg.skeletonReserveEnabled=true;Probe optimized(master);growth=reserves=0;skeleton_reserve::Load(optimized.p);optimized.grows=growth;
 C(baseline.Canary()&&optimized.Canary());C(optimized.grows==0);C(reserves==1);C(optimized.p->getNumBones()==baseline.p->getNumBones());
 if(count==128&&!reverse)std::printf("native storage allocation changes (%u names): baseline=%u prepared=%u+%u\n",names,baseline.grows,reserves,optimized.grows);
 for(unsigned short h=0;h<count;++h){Ogre::OldBone*a=baseline.p->getBone(h),*b=optimized.p->getBone(h);
  C(a!=b);C(a->getHandle()==b->getHandle());C(a->getName()==b->getName());C(a->getPosition()==b->getPosition());C(a->getScale()==b->getScale());C(a->getOrientation()==b->getOrientation());C(a->numChildren()==b->numChildren());
  C((a->getParent()==NULL)==(b->getParent()==NULL));
  if(a->getParent())C(((Ogre::OldBone*)a->getParent())->getHandle()==((Ogre::OldBone*)b->getParent())->getHandle());
  Ogre::Matrix4 x,y;a->_getOffsetTransform(x);b->_getOffsetTransform(y);C(x==y);
 }
 Ogre::Skeleton::BoneIterator a=baseline.p->getRootBoneIterator(),b=optimized.p->getRootBoneIterator();
 while(a.hasMoreElements()&&b.hasMoreElements())C(a.getNext()->getHandle()==b.getNext()->getHandle());
 C(!a.hasMoreElements()&&!b.hasMoreElements());
 // An adequate existing allocation is untouched, with no hidden reserve call.
 Probe enough(master);skeleton_reserve::BonesAccess::Bones((Ogre::Skeleton*)enough.p).reserve(256);
 growth=reserves=0;skeleton_reserve::Load(enough.p);C(growth==0&&reserves==0);
 return 0;
}
int main(){
 Ogre::Root root("","","");HMODULE module=GetModuleHandleA("OgreMain_x64.dll");
 nativeLoad=(skeleton_reserve::LoadFn)GetProcAddress(module,"?loadImpl@OldSkeletonInstance@Ogre@@MEAAXXZ");C(nativeLoad!=NULL);
 masterCtor=(MasterCtor)GetProcAddress(module,"??0Skeleton@Ogre@@QEAA@PEAVResourceManager@1@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_K1_NPEAVManualResourceLoader@1@@Z");C(masterCtor!=NULL);
 skeleton_reserve::original=Spy;owner=false;expectedSelf=(void*)1;calls=0;skeleton_reserve::Load(expectedSelf);C(calls==1&&lastSelf==expectedSelf);owner=true;
 skeleton_reserve::original=Native;skeleton_reserve::admitted=true;
 for(unsigned names=0;names<3;++names)for(unsigned k=0;k<4;++k){unsigned counts[]={1,8,128,256};C(Case(counts[k],names,false)==0);}
 C(Case(128,2,true)==0);
 // Real empty source and reserve failure, then exactly one original native load.
 Ogre::SkeletonPtr empty=Master();Probe noBones(empty);growth=reserves=0;skeleton_reserve::original=Spy;expectedSelf=noBones.p;skeleton_reserve::Load(noBones.p);C(reserves==0&&noBones.p->getNumBones()==0);skeleton_reserve::original=Native;
 Ogre::SkeletonPtr master=Master();for(unsigned short i=0;i<80;++i)master->createBone(i);master->setBindingPose();
 Probe failed(master);reserveFault=true;growth=reserves=errors=0;unsigned before=calls;skeleton_reserve::Load(failed.p);reserveFault=false;C(errors==1&&reserves==1&&growth>1&&calls==before+1);C(failed.p->getNumBones()==80);
 // A failing allocation may also make error formatting throw. Native must
 // still run once and own its normal result.
 Probe failedLog(master);reserveFault=throwLogger=true;growth=reserves=errors=0;before=calls;bool unexpected=false;try{skeleton_reserve::Load(failedLog.p);}catch(...){unexpected=true;}reserveFault=throwLogger=false;C(!unexpected);C(errors==1&&reserves==1&&growth>1&&calls==before+1);C(failedLog.p->getNumBones()==80);
 // Gates/metadata-only cases deliberately use a spy instead of invoking native
 // loading on malformed or partially prepared fixture objects.
 skeleton_reserve::original=Spy;errors=reserves=0;
 for(unsigned kind=0;kind<3;++kind){Probe gated(master);expectedSelf=gated.p;before=calls;if(kind==0)owner=false;if(kind==1)skeleton_reserve::admitted=false;if(kind==2)fixes::g_fixesCfg.skeletonReserveEnabled=false;skeleton_reserve::Load(expectedSelf);C(calls==before+1&&reserves==0);owner=true;skeleton_reserve::admitted=true;fixes::g_fixesCfg.skeletonReserveEnabled=true;}
 expectedSelf=NULL;before=calls;skeleton_reserve::Load(NULL);C(calls==before+1&&reserves==0);
 Ogre::SkeletonPtr nil;Probe nullMaster(nil);expectedSelf=nullMaster.p;reserves=0;skeleton_reserve::Load(nullMaster.p);C(reserves==0);
 // A source still being prepared is not inspected or reserved. The spy
 // deliberately avoids asking native loadImpl to load this incomplete master.
 Ogre::SkeletonPtr unloaded=Master();unloaded->unload();unloaded->createBone((unsigned short)0);Probe incomplete(unloaded);expectedSelf=incomplete.p;reserves=0;skeleton_reserve::Load(incomplete.p);C(reserves==0);
 // Sparse source span255+1: holes must be included, never size==bone count.
 Ogre::SkeletonPtr sparse=Master();sparse->createBone((unsigned short)255);Probe holes(sparse);expectedSelf=holes.p;reserves=0;skeleton_reserve::Load(holes.p);C(reserves==1);C(skeleton_reserve::BonesAccess::Bones((Ogre::Skeleton*)holes.p).empty());C(skeleton_reserve::BonesAccess::Bones((Ogre::Skeleton*)holes.p).capacity()==256);
 expectedSelf=failed.p;reserves=0;skeleton_reserve::Load(failed.p);C(reserves==0&&failed.p->getNumBones()==80);
 Probe partial(master);((Ogre::Skeleton*)partial.p)->createBone((unsigned short)0);expectedSelf=partial.p;reserves=0;skeleton_reserve::Load(partial.p);C(reserves==0&&partial.p->getNumBones()==1);
 // Oversized metadata is refused without changing the destination.
 Ogre::SkeletonPtr huge=Master();skeleton_reserve::BonesAccess::Bones(huge.get()).resize(257,NULL);Probe over(huge);expectedSelf=over.p;reserves=0;skeleton_reserve::Load(over.p);C(reserves==0);
 expectedSelf=failed.p;throwNative=true;before=calls;try{skeleton_reserve::Load(failed.p);C(false);}catch(const std::runtime_error&){}throwNative=false;C(calls==before+1&&errors==0);
 // Admission checks actual bytes; fake image allows refusals without patching
 // the loaded engine. No native game hooks or game processes are involved.
 C(skeleton_reserve::Admit(module)==(char*)module+0x1e6db0);C(skeleton_reserve::Admit(NULL)==NULL);C(skeleton_reserve::Admit((HMODULE)1)==NULL);
 char*fake=(char*)VirtualAlloc(NULL,0x9c9000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(fake!=NULL);memcpy(fake,module,4096);
 for(unsigned i=0;i<sizeof(skeleton_reserve::proofs)/sizeof(skeleton_reserve::proofs[0]);++i){const skeleton_reserve::Proof&p=skeleton_reserve::proofs[i];memcpy(fake+p.rva,p.bytes,p.size);}
 moduleOverride=(HMODULE)fake;overrideModule=true;C(skeleton_reserve::Admit(moduleOverride)==fake+0x1e6db0);
 fakeSymbolWrong=true;C(skeleton_reserve::Admit(moduleOverride)==NULL);fakeSymbolWrong=false;
 for(unsigned i=0;i<4;++i){uintptr_t r=skeleton_reserve::proofs[i].rva;fake[r]^=1;C(skeleton_reserve::Admit(moduleOverride)==NULL);fake[r]^=1;}
 IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)(fake+((IMAGE_DOS_HEADER*)fake)->e_lfanew);nt->FileHeader.TimeDateStamp^=1;C(skeleton_reserve::Admit(moduleOverride)==NULL);nt->FileHeader.TimeDateStamp^=1;
 nt->OptionalHeader.SizeOfImage^=1;C(skeleton_reserve::Admit(moduleOverride)==NULL);nt->OptionalHeader.SizeOfImage^=1;
 skeleton_reserve::tried=skeleton_reserve::admitted=false;hookCalls=0;hookFailure=true;skeleton_reserve::Install();C(hookCalls==1&&!skeleton_reserve::admitted&&skeleton_reserve::original!=NULL);skeleton_reserve::Install();C(hookCalls==1);
 skeleton_reserve::tried=false;hookFailure=false;skeleton_reserve::Install();C(hookCalls==2&&skeleton_reserve::admitted);skeleton_reserve::Install();C(hookCalls==2);
 VirtualFree(fake,0,MEM_RELEASE);moduleOverride=NULL;overrideModule=false;
 std::printf("%u actual native skeleton checks\n",checks);return 0;
}
