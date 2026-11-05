#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "game/native_profile.h"
#include "game/gog_profile.h"
#include <Windows.h>
#include <wincrypt.h>
#include <string.h>
#include <stdio.h>
#pragma comment(lib,"advapi32.lib")
namespace {
static uintptr_t verifiedGogBase=0;
static bool gogRequested=false;
static const char sdkSha[]="5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b";
// Fixed-size bounded streaming read. Owned file/hash/provider are released on
// every exit. No native call, network, persistent buffer or per-frame I/O.
bool FileSha(const wchar_t*path,char out[65]) {
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=false;BY_HANDLE_FILE_INFORMATION before={},after={};
 unsigned char buffer[16384],digest[32];DWORD size=sizeof(digest),count=0;unsigned __int64 total=0;
 if(GetFileInformationByHandle(file,&before)&&!before.nFileSizeHigh&&before.nFileSizeLow>0&&before.nFileSizeLow<128u*1024u*1024u
  &&CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
  ok=true;
  for(;;) { if(!ReadFile(file,buffer,sizeof(buffer),&count,NULL)){ok=false;break;} if(!count)break;
   total+=count;if(total>before.nFileSizeLow||!CryptHashData(hash,buffer,count,0)){ok=false;break;}}
  ok=ok&&total==before.nFileSizeLow&&GetFileInformationByHandle(file,&after)
   &&before.dwVolumeSerialNumber==after.dwVolumeSerialNumber&&before.nFileIndexHigh==after.nFileIndexHigh
   &&before.nFileIndexLow==after.nFileIndexLow&&before.nFileSizeLow==after.nFileSizeLow
   &&CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)==0
   &&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==sizeof(digest);
 }
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
 if(ok){const char*hex="0123456789abcdef";for(unsigned i=0;i<32;++i){out[2*i]=hex[digest[i]>>4];out[2*i+1]=hex[digest[i]&15];}out[64]=0;}
 return ok;
}
bool Headers(uintptr_t base,uint32_t*stamp,uint32_t*size) {
 __try {const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)base;
  if(!base||d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;
  const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)(base+d->e_lfanew);
  if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||n->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
  *stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ModuleSha(HMODULE module,char out[65]) {wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);return n>0&&n<32768&&FileSha(path,out);}
}
bool NativeProfilePrepareGog(uintptr_t base,void(*log)(const char*)) {
 if(verifiedGogBase)return verifiedGogBase==base;
 gogRequested=true;
 uint32_t stamp=0,size=0;char exe[65]={0},sdk[65]={0};HMODULE lib=GetModuleHandleW(L"KenshiLib.dll");
 if(!Headers(base,&stamp,&size)||!ModuleSha((HMODULE)base,exe)||!GogProfile::MatchesImage(stamp,size,exe)
  ||!lib||!ModuleSha(lib,sdk)||strcmp(sdk,sdkSha)!=0) {
  if(log){char message[240];_snprintf_s(message,sizeof(message),_TRUNCATE,"GOG native profile refused: EXE SHA256=%s SDK SHA256=%s; no native hooks installed",exe[0]?exe:"unreadable",sdk[0]?sdk:"unreadable");log(message);}return false;
 }
 verifiedGogBase=base;return true;
}
bool NativeProfileIsGog(){return verifiedGogBase!=0;}
uintptr_t NativeProfileAddress(uintptr_t base,uintptr_t key){
 if(!gogRequested)return base+key;
 if(!verifiedGogBase)return 0;
 uintptr_t result=0;return base==verifiedGogBase&&GogProfile::Absolute(base,key,&result)?result:0;
}
uintptr_t NativeProfileCanonical(uintptr_t rva){if(!gogRequested)return rva;if(!verifiedGogBase)return 0;uintptr_t key=0;return GogProfile::CanonicalExact(rva,&key)?key:0;}
const unsigned char* NativeProfileBytes(uintptr_t key,const unsigned char*steam,size_t count){return gogRequested?(verifiedGogBase?GogProfile::ExpectedBytes(key,count):NULL):steam;}

bool NativeProfileMatches(uintptr_t key,const void*actual,const unsigned char*steam,size_t count){const unsigned char*expected=NativeProfileBytes(key,steam,count);return actual&&expected&&memcmp(actual,expected,count)==0;}
