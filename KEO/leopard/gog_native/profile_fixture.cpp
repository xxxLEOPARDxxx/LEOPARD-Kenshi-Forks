#include "gog_profile.h"
#include "pathfind/path_priority_publication.h"
#include "fixes/world/town_claim_policy.h"
#include "fixes/stitch/stitch_byte_guard_policy.h"
#include "fixes/physx/physx_query_guard_policy.h"
#include "inventory/dialogue_thunk_policy.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <stdint.h>
struct Section {uint32_t rva,virtualSize,disk,diskSize;};
static std::vector<unsigned char> image;static std::vector<Section> sections;static size_t reads=0;static bool failRead=false,flipByte=false;static int checks=0;
static void Check(bool value,const char*why){++checks;if(!value){printf("FAIL %s\n",why);throw 1;}}
static uint16_t U16(size_t at){Check(at+2<=image.size(),"fixture u16");uint16_t out;memcpy(&out,&image[at],2);return out;}
static uint32_t U32(size_t at){Check(at+4<=image.size(),"fixture u32");uint32_t out;memcpy(&out,&image[at],4);return out;}
static bool Read(void*,uintptr_t rva,void*out,size_t count){++reads;if(failRead)return false;for(size_t i=0;i<sections.size();++i){const Section&s=sections[i];if(rva>=s.rva&&rva-s.rva<=s.diskSize&&count<=s.diskSize-(rva-s.rva)){memcpy(out,&image[s.disk+rva-s.rva],count);if(flipByte)((unsigned char*)out)[0]^=1;return true;}}return false;}
static bool TranslatedRead(uintptr_t key,unsigned char*out,size_t count,void*){uintptr_t rva=0;return GogProfile::Translate(key,&rva)&&Read(NULL,rva,out,count);}
static uintptr_t Mapped(uintptr_t key){uintptr_t rva=0;Check(GogProfile::Translate(key,&rva),"native key maps");return rva;}
static long Rel32(const unsigned char*p){int v=0;memcpy(&v,p,4);return v;}
static void InlineFixtures(){
 Check(PathPriorityProfile(TranslatedRead,NULL,GogProfile::ExpectedBytes),"actual GOG28 priority contracts");
 flipByte=true;Check(!PathPriorityProfile(TranslatedRead,NULL,GogProfile::ExpectedBytes),"GOG priority corruption refuses");flipByte=false;
 const unsigned __int64 base=0x140000000ULL,stub=base+0x01000000,slot=stub+0x1000;unsigned char code[128],patch[16],actual[32];
 Check(TranslatedRead(kTownClaimSiteRva,actual,13,NULL)&&TownClaimBytesMatch(actual,kTownClaimSiteBytes,13),"actual GOG town site ABI");
 Check(TranslatedRead(kTownClaimLeadRva,code,5,NULL)&&TownClaimBytesMatch(code,kTownClaimLeadBytes,5),"actual GOG town lead ABI");
 Check(TranslatedRead(kTownClaimResumeRva,code,7,NULL)&&TownClaimBytesMatch(code,kTownClaimResumeBytes,7),"actual GOG town resume ABI");
 Check(TranslatedRead(kTownClaimKeepRva,code,11,NULL)&&TownClaimBytesMatch(code,kTownClaimKeepBytes,11),"actual GOG town keep ABI");
 Check(BuildTownClaimSitePatch(actual,base+Mapped(kTownClaimSiteRva),stub,patch),"GOG town patch builder");
 Check(base+Mapped(kTownClaimSiteRva)+5+Rel32(patch+1)==stub,"GOG town patch reaches stub");
 Check(BuildTownClaimStub(code,sizeof(code),stub,slot,base+Mapped(kTownClaimKeepRva),base+Mapped(kTownClaimResumeRva),NULL),"GOG town stub targets accepted");
 Check(TranslatedRead(kStitchByteSiteRva,actual,8,NULL)&&StitchByteBytesMatch(actual,kStitchByteSiteBytes,8),"actual GOG stitch ABI");
 Check(TranslatedRead(kStitchByteResumeRva,code,5,NULL)&&StitchByteBytesMatch(code,kStitchByteResumeBytes,5),"actual GOG stitch resume ABI");
 Check(TranslatedRead(kStitchByteBranchRva,code,10,NULL)&&StitchByteBytesMatch(code,kStitchByteBranchBytes,10),"actual GOG stitch branch ABI");
 Check(BuildStitchByteSitePatch(actual,base+Mapped(kStitchByteSiteRva),stub,patch),"GOG stitch patch builder");
 Check(base+Mapped(kStitchByteSiteRva)+5+Rel32(patch+1)==stub,"GOG stitch patch reaches stub");
 Check(BuildStitchByteStub(code,sizeof(code),stub,slot,base+Mapped(kStitchByteResumeRva),NULL),"GOG stitch resume accepted");
 uintptr_t phys=Mapped(kPhysQuerySiteRva);Check(Read(NULL,phys-2,actual,8)&&PhysQueryBytesMatch(actual,kPhysQuerySiteBlock,8),"actual GOG PhysQuery site ABI");
 Check(TranslatedRead(kPhysQueryResumeRva,code,4,NULL)&&PhysQueryBytesMatch(code,kPhysQueryResumeBytes,4),"actual GOG PhysQuery resume ABI");
 Check(TranslatedRead(kPhysQuerySkipRva,code,6,NULL)&&PhysQueryBytesMatch(code,kPhysQuerySkipBytes,6),"actual GOG PhysQuery skip ABI");
 unsigned __int64 qword=0;Check(BuildPhysQuerySiteQword(actual,base+phys,stub,&qword),"GOG PhysQuery aligned patch builder");
 memcpy(patch,&qword,8);Check(base+phys+5+Rel32(patch+3)==stub,"GOG PhysQuery patch reaches stub");
 Check(BuildPhysQueryStub(code,sizeof(code),stub,slot,base+Mapped(kPhysQueryResumeRva),base+Mapped(kPhysQuerySkipRva),NULL),"GOG PhysQuery target ABI");
 using namespace keo_inventory;
 Check(TranslatedRead(kDialogLeadRva,actual,23,NULL),"actual GOG dialog lead");
 unsigned __int64 thunk=DialogueOriginalCallTarget(base+Mapped(kDialogCallRva),actual);
 Check(thunk==base+Mapped(0x4b812),"actual GOG dialog thunk");
 Check(DialogThunkBuild(code,sizeof(code),stub),"dialog wrapper uses original ABI registers");
 int relative=0;Check(DialogCallRel32(base+Mapped(kDialogCallRva),stub,&relative),"GOG dialog patch displacement");
}

