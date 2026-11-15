from pathlib import Path
import re,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/path-telemetry-gate';OUT.mkdir(parents=True,exist_ok=True)
def section(s,marker,semi=False):
 a=s.index(marker);p=s.index('{',a)+1;d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[a:p+(1 if semi else 0)]
def strip(s):return re.sub(r'^#(?:include|pragma intrinsic)[^\n]*\n','',s,flags=re.M)
pool=(ROOT/'src/pathfind/path_pool_hooks.cpp').read_text();full=(ROOT/'src/pathfind/pathfind_hooks.cpp').read_text();cost=(ROOT/'src/pathfind/astar_cost.cpp').read_text();header=(ROOT/'src/pathfind/path_pool_internal.h').read_text()
types='namespace path_pool_detail {struct PPHist { volatile LONG n; };\n'+section(header,'struct PathPassWindow',True)+'\n}'
context=section(full,'struct FindPathFullCtx',True)
helpers='\n'.join(section(full,m) for m in ['static void TakeFindPathWave(','static void ResolveFindPathRequest(','static void ReadFindPathInput(','static void CountFindPathWave(','static void BoostFindPathBudget(']).replace('static void ReadFindPathInput(','static void ReadInputActual(')
fallback=section(full,'static bool CapturePathBudgetRequest(')+'\n'+section(full,'char hook_csFindPathFallback(')
template=(ROOT/'leopard/test_path_telemetry_gate.cpp').read_text()
costtypes=section((ROOT/'src/pathfind/astar_cost.h').read_text(),'struct AstarCostSample',True)
def compose(p,f,c):
 return template.replace('// POOL_TYPES',types).replace('// POOL_PRODUCTION',strip(p)).replace('// FULL_TYPES',context).replace('// FULL_HELPERS',helpers).replace('// CONN_OWNER',section(f,'static int ConnGateOwner(')).replace('// FULL_HOOK',section(f,'void hook_findPathFull(')).replace('// FALLBACK',fallback).replace('// COST_TYPES',costtypes).replace('// COST_PRODUCTION',strip(c))
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,opt,p=pool,f=full,c=cost):
 cpp=OUT/(name+'.cpp');cpp.write_text(compose(p,f,c));exe=OUT/(name+'.exe')
 b=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/DKEO_DEBUG','/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/pathfind/path_budget_policy.cpp'),str(ROOT/'src/pathfind/astar_cost_policy.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);(OUT/(name+'-build.txt')).write_bytes(b.stdout+b.stderr);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
 r=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(r.stdout+r.stderr);print((r.stdout+r.stderr).decode(errors='replace'),end='');return r
for opt in ('Od','O2'):
 r=run('red-original' if '--expect-red' in sys.argv else opt,opt)
 if '--expect-red' in sys.argv:assert r.returncode!=0 and b'qpcs==0&&hists==0' in r.stderr;print('expected production telemetry OFF RED');break
 assert r.returncode==0
if '--mutants' in sys.argv:
 cases=[
  ('off-pass-measured','pool','if (!t_measureContentStream)','if (false)'),
  ('off-guard-bypassed','pool','return fixes::g_fixesCfg.navMeshUpdateGuardEnabled\n\t\t\t? NavMeshUpdateGuardCall(sectionMgr, orig_contentStream)\n\t\t\t: orig_contentStream(sectionMgr);','return orig_contentStream(sectionMgr);'),
  ('stale-request-stamp','pool','*(LONGLONG*)((char*)req + REQ_STAMP_OFFSET) = 0;','/* stale stamp */'),
  ('off-pass-toggle-stamped','pool','!DetailedLogEnabled() || !t_measureContentStream','!DetailedLogEnabled()'),
  ('off-result-diagnosed','pool','DetailedLogEnabled() && t_measureContentStream && req && mgr','req && mgr'),
  ('priority-skipped','pool','PathPriorityBeforeEnqueue(queueBase,itemPtr,_ReturnAddress());','/* missing priority */'),
  ('nested-tls-lost','pool','*flag = previous;','*flag = 0;'),
  ('nested-timing-lost','pool','g_passStartTicks = start;','/* stale inner start */'),
  ('off-search-measured','full','if (!DetailedLogEnabled())','if (false)'),
  ('off-budget-lost','full','BoostFindPathBudget(searchState, c);','/* missing player budget */'),
  ('off-hierarchy-lost','full','AstarHierSearch(streamingCollection, searchState, findPathOutput, astarReturnAddr, c.pathPoolPlayerByReq);','/* missing native/hier search */'),
  ('off-owner-token-stale','full','t_waveAuth = 0;','/* stale owner token */'),
  ('cost-still-sampled','cost','if (!DetailedLogEnabled() || !s)','if (!s)'),
  ('off-transition-counted','cost','if (!DetailedLogEnabled()) return;','/* sampled cap denominator lost */'),
 ]
 for name,unit,before,after in cases:
  p,f,c=pool,full,cost
  if unit=='full':
   hook=section(f,'void hook_findPathFull(');assert before in hook,name;f=f.replace(hook,hook.replace(before,after,1))
  elif unit=='pool':assert before in p,name;p=p.replace(before,after,1)
  else:assert before in c,name;c=c.replace(before,after,1)
  r=run(name,'O2',p,f,c);assert r.returncode!=0,name+' survived';print(name+': compiled expected runtime RED')
