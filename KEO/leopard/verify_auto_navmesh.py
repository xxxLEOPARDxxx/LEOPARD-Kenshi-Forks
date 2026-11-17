"""Offline real production policy/runtime under fake native boundaries; no game."""
from pathlib import Path
import ast, gettext, hashlib, json, os, re, struct, subprocess
import importlib.util
root=Path(__file__).resolve().parents[1]
out=root/'build/auto-nav-verification';out.mkdir(parents=True,exist_ok=True)
src=root/'src/navmesh/cache/nm_auto_rebuild.cpp'
body=src.read_text()
(out/'nm_auto_runtime.inc').write_text(re.sub(r'^#include[^\n]*\n','',body,flags=re.M))
hook=(root/'src/zone/camera_zone_hook.cpp').read_text().split('void hook_updateCameraZone(',1)[1]
assert hook.index('CameraZoneFocusAndPreload(')<hook.index('NmAutoRebuildTick(')
assert 'NmAutoRebuildOnWorldReset();' in (root/'src/zone/reset/preload_saveload.cpp').read_text()
assert 's_repairFurniture(editor)' in body
assert 'NmForceRebuildFullZone(navMesh,zone)' in body
assert 'ZoneHandoffAdoptionCycleInFlight' not in body
results={}
spec=importlib.util.spec_from_file_location('pe',root/'tools/kenshilib/check_imports.py')
pe=importlib.util.module_from_spec(spec);spec.loader.exec_module(pe)
game=Path('D:/SteamLibrary/steamapps/common/Kenshi')
repair_head=bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',body.split('kRepairHead[32] =',1)[1].split('};',1)[0]))
assert len(repair_head)==32
results['nativeRepairProof']={}
for label,path,rva,digest in (
 ('Steam',game/'RE_Kenshi/Kenshi_x64.exe',0x771C70,'504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'),
 ('GOG',game/'_codex_work/20261105-keo-gog-native/re_kenshi/kenshi_GOG_x64.exe',0x7715A0,'2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681')):
 image=pe.PE(path);actual=hashlib.sha256(image.data).hexdigest()
 # Authenticate both supported executables and actual native function windows.
 assert actual==digest
 assert image.data[image.offset(rva):image.offset(rva)+32]==repair_head
 results['nativeRepairProof'][label]={'exe_sha256':actual,'rva':hex(rva),'matchingBytes':32}
force=(root/'src/navmesh/cache/nm_force_rebuild.cpp').read_text()
def function(name):
 start=force.index('bool '+name+'(');brace=force.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(force[end]=='{')-(force[end]=='}');end+=1
 return force[start:end]+'\n'
(out/'nm_full_zone.inc').write_text(''.join(function(n) for n in ('NmForceRebuildFullZoneReady','NmForceRebuildKeyPending','NmForceRebuildFullZone')))
include=out/'nm_auto_runtime.inc'
clean=include.read_text()
for optimization in ('Od','O2'):
 exe=out/f'runtime-{optimization}.exe'
 cmd=['cl','/nologo','/EHsc','/'+optimization,'/Isrc','/Itools/tests','/I'+str(out),
      'tools/tests/nm_auto_runtime_fixture.cpp','src/navmesh/cache/nm_auto_rebuild_policy.cpp',
      '/Fe'+str(exe),'/Fo'+str(out)+'\\']
 p=subprocess.run(cmd,cwd=root,text=True,capture_output=True)
 (out/f'compile-{optimization}.log').write_text(p.stdout+p.stderr)
 assert p.returncode==0,p.stdout+p.stderr
 p=subprocess.run([str(exe)],cwd=root,text=True,capture_output=True)
 (out/f'runtime-{optimization}.log').write_text(p.stdout+p.stderr)
 assert p.returncode==0,p.stdout+p.stderr
 results[optimization]=p.stdout.strip()
 full_exe=out/f'full-zone-{optimization}.exe'
 full_cmd=['cl','/nologo','/EHsc','/'+optimization,'/Itools/tests','/I'+str(out),
           'tools/tests/nm_full_zone_fixture.cpp','/Fe'+str(full_exe),'/Fo'+str(out)+'\\']
 p=subprocess.run(full_cmd,cwd=root,text=True,capture_output=True)
 assert p.returncode==0,p.stdout+p.stderr
 p=subprocess.run([str(full_exe)],cwd=root,text=True,capture_output=True)
 assert p.returncode==0,p.stdout+p.stderr
 results['full-zone-'+optimization]=p.stdout.strip()