int main(int argc,char**argv){
 try{
  Check(argc==2,"actual GOG EXE fixture required");FILE*f=NULL;Check(fopen_s(&f,argv[1],"rb")==0&&f,"fixture open");fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);Check(n==36708352,"fixture size");image.resize(n);Check(fread(&image[0],1,image.size(),f)==image.size(),"fixture full read");fclose(f);
  size_t pe=U32(60),opt=pe+24,sec=opt+U16(pe+20);Check(U16(pe+4)==0x8664,"fixture AMD64");Check(GogProfile::MatchesImage(U32(pe+8),U32(opt+56),GogProfile::ExecutableSha256()),"pinned PE profile");
  for(unsigned i=0;i<U16(pe+6);++i){Section s;s.virtualSize=U32(sec+i*40+8);s.rva=U32(sec+i*40+12);s.diskSize=U32(sec+i*40+16);s.disk=U32(sec+i*40+20);Check(s.disk+s.diskSize<=image.size(),"fixture section bound");sections.push_back(s);}
  Check(!GogProfile::MatchesImage(0x65D60518,0x232A000,GogProfile::ExecutableSha256()),"timestamp refused");Check(!GogProfile::MatchesImage(0x65D60519,0x232A001,GogProfile::ExecutableSha256()),"image size refused");Check(!GogProfile::MatchesImage(0x65D60519,0x232A000,NULL),"null digest refused");Check(!GogProfile::MatchesImage(0x65D60519,0x232A000,"bad"),"wrong digest refused");
  uintptr_t out=123;Check(!GogProfile::Translate(0,&out)&&out==0,"unknown zero refused");Check(!GogProfile::Translate(0xDEADBEEF,&out)&&out==0,"unknown cannot use Steam fallback");Check(!GogProfile::Translate(1,NULL),"null output");Check(!GogProfile::Absolute(0,0xDD92F0,&out)&&out==0,"zero base refused");Check(!GogProfile::Absolute((uintptr_t)-1,0xDD92F0,&out)&&out==0,"overflow refused");
  Check(!GogProfile::Translate(0x3B40D0,&out)&&out==0,"PhysX concrete vtable is not EXE");Check(!GogProfile::Translate(0x40AF80,&out)&&out==0,"PhysX handler slot is not EXE");Check(!GogProfile::Translate(0x3B3F90,&out)&&out==0,"PhysX abstract vtable is not EXE");Check(!GogProfile::Translate(0x47624,&out)&&out==0,"PhysX purecall is not EXE");Check(GogProfile::AddressCount()==393,"explicit address count");Check(GogProfile::WindowCount()==344,"window count");Check(!GogProfile::AddressAt(GogProfile::AddressCount()),"address index bound");Check(!GogProfile::WindowKeyAt(GogProfile::WindowCount()),"window index bound");
  for(size_t i=0;i<GogProfile::AddressCount();++i){const GogProfile::AddressPair*p=GogProfile::AddressAt(i);Check(p&&GogProfile::Translate(p->from,&out)&&out==p->to,"address lookup");Check(GogProfile::CanonicalExact(p->to,&out)&&out==p->from,"exact reverse");Check(GogProfile::Absolute(0x140000000ULL,p->from,&out)&&out==0x140000000ULL+p->to,"absolute translated");}
  Check(GogProfile::Translate(0xDD92F0,&out)&&out==0xDD8A10,"edgeProcess actual GOG");Check(GogProfile::Translate(0x214853E,&out)&&out==0x21464AE,"keycode flag actual GOG");Check(GogProfile::Translate(0x212D358,&out)&&out==0x212B298,"gates singleton is not blanket delta");
  for(size_t i=0;i<GogProfile::WindowCount();++i){uintptr_t key=GogProfile::WindowKeyAt(i);Check(GogProfile::VerifyWindow(Read,NULL,key,32),"actual GOG bytes match frozen profile");Check(GogProfile::VerifyWindow(Read,NULL,key,5),"bounded prefix");flipByte=true;Check(!GogProfile::VerifyWindow(Read,NULL,key,32),"tampered window refused");flipByte=false;failRead=true;Check(!GogProfile::VerifyWindow(Read,NULL,key,32),"unreadable window refused");failRead=false;}
  size_t oldReads=reads;Check(!GogProfile::VerifyWindow(Read,NULL,0,1)&&reads==oldReads,"unknown never read");Check(!GogProfile::VerifyWindow(Read,NULL,0xDD92F0,33)&&reads==oldReads,"oversized never read");Check(!GogProfile::VerifyWindow(NULL,NULL,0xDD92F0,16)&&reads==oldReads,"missing reader");
  InlineFixtures();
  printf("PASS %d actual-production GOG profile checks; %u addresses/%u frozen windows; native code never executed\n",checks,(unsigned)GogProfile::AddressCount(),(unsigned)GogProfile::WindowCount());return 0;
 }catch(...){return 1;}
}
