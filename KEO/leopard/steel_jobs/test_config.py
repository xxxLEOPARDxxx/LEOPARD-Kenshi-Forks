"""Actual config registry and MCM callbacks for the three independent live jobs."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/steel-jobs-tests/config';OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
unit=(ROOT/'tools/tests/mcm_bridge_units.cpp').read_text()
start='int main(){'
probe='''const char*steelKeys[]={"gatherOutputJob","stockCounterJob","waypointRouteJob"};for(int steel=0;steel<3;++steel){const ConfigModule*m=NULL;const ConfigKey*k=FindConfigKey(steelKeys[steel],&m);Check(k&&m&&k->live&&k->kind==CK_BOOL&&ConfigFormatValue(*m,*k,m->defaults)=="false","Steel job exists, independent live bool defaultOFF");if(!k)return CheckExit("steel_jobs_config");}'''
assert start in unit;unit=unit.replace(start,start+probe)
extra=r'''
static const char*const steelNames[]={"gatherOutputJob","stockCounterJob","waypointRouteJob"};
static bool SteelLive(int n){const ConfigModule*m=NULL;const ConfigKey*k=FindConfigKey(steelNames[n],&m);return *reinterpret_cast<const bool*>(reinterpret_cast<const char*>(m->state)+k->offset);}
static void SteelCallbacks(){char error[256];int got;
 for(int n=0;n<3;++n){Row*r=Find(steelNames[n]);Check(r&&r->flags==0&&r->def=="0"&&!SteelLive(n),"three job rows defaultOFF and no restart markers");}
 for(int mask=0;mask<8;++mask)for(int n=0;n<3;++n){bool before[3]={SteelLive(0),SteelLive(1),SteelLive(2)};int value=(mask>>n)&1;Row*r=Find(steelNames[n]);unsigned old=saves;Check(!r->si(r->ud,value,error,sizeof(error))&&!r->gi(r->ud,&got)&&got==value&&SteelLive(n)==(value!=0),"each toggle persists and changes admitted runtime immediately");for(int sibling=0;sibling<3;++sibling)if(sibling!=n)Check(SteelLive(sibling)==before[sibling],"one live job toggle cannot change either sibling");Check(saves==old+(before[n]!=(value!=0)),"unchanged live job toggle makes no disk write");if(before[n]!=(value!=0))Check(lastSavedEntries.size()==1&&lastSavedEntries[0].key==steelNames[n],"exact single requested job preference persisted");}
 for(int n=0;n<3;++n){Row*r=Find(steelNames[n]);Check(!r->si(r->ud,0,error,sizeof(error)),"reset job prefs OFF");}
 Row*r=Find(steelNames[1]);saveOK=false;unsigned old=saves;Check(r->si(r->ud,1,error,sizeof(error))!=0&&saves==old+1&&!SteelLive(1)&&!r->gi(r->ud,&got)&&!got,"failed persistence changes neither live nor requested job state");saveOK=true;
 KeoMcmSetRuntimeReady(false);Check(!r->si(r->ud,1,error,sizeof(error))&&!SteelLive(1)&&!r->gi(r->ud,&got)&&got==1,"refused native gate saves preference without runtime activation");KeoMcmSetRuntimeReady(true);Check(!r->si(r->ud,0,error,sizeof(error))&&!SteelLive(1),"explicit OFF applies after gate resumes");
 unsigned oldSaves=saves,oldFloor=floorReads,oldApply=applies,oldTranslations=translations;for(int frame=0;frame<100;++frame)for(int n=0;n<3;++n){Row*t=Find(steelNames[n]);Check(!t->gi(t->ud,&got)&&got==0,"repeated job getters are stable cached reads");}Check(saves==oldSaves&&floorReads==oldFloor&&applies==oldApply&&translations==oldTranslations,"job getters have no IO/native/translation work");
}
'''
unit=unit.replace(start,extra+'\n'+start)
needle='KeoMcmSetRuntimeReady(true);Check(!retain.si';assert needle in unit;unit=unit.replace(needle,'KeoMcmSetRuntimeReady(true);SteelCallbacks();Check(!retain.si')
line=next(x for x in (ROOT/'tools/tests/suites.txt').read_text().splitlines()if x.startswith('mcm_bridge_units |'));sources=[ROOT/x for x in line.split('|')[2].split()]
if '--red-control'in sys.argv:
 p=ROOT/'src/inventory/inventory_config.cpp';s=p.read_text().replace('CFG_OBOOL_LIVE("gatherOutputJob"','CFG_OBOOL("gatherOutputJob"');assert s!=p.read_text();mutant=OUT/'nonlive_inventory.cpp';mutant.write_text(s);sources=[mutant if x.name=='inventory_config.cpp' else x for x in sources]
for label,defines in [('prod',()),('dev',('/DKEO_DEBUG',))]:
 folder=OUT/label;folder.mkdir(exist_ok=True);cpp=folder/'unit.cpp';cpp.write_text(unit);exe=folder/'test.exe';cmd=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3','/DZONEHAND_STEP=3',*defines,str(cpp),*map(str,sources),'/Fo'+str(folder)+'/', '/Fe'+str(exe)];r=subprocess.run(cmd,env=env,cwd=ROOT,capture_output=True,text=True);(folder/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True);tag='RED-'if '--expect-missing'in sys.argv or '--red-control'in sys.argv else '';print(tag+label,r.stdout.strip(),flush=True);(folder/(tag+'run.log')).write_text(r.stdout+r.stderr)
 if tag:assert r.returncode==1 and'Steel job exists, independent live bool defaultOFF'in r.stdout
 else:assert not r.returncode,r.stdout+r.stderr
