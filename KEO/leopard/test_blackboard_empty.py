from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/blackboard-empty';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for opt in ['Od','O2']:
 exe=OUT/(opt+'.exe');r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(ROOT/'leopard/test_blackboard_empty.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert r.returncode==0,(r.stdout+r.stderr).decode(errors='replace')
 r=subprocess.run([str(exe)],capture_output=True);print((r.stdout+r.stderr).decode(),end='')
 if '--expect-red' in sys.argv:assert r.returncode!=0 and b'FAIL 'in r.stderr
 else:assert r.returncode==0

if '--mutants' in sys.argv:
 source=(ROOT/'src/fixes/world/blackboard_empty.cpp').read_text();fixture=(ROOT/'leopard/test_blackboard_empty.cpp').read_text()
 cases=[('missing-phase','if(phase&&','if(true&&'),('missing-owner','&&IsMainThread()','&&true'),('unknown-owner-caller','(uintptr_t)_ReturnAddress()==imageBase+0x78848a','true'),('unknown-board-caller','(uintptr_t)_ReturnAddress()==imageBase+0x7eb1bd','true'),('partial-ready','!ownerEntered&&InterlockedCompareExchange(&ready,0,0)!=0','!ownerEntered'),('reenter-masked-phase','const bool allowed=!ownerEntered&&','const bool allowed=!phase&&'),('stale-scope','~PhaseScope(){phase=previous;ownerEntered=entered;}','~PhaseScope(){}'),('ignore-nonempty','&&ReadableEmpty(self)','&&self'),('ignore-live-toggle','&&fixes::g_fixesCfg.blackboardEmptyEnabled&&ReadableEmpty(self)','&&ReadableEmpty(self)'),('retry-install','if(tried)return;tried=true;','tried=true;'),('ignore-proof','||memcmp((const char*)module+p.rva,p.bytes,p.size)',''),('early-ready','if(KenshiLib::AddHook((void*)(imageBase+0x26a2e0)','InterlockedExchange(&ready,1);if(KenshiLib::AddHook((void*)(imageBase+0x26a2e0)')]
 for name,before,after in cases:
  assert source.count(before)==1,name
  d=OUT/name;d.mkdir(exist_ok=True);cpp=d/'production.cpp';cpp.write_text(source.replace(before,after));test=d/'fixture.cpp';test.write_text(fixture.replace('../src/fixes/world/blackboard_empty.cpp',cpp.as_posix()))
  exe=d/'test.exe';r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/O2','/I'+str(ROOT/'src'),str(test),'/Fo'+str(d)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert r.returncode==0,(r.stdout+r.stderr).decode(errors='replace')
  r=subprocess.run([str(exe)],capture_output=True);assert r.returncode!=0 and b'FAIL 'in r.stderr,name;print(name+': compiled runtime RED')
