"""Actual Options hooks at fake native/MCM boundaries; no native UI."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/mcm-options-tests';OUT.mkdir(exist_ok=True)
s=(ROOT/'src/gui/settings_panel.cpp').read_text()
def body(marker):
 a=s.index(marker);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
actual='\n'.join(body(x) for x in ['static void hook_OptionsCreate(', 'static void hook_OptionsSaveOptions('])
code=r'''
#include <cstdio>
struct OptionsWindow{};bool DEV_BUILD=false,s_active=true,s_staged=false,compatible=false,mcmInside=false;unsigned originals=0,saves=0,builds=0,commits=0,discoveries=0;int s_buttonCount=0,s_gatedCount=0;
void Original(OptionsWindow*){++originals;if(mcmInside)++discoveries;}void Save(OptionsWindow*){++saves;}
void(*s_origCreate)(OptionsWindow*)=&Original;void(*s_origSave)(OptionsWindow*)=&Save;
bool KeoMcmAvailable(){return compatible;}void BuildTab(OptionsWindow*){++builds;s_staged=true;}void CommitStaging(){++commits;s_staged=false;}
ACTUAL
unsigned checks=0;bool C(bool ok,const char*s){++checks;if(!ok)printf("FAIL %s\n",s);return ok;}
int main(){OptionsWindow w;
 hook_OptionsCreate(&w);if(!C(originals==1&&builds==1&&s_staged,"MCM absent retains native editor and original once"))return 1;
 hook_OptionsSaveOptions(&w);if(!C(saves==1&&commits==1&&!s_staged,"fallback close commits once"))return 1;
 compatible=true;mcmInside=true;hook_OptionsCreate(&w);if(!C(originals==2&&builds==1&&discoveries==1&&!s_staged,"MCM inner first-open skips duplicate editor"))return 1;
 mcmInside=false;discoveries=0;hook_OptionsCreate(&w);++discoveries;if(!C(originals==3&&builds==1&&discoveries==1,"MCM outer first-open independent of Describe order"))return 1;
 hook_OptionsSaveOptions(&w);if(!C(saves==2&&commits==1,"suppressed editor has no stale close-save"))return 1;
 compatible=false;hook_OptionsCreate(&w);if(!C(builds==2,"old/incompatible MCM export retains fallback"))return 1;
 // Simulate teardown without save, then a compatible module on next open.
 compatible=true;hook_OptionsCreate(&w);hook_OptionsSaveOptions(&w);if(!C(commits==1&&!s_staged,"destroyed prior tab cannot commit stale staging"))return 1;
 DEV_BUILD=true;hook_OptionsCreate(&w);if(!C(builds==3,"DEV diagnostic and benchmark tab retained with MCM"))return 1;
 s_active=false;hook_OptionsCreate(&w);if(!C(builds==3&&originals==7&&!s_staged,"user OFF or partial panel hooks call original only"))return 1;
 printf("PASS actual optional MCM Options lifecycle %u checks\n",checks);return 0;}
'''.replace('ACTUAL',actual)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
for name,changed in [('actual',code),('describe-order-mutant',code.replace('DEV_BUILD || !KeoMcmAvailable()','DEV_BUILD || !discoveries')),('stale-tab-mutant',code.replace('s_staged = false;\n\ts_buttonCount','\n\ts_buttonCount'))]:
 cpp=OUT/(name+'.cpp');cpp.write_text(changed);r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(cpp),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(OUT/(name+'.exe'))],env=env,capture_output=True,text=True);(OUT/(name+'-build.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(OUT/(name+'.exe'))],capture_output=True,text=True);(OUT/(name+'-run.log')).write_text(r.stdout+r.stderr);print(name,r.stdout.strip());assert (r.returncode==0)==(name=='actual')
 if name!='actual':assert 'FAIL ' in r.stdout
entry=(ROOT/'src/plugin/plugin_entry.cpp').read_text();start=entry[entry.index('__declspec(dllexport) void startPlugin()'):]
assert start.index('KeoMcmSetRuntimeReady(false)')<start.index('CheckBuildGate(ctx)')<start.index('InstallHooksAndGuards(ctx)')
assert 'KeoMcmSetRuntimeReady(installed == totalHooks && ctx.renderInstalled == ctx.renderWanted);' in entry
print('PASS default/refused/partial runtime admission wiring')
