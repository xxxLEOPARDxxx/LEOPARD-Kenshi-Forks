"""Run bounded native/production contracts without executing Kenshi."""
from pathlib import Path
import sys,subprocess,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/skeleton-reserve-final';OUT.mkdir(parents=True,exist_ok=True)
paths=['src/fixes/world/skeleton_reserve.cpp','src/fixes/world/skeleton_reserve.h','leopard/test_skeleton_reserve.cpp','leopard/test_skeleton_reserve.cmd','leopard/test_skeleton_reserve_native.py','leopard/test_skeleton_reserve_mutants.py']
snapshot={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in paths}
commands=[['cmd','/c','leopard\\test_skeleton_reserve.cmd','Od'],['cmd','/c','leopard\\test_skeleton_reserve.cmd'],[sys.executable,'leopard/test_skeleton_reserve_native.py'],[sys.executable,'leopard/test_skeleton_reserve_mutants.py']]
rows=[]
for i,cmd in enumerate(commands):
 result=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True);log=OUT/f'{i+1}.txt';log.write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:print(result.stdout+result.stderr);sys.exit(result.returncode)
 rows.append({'command':cmd,'exit':result.returncode});print('PASS',i+1,' '.join(cmd),flush=True)
assert snapshot=={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in paths}
(OUT/'verification.json').write_text(json.dumps({'commands':rows,'source':snapshot,'game_run':False},indent=2),encoding='utf-8')
