"""Offline pinned-SDK ABI admission and actual production admission branches."""
from pathlib import Path
import os,subprocess,hashlib,importlib.util
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/hook-batch-admission';OUT.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('pe',ROOT/'tools/kenshilib/check_imports.py');pe=importlib.util.module_from_spec(spec);spec.loader.exec_module(pe)
game=Path(os.environ.get('KENSHI_ROOT', 'D:/SteamLibrary/steamapps/common/Kenshi'))
p=pe.PE(game/'KenshiLib.dll');raw=(ROOT/'src/plugin/hook_manifest.cpp').read_text();native=(ROOT/'src/game/native_profile.cpp').read_text()
sha=hashlib.sha256(p.data).hexdigest();assert 'sdkSha[]="'+sha+'"' in native
nt=p.u32(0x3c);assert p.u32(nt+8)==0x6a8e4636 and p.u32(nt+24+56)==0xec000
ex=p.offset(p.directories[0][0]);names=p.offset(p.u32(ex+32));ords=p.offset(p.u32(ex+36));funcs=p.offset(p.u32(ex+28));exports={p.string(p.u32(names+4*i)):p.u32(funcs+4*p.u16(ords+2*i)) for i in range(p.u32(ex+24))}
for name,rva,hex in [('QueueHookInternal',0x2540,'4053574881ecb800000048c7442420fe'),('ApplyQueuedHooksInternal',0x26a0,'40534881ecb000000048c7442420feff')]:
 symbol=next(x for x in exports if x.startswith('?'+name+'@@'));assert symbol in raw and exports[symbol]==rva and p.data[p.offset(rva):p.offset(rva)+16].hex()==hex
a=raw.index('bool BeginNativeHookBatch(');b=raw.index('\nstatic void IndependentComplete',a);body=raw[a:b]
source=r'''
#include <new>
#include <cstdio>
#include <cstring>
#include "plugin/independent_hook_batch.h"
typedef unsigned long DWORD;typedef char* HMODULE;typedef const wchar_t* LPCWSTR;
enum{GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4,GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT=2};
static char image[0xec000];static int fault=0,checks=0;static HookBatch s_independentBatch;
static bool s_allowFurtherNativeBatches=true;
static bool NativeProfileVerifySdk(){return fault!=1;}
static HMODULE GetModuleHandleW(const wchar_t*){return image;}
static bool ReadModuleImageId(HMODULE,DWORD*t,DWORD*s){*t=fault==2?0:0x6a8e4636u;*s=fault==3?0:0xec000u;return fault!=4;}
static void* GetProcAddress(HMODULE,const char*n){bool q=strstr(n,"?Queue")!=NULL;return fault==(q?5:6)?NULL:image+(q?0x2540:0x26a0);}
static bool GetModuleHandleExW(DWORD f,LPCWSTR,HMODULE*c){if(f!=6)throw 1;*c=image+0x5000;return fault!=9;}
// ACTUAL_ADMISSION
int main(){
 for(fault=0;fault<11;++fault){s_independentBatch.~HookBatch();new(&s_independentBatch)HookBatch();
 const unsigned char qb[16]={0x40,0x53,0x57,0x48,0x81,0xec,0xb8,0,0,0,0x48,0xc7,0x44,0x24,0x20,0xfe};
 const unsigned char ab[16]={0x40,0x53,0x48,0x81,0xec,0xb0,0,0,0,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff};
 memcpy(image+0x2540,qb,16);memcpy(image+0x26a0,ab,16);if(fault==7)image[0x2540]=0;if(fault==8)image[0x26a0]=0;
 s_allowFurtherNativeBatches=fault!=10;
 bool ok=BeginIndependentHooks();++checks;if(ok!=(fault==0)||s_independentBatch.Collecting()!=ok){fprintf(stderr,"FAIL admission %d\n",fault);return 1;}}
 printf("PASS %d actual SDK admission controls\n",checks);return 0;}
'''
source=source.replace('// ACTUAL_ADMISSION',body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for opt in ['Od','O2']:
 cpp=OUT/(opt+'.cpp');cpp.write_text(source);exe=OUT/(opt+'.exe');r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(cpp),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert r.returncode==0,(r.stdout+r.stderr).decode(errors='replace');r=subprocess.run([str(exe)],capture_output=True);print(r.stdout.decode(),end='');assert r.returncode==0,r.stderr
print('PASS pinned installed SDK SHA/export/RVA/PE/prefix admission; no SDK execution')
