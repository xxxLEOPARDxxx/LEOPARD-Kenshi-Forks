#include <Windows.h>
#include <stdint.h>
#include <string.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include "production_native_action.inc"
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char**argv) {
 CHECK(argc==2);std::ifstream file(argv[1],std::ios::binary|std::ios::ate);CHECK(!!file);
 const size_t bytes=static_cast<size_t>(file.tellg());file.seekg(0);std::vector<unsigned char> raw(bytes);file.read(reinterpret_cast<char*>(&raw[0]),bytes);CHECK(!!file);
 const IMAGE_DOS_HEADER* d=reinterpret_cast<const IMAGE_DOS_HEADER*>(&raw[0]);
 const IMAGE_NT_HEADERS64* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(&raw[d->e_lfanew]);
 unsigned char* image=static_cast<unsigned char*>(VirtualAlloc(0,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));CHECK(image!=0);
 memcpy(image,&raw[0],nt->OptionalHeader.SizeOfHeaders);const IMAGE_SECTION_HEADER* sections=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(image+sections[i].VirtualAddress,&raw[sections[i].PointerToRawData],sections[i].SizeOfRawData);
 const intptr_t delta=reinterpret_cast<uintptr_t>(image)-nt->OptionalHeader.ImageBase;
 const IMAGE_DATA_DIRECTORY& relocation=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
 for(unsigned at=0;at<relocation.Size;) {
  IMAGE_BASE_RELOCATION* block=reinterpret_cast<IMAGE_BASE_RELOCATION*>(image+relocation.VirtualAddress+at);CHECK(block->SizeOfBlock>=8);
  const WORD* entries=reinterpret_cast<const WORD*>(block+1);
  for(unsigned n=0;n<(block->SizeOfBlock-8)/2;++n)if((entries[n]>>12)==IMAGE_REL_BASED_DIR64)
   *reinterpret_cast<uintptr_t*>(image+block->VirtualAddress+(entries[n]&0xfff))+=delta;
  at+=block->SizeOfBlock;
 }
 const uintptr_t base=reinterpret_cast<uintptr_t>(image);
#include "production_registration_bytes.inc"
 CHECK(!memcmp(image+0x60DF70,registration,sizeof(registration)));
 CHECK(!memcmp(image+0x283F40,lookupSteam,sizeof(lookupSteam)));
 // Independent native contract: IDs 294/295 exceed the stock switch's 290
 // entries, take its default Task_Blank allocation, and retain the ORIGINAL
 // registered metadata key. Native code is inspected, never called.
 const unsigned char dispatch[]={0x8d,0x43,0xff,0x45,0x33,0xed,0x3d,0x21,0x01,0,0,0x0f,0x87};
 CHECK(!memcmp(image+0x32EBE2,dispatch,sizeof(dispatch)));
 CHECK(0x32EBF3+*reinterpret_cast<const LONG*>(image+0x32EBEF)==0x330166);
 CHECK((294-1)>0x121&&(295-1)>0x121);
 CHECK(image[0x330166]==0xB9&&*reinterpret_cast<const DWORD*>(image+0x330167)==0x78);
 CHECK(image[0x330185]==0x4C&&image[0x330186]==0x8D&&image[0x330187]==0x1D);
 CHECK(0x33018C+*reinterpret_cast<const LONG*>(image+0x330188)==0x16BE4C8);
 const unsigned char metadataAssignment[]={0x48,0x8b,0x48,0x08,0x48,0x89,0x4b,0x70};
 CHECK(!memcmp(image+0x3301A4,metadataAssignment,sizeof(metadataAssignment)));
 const unsigned char saveThisAndSret[]={0x4c,0x8b,0xf2,0x48,0x8b,0xf9,0x48,0x89,0x55,0xb8};
 CHECK(!memcmp(image+0x62764B,saveThisAndSret,sizeof(saveThisAndSret)));
 const unsigned char blank[]={0xC2,0,0};
 const unsigned char stealth[]={0x48,0x8B,0xC4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x08,0xFE,0xFF,0xFF};
 const uintptr_t a=NativeActionAt(base,".?AVTask_Blank@@",blank,sizeof(blank));
 const uintptr_t ko=NativeActionAt(base,".?AVTask_StealthKO@@",stealth,sizeof(stealth));
 CHECK(a==base+0x1E65);CHECK(ko==base+0x151D6);
 CHECK(!NativeActionAt(base,".?AVMissing@@",blank,sizeof(blank)));
 unsigned char corrupt[sizeof(stealth)];memcpy(corrupt,stealth,sizeof(stealth));corrupt[3]^=1;
 CHECK(!NativeActionAt(base,".?AVTask_StealthKO@@",corrupt,sizeof(corrupt)));
 const unsigned char previous=image[0x151D6];image[0x151D6]=0x90;
 CHECK(!NativeActionAt(base,".?AVTask_StealthKO@@",stealth,sizeof(stealth)));image[0x151D6]=previous;
 IMAGE_DOS_HEADER* liveDos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);liveDos->e_magic=0;
 CHECK(!NativeActionAt(base,".?AVTask_Blank@@",blank,sizeof(blank)));
 VirtualFree(image,0,MEM_RELEASE);printf("%d offline native PE checks passed; no game code executed\n",checks);return 0;
}
