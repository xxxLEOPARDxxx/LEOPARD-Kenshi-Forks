from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/hook-batch';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
if '--sdk-red' in sys.argv:
 src=Path('D:/DEV/Kenshi/deps/KenshiLib_keo/Source/core/Functions_static.cpp').read_text();src=re.sub(r'^#include[^\n]*\n','',src,flags=re.M)
 p=OUT/'sdk-original.cpp';p.write_text((ROOT/'leopard/test_sdk_queue_ids.cpp').read_text().replace('// SDK_STATIC_WRAPPERS',src));paths=[('sdk-original','Od',p)]
else:paths=[(opt,opt,ROOT/'leopard/test_hook_batch.cpp')for opt in ('Od','O2')]
for name,opt,p in paths:
 exe=OUT/(name+'.exe');b=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(p),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
 r=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((r.stdout+r.stderr).decode(errors='replace'),end='')
 if '--sdk-red' in sys.argv:assert r.returncode!=0 and b'EXPECTED RED' in r.stdout
 else:assert r.returncode==0

if '--mutants' in sys.argv:
 raw=(ROOT/'src/plugin/independent_hook_batch.h').read_text()
 cases=[('split-module-id','apply(id)','apply(id+1)'),('early-collect','state=2;','state=1;'),('missing-original-gate','if(count&&originalsValid)','if(count)'),('false-apply-success','applied=apply(id)==0','applied=(apply(id),true)'),('lost-duplicate-guard','if(rows[i].row==row)return false;','if(false)return false;'),('repeat-apply','state=3;\n  for(int i=0;i<count;++i)','state=1;\n  for(int i=0;i<count;++i)')]
 for name,before,after in cases:
  assert raw.count(before)==1,name
  d=OUT/name;(d/'plugin').mkdir(parents=True,exist_ok=True);(d/'plugin/independent_hook_batch.h').write_text(raw.replace(before,after))
  exe=d/'test.exe';b=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/O2','/I'+str(d),str(ROOT/'leopard/test_hook_batch.cpp'),'/Fo'+str(d)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
  r=subprocess.run([str(exe)],capture_output=True);assert r.returncode!=0,name;print(name+': compiled expected RED')
