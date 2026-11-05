"""Audit player translations through actual native-independent MCM metadata.
Run existing production callback suite with a recording KeoTr boundary, then
verify all labels/tooltips/sections/choice labels and all Error/Text literals
against the three compiled source catalogs.
"""
from pathlib import Path
import os,subprocess,sys,re,importlib.util
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task3-player-keys';OUT.mkdir(exist_ok=True)
spec=importlib.util.spec_from_file_location('catalogs',ROOT/'tools/localization/compile_catalogs.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
unit=(ROOT/'tools/tests/mcm_bridge_units.cpp').read_text();old='const char*KeoTr(const char*s){++translations;return s?s:"";}'
assert old in unit;unit=unit.replace(old,'const char*KeoTr(const char*s){++translations;if(s&&*s)printf("KEY:%s\\n",s);return s?s:"";}')
(OUT/'actual.cpp').write_text(unit)
line=next(x for x in (ROOT/'tools/tests/suites.txt').read_text().splitlines() if x.startswith('requested_settings_units |'))
sources=[ROOT/x for x in line.split('|')[2].split() if Path(x).name!='keo_tr_stub.cpp']+[ROOT/'src/gui/mcm_bridge.cpp']
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3','/DZONEHAND_STEP=3',str(OUT/'actual.cpp'),*map(str,sources),'/Fo'+str(OUT)+'/', '/Fe'+str(OUT/'actual.exe')],env=env,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
r=subprocess.run([str(OUT/'actual.exe')],capture_output=True,text=True);(OUT/'run.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
keys=set(x[4:] for x in r.stdout.splitlines() if x.startswith('KEY:'))
bridge=(ROOT/'src/gui/mcm_bridge.cpp').read_text()
import ast
for m in re.finditer(r'(?:Error\(err,size,|Text\()((?:"(?:\\.|[^"\\])*"))',bridge):keys.add(ast.literal_eval(m[1]))
keys={k for k in keys if not k.isdigit()}
for lang in mod.LANGUAGES:
 catalog=mod.read_po(ROOT/f'locale/{lang}/LC_MESSAGES/keo.po');missing=keys-set(catalog)
 assert not missing,(lang,missing)
print(f'Actual MCM player metadata + all Error/Text literal keys: {len(keys)} keys x 3 locales PASS')
(OUT/'keys.txt').write_text('\n'.join(sorted(keys))+'\n',encoding='utf-8')
