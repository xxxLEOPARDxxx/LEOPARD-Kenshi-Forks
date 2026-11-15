from pathlib import Path
import os,subprocess,sys,hashlib,json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/readiness-debug-gate';OUT.mkdir(parents=True,exist_ok=True)
def section(s,marker):
 a=s.index(marker);b=s.index('{',a);p=b+1;depth=1
 while depth:depth+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[a:p]
s=(ROOT/'src/zone/readiness/readiness_hook.cpp').read_text();body=section(s,'bool hook_isContentPending(')
classifier=section(s,'int ClassifyZoneReadiness(')
CLASSIFIER_SHA256='a5b5ae94d5da55a5491a11889859fece9ea1408f6fce7cf995f2d8eed74da72a'
BASELINE_HOOK_SHA256='8460aefdae38d55063a7b3aa6d2e4275eed48210f29212227b215352b84f9418'
assert hashlib.sha256(classifier.encode()).hexdigest()==CLASSIFIER_SHA256,'functional classifier must be unchanged'
gate='''#if ZONEHAND_STEP >= 2
\t// Native readiness is unconditional at this step. Everything below is
\t// diagnostic classification/counting; keep its collection scans, locks,
\t// clocks and atomics out of ordinary play. Functional classifier callers
\t// in island/K7 paths remain independent of this hook's diagnostic gate.
\tif (!DetailedLogEnabled()) return result;
#endif
'''
if '--expect-red' not in sys.argv:assert hashlib.sha256(body.replace(gate,'').encode()).hexdigest()==BASELINE_HOOK_SHA256,'the gated block is the only hook change; legacy preprocessor arm stays identical'
(OUT/'classifier-source-proof.json').write_text(json.dumps({'sha256':hashlib.sha256(classifier.encode()).hexdigest(),'baseline-identical':True},indent=2))
template=(ROOT/'leopard/test_readiness_debug_gate.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,src,opt):
 cpp=OUT/(name+'.cpp');cpp.write_text(template.replace('// PRODUCTION',src));exe=OUT/(name+'.exe')
 p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/DZONEHAND_STEP=5','/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/zone/readiness/zone_readiness_contract.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],cwd=ROOT,env=env,capture_output=True)
 (OUT/(name+'-build.txt')).write_bytes(p.stdout+p.stderr);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(p.stdout+p.stderr);return p
for opt in ('Od','O2'):
 p=run('red-original' if '--expect-red' in sys.argv else opt,body,opt);print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'classifyCalls==0&&readinessCalls==0&&bridgeCalls==0' in p.stderr;print('expected actual readiness Debug-OFF RED');raise SystemExit(0)
 assert p.returncode==0
if '--mutants' in sys.argv:
 for name,before,after in [
  ('debug-gate-removed','if (!DetailedLogEnabled()) return result;','if (false) return result;'),
  ('off-answer-forced-false','if (!DetailedLogEnabled()) return result;','if (!DetailedLogEnabled()) return false;'),
  ('off-answer-forced-true','if (!DetailedLogEnabled()) return result;','if (!DetailedLogEnabled()) return true;'),
  ('diagnostics-never-resume','if (!DetailedLogEnabled()) return result;','return result;'),
  ('worker-still-diagnoses','if (!DetailedLogEnabled()) return result;','if (!DetailedLogEnabled() && onMainThread) return result;'),
  ('priority-disabled-with-debug','if (onMainThread && isTransitionActive && !prioritizedThisTransition)','if (DetailedLogEnabled() && onMainThread && isTransitionActive && !prioritizedThisTransition)'),
 ]:
  assert before in body,name;p=run(name,body.replace(before,after),'O2');assert p.returncode!=0 and b'FAIL ' in p.stderr,name;print(name+': expected RED')
