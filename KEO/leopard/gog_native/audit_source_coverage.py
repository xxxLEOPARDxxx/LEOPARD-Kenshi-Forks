"""Bounded source inventory check. Not a substitute for runtime GOG testing."""
from pathlib import Path
import re,json
here=Path(__file__).resolve().parent
root=here.parents[1]
text=(root/'src/game/gog_profile_data.inc').read_text(encoding='utf8')
pairs={int(a,16):int(b,16) for a,b in re.findall(r'\{0x([0-9A-F]+)u,0x([0-9A-F]+)u\}',text.split('reverseAddresses')[0])}
report=json.loads((here/'source-inventory.json').read_text(encoding='utf8'))
constants={x['name']:int(x['steam'],16) for x in report['constants']}
# Actual source constants, including arithmetic's fixed offset.
for file in (root/'src').rglob('*'):
 if file.suffix not in ('.cpp','.h'):continue
 s=file.read_text(encoding='utf8')
 for name,value in re.findall(r'\b([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\b',s):constants[name]=int(value,16)
constants['FOG_CALL_TARGET_AT']=7
for file in (root/'src').rglob('*'):
 if file.suffix not in ('.cpp','.h') or file.name.startswith('gog_profile'):continue
 for name,value in re.findall(r'\b(RVA_\w+|k\w+Rva)\s*=\s*(0x[0-9A-Fa-f]+)\b',file.read_text(encoding='utf8')):
  if name not in {'RVA_BARRIER_SYNC','RVA_UPDATE_VB','RVA_BASE_CULL','kAbstractShapeVtableRva','kPurecallRva','kConcreteShapeVtableRva','kPurecallHandlerPtrRva'}:
   assert int(value,16)in pairs,(file,name,'new unprofiled symbolic address')

modules={'RVA_BARRIER_SYNC','RVA_UPDATE_VB','RVA_BASE_CULL','kAbstractShapeVtableRva','kPurecallRva','kConcreteShapeVtableRva','kPurecallHandlerPtrRva'}
for item in report['constants']:
 if item['name']not in modules:assert int(item['steam'],16)in pairs,item
# These variables have finite, separately inventoried domains: address table
# validators/manifest rows, source byte checks, input flags and clock anchors.
dynamic={'rva','c.rva','KEYCODE_FLAGS[i]','expectRva','fnRva','s.needleRva'}
rows=[];deferred=[]
for file in (root/'src').rglob('*'):
 if file.suffix not in ('.cpp','.h')or file.name.startswith('gog_profile'):continue
 s=re.sub(r'/\*.*?\*/|//[^\n]*','',file.read_text(encoding='utf8'),flags=re.S)
 for m in re.finditer(r'\bGameAddr\(',s):
  at=m.end();end=at;depth=1
  while depth:
   if s[end]=='(':depth+=1
   elif s[end]==')':depth-=1
   end+=1
  arg=s[at:end-1].strip();arg=re.sub(r'\((?:size_t|uintptr_t)\)','',arg).strip()
  if arg in ('uintptr_t rva','size_t rva'):continue
  if arg in dynamic:deferred.append((str(file.relative_to(root)),arg));continue
  expr=re.sub(r'\b[A-Za-z_]\w*\b',lambda x:str(constants[x[0]])if x[0]in constants else x[0],arg)
  assert re.fullmatch(r'[0-9xXa-fA-F+()\s]+',expr),(file,arg,'unclassified dynamic expression')
  value=eval(expr,{'__builtins__':{}},{})
  assert value in pairs,(file,arg,hex(value),'unmapped key')
  rows.append((str(file.relative_to(root)),arg,hex(value),hex(pairs[value])))
result={'static_calls':len(rows),'finite_domain_calls':deferred,'addresses':len(pairs),'scope':'All current inventoried symbolic keys and explicit GameAddr call expressions. Finite variable domains audited separately. Member offsets and module keys are not EXE RVAs.'}
(root/'build/gog-native-tests/source-coverage.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS source address coverage:',len(pairs),'explicit keys,',len(rows),'static GameAddr sites,',len(deferred),'finite-domain sites')

# Direct raw globals keep a fast Steam addition. Audit their second argument
# independently from GameAddr, without treating PE/member arithmetic as RVAs.
raw_sites=[]
for file in (root/'src').rglob('*'):
 if file.suffix not in ('.cpp','.h') or file.name in ('native_profile.cpp','native_profile.h'):continue
 t=re.sub(r'/\*.*?\*/|//[^\n]*','',file.read_text(encoding='utf8'),flags=re.S)
 for m in re.finditer(r'NativeProfileAddress\(\s*([^,]+),\s*([^\)]+)\)',t):
  key=m[2].strip()
  if key in ('s_entries[i].implementationRva','legacyRva'):continue # explicit SDK registry / central dispatch
  assert key in constants and constants[key]in pairs,(file,key,'unknown direct raw key')
  raw_sites.append((str(file.relative_to(root)),key))
# Every explicit functional literal discovered by review must stay mapped.
functional=[0x2EF6EF,0x2EF88F,0x2EFB10,0x3AE670,0x3AE99F,0x3AE6DB,0x3AE8A8,0x787CF9]
assert all(key in pairs for key in functional)
result['raw_profile_sites']=raw_sites
result['literal_functional_keys']=[hex(key)for key in functional]
(root/'build/gog-native-tests/source-coverage.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS direct raw access:',len(raw_sites),'sites; functional literals:',len(functional))

# Resolve's global Add loop must be included, not merely literal Bind/Add calls.
startup=(root/'src/game/klib_bindings.cpp').read_text(encoding='utf8')
match=re.search(r'const uintptr_t globals\[\]\s*=\s*\{([^}]+)\}',startup,re.S);assert match
globals_=[int(v,16)for v in re.findall(r'0x[0-9A-Fa-f]+',match[1])];assert len(globals_)==9
assert all(value in pairs for value in globals_),('missing startup global',[hex(v)for v in globals_ if v not in pairs])
print('PASS all9 actual Resolve-loop global keys covered')
