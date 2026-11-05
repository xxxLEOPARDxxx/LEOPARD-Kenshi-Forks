"""Owned-buffer execution of production exact shared-hook guard; no game calls."""
from pathlib import Path
import os,subprocess
R=Path(__file__).resolve().parents[2];O=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-steel-jobs/shared-tests');O.mkdir(parents=True,exist_ok=True)
s=(R/'src/jobs/steel_jobs_transfer.cpp').read_text(encoding='utf-8-sig');marker='static bool TransferSharedShape('
if marker in s:
 a=s.index(marker);b=s.index('{',a);n=1;e=b+1
 while n:
  if s[e]=='{':n+=1
  elif s[e]=='}':n-=1
  e+=1
 production=s[a:e]
else:production='static bool TransferSharedShape(uintptr_t,const unsigned char*,const unsigned char*,uintptr_t,const unsigned char*,uintptr_t,uintptr_t,const unsigned char*,const unsigned char*){return false;}'
code=r'''
#include <stdint.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
PRODUCTION
static unsigned checks=0;static void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
int main(){unsigned char head[32],expected[32],relay[14],target[32],targetExpected[32];for(int i=0;i<32;++i)head[i]=expected[i]=(unsigned char)(i+20),target[i]=targetExpected[i]=(unsigned char)(99-i);
 head[0]=0xE9;uintptr_t site=(uintptr_t)head,r=(uintptr_t)relay,base=(uintptr_t)target-0x1234;int32_t delta=(int32_t)(r-site-5);memcpy(head+1,&delta,4);relay[0]=0xFF;relay[1]=0x25;memset(relay+2,0,4);uintptr_t dest=(uintptr_t)target;memcpy(relay+6,&dest,8);
 C(TransferSharedShape(site,head,expected,r,relay,base,0x1234,target,targetExpected),"known exact owner E9 relay/tail/target admitted");
 for(int i=5;i<32;++i){head[i]^=1;C(!TransferSharedShape(site,head,expected,r,relay,base,0x1234,target,targetExpected),"each changed original tail byte rejected");head[i]^=1;}
 for(int i=0;i<14;++i){relay[i]^=1;C(!TransferSharedShape(site,head,expected,r,relay,base,0x1234,target,targetExpected),"each changed exact relay byte rejected");relay[i]^=1;}
 for(int i=0;i<32;++i){target[i]^=1;C(!TransferSharedShape(site,head,expected,r,relay,base,0x1234,target,targetExpected),"each changed pinned owner target byte rejected");target[i]^=1;}
 C(!TransferSharedShape(site,head,expected,r+1,relay,base,0x1234,target,targetExpected),"wrong intermediate relay rejected");
 C(!TransferSharedShape(site,head,expected,r,relay,base+1,0x1234,target,targetExpected),"wrong owner image rejected");
 C(!TransferSharedShape(site,head,expected,r,relay,base,0x1235,target,targetExpected),"wrong owner target rejected");
 C(!TransferSharedShape(~uintptr_t(0)-2,head,expected,r,relay,base,0x1234,target,targetExpected),"native next instruction overflow rejected");
 C(!TransferSharedShape(site,head,expected,r,relay,~uintptr_t(0)-4,0x1234,target,targetExpected),"owner target overflow rejected");
 C(!TransferSharedShape(site,NULL,expected,r,relay,base,0x1234,target,targetExpected),"unreadable head rejected");
 C(!TransferSharedShape(site,head,expected,r,NULL,base,0x1234,target,targetExpected),"unreadable relay rejected");
 C(!TransferSharedShape(site,head,expected,r,relay,base,0x1234,NULL,targetExpected),"unreadable target rejected");
 head[0]=0xEB;C(!TransferSharedShape(site,head,expected,r,relay,base,0x1234,target,targetExpected),"unrecognized detour shape rejected");
 printf("PASS %u actual production shared hook controls\n",checks);return 0;}
'''.replace('PRODUCTION',production)
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');W=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{V}/include;{W}/Include',LIB=f'{V}/lib/amd64;{W}/Lib/x64')
for opt in ('Od','O2'):
 file=O/(opt+'.cpp');file.write_text(code,encoding='utf-8');exe=O/(opt+'.exe')
 q=subprocess.run([str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(file),'/Fo'+str(O)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);assert not q.returncode,q.stdout+q.stderr
 q=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,q.stdout.strip());assert not q.returncode,q.stdout+q.stderr
