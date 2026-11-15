"""Existing real settings-table fixture plus the actual panel capacity declaration."""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/settings-gated-capacity';OUT.mkdir(parents=True,exist_ok=True)
panel=(ROOT/'src/gui/settings_panel.cpp').read_text();decl=re.search(r'^static const int MAX_GATED = [^;]+;',panel,re.M).group()
assert 'static GatedLine s_gated[MAX_GATED];' in panel
fixture=(ROOT/'tools/tests/settings_layout_units.cpp').read_text()
fixture=fixture.replace('#include "check.h"','#include "check.h"\n'+decl)
anchor='Check(gated && gatedRows == 16, "groups: each member\'s enabledByRow is its head\'s row, a checkbox");'
assert fixture.count(anchor)==1
fixture=fixture.replace(anchor,anchor+'\n\tCheck(gatedRows <= MAX_GATED, "panel capacity: every actual grouped child fits, including all nine RAM children and later groups");\n\tCheck(kConfigModuleCount <= CONFIG_MODULE_MAX, "panel bound: every actual module fits the staging module bound");\n\tfor (int m = 0; m < kConfigModuleCount; ++m) { int n=0; while(kConfigModules[m].keys[n].name)++n; Check(n <= CONFIG_STAGE_MAX, "panel bound: actual module keys fit staging capacity"); }')
row=next(l for l in (ROOT/'tools/tests/suites.txt').read_text().splitlines() if l.startswith('settings_layout_prod_units |'))
extras=row.split('|')[2].split()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=';'.join([str(vc/'bin/amd64'),str(vc/'../Common7/IDE'),env['PATH']])
def run(name,mode,text):
 cpp=OUT/(name+'.cpp');cpp.write_text(text);exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+mode,'/DZONEHAND_STEP=3','/I'+str(ROOT/'src'),'/I'+str(ROOT/'tools/tests'),str(cpp)]+[str(ROOT/n) for n in extras]+['/Fo'+str(OUT)+'\\','/Fe'+str(exe)]
 b=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-build.txt')).write_bytes(b.stdout);assert b.returncode==0,b.stdout.decode(errors='replace')
 r=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-run.txt')).write_bytes(r.stdout);print(name+': '+r.stdout.decode(errors='replace'),end='');return r.returncode
for mode in ['Od','O2']:
 rc=run(mode,mode,fixture)
 if '--expect-red' in sys.argv:assert rc!=0;print('expected runtime RED');break
 assert rc==0
if '--expect-red' not in sys.argv:
 mutant=fixture.replace(decl,'static const int MAX_GATED = 8;');assert mutant!=fixture
 assert run('old-eight-capacity','O2',mutant)!=0;print('old-eight-capacity: compiled, expected runtime RED')
