"""Standalone aggregate native morph-scope verification, no game execution."""
from pathlib import Path
import subprocess,sys,hashlib,json,re
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/morph-pose-scope-verification';OUT.mkdir(parents=True,exist_ok=True)
commands=[['cmd','/c',str(ROOT/'leopard/test_morph_pose_scope.cmd'),'Od'],['cmd','/c',str(ROOT/'leopard/test_morph_pose_scope.cmd')],[sys.executable,str(ROOT/'leopard/test_morph_pose_scope_native.py')]]
if '--reuse-controls' not in sys.argv:commands.append([sys.executable,str(ROOT/'leopard/test_morph_pose_scope_mutants.py')])
rows=[]
for index,command in enumerate(commands):
 result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
 output=result.stdout+result.stderr;(OUT/('%02d.log'%index)).write_text(output,encoding='utf-8')
 assert result.returncode==0 and 'PASS ' in output and 'CRASH' not in output,(command,result.returncode,output[-3000:])
 rows.append({'command':command,'exit':result.returncode,'pass':re.findall(r'^PASS .+$',output,re.MULTILINE)})
 print(*rows[-1]['pass'],sep='\n',flush=True)
controls=json.loads((ROOT/'build/morph-pose-scope-mutants/verification.json').read_text())
for path,expected in controls['canonical_sources'].items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==expected,path
assert len(controls['controls'])==25 and all(row['compile_exit']==0 and row['run_exit']==1 and row['check'] for row in controls['controls'])
paths=['src/fixes/world/morph_pose_scope.h','src/fixes/world/morph_pose_scope.cpp','leopard/test_morph_pose_scope.cpp','leopard/test_morph_pose_scope.cmd','leopard/test_morph_pose_scope_native.py','leopard/test_morph_pose_scope_mutants.py','leopard/test_morph_pose_scope_all.py']
receipt={'commands':rows,'controls':25,'controls_reused':'--reuse-controls' in sys.argv,'owned_files':{path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest()for path in paths},'game_execution':False,'process_access':False}
(OUT/'verification.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print('PASS aggregate native morph-pose scope verification')
