// Actual native material/pass copy fixture. No game initialization.
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreTechnique.h>
#include <ogre/OgrePass.h>
#include <ogre/OgreTextureUnitState.h>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <string>
#include <stdexcept>
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
typedef int(*SdkAdd)(void*,void*,void**);static SdkAdd sdkAdd=NULL;
struct Access:Ogre::Pass{using Ogre::Pass::mTextureUnitStates;using Ogre::Pass::TextureUnitStates;
 static TextureUnitStates&States(Ogre::Pass*p){TextureUnitStates Ogre::Pass::*member=static_cast<TextureUnitStates Ogre::Pass::*>(&Access::mTextureUnitStates);return p->*member;}};
static_assert(offsetof(Access,mTextureUnitStates)==0x118,"native texture vector");
static_assert(sizeof(Access::TextureUnitStates)==32&&sizeof(Ogre::Pass)==0x210,"native material ABI");
static bool owner=true,failReserve=false,throwLog=false;static unsigned reserveCalls=0,errors=0;
static void*watchedBuffer=NULL;static unsigned freed=0;
bool IsMainThread(){return owner;}
namespace fixes{struct Config{bool materialReserveEnabled;};Config g_fixesCfg={true};}
static void LogError(const std::string&){++errors;if(throwLog)throw std::runtime_error("logger");}
#define KEO_DETAIL(x) ((void)0)
static bool fakeHooks=false;static unsigned hookCalls=0;static int fakeStatus=0;static void*fakeOriginal=NULL;
namespace KenshiLib{enum Status{SUCCESS,FAILURE};Status AddHook(void*t,void*h,void**out){++hookCalls;if(fakeHooks){*out=fakeOriginal;return (Status)fakeStatus;}return (Status)sdkAdd(t,h,out);}}
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((const char*)m+d->e_lfanew);if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;*stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;}
static bool nestedInject=false;static Ogre::Pass*nestedSource=NULL;static Ogre::Technique*nestedParent=NULL;static unsigned nestedUnits=0;
template<class List>static void Reserve(List&list,size_t n){++reserveCalls;if(failReserve)throw std::bad_alloc();if(nestedInject){nestedInject=false;Ogre::Pass nested(nestedParent,7,*nestedSource);nestedUnits=nested.getNumTextureUnitStates();}list.reserve(n);memcpy(&watchedBuffer,&list,sizeof(watchedBuffer));}
static HMODULE fakeModule=NULL;static bool useFakeModule=false,fakeSymbolFault=false;
static void*Symbol(HMODULE module,const char*name){if(module!=fakeModule)return (void*)GetProcAddress(module,name);return (char*)module+(strncmp(name,"??4Pass",7)==0?0x212020:0x211c90)+(fakeSymbolFault?1:0);}
#define KEO_MATERIAL_MODULE() (useFakeModule?fakeModule:GetModuleHandleA("OgreMain_x64.dll"))
#define KEO_MATERIAL_SYMBOL(module,symbol) Symbol(module,symbol)
#define KEO_MATERIAL_RESERVE_TEST
#define KEO_MATERIAL_RESERVE_OPERATION(list,count) Reserve(list,count)
#ifndef KEO_MATERIAL_RESERVE_SOURCE
#define KEO_MATERIAL_RESERVE_SOURCE "../src/fixes/world/material_reserve.cpp"
#endif
#include KEO_MATERIAL_RESERVE_SOURCE
static unsigned forwards=0;static void*ForwardAssign(void*,const void*){++forwards;return (void*)0x123456;}
static void*ThrowAssign(void*,const void*){throw std::runtime_error("native assignment");}
typedef void(*FreeFn)(void*,void*,int);static FreeFn originalFree=NULL;
static void FreeSpy(void*pool,void*memory,int flags){if(memory==watchedBuffer&&memory)++freed;originalFree(pool,memory,flags);}
typedef Ogre::Pass*(*ParentFn)(const Ogre::TextureUnitState*);static ParentFn NativeParent=NULL;
static unsigned grows=0;
typedef void*(*TextureCtor)(void*,Ogre::Pass*,const void*);static TextureCtor originalTextureCtor=NULL;
static void*TextureCopy(void*self,Ogre::Pass*parent,const void*other){
 void*result=originalTextureCtor(self,parent,other);
 const Access::TextureUnitStates&list=Access::States(parent);
 if(list.size()==list.capacity())++grows;
 return result;
}
static bool DiskHash(HMODULE module,const char*expected){
 char path[MAX_PATH];if(!GetModuleFileNameA(module,path,MAX_PATH))return false;
 HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);if(f==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
 if(ok)ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
 unsigned char buf[8192];DWORD got=0;
 while(ok){if(!ReadFile(f,buf,sizeof(buf),&got,NULL)){ok=false;break;}if(!got)break;ok=CryptHashData(hash,buf,got,0)!=0;}
 unsigned char digest[32];DWORD n=sizeof(digest);if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0)!=0&&n==32;
 char hex[65];if(ok){for(unsigned i=0;i<32;++i)sprintf_s(hex+i*2,3,"%02x",digest[i]);ok=strcmp(hex,expected)==0;}
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(f);return ok;
}
static int InitSdk(){
 HMODULE sdk=LoadLibraryA("KenshiLib.dll");C(sdk!=NULL);
 C(DiskHash(sdk,"5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b"));
 static const unsigned char prefix[]={0x40,0x53,0x48,0x83,0xec,0x60,0x48,0x8b,0x05,0xff,0x15,0x0d,0x00,0x48,0x33,0xc4,0x48,0x89,0x44,0x24,0x58,0x33,0xdb,0x48,0x39,0x1d,0x86,0x2b,0x0d,0x00,0x74,0x08};
 C(memcmp((char*)sdk+0x14884,prefix,sizeof(prefix))==0);
 C(*(unsigned char*)((char*)sdk+0x227f)==0xe8);
 int displacement=0;memcpy(&displacement,(char*)sdk+0x2280,4);C(0x227f+5+displacement==0x14884);
 sdkAdd=(SdkAdd)GetProcAddress(sdk,"?AddHook@KenshiLib@@YA?AW4HookStatus@1@PEAX0PEAPEAX@Z");C(sdkAdd!=NULL);
 // Private test-only MinHook initialization; never InitRVAs or game initialization.
 typedef int(*Init)();C(((Init)((char*)sdk+0x14884))()==0);return 0;
}

