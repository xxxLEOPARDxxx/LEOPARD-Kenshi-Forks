"""Executable negative controls extracted from the production current-end helper."""
from pathlib import Path
import os,subprocess,json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/animation_lookup_controls';OUT.mkdir(exist_ok=True)
source=(ROOT/'src/fixes/world/equipment_animation_lookup.cpp').read_text(encoding='utf-8')
mutations={
 'ignore-enabled':('!enabled||','false||'),
 'ignore-owner':('!onOwnerThread||','false||'),
 'signed-id':('*key<=Read<unsigned>(last,0x18)','(int)*key<=(int)Read<unsigned>(last,0x18)'),
 'stale-end-before-allocator':('Read<void*>(map,8),node','head,node'),
 'wrong-mapped-return':('(char*)out+0x20','(char*)out+0x18'),
 'nondefault-state':('const Pair pair={*key,0,NULL}','const Pair pair={*key,0,(void*)1}'),
 'skip-native-allocation':('void*node=allocate(map,&pair);','void*node=(void*)&pair;'),
 'double-fallback':('return original(map,key);','{original(map,key);return original(map,key);}'),
}
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ);env['INCLUDE']=f'{vc}/include;{sdk}/Include';env['LIB']=f'{vc}/lib/amd64;{sdk}/Lib/x64';env['PATH']=f'{vc}/bin/amd64;{sdk}/Bin;'+env['PATH']
rows=[]
for name,(before,after) in mutations.items():
 assert before in source,name
 file=OUT/(name+'.cpp');file.write_text(source.replace(before,after),encoding='utf-8');exe=OUT/(name+'.exe')
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I',str(ROOT/'src'),str(ROOT/'leopard/equipment_animation_boundaries.cpp'),str(file),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)]
 result=subprocess.run(cmd,env=env,capture_output=True,text=True);(OUT/(name+'-build.log')).write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==0,name
 result=subprocess.run([str(exe)],capture_output=True,text=True);(OUT/(name+'-run.log')).write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==1 and 'CHECK line' in result.stdout,(name,result.returncode,result.stdout)
 rows.append(dict(name=name,exit=result.returncode));print(name,'CHECK exit1')
(OUT/'verification.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('PASS',len(rows),'compiled animation helper controls')
