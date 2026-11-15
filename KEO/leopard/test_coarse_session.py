from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/coarse-session';OUT.mkdir(parents=True,exist_ok=True)
raw=(ROOT/'src/planner/coarse_graph.cpp').read_text()
# Observe only array accesses in the actual source. No algorithm is duplicated.
def compose(src):
 src=src.replace('static volatile LONG  s_ready = 0;', 'static volatile LONG  s_ready = 0;\nstatic CgBlock* volatile& ObserveRecord(int slot){++recordReads;return s_store->records[slot];}')
 src=src.replace('s_store->records[slot]','ObserveRecord(slot)')
 src=src.replace('return ObserveRecord(slot);','return s_store->records[slot];',1)
 src=src.replace('CgBlock* cur = ent->over;\n\tconst unsigned gen = CgStoreGen();','CgBlock* cur = ent->over;\n\tconst unsigned gen = CgStoreGen();\n\tAfterGenerationProbe();')
 return (ROOT/'leopard/test_coarse_session.cpp').read_text().replace('// PRODUCTION_STORE',src)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,opt,source):
 p=OUT/(name+'.cpp');p.write_text(compose(source));exe=OUT/(name+'.exe')
 b=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(p),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
 r=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((r.stdout+r.stderr).decode(errors='replace'),end='')
 return r
for opt in ('Od','O2'):
 r=run(opt,opt,raw)
 if '--expect-red' in sys.argv:assert r.returncode!=0 and b'recordReads==0' in r.stderr;print('expected baseline RED');break
 assert r.returncode==0
if '--mutants' in sys.argv:
 cases=[('idle-scan','if (InterlockedCompareExchange(&s_livePending, 0, 0) == 0)','if (false)'),('missing-reserve','InterlockedIncrement(&s_livePending);','/* no reservation */'),('missing-take-decrement','\t\tInterlockedDecrement(&s_livePending);\n\t\t++taken;','\t\t++taken;'),('missing-replacement-decrement','\t\tInterlockedDecrement(&s_livePending);\n\t\tInterlockedIncrement(&s_count.superseded);','\t\tInterlockedIncrement(&s_count.superseded);'),('stale-admitted','if (b->storeGen != gen)\n\t{\n\t\tReleaseEntry(ent, e);','if (false)\n\t{\n\t\tReleaseEntry(ent, e);'),('reset-no-sweep','s_staleCursor = 0;','s_staleCursor = CG_DIR_SLOTS;'),('unbounded-sweep','visited < 32','visited < CG_DIR_SLOTS'),('busy-empty-entry-skipped','LONG e = TakeEntry(ent, 1);','if (!ent->over) { ++s_staleCursor; continue; }\n\t\tLONG e = TakeEntry(ent, 1);')]
 for name,before,after in cases:
  assert raw.count(before)==1,(name,raw.count(before));r=run(name,'O2',raw.replace(before,after));assert r.returncode!=0 and b'FAIL ' in r.stderr,name;print(name+': expected compiled RED')
