"""Reproduce bounded production/native model-clone checks without Kenshi."""
from pathlib import Path
import sys,subprocess,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/model-clone-reserve-final';OUT.mkdir(parents=True,exist_ok=True)
paths=['src/fixes/world/model_clone_reserve.cpp','src/fixes/world/model_clone_reserve.h','leopard/test_model_clone_reserve.cpp','leopard/test_model_clone_reserve.cmd','leopard/test_model_clone_reserve_native.py','leopard/test_model_clone_reserve_mutants.py','leopard/verify_model_clone_reserve.py']
snapshot={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in paths}
commands=[[sys.executable,'leopard/test_model_clone_reserve_native.py'],['cmd.exe','/d','/c','leopard\\test_model_clone_reserve.cmd','Od'],['cmd.exe','/d','/c','leopard\\test_model_clone_reserve.cmd','O2'],[sys.executable,'leopard/test_model_clone_reserve_mutants.py']]
rows=[]
for index,command in enumerate(commands):
 result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
 log=OUT/f'{index+1}.txt';log.write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:print(result.stdout+result.stderr);sys.exit(result.returncode)
 rows.append({'command':command,'exit_code':result.returncode,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()});print('PASS',index+1,' '.join(command),flush=True)
assert snapshot=={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in paths}
(OUT/'verification.json').write_text(json.dumps({'commands':rows,'source':snapshot,'game_execution':False,'external_process_access':False,'native_fixture_process_only':True},indent=2),encoding='utf-8')
print('PASS four model-clone verification commands')
