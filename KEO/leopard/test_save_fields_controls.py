"""Compiled negative controls against actual boundary fixtures; no game process."""
from pathlib import Path
import json,subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/save_fields_controls';OUT.mkdir(parents=True,exist_ok=True)
value=(ROOT/'src/fixes/world/save_field_value_transfer.cpp').read_text(encoding='utf-8')
leaf=(ROOT/'src/fixes/world/save_field_key_leaf.cpp').read_text(encoding='utf-8')
cases=[
 ('value_no_transfer',value,'dst->swap(*src);return true;','return false;','value'),
 ('value_no_swap',value,'dst->swap(*src);return true;','return true;','value'),
 ('value_reserved_destination',value,'||dst->capacity()!=15','','value'),
 ('value_retains_excess_capacity',value,'||capacity>(size|15)','','value'),
 ('value_unbounded',value,'||size>1048576','','value'),
 ('key_ignores_permissions',leaf,'OP(0x48,0x83,0x7e,0x20,0);c.J(0x85);','OP(0x48,0x83,0x7e,0x20,0);','key'),
 ('key_ignores_live_off',leaf,'OP(0x80,0x38,0);c.J(0x84);','OP(0x80,0x38,0);','key'),
 ('key_ignores_count_bound',leaf,'OP(0x49,0x83,0xf8,63);c.J(0x87);','OP(0x49,0x83,0xf8,63);','key'),
]
results=[]
for name,source,before,after,kind in cases:
 assert source.count(before)==1,name
 file=OUT/(name+'.cpp');file.write_text(source.replace(before,after),encoding='utf-8');exe=OUT/(name+'.exe')
 test='leopard/test_save_field_value.cpp'if kind=='value'else'leopard/test_save_field_keys.cpp'
 command=['cl','/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/I','src',test,str(file)]
 if kind=='key':command+=['src/fixes/near_page.cpp']
 command+=['/Fo'+str(OUT)+'/', '/Fe'+str(exe)]
 build=subprocess.run(command,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 assert build.returncode==0,(name,build.stdout)
 result=subprocess.run([str(exe)],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (OUT/(name+'.log')).write_text(build.stdout+'\n'+result.stdout,encoding='utf-8')
 assert result.returncode==1 and ('FAIL'in result.stdout or 'CHECK'in result.stdout),(name,result.returncode,result.stdout)
 results.append({'name':name,'exit':result.returncode,'ordinaryCheckFailure':True})
(OUT/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print('PASS:',len(results),'compiled negative controls rejected by ordinary checks')
