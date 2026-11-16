"""Compile wrong production variants; require ordinary CHECK exit1, no crash."""
from pathlib import Path
import os,subprocess,json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/save_instance_controls';OUT.mkdir(exist_ok=True)
source=(ROOT/'src/fixes/world/save_instance_lookup.cpp').read_text(encoding='utf-8')
fixture=(ROOT/'leopard/test_save_instance_lookup.cpp').read_text(encoding='utf-8')
mutations={
 'repeat-native-search':('if(node)return node;','if(node)return original(map,key);'),
 'ignore-admission':('admitted&&fixes::','true&&fixes::'),
 'ignore-live':('&&fixes::g_fixesCfg.saveInstanceLookupEnabled){','&&true){'),
 'ignore-owner':('IsMainThread()&&admitted','true&&admitted'),
 'ignore-inner':('*(const uintptr_t*)slot==innerReturn&&','true&&'),
 'ignore-outer':('*(const uintptr_t*)(slot+0x160)==outerReturn&&','true&&'),
 'ignore-parser':('*(const uintptr_t*)(slot+0x1d0)==parserReturn&&','true&&'),
 'ignore-map':('*(void*const*)(slot+0x150)==map&&','true&&'),
 'ignore-key':('*(const void*const*)(slot+0x148)==key){','true){'),
 'ignore-node-alignment':('node&&!((uintptr_t)node&7)','node'),
 'double-fallback':('return original(map,key);','original(map,key);return original(map,key);'),
 'ignore-native-proof':('memcmp((char*)game+proofs[i].rva,proofs[i].bytes,proofs[i].size)','false'),
 'ignore-world':('||*(void**)GameAddr(RVA_GLOBAL_SECTION_MGR)','||false'),
 'ignore-stamp':('stamp!=0x65d604d7','false'),
 'ignore-size':('size!=0x232c000','false'),
 'ignore-import':('*(void**)((char*)game+0x2244b08)!=(void*)GetProcAddress(crt,"memcmp")','false'),
}
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ);env['INCLUDE']=f'{vc}/include;{sdk}/Include';env['LIB']=f'{vc}/lib/amd64;{sdk}/Lib/x64';env['PATH']=f'{vc}/bin/amd64;{sdk}/Bin;'+env['PATH']
rows=[]
for name,(before,after) in mutations.items():
 assert before in source,name
 path=OUT/(name+'.cpp');path.write_text(source.replace(before,after),encoding='utf-8')
 main=OUT/(name+'-test.cpp');main.write_text(fixture.replace('../src/fixes/world/save_instance_lookup.cpp',path.as_posix()),encoding='utf-8')
 exe=OUT/(name+'.exe')
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/I',str(ROOT/'src'),str(main),str(ROOT/'src/fixes/near_page.cpp'),str(ROOT/'src/fixes/code_patch_threads.cpp'),str(ROOT/'src/fixes/code_patch_batch.cpp'),str(ROOT/'build/save_instance_test/frame.obj'),'/Fo'+str(OUT)+'/', '/Fe'+str(exe)]
 build=subprocess.run(cmd,env=env,capture_output=True,text=True);(OUT/(name+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');assert build.returncode==0,name
 run=subprocess.run([str(exe)],capture_output=True,text=True);(OUT/(name+'-run.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
 assert run.returncode==1 and 'CHECK line' in run.stdout,(name,run.returncode,run.stdout)
 rows.append(dict(name=name,exit=run.returncode));print(name,'CHECK exit1')
(OUT/'verification.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print('PASS',len(rows),'compiled saved-object production controls')
