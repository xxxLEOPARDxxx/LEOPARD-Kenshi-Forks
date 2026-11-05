"""Existing production config/layout/MCM suites, limited to added metadata integration."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/carried-weight-tests/settings';OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{ROOT}/tools/tests',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
lines=(ROOT/'tools/tests/suites.txt').read_text().splitlines()
for name in ('config_modules_units','settings_layout_units','settings_layout_prod_units','mcm_bridge_units','mcm_bridge_dev_units'):
 p=next(x for x in lines if x.startswith(name+' |')).split('|');folder=OUT/name;folder.mkdir(exist_ok=True);exe=folder/'test.exe'
 cmd=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3',*p[3].split(),str(ROOT/p[1].strip()),*[str(ROOT/x)for x in p[2].split()],'/Fo'+str(folder)+'/', '/Fe'+str(exe)]
 r=subprocess.run(cmd,env=env,cwd=ROOT,capture_output=True,text=True);(folder/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True);(folder/'run.log').write_text(r.stdout+r.stderr);print(name,r.stdout.strip(),flush=True);assert not r.returncode,r.stdout+r.stderr
