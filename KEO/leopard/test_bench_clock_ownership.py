"""Production pin-run inclusion with deterministic game facade; no game calls."""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/bench-clock-ownership';OUT.mkdir(parents=True,exist_ok=True)
src=re.sub(r'^#include[^\n]*\n','',(ROOT/'src/bench/bench_pin_run.cpp').read_text(),flags=re.M);fixture=(ROOT/'leopard/test_bench_clock_ownership.cpp').read_text()
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=';'.join([str(vc/'bin/amd64'),str(vc/'../Common7/IDE'),env['PATH']])
def run(name,mode,body):
 cpp=OUT/(name+'.cpp');cpp.write_text(fixture.replace('// PRODUCTION_BODY',body));exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+mode,'/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/bench/bench_pin.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)]
 b=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-build.txt')).write_bytes(b.stdout);assert b.returncode==0,b.stdout.decode(errors='replace')
 r=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-run.txt')).write_bytes(r.stdout);print(name+': '+r.stdout.decode(errors='replace'),end='');return r.returncode
for mode in ['Od','O2']:
 rc=run(mode,mode,src)
 if '--expect-red' in sys.argv:assert rc!=0;print('expected runtime RED');break
 assert rc==0

if '--expect-red' not in sys.argv:
 mutations=[
  ('idle-normalizes-foreign','if (!s_clockRestorePending || !BenchClockReady())','if (!BenchClockReady())'),
  ('refused-write-creates-debt','if (!BenchSetClockRate(rate))\n\t\treturn false;','if (!BenchSetClockRate(rate)) { s_clockRestorePending = true; s_ownedClockRate = BenchClockRate(); return false; }'),
  ('failed-restore-loses-debt','if (rate != normal && !BenchSetClockRate(normal))\n\t\treturn false;','if (rate != normal && !BenchSetClockRate(normal)) { s_clockRestorePending = false; return false; }'),
  ('begin-erases-debt','memset(&s_pin, 0, sizeof(s_pin));','memset(&s_pin, 0, sizeof(s_pin)); s_clockRestorePending = false;'),
  ('restore-overwrites-later-foreign','if (rate != normal && rate != s_ownedClockRate)','if (false)'),
  ('shutdown-accesses-facade','if (g_navMeshStopSeen)','if (false)'),
 ]
 for name,before,after in mutations:
  assert src.count(before)==1,(name,'unique mutation')
  assert run(name,'O2',src.replace(before,after))!=0,name+' survived'
  print(name+': compiled, expected runtime RED')
