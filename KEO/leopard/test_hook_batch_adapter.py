from pathlib import Path
import re,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/hook-batch-adapter';OUT.mkdir(parents=True,exist_ok=True)
raw=(ROOT/'src/plugin/hook_manifest.cpp').read_text()
def body(s,mark,semi=False):
 a=s.index(mark);p=s.index('{',a)+1;d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[a:p+(1 if semi else 0)]
globals=raw[raw.index('static HookBatch s_independentBatch;'):raw.index('static bool IndependentRow(')]
template=(ROOT/'leopard/test_hook_batch_adapter.cpp').read_text()
def compose(s):
 prod='\n'.join(body(s,m,m=='struct IndependentStartupScope {')for m in ['static bool IndependentRow(','static void IndependentComplete(','struct IndependentStartupScope {','void HookInstallIndependentRow('])
 return template.replace('// BATCH_GLOBALS',globals).replace('// ADAPTER_PRODUCTION',prod)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,opt,s):
 p=OUT/(name+'.cpp');p.write_text(compose(s));exe=OUT/(name+'.exe')
 b=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(p),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
 r=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((r.stdout+r.stderr).decode(errors='replace'),end='');return r
for opt in ['Od','O2']:assert run(opt,opt,raw).returncode==0
if '--mutants' in sys.argv:
 cases=[('reentry-guard-lost','if(s_independentClaimed[id]||(s_independentPhase&&!s_independentBatch.Collecting()))','if(false)'),('no-claim','s_independentClaimed[id]=true;','/* missing claim */'),('success-before-enable','if(!why){InterlockedExchange','if(true){InterlockedExchange'),('retain-stack-counter','~IndependentStartupScope(){s_independentCount=NULL;s_independentPhase=false;}','~IndependentStartupScope(){}'),('clear-original-on-partial','if(s_independentDone[row])s_independentDone[row](why);','if(why){for(int i=0;i<4;++i)originals[i]=NULL;}if(s_independentDone[row])s_independentDone[row](why);')]
 for name,before,after in cases:
  assert raw.count(before)==1,name;r=run(name,'O2',raw.replace(before,after));assert r.returncode!=0 and b'FAIL ' in r.stderr,name;print(name+': compiled expected RED')