static int Run(Ogre::Technique*tech,unsigned n,bool prepare){
 Ogre::Pass source(tech,0);source.setAmbient(.2f,.3f,.4f);source.setDiffuse(.7f,.5f,.9f,.6f);source.setShininess(13);source.setDepthWriteEnabled(false);for(unsigned i=0;i<n;++i){char name[64];sprintf_s(name,"copy-texture-%u.dds",i);Ogre::TextureUnitState*t=source.createTextureUnitState(name);t->setTextureScroll(.25f,-.5f);t->setTextureScale(1.5f,2.5f);}
 grows=reserveCalls=0;watchedBuffer=NULL;
 {Ogre::Pass copy(tech,1,source);bool active=prepare&&n>=2&&n<=64;
  C(copy.getNumTextureUnitStates()==n);C(source.getNumTextureUnitStates()==n);C(copy.getAmbient()==source.getAmbient()&&copy.getDiffuse()==source.getDiffuse());C(copy.getShininess()==13&&!copy.getDepthWriteEnabled());
  C(reserveCalls==(active?1u:0u));unsigned baseline=n==0?0:n==1?1:n==2?2:n==3?3:n==8?6:n==16?8:12;C(grows==(active?0u:baseline));
  if(active)C(Access::States(&copy).capacity()==n);
  for(unsigned i=0;i<n;++i){Ogre::TextureUnitState*a=source.getTextureUnitState(i),*b=copy.getTextureUnitState(i);C(a!=b);C(a->getTextureName()==b->getTextureName());C(NativeParent(a)==&source&&NativeParent(b)==&copy);C(b->getTextureUScroll()==a->getTextureUScroll()&&b->getTextureVScroll()==a->getTextureVScroll());C(b->getTextureUScale()==a->getTextureUScale()&&b->getTextureVScale()==a->getTextureVScale());}
  if(n){copy.getTextureUnitState(0)->setTextureName("changed.dds");C(source.getTextureUnitState(0)->getTextureName()!="changed.dds");}
 }
 return 0;
}
int main(){setvbuf(stdout,NULL,_IONBF,0);C(InitSdk()==0);Ogre::Root root("","","");
 HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");C(DiskHash(ogre,"7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6"));
 C(material_reserve::Admit(ogre)==(char*)ogre+0x212020);C(material_reserve::Admit(NULL)==NULL);C(material_reserve::Admit((HMODULE)1)==NULL);
 fakeModule=(HMODULE)VirtualAlloc(NULL,0x9c9000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(fakeModule!=NULL);
 IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)fakeModule;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)((char*)fakeModule+0x100);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt->FileHeader.TimeDateStamp=0x5ca5f929;nt->OptionalHeader.SizeOfImage=0x9c9000;
 for(size_t i=0;i<sizeof(material_reserve::proofs)/sizeof(material_reserve::proofs[0]);++i){const material_reserve::Proof&p=material_reserve::proofs[i];memcpy((char*)fakeModule+p.rva,p.bytes,p.size);}
 C(material_reserve::Admit(fakeModule)==(char*)fakeModule+0x212020);
 nt->FileHeader.TimeDateStamp^=1;C(material_reserve::Admit(fakeModule)==NULL);nt->FileHeader.TimeDateStamp^=1;nt->OptionalHeader.SizeOfImage^=1;C(material_reserve::Admit(fakeModule)==NULL);nt->OptionalHeader.SizeOfImage^=1;
 for(size_t i=0;i<sizeof(material_reserve::proofs)/sizeof(material_reserve::proofs[0]);++i){uintptr_t r=material_reserve::proofs[i].rva;((char*)fakeModule)[r]^=1;C(material_reserve::Admit(fakeModule)==NULL);((char*)fakeModule)[r]^=1;}
 fakeSymbolFault=true;C(material_reserve::Admit(fakeModule)==NULL);fakeSymbolFault=false;
 fakeHooks=true;useFakeModule=true;for(unsigned mode=0;mode<4;++mode){material_reserve::tried=material_reserve::admitted=false;material_reserve::original=NULL;hookCalls=0;fakeStatus=mode&1;fakeOriginal=mode&2?NULL:(void*)ForwardAssign;
  material_reserve::Install(NULL,NULL);C(hookCalls==1);C((void*)material_reserve::original==fakeOriginal);C(material_reserve::admitted==(mode==0));material_reserve::Install(NULL,NULL);C(hookCalls==1);if(fakeOriginal){unsigned before=forwards,prepared=reserveCalls;C(material_reserve::Assign(NULL,NULL)==(void*)0x123456);C(forwards==before+1&&reserveCalls==prepared);}}
 fakeHooks=useFakeModule=false;VirtualFree(fakeModule,0,MEM_RELEASE);fakeModule=NULL;material_reserve::tried=material_reserve::admitted=false;material_reserve::original=NULL;material_reserve::Install(NULL,NULL);C(material_reserve::admitted&&material_reserve::original!=NULL);
 C(sdkAdd((char*)ogre+0x37a690,(void*)&TextureCopy,(void**)&originalTextureCtor)==0);
 static const unsigned char freePrefix[]={0x48,0x85,0xd2,0x0f,0x84,0x27,0x01,0x00,0x00,0x48,0x89,0x4c,0x24,0x08,0x53,0x57,0x48,0x83,0xec,0x28};C(memcmp((char*)ogre+0x1a1340,freePrefix,sizeof(freePrefix))==0);
 C(sdkAdd((char*)ogre+0x1a1340,(void*)&FreeSpy,(void**)&originalFree)==0);
 NativeParent=(ParentFn)GetProcAddress(ogre,"?getParent@TextureUnitState@Ogre@@QEBAPEAVPass@2@XZ");C(NativeParent!=NULL);
 Ogre::MaterialPtr material=Ogre::MaterialManager::getSingleton().create("material-copy-test","General");Ogre::Technique*tech=material->createTechnique();
 unsigned counts[]={0,1,2,3,8,16,64,65};for(unsigned i=0;i<sizeof(counts)/sizeof(counts[0]);++i)C(Run(tech,counts[i],true)==0);
 fixes::g_fixesCfg.materialReserveEnabled=false;C(Run(tech,8,false)==0);fixes::g_fixesCfg.materialReserveEnabled=true;
 owner=false;C(Run(tech,8,false)==0);owner=true;
 material_reserve::admitted=false;C(Run(tech,8,false)==0);material_reserve::admitted=true;
 {Ogre::Pass source(tech,0);for(unsigned i=0;i<8;++i)source.createTextureUnitState("test.dds");
  unsigned beforeErrors=errors;failReserve=true;grows=reserveCalls=0;{Ogre::Pass copy(tech,1,source);C(copy.getNumTextureUnitStates()==8);C(reserveCalls==1&&grows==6&&errors==beforeErrors+1);}failReserve=false;
  beforeErrors=errors;failReserve=throwLog=true;grows=reserveCalls=0;{Ogre::Pass copy(tech,1,source);C(copy.getNumTextureUnitStates()==8);C(reserveCalls==1&&grows==6&&errors==beforeErrors+1);}failReserve=throwLog=false;
  Ogre::Pass assigned(tech,2);reserveCalls=0;assigned=source;C(reserveCalls==0&&assigned.getNumTextureUnitStates()==8); // Wrong caller: ordinary assignment.
  {Ogre::Pass existing(tech,8);existing.createTextureUnitState("existing.dds");size_t cap=Access::States(&existing).capacity();reserveCalls=0;material_reserve::Prepare(&existing,&source);C(reserveCalls==0&&Access::States(&existing).capacity()==cap&&existing.getNumTextureUnitStates()==1);}
  size_t beforeCapacity=Access::States(&assigned).capacity();material_reserve::Prepare(&assigned,&source);C(Access::States(&assigned).capacity()==beforeCapacity); // Nonempty.
  assigned.removeAllTextureUnitStates();Access::States(&assigned).reserve(64);reserveCalls=0;material_reserve::Prepare(&assigned,&source);C(reserveCalls==0); // Already enough.
  material_reserve::Prepare(&source,&source);material_reserve::Prepare(NULL,&source);material_reserve::Prepare(&source,NULL);C(reserveCalls==0);
  material_reserve::AssignFn native=material_reserve::original;material_reserve::original=ThrowAssign;bool caught=false;reserveCalls=freed=0;watchedBuffer=NULL;
  try{Ogre::Pass copy(tech,3,source);}catch(const std::runtime_error&){caught=true;}
  material_reserve::original=native;C(caught&&reserveCalls==1&&watchedBuffer!=NULL&&freed==1);
  nestedSource=&source;nestedParent=tech;nestedInject=true;nestedUnits=0;reserveCalls=grows=0;{Ogre::Pass copy(tech,4,source);C(copy.getNumTextureUnitStates()==8&&nestedUnits==8&&reserveCalls==2&&grows==0);}nestedSource=NULL;
 }
 for(unsigned pass=0;pass<3;++pass){Ogre::Pass*p=tech->createPass();for(unsigned unit=0;unit<8;++unit)p->createTextureUnitState("graph.dds");}
 reserveCalls=grows=0;Ogre::MaterialPtr cloned=material->clone("material-cloned");C(cloned->getNumTechniques()==1&&cloned->getTechnique(0)->getNumPasses()==3);C(reserveCalls==3&&grows==0);
 for(unsigned p=0;p<3;++p){Ogre::Pass*a=tech->getPass(p),*b=cloned->getTechnique(0)->getPass(p);C(a!=b&&a->getNumTextureUnitStates()==8&&b->getNumTextureUnitStates()==8);C(a->getTextureUnitState(0)!=b->getTextureUnitState(0));}
 cloned.setNull();material.setNull();Ogre::MaterialManager::getSingleton().removeAll();Ogre::Pass::processPendingPassUpdates();
 printf("PASS %u actual native material reserve checks\n",checks);return 0;
}