# Full unchanged production startup/hot admission: owned executable/module images,
# real CryptoAPI reads of pinned DLL FILES, mocked loader lookup, no DLL loading.
block=s[s.index('struct TransferContract {'):s.index('static bool BindTransfer(){')]
fixture=r'''
#define NOMINMAX
#define UNICODE
#include <Windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#pragma comment(lib,"advapi32.lib")
static uintptr_t gameBase=0;static volatile LONG s_transferReady=1;
static unsigned checks=0,ioCalls=0,loaderCalls=0;static int unknownModule=-1,badFile=-1;
static std::vector<unsigned char>image,modules[3];
static void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
static bool NativeProfileIsGog(){return false;}
static BOOL MockReadFile(HANDLE h,LPVOID out,DWORD n,LPDWORD got,LPOVERLAPPED ov){++ioCalls;return ::ReadFile(h,out,n,got,ov);}
static HMODULE MockModule(const wchar_t*);static DWORD MockFile(HMODULE,wchar_t*,DWORD);
#define GetModuleHandleW MockModule
#define GetModuleFileNameW MockFile
#define ReadFile MockReadFile
PRODUCTION
#undef GetModuleHandleW
#undef GetModuleFileNameW
#undef ReadFile
static const wchar_t*paths[]={L"D:/SteamLibrary/steamapps/common/Kenshi/mods/StackSort/StackSort.dll",L"D:/SteamLibrary/steamapps/common/Kenshi/mods/KenshiRotate/KenshiRotate.dll",L"D:/SteamLibrary/steamapps/common/Kenshi/mods/KenshiExtensionPlugin/plugin/KenshiExtensionPlugin.dll"};
static HMODULE MockModule(const wchar_t*n){++loaderCalls;for(unsigned i=0;i<3;++i)if(!wcscmp(n,kTransferModules[i].name))return unknownModule==(int)i?NULL:(HMODULE)&modules[i][0];return NULL;}
static DWORD MockFile(HMODULE h,wchar_t*out,DWORD cap){++loaderCalls;for(unsigned i=0;i<3;++i)if(h==(HMODULE)&modules[i][0]){const wchar_t*p=badFile==(int)i?L"D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-steel-jobs/shared-tests/changed-module.bin":paths[i];size_t n=wcslen(p);if(n+1>cap)return cap;memcpy(out,p,(n+1)*sizeof(wchar_t));return (DWORD)n;}return 0;}
static void Reset(bool sharing){unknownModule=badFile=-1;memset(&image[0],0,image.size());gameBase=(uintptr_t)&image[0];
 for(unsigned i=0;i<3;++i){memset(&modules[i][0],0,modules[i].size());IMAGE_DOS_HEADER*d=(IMAGE_DOS_HEADER*)&modules[i][0];d->e_magic=IMAGE_DOS_SIGNATURE;d->e_lfanew=0x80;IMAGE_NT_HEADERS64*n=(IMAGE_NT_HEADERS64*)((unsigned char*)d+0x80);n->Signature=IMAGE_NT_SIGNATURE;n->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;n->FileHeader.TimeDateStamp=kTransferModules[i].stamp;n->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;n->OptionalHeader.SizeOfImage=kTransferModules[i].size;}
 for(unsigned i=0;i<10;++i)memcpy((void*)(gameBase+kTransferContracts[i].steam),kTransferContracts[i].a,32);
 if(sharing)for(unsigned i=0;i<4;++i){const TransferHookPin&p=kTransferHooks[i];unsigned char*head=(unsigned char*)(gameBase+kTransferContracts[p.row].steam),*relay=(unsigned char*)(gameBase+0x1000+i*0x100),*target=&modules[p.module][0]+p.target;head[0]=0xE9;int32_t delta=(int32_t)(relay-head-5);memcpy(head+1,&delta,4);relay[0]=0xFF;relay[1]=0x25;uintptr_t address=(uintptr_t)target;memcpy(relay+6,&address,8);memcpy(target,p.bytes,32);}
}
int main(){image.resize(0x760000);for(unsigned i=0;i<3;++i)modules[i].resize(kTransferModules[i].size);Reset(false);ioCalls=loaderCalls=0;C(CaptureTransferContracts()&&NativeContractReady(),"untouched native admission retained");C(!ioCalls&&!loaderCalls,"untouched native requires no foreign file/loader work");
 Reset(true);C(CaptureTransferContracts()&&NativeContractReady(),"all four actual SHA pinned observers/predicates admitted");for(unsigned i=0;i<4;++i)C(s_sharedTransfer[kTransferHooks[i].row].pin==&kTransferHooks[i],"four hooks have exact cached owner tickets");unsigned io=ioCalls,load=loaderCalls;for(unsigned i=0;i<100;++i)C(NativeContractReady(),"stable cached admission");C(ioCalls==io&&loaderCalls==load,"hot admission no file hashing or module lookup");
 for(unsigned row=0;row<10;++row)if(!s_sharedTransfer[row].pin){unsigned char*b=(unsigned char*)(gameBase+kTransferContracts[row].steam);b[15]^=1;C(!NativeContractReady(),"each of six mutation/native windows remains exact");b[15]^=1;}
 TransferSharedTicket&t=s_sharedTransfer[5];unsigned char*head=(unsigned char*)(gameBase+kTransferContracts[5].steam);head[1]^=1;C(!NativeContractReady(),"cached native head changed refuses");head[1]^=1;((unsigned char*)t.relay)[6]^=1;C(!NativeContractReady(),"cached relay changed refuses");((unsigned char*)t.relay)[6]^=1;unsigned char*target=(unsigned char*)(t.module+t.pin->target);target[20]^=1;C(!NativeContractReady(),"cached hook body changed refuses");target[20]^=1;IMAGE_NT_HEADERS64*n=(IMAGE_NT_HEADERS64*)(t.module+0x80);n->FileHeader.TimeDateStamp^=1;C(!NativeContractReady(),"cached owner identity changed refuses");n->FileHeader.TimeDateStamp^=1;
 Reset(true);((unsigned char*)(gameBase+kTransferContracts[0].steam))[0]=0xE9;C(!CaptureTransferContracts(),"unknown mutation detour never admitted");
 Reset(true);uintptr_t wrong=(uintptr_t)&modules[1][0]+0x1234;memcpy((void*)(gameBase+0x1000+6),&wrong,8);C(!CaptureTransferContracts(),"unknown owner target refused");
 Reset(true);unknownModule=0;C(!CaptureTransferContracts(),"missing or unknown named owner refused");
 Reset(true);badFile=0;C(!CaptureTransferContracts(),"changed module SHA refused despite matching image/code");
 printf("PASS %u actual production startup/hot shared controls\n",checks);return 0;}
'''.replace('PRODUCTION',block)
(O/'changed-module.bin').write_bytes(b'changed module identity, no executable code')
for opt in ('Od','O2'):
 file=O/('startup-'+opt+'.cpp');file.write_text(fixture,encoding='utf-8');exe=O/('startup-'+opt+'.exe')
 q=subprocess.run([str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(file),'/Fo'+str(O)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);assert not q.returncode,q.stdout+q.stderr
 q=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,q.stdout.strip());assert not q.returncode,q.stdout+q.stderr
