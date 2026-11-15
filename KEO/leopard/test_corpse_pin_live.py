from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/corpse-pin-live';OUT.mkdir(parents=True,exist_ok=True)
text=(ROOT/'src/fixes/world/corpse_pin.cpp').read_text()
start=text.index('static Ogre::Vector3* __fastcall hook_calculateCurrentPos(')
end=text.index('\nvoid InstallCorpsePin(',start)
body=text[start:end]
fixture=(ROOT/'leopard/test_corpse_pin_live.cpp').read_text().replace('// PRODUCTION_HOOK',body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'))
env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for opt in ('Od','O2'):
 cpp=OUT/(opt+'.cpp');cpp.write_text(fixture);exe=OUT/(opt+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/fixes/world/corpse_pin_policy.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)]
 p=subprocess.run(args,env=env,cwd=ROOT,capture_output=True);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'queries==1' in p.stderr;print('expected RED');break
 assert p.returncode==0
