"""Actual hook/TLS native-body boundary over owned buffers; no game or plugin build."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/carried-weight-tests/adapter';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'src/inventory/carried_weight.cpp').read_text(encoding='utf-8-sig')if(ROOT/'src/inventory/carried_weight.cpp').exists()else''
def body(sig):
 a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "inventory/carried_weight_policy.h"
using namespace keo_inventory;
static volatile LONG calls=0,carryCalls=0,checks=0,s_admitted=1;
static __declspec(thread) int failMode=0;
void C(bool b,const char*s){InterlockedIncrement(&checks);if(!b){printf("FAIL %s\n",s);std::exit(1);}}
float Original(void*inv){InterlockedIncrement(&calls);return (uintptr_t)inv==3?30.0f:20.0f;}
static float(*s_originalWeight)(void*)=Original;
static bool RootInventory(void*i){return i==(void*)1;}
struct WeightScope{WeightScope*previous;bool raw,seen;uintptr_t target;float nativeInv;};
static __declspec(thread) WeightScope*t_weightScope=NULL;
static float HookWeight(void*);
static float GetNativeCarryWeight(uintptr_t){InterlockedIncrement(&carryCalls);float base=HookWeight((void*)1);if(failMode==1)throw std::runtime_error("owned native throw");if(failMode==2)RaiseException(0xe1122334,0,0,NULL);return base+30;}
RAWBODY
float Native(void*,uintptr_t i){return s_originalWeight((void*)i);}uintptr_t Owner(void*,uintptr_t i){return i==1?1:0;}int Count(void*,uintptr_t){return 0;}uintptr_t Child(void*,uintptr_t,int){return 0;}uintptr_t Carried(void*,uintptr_t){return 2;}uintptr_t Inventory(void*,uintptr_t){return 3;}bool Body(void*,uintptr_t,WeightBody*b){WeightBody h={5,17,5,1,100,100,3,0};*b=h;return true;}
static WeightOps WeightOpsForGame(){WeightOps o={NULL,Native,Owner,Count,Child,Carried,Inventory,Body,ReadNativeBody};return o;}
HOOK
void Seh(){__try{HookWeight((void*)1);}__except(GetExceptionCode()==0xe1122334?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){}}
DWORD WINAPI Worker(void*){for(int i=0;i<100;++i){C(HookWeight((void*)1)==50,"owned thread calculates corrected weight");C(t_weightScope==NULL,"thread scope restored after each query");}return 0;}
int main(){C(HookWeight((void*)1)==50,"adapter adds carried inventory while body baseline is captured without recursion/doublecount");C(calls==3&&carryCalls==1,"one original per invocation: root+child+one paired baseline only");C(!t_weightScope,"normal query leaves no stack pointer");s_admitted=0;LONG before=calls;C(HookWeight((void*)1)==20&&calls==before+1,"not admitted is exactly one native call");s_admitted=1;before=calls;C(HookWeight((void*)3)==30&&calls==before+1,"bag queries retain native result");
 WeightScope prior={};t_weightScope=&prior;C(HookWeight((void*)1)==20&&t_weightScope==&prior,"nested active query is native only and keeps outer context");t_weightScope=NULL;
 failMode=1;try{HookWeight((void*)1);C(false,"C++ original throw propagated");}catch(const std::runtime_error&){}C(!t_weightScope,"C++ unwind restores both TLS frames");failMode=2;Seh();C(!t_weightScope,"SEH unwind restores both TLS frames");failMode=0;
 HANDLE t[2]={CreateThread(NULL,0,Worker,NULL,0,NULL),CreateThread(NULL,0,Worker,NULL,0,NULL)};C(t[0]&&t[1],"two owned concurrent query threads");C(WaitForMultipleObjects(2,t,TRUE,5000)==WAIT_OBJECT_0,"owned threads joined");CloseHandle(t[0]);CloseHandle(t[1]);C(!t_weightScope,"other threads cannot publish into Main scope");printf("PASS %ld actual hook/TLS controls\n",checks);}
'''
if s:code=code.replace('RAWBODY',body('static bool ReadNativeBody(')).replace('HOOK',body('static float HookWeight(void* inventory)'))
else:code=code.replace('RAWBODY','static bool ReadNativeBody(void*,uintptr_t,uintptr_t,float*b){*b=30;return true;}').replace('HOOK','static float HookWeight(void*inventory){return s_originalWeight(inventory);}')
cpp=OUT/'adapter.cpp';cpp.write_text(code)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{ROOT}/src;{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
for opt in ['Od','O2']:
 exe=OUT/('adapter-'+opt+'.exe');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp),str(ROOT/'src/inventory/carried_weight_policy.cpp'),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);(OUT/('build-'+opt+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,r.stdout.strip());(OUT/('run-'+opt+'.log')).write_text(r.stdout+r.stderr)
 if '--expect-red'in sys.argv:assert r.returncode==1 and'adapter adds' in r.stdout
 else:assert r.returncode==0,r.stdout+r.stderr
