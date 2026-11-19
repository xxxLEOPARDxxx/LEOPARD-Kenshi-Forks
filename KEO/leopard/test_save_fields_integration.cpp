#define NOMINMAX
#include <windows.h>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "save_gog_test_fixture.h"
namespace fixes{struct Config{bool saveStringTransferEnabled;};Config g_fixesCfg={true};}
#define KEO_FIELD_KEY_PATCH_TEST
#include "../src/fixes/world/save_field_key_patch.cpp"
static bool mainThread=true;static DWORD testStamp=0x65d604d7,testSize=0x232c000;static void*world=NULL;
static bool IsMainThread(){return mainThread;}
static bool ReadModuleImageId(HMODULE m,DWORD*s,DWORD*n){if(!m)return false;*s=testStamp;*n=testSize;return true;}
static uintptr_t GameAddr(unsigned){return (uintptr_t)&world;}
#define RVA_GLOBAL_SECTION_MGR 1
namespace save_field_key_patch{
#include "../build/save_fields_integration/admission.inc"
}
static unsigned checks=0,originals=0,keys=0;static bool throwOriginal=false,sehOriginal=false;
#define C(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static std::string*__fastcall Original(std::string*d,const std::string*s,size_t off,size_t n){++originals;if(throwOriginal)throw std::runtime_error("native fallback");if(sehOriginal)RaiseException(0xe1234567,0,0,NULL);d->assign(*s,off,n);return d;}
static void*__fastcall Key(void*d,const char*s,size_t n){++keys;((std::string*)d)->assign(s,n);return d;}
typedef std::string*(__fastcall*Value)(std::string*,const std::string*,size_t,size_t);
typedef void*(__fastcall*KeyDriver)(void*,const char*,size_t,void*);
static bool Seh(Value f,std::string*d,const std::string*s){__try{f(d,s,0,std::string::npos);return false;}__except(GetExceptionCode()==0xe1234567?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
int main(){
 using namespace save_field_key_patch;
 SaveGogTestImage gog;
 if(gog.requested){
  C(gog.bytes);testStamp=0x65d60519;testSize=0x232a000;
  C(Admit(gog.Module())&&gogProfile&&original==gog.bytes+0x69cd0&&(void*)valueOriginal==gog.bytes+0x69b80&&valueReturn==(uintptr_t)gog.bytes+0x6c950d);
  for(unsigned i=0;i<7;++i)C(memcmp(gog.bytes+gogSitesRva[i],gogCallBytes[i],5)==0);
  for(size_t i=0;i<sizeof(gogProofs)/sizeof(gogProofs[0]);++i){
   unsigned char&byte=gog.bytes[gogProofs[i].rva];byte^=1;C(!Admit(gog.Module()));byte^=1;
  }
  *(void**)(gog.bytes+save_gog_native::sectionManagerRva)=gog.bytes;C(!Admit(gog.Module()));
  testStamp=0x65d604d7;testSize=0x232c000;
 }
 unsigned char*image=(unsigned char*)VirtualAlloc(NULL,0x232c000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(image!=NULL);
 for(unsigned i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)memcpy(image+proofs[i].rva,proofs[i].bytes,proofs[i].size);
 original=NULL;C(!Admit(NULL)&&original==NULL);C(!Admit((HMODULE)1)&&original==NULL);mainThread=false;C(!Admit((HMODULE)image)&&original==NULL);mainThread=true;
 world=image;C(!Admit((HMODULE)image)&&original==NULL);world=NULL;++testStamp;C(!Admit((HMODULE)image)&&original==NULL);--testStamp;++testSize;C(!Admit((HMODULE)image)&&original==NULL);--testSize;
 for(unsigned i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)for(unsigned edge=0;edge<2;++edge){unsigned char*p=image+proofs[i].rva+(edge?proofs[i].size-1:0);*p^=1;C(!Admit((HMODULE)image)&&original==NULL);*p^=1;}
 C(Admit((HMODULE)image)&&original==image+0x69cd0&&(void*)valueOriginal==image+0x69b80&&valueReturn==(uintptr_t)image+0x6c9bfd);VirtualFree(image,0,MEM_RELEASE);
 unsigned char*code=AllocateNearPages((unsigned __int64)(uintptr_t)&Original,4096);C(code!=NULL);
 unsigned char*sites[7];unsigned char expected[7][5];RUNTIME_FUNCTION functions[7]={0};
 const unsigned char keyBody[]={0x56,0x48,0x83,0xec,0x20,0x4c,0x89,0xce,0xe8,0,0,0,0,0x48,0x83,0xc4,0x20,0x5e,0xc3};
 const unsigned char valBody[]={0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3};
 const unsigned char ku[]={1,5,2,0,5,0x32,1,0x60},vu[]={1,4,1,0,4,0x42,0,0};memcpy(code+0x300,ku,8);memcpy(code+0x308,vu,8);
 for(unsigned i=0;i<7;++i){unsigned char*f=code+i*64;memcpy(f,i<6?keyBody:valBody,i<6?sizeof(keyBody):sizeof(valBody));sites[i]=f+(i<6?8:4);const __int64 delta=(__int64)(uintptr_t)(i<6?(void*)&Key:(void*)&Original)-(__int64)(uintptr_t)(sites[i]+5);C(delta>=-2147483647LL-1&&delta<=2147483647LL);const int rel=(int)delta;memcpy(sites[i]+1,&rel,4);memcpy(expected[i],sites[i],5);functions[i].BeginAddress=i*64;functions[i].EndAddress=i*64+(i<6?sizeof(keyBody):sizeof(valBody));functions[i].UnwindData=i<6?0x300:0x308;}
 DWORD old=0;C(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)!=0);C(RtlAddFunctionTable(functions,7,(DWORD64)code)!=0);
 original=(void*)&Key;valueOriginal=Original;ownerThread=GetCurrentThreadId();valueReturn=(uintptr_t)sites[6]+5;admitted=true;volatile bool enabled=true;
 C(code_patch_threads::Prepare("build/save_fields_integration/emergency.log"));C(Arm(sites,expected,&enabled));Value call=(Value)(code+6*64);
 for(unsigned mode=0;mode<7;++mode){std::string src(mode==0?15:47,'x'),dst;const char*buffer=src.data();originals=0;
  if(mode==1)fixes::g_fixesCfg.saveStringTransferEnabled=false;if(mode==2)admitted=false;if(mode==3)++valueReturn;if(mode==4)++ownerThread;
  C(call(&dst,&src,mode==5?2:0,mode==6?3:std::string::npos)==&dst);
  C(originals==1);C(src.size()==(mode==0?15:47));C(dst.size()==(mode==0?15:mode==5?45:mode==6?3:47));
  fixes::g_fixesCfg.saveStringTransferEnabled=true;admitted=true;valueReturn=(uintptr_t)sites[6]+5;ownerThread=GetCurrentThreadId();C(src.data()==buffer);
 }
 {std::string src(47,'x'),dst;src[3]=0;const char*buffer=src.data();originals=0;C(call(&dst,&src,0,std::string::npos)==&dst&&originals==0&&dst.data()==buffer&&src.empty()&&src.capacity()==15&&dst[3]==0&&dst.size()==47);}
 {std::string src(47,'x'),dst("old");originals=0;C(call(&dst,&src,0,std::string::npos)==&dst&&originals==1&&src.size()==47&&dst.size()==47);}
 {std::string src(47,'x'),dst;fixes::g_fixesCfg.saveStringTransferEnabled=false;throwOriginal=true;originals=0;bool caught=false;try{call(&dst,&src,0,std::string::npos);}catch(const std::runtime_error&){caught=true;}throwOriginal=false;C(caught&&originals==1&&src.size()==47&&dst.empty());sehOriginal=true;originals=0;C(Seh(call,&dst,&src)&&originals==1&&src.size()==47&&dst.empty());sehOriginal=false;fixes::g_fixesCfg.saveStringTransferEnabled=true;}
 __declspec(align(16)) unsigned char permission[64]={0};for(unsigned i=0;i<6;++i){std::string dst;keys=0;C(((KeyDriver)(code+i*64))(&dst,"abcdefghijklmnop",16,permission)==&dst&&dst.empty()&&keys==0);*(uint64_t*)(permission+32)=1;C(((KeyDriver)(code+i*64))(&dst,"abcdefghijklmnop",16,permission)==&dst&&dst.size()==16&&keys==1);*(uint64_t*)(permission+32)=0;}
 // Even if the OS cannot roll back a partial publication, each authenticated
 // key leaf is independent. Value calls remain native until the batch is ready.
 admitted=false;{std::string dst,src(47,'v'),key;keys=originals=0;C(((KeyDriver)code)(&key,"abcdefghijklmnop",16,permission)==&key&&key.empty()&&keys==0);C(call(&dst,&src,0,std::string::npos)==&dst&&originals==1&&src.size()==47);}
 // Restore all seven CALLs atomically before freeing either executable page.
 code_patch_batch::Edit undo[7]={0};for(unsigned i=0;i<7;++i){undo[i].address=sites[i];undo[i].size=5;memcpy(undo[i].before,sites[i],5);memcpy(undo[i].after,expected[i],5);}bool possible=false;C(code_patch_batch::Apply(undo,7,&threads,&possible));VirtualFree(page,0,MEM_RELEASE);page=NULL;RtlDeleteFunctionTable(functions);VirtualFree(code,0,MEM_RELEASE);printf("PASS save fields integration: %u checks\n",checks);return 0;
}
