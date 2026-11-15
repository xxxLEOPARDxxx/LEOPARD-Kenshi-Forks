from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/island-empty-rebuild';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/movement/island_components.cpp').read_text()
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
template=(ROOT/'leopard/test_island_empty_rebuild.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,src,opt):
 cpp=OUT/(name+'.cpp');cpp.write_text(template.replace('// PRODUCTION',src));exe=OUT/(name+'.exe')
 p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/DKEO_DEBUG',str(cpp),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],cwd=ROOT,env=env,capture_output=True)
 (OUT/(name+'-build.txt')).write_bytes(p.stdout+p.stderr);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(p.stdout+p.stderr);return p
for opt in ('Od','O2'):
 p=run('red-original' if '--expect-red' in sys.argv else opt,source,opt)
 print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'atCalls==0&&accessCalls==0&&indexCalls==0' in p.stderr;print('expected actual Rebuild native-call RED');raise SystemExit(0)
 assert p.returncode==0
if '--mutants' in sys.argv:
 for name,before,after in [
  ('native-read-returned','g_lastEligibility = now;','g_lastEligibility = now; ZoneAccess(ZoneAt(zm, 0));'),
  ('eligibility-clock-stale','g_lastEligibility = now;','/* stale eligibility */'),
  ('first-snapshot-missing','if (g_curValid) return;','return;'),
  ('republish-every-rebuild','if (g_curValid) return;','/* republish */'),
  ('startup-components-zero','g_curComp[i] = -1;','g_curComp[i] = 0;'),
  ('generation-not-advanced','g_snapGen++;\n\tPublishSnapshot(true, zm);','/* missing gen */\n\tPublishSnapshot(true, zm);'),
  ('invalid-published','PublishSnapshot(true, zm);','PublishSnapshot(false, zm);'),
 ]:
  assert before in source,name
  p=run(name,source.replace(before,after),'O2');assert p.returncode!=0 and b'FAIL ' in p.stderr,name;print(name+': expected RED')
