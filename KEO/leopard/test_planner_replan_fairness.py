"""Compile/exercise actual production scheduler; no game/process or native calls."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/planner-replan-fairness';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/planner/planner_tick.cpp').read_text()
def function(marker):
 start=source.index(marker);brace=source.index('{',start);at=brace+1;depth=1
 while depth:
  depth+=(source[at]=='{')-(source[at]=='}');at+=1
 return source[start:at]
helper=function('static void TickPlanSlots(') if 'static void TickPlanSlots(' in source else ''
tick=function('void PlannerTick(void* zoneMgr, double now)')
template=(ROOT/'leopard/test_planner_replan_fairness.cpp').read_text()
def compose(h,t):return template.replace('// PRODUCTION_HELPER',h).replace('// PRODUCTION_TICK',t)
fixture=compose(helper,tick)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,text,opt):
 cpp=OUT/(name+'.cpp');cpp.write_text(text);exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(cpp),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe)]
 if helper:args.append('/DHAVE_PRODUCTION_HELPER')
 build=subprocess.run(args,cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-build.txt')).write_bytes(build.stdout+build.stderr)
 assert build.returncode==0,(build.stdout+build.stderr).decode(errors='replace')
 result=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(result.stdout+result.stderr)
 return result
for opt in ('Od','O2'):
 result=run('red-original' if '--expect-red' in sys.argv else opt,fixture,opt);print(result.stdout.decode(errors='replace')+result.stderr.decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert result.returncode!=0 and b'attempts[4]==1' in result.stderr;print('expected production scheduler RED');raise SystemExit(0)
 assert result.returncode==0
if '--mutants' in sys.argv:
 cases=[
  ('fixed-first-slot','helper','const int first = s_replanCursor;','const int first = 0;'),
  ('cursor-not-advanced','helper','s_replanCursor = (slot + 1) % PLAN_SLOTS;','s_replanCursor = first;'),
  ('cursor-skips-budget-sized-block','helper','s_replanCursor = (slot + 1) % PLAN_SLOTS;','s_replanCursor = (slot + replanBudget) % PLAN_SLOTS;'),
  ('hardcoded-wrap','helper','s_replanCursor = (slot + 1) % PLAN_SLOTS;','s_replanCursor = (slot + 1) % 64;'),
  ('unbounded-attempts','helper','now, replans < replanBudget)','now, true)'),
  ('validation-stops-at-budget','helper','scanned < PLAN_SLOTS;','scanned < PLAN_SLOTS && replans < replanBudget;'),
  ('overcap-still-validated','helper','if (nPlayers < 0)','if (false)'),
  ('world-cursor-not-reset','tick','\t\ts_replanCursor = 0;','\t\t/* cursor survives reset */'),
 ]
 for name,part,before,after in cases:
  original=helper if part=='helper' else tick;assert original.count(before)==1,(name,'unique production mutation')
  changed=original.replace(before,after)
  result=run(name,compose(changed if part=='helper' else helper,changed if part=='tick' else tick),'O2')
  assert result.returncode!=0 and b'FAIL ' in result.stderr,(name,'expected compiled assertion failure')
  print(name+': expected RED')
 print('PASS',len(cases),'compiled actual production scheduling mutants')
