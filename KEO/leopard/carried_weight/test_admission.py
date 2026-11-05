"""Actual startup/tick extraction; owned module/native-hook boundaries, no game calls."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/carried-weight-tests';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'src/inventory/carried_weight.cpp').read_text()
def body(needle):
 start=s.index(needle);opening=s.index('{',start);n=1;i=opening+1
 while n:
  n+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
assert 'GetModuleHandle' not in body('static float HookWeight(void* inventory)')
side=(ROOT/'src/inventory/backpack_sidecar.cpp').read_text();assert side.index('CarriedWeightTick(now);')<side.index('if (InterlockedCompareExchange(&s_armed',side.index('void BackpackRekeyTick'))
main=(ROOT/'src/zone/camera_zone_hook.cpp').read_text();assert 'keo_inventory::BackpackRekeyTick(tickNow, saveLoading);'in main
code=r'''
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdint.h>
static int checks=0,moduleCalls=0,errors=0,hookCalls=0,allocs=0;static bool loaded=false,binding=true,hookOk=true;
void*operator new(size_t n){++allocs;void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){free(p);}
void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
static HMODULE FakeModule(const wchar_t*){++moduleCalls;return loaded?(HMODULE)1:NULL;}
#define GetModuleHandleW FakeModule
#define KEO_DETAIL(s) ((void)0)
void LogError(const char*){++errors;}
struct Config{bool carriedWeightEnabled;}g_inventoryCfg={true};
static volatile LONG s_admitted=0;static bool s_installed=false,s_blocked=false;static double s_nextCheck=0;static float(*s_originalWeight)(void*)=NULL;
static float HookWeight(void*){return 0;}
bool Bindings(uintptr_t*p){*p=7;return binding;}
namespace KenshiLib{enum {SUCCESS=0};int AddHook(void*,float(*)(void*),float(**)(void*)){++hookCalls;return hookOk?0:1;}}
TICK
INSTALL
void Fresh(){s_admitted=0;s_installed=s_blocked=loaded=false;binding=hookOk=true;s_nextCheck=0;moduleCalls=errors=hookCalls=0;g_inventoryCfg.carriedWeightEnabled=true;}
int main(){int installed=0,total=0;Fresh();g_inventoryCfg.carriedWeightEnabled=false;InstallCarriedWeight(&installed,&total);CarriedWeightTick(0);C(!total&&!moduleCalls&&!hookCalls&&!s_admitted,"default OFF no loader or hook work");
 Fresh();loaded=true;InstallCarriedWeight(&installed,&total);C(!installed&&s_blocked&&errors==1&&hookCalls==0,"already loaded QOL refuses hook regardless toggle");CarriedWeightTick(3);C(moduleCalls==1&&!s_admitted,"startup refused stays blocked");
 Fresh();binding=false;InstallCarriedWeight(&installed,&total);C(!s_installed&&errors==1&&hookCalls==0,"unknown native contract fails closed");
 Fresh();hookOk=false;InstallCarriedWeight(&installed,&total);C(!s_installed&&!s_originalWeight&&errors==1,"failed hook remains native");
 Fresh();InstallCarriedWeight(&installed,&total);C(s_installed&&!s_admitted&&hookCalls==1,"installed native-only until first Main check");int before=allocs;CarriedWeightTick(10);C(s_admitted&&moduleCalls==2,"first post-startup Main check admits");for(int i=0;i<1000;++i)CarriedWeightTick(10.0+i*.0009);C(moduleCalls==2,"1000 frame calls below one second do not query loader");CarriedWeightTick(11);C(moduleCalls==3&&s_admitted,"one second performs exactly one check");loaded=true;CarriedWeightTick(12);C(!s_admitted&&s_blocked&&errors==1,"later observed QOL closes admission once");loaded=false;CarriedWeightTick(50);C(moduleCalls==4&&!s_admitted&&errors==1,"conflict never silently readmits");C(allocs==before,"compatibility tick has zero allocations");
 Fresh();InstallCarriedWeight(&installed,&total);loaded=true;CarriedWeightTick(0);C(!s_admitted&&s_blocked,"DLL loaded after KEO startup but before Main never enables correction");printf("PASS %d actual startup/admission controls\n",checks);}
'''.replace('TICK',body('void CarriedWeightTick(')).replace('INSTALL',body('void InstallCarriedWeight('))
cpp=OUT/'admission.cpp';cpp.write_text(code)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
for opt in ['Od','O2']:
 exe=OUT/('admission-'+opt+'.exe');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)],env=env,capture_output=True,text=True);(OUT/('admission-build-'+opt+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,r.stdout.strip());(OUT/('admission-run-'+opt+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
