"""Offline production/header/native-Ogre verification, no game/renderer run."""
from pathlib import Path
import sys, subprocess, hashlib, json, time
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/retention-victim-verification';OUT.mkdir(parents=True,exist_ok=True)
production=['src/resources/resource_geometry.cpp','src/resources/resource_texture_retention.cpp','src/resources/retention_victim_index.h']
tests=['leopard/test_resource_geometry.cpp','leopard/test_resource_texture_retention.cpp','leopard/test_retention_alloc.h','leopard/test_retention_victim_index.cpp','leopard/test_retention_victim_index.cmd','leopard/test_retention_victim_cost.py','leopard/test_retention_victim_mutants.py','leopard/verify_retention_victim_index.py']
def snapshot(files):return {f:{'sha256':hashlib.sha256((ROOT/f).read_bytes()).hexdigest(),'bytes':(ROOT/f).stat().st_size}for f in files}
before=snapshot(production+tests);results=[]
commands=[]
for opt in ['O2','Od']:
    for target,cmd in [('index','test_retention_victim_index.cmd'),('geometry','test_resource_geometry.cmd'),('texture','test_resource_texture_retention.cmd')]:
        commands.append((target+'-'+opt,['cmd','/c','leopard\\'+cmd,opt]))
commands += [('cost',[sys.executable,'leopard/test_retention_victim_cost.py']),('mutants',[sys.executable,'leopard/test_retention_victim_mutants.py'])]
for name,cmd in commands:
    start=time.monotonic();r=subprocess.run(cmd,cwd=ROOT,capture_output=True,timeout=180)
    log=OUT/(name+'.log');log.write_bytes(r.stdout+r.stderr)
    summary='\n'.join(line for line in (r.stdout+r.stderr).decode(errors='replace').splitlines() if any(word in line for word in ['checks=','comparisons=','entry bytes=','actions=','compiled runtime RED controls=']))
    print(name+': exit='+str(r.returncode)+' '+summary,flush=True)
    results.append({'name':name,'command':cmd,'exit':r.returncode,'elapsed_seconds':round(time.monotonic()-start,3),'summary':summary,'log':str(log.relative_to(ROOT))})
    if r.returncode:print((r.stdout+r.stderr).decode(errors='replace'));sys.exit(r.returncode)
assert snapshot(production+tests)==before,'inputs changed during verification'
receipt={'status':'PASS','production':{f:before[f]for f in production},'tests':{f:before[f]for f in tests},'results':results,'baseline':json.loads((ROOT/'leopard/retention-victim-cost-baseline.json').read_text()),'mutants':json.loads((ROOT/'leopard/retention-victim-mutants-20261116.json').read_text()),'fixed_memory_bytes':{'geometry_index':16400,'texture_index':8208,'per_entry_increment':8,'geometry_capacity':2048,'texture_capacity':1024,'combined_at_capacity':49184}}
(ROOT/'leopard/retention-victim-index-20261116.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('retention victim verification PASS: '+str(len(results))+' commands')