# Each changed production rule must make a compiled runtime CHECK fail. This
# also checks that the fake native boundaries do not accidentally hide gates.
mutants={
 'ownership':('!ZoneInSetB(zoneMgr,zone)','false'),
 'live-off':('!navmesh::g_navmeshCfg.autoNavmeshRepairEnabled','false'),
 'adoption':('!ZoneHandoffAdoptedThisFrame()','true'),
 'queue':('!QueueIdle(nmg)','false'),
 'cooldown-clock':('NmAutoAccepted(&s_auto,cell,completed)','NmAutoAccepted(&s_auto,cell,now)'),
 'world-reset':('NmAutoReset(&s_auto);','/* lost reset */'),
}
results['mutants']={}
try:
 for name,(old,new) in mutants.items():
  assert clean.count(old)==1,(name,old)
  include.write_text(clean.replace(old,new))
  mutant_exe=out/'mutant.exe'
  mutant_cmd=[('/Fe'+str(mutant_exe)) if x.startswith('/Fe') else x for x in cmd]
  p=subprocess.run(mutant_cmd,cwd=root,text=True,capture_output=True)
  assert p.returncode==0,(name,p.stdout+p.stderr)
  p=subprocess.run([str(mutant_exe)],cwd=root,text=True,capture_output=True)
  (out/f'mutant-{name}.log').write_text(p.stdout+p.stderr)
  assert p.returncode==1 and 'FAIL ' in p.stdout,(name,p.returncode,p.stdout+p.stderr)
  results['mutants'][name]='compiled runtime CHECK failure'
finally:
 include.write_text(clean)

bench=out/'scheduling-benchmark.exe'
bench_cmd=['cl','/nologo','/EHsc','/O2','/DKEO_AUTO_BENCH','/Isrc','/Itools/tests','/I'+str(out),
 'tools/tests/nm_auto_runtime_fixture.cpp','src/navmesh/cache/nm_auto_rebuild_policy.cpp','/Fe'+str(bench),'/Fo'+str(out)+'\\']
p=subprocess.run(bench_cmd,cwd=root,text=True,capture_output=True);assert p.returncode==0,p.stdout+p.stderr
results['schedulingOnlyBenchmark']=[]
for repeat in range(5):
 p=subprocess.run([str(bench)],cwd=root,text=True,capture_output=True);assert p.returncode==0,p.stdout+p.stderr
 rows=[line for line in p.stdout.splitlines() if line.startswith('SCHEDULING_BENCH ')]
 assert len(rows)==3
 results['schedulingOnlyBenchmark'].append(rows)
(out/'scheduling-benchmark.log').write_text('\n'.join(row for group in results['schedulingOnlyBenchmark'] for row in group)+'\n')

catalogs={}
for lang in ('en_GB','ru_RU','zh_CN'):
 cat={};key=None;value='';mode=None
 for line in (root/f'locale/{lang}/LC_MESSAGES/keo.po').read_text(encoding='utf-8-sig').splitlines()+['']:
  if not line.strip():
   if key is not None:
    assert key not in cat,(lang,key);cat[key]=value
   key=None;value='';mode=None
  elif line.startswith('msgid '):key=ast.literal_eval(line[6:]);mode='id'
  elif line.startswith('msgstr '):value=ast.literal_eval(line[7:]);mode='str'
  elif line.startswith('"'):
   if mode=='id':key+=ast.literal_eval(line)
   elif mode=='str':value+=ast.literal_eval(line)
 assert all(not k or v for k,v in cat.items())
 assert 'Pathing repairs itself' in cat
 if lang=='ru_RU':assert all(not re.search('[\u2010-\u2015\u2212]',v) for k,v in cat.items() if k)
 keys=sorted(cat);n=len(keys);offset=28+16*n;tables=[];data=b''
 for vals in ([k.encode() for k in keys],[cat[k].encode() for k in keys]):
  table=[]
  for v in vals:table.append((len(v),offset+len(data)));data+=v+b'\0'
  tables.append(b''.join(struct.pack('<II',*x) for x in table))
 dest=out/lang/'LC_MESSAGES/keo.mo';dest.parent.mkdir(parents=True,exist_ok=True)
 dest.write_bytes(struct.pack('<7I',0x950412de,0,n,28,28+8*n,0,0)+b''.join(tables)+data)
 with dest.open('rb') as f:t=gettext.GNUTranslations(f)
 assert all(t.gettext(k)==v for k,v in cat.items() if k)
 catalogs[lang]={'keys':n-1,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
results['catalogs']=catalogs
results['production_sha256']=hashlib.sha256(src.read_bytes()).hexdigest()
(out/'verification.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
print(json.dumps(results,ensure_ascii=False,indent=2))
