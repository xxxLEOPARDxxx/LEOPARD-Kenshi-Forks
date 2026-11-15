from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/zone-ready-idle';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'src/zone/handoff/zone_handoff.cpp').read_text();a=s.index('static void AdmitCohort(');b=s.index('\n\n// =========================================================================',a);body=s[a:b]
template=(ROOT/'leopard/test_zone_ready_idle.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,src,opt):
 cpp=OUT/(name+'.cpp');cpp.write_text(template.replace('// PRODUCTION','#define ZonePrepLedgerCollectReady CountedCollect\n'+src+'\n#undef ZonePrepLedgerCollectReady'));exe=OUT/(name+'.exe')
 p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/zone/handoff/zone_prep_ledger.cpp'),str(ROOT/'src/zone/zone_ledger_core.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],cwd=ROOT,env=env,capture_output=True)
 (OUT/(name+'-build.txt')).write_bytes(p.stdout+p.stderr);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(p.stdout+p.stderr);return p
for opt in ('Od','O2'):
 p=run('red-original' if '--expect-red' in sys.argv else opt,body,opt);print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'collectCalls==0&&inserts==0' in p.stderr;print('expected actual cohort idle-scan RED');raise SystemExit(0)
 assert p.returncode==0
if '--mutants' in sys.argv:
 for name,before,after in [('guard-removed','if (ZonePrepLedgerCountInState(&g_zonePrepLedger, ZONE_STATE_READY_FOR_ADOPTION) == 0)','if (false)'),('guard-inverted','== 0)\n\t\treturn;','!= 0)\n\t\treturn;'),('guard-skips-one','== 0)\n\t\treturn;','<= 1)\n\t\treturn;')]:
  assert before in body,name;p=run(name,body.replace(before,after),'O2');assert p.returncode!=0 and b'FAIL ' in p.stderr,name;print(name+': expected RED')
