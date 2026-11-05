"""Host actual MCM callbacks, staged commit and native-caller routing controls.
No native plugin build, personal/installed INI, UI or game calls.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/mcm-task2';OUT.mkdir(exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
line=next(x for x in (ROOT/'tools/tests/suites.txt').read_text().splitlines() if x.startswith('requested_settings_units |'))
sources=[ROOT/x for x in line.split('|')[2].split() if Path(x).name!='keo_tr_stub.cpp']+[ROOT/'src/gui/mcm_bridge.cpp']
def run(name,sources,unit,defines=(),success=True,native_fault_marker=None):
 folder=OUT/name;folder.mkdir(exist_ok=True)
 r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3','/DZONEHAND_STEP=3',*defines,str(unit),*map(str,sources),'/Fo'+str(folder)+'/', '/Fe'+str(folder/'test.exe')],env=env,cwd=ROOT,capture_output=True,text=True)
 (folder/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(folder/'test.exe')],capture_output=True,text=True);(folder/'run.log').write_text('exit='+str(r.returncode)+'\n'+r.stdout+r.stderr)
 if native_fault_marker:
  assert r.returncode in (-1073741819,0xC0000005) and native_fault_marker in r.stdout,(name,r.returncode,r.stdout+r.stderr)
  print(name,'expected access violation after '+native_fault_marker);return
 assert (r.returncode==0)==success,(name,r.stdout+r.stderr)
 if not success:assert 'FAIL ' in r.stdout,(name,r.stdout+r.stderr)
 print(name,r.stdout.strip())
for label,defines in [('prod',()),('dev',('/DKEO_DEBUG',))]:run(label,sources,ROOT/'tools/tests/mcm_bridge_units.cpp',defines)
source=(ROOT/'src/gui/mcm_bridge.cpp').read_text()
cases=[('effective-getter','RequestedConfig().CopyModule(c->module,&s)','(StageModule(*m,&s),true)'),
       ('publish-before-save','if(!SaveIniEntries(entries,NULL,"MCM"))','PublishRequestedSettings(next);if(!SaveIniEntries(entries,NULL,"MCM"))'),
       ('revive-unrelated-render','RenderConfig renderNext=g_renderCfg;','RenderConfig renderNext=StagedRender(&next);'),
       ('no-runtime-gate','if(runtimeReady){','if(true){'),
       ('native-translation-on-refused-build','platformReady&&IsMainThread()?KeoTr(english):english','KeoTr(english)'),

       ('no-reentry','if(!IsMainThread()||committing)','if(!IsMainThread())')]
# Specific mutations are extended alongside the production callback coverage;
# every negative control compiles and fails a behavior assertion, or reproduces
# the exact native access violation at the separately marked clamp boundary.
for name,old,new in cases:
 assert old in source,name
 folder=OUT/('mutant-'+name);folder.mkdir(exist_ok=True);cpp=folder/'bridge.cpp';cpp.write_text(source.replace(old,new))
 selected=[cpp if p.name=='mcm_bridge.cpp' else p for p in sources];run('mutant-'+name,selected,ROOT/'tools/tests/mcm_bridge_units.cpp',success=False)
for name,old,marker in [
 ('null-notes-first','StagedRender(const_cast<SettingsStaging*>(&baseline)),NULL,&renderNotes','CHECK finite render clamp'),
 ('null-notes-merged','StagedRender(&current),NULL,&renderNotes','CHECK merged sibling render clamp')]:
 assert old in source,name
 folder=OUT/('mutant-'+name);folder.mkdir(exist_ok=True);cpp=folder/'bridge.cpp';cpp.write_text(source.replace(old,old.replace('&renderNotes','NULL')))
 selected=[cpp if p.name=='mcm_bridge.cpp' else p for p in sources];run('mutant-'+name,selected,ROOT/'tools/tests/mcm_bridge_units.cpp',native_fault_marker=marker)
