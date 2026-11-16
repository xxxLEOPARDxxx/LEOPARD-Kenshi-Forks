"""Compile the three source catalogs into private build output and roundtrip."""
from pathlib import Path
import ast,gettext,json,re,struct,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/material-reserve-locales';rows={}
new=['Prepare copied material storage','Reduces repeated small memory allocations when the game copies materials for models and equipment. Texture layers, colours, shaders and independent material copies stay native. Applies to newly copied materials on supported engine builds.']
for lang in ['en_GB','ru_RU','zh_CN']:
 p=ROOT/f'locale/{lang}/LC_MESSAGES/keo.po';catalog={};key=None;value='';mode=None
 for line in p.read_text(encoding='utf-8-sig').splitlines()+['']:
  if not line.strip():
   if key is not None:
    assert key not in catalog,(lang,key);catalog[key]=value
   key=None;value='';mode=None
  elif line.startswith('msgid '):key=ast.literal_eval(line[6:]);mode='id'
  elif line.startswith('msgstr '):value=ast.literal_eval(line[7:]);mode='str'
  elif line.startswith('"'):
   if mode=='id':key+=ast.literal_eval(line)
   elif mode=='str':value+=ast.literal_eval(line)
  else:assert not line.startswith(('msgid_plural','msgstr['))
 assert all(not k or v for k,v in catalog.items());assert all(k in catalog for k in new)
 assert len(catalog)-1==139,(lang,len(catalog)-1)
 if lang=='ru_RU':assert all(not re.search('[\u2010-\u2015\u2212]',v) for k,v in catalog.items() if k)
 keys=sorted(catalog);n=len(keys);offset=28+16*n;tables=[];data=b''
 for vals in ([k.encode() for k in keys],[catalog[k].encode() for k in keys]):
  table=[]
  for v in vals:table.append((len(v),offset+len(data)));data+=v+b'\0'
  tables.append(b''.join(struct.pack('<II',*x) for x in table))
 dest=OUT/lang/'LC_MESSAGES/keo.mo';dest.parent.mkdir(parents=True,exist_ok=True)
 dest.write_bytes(struct.pack('<7I',0x950412de,0,n,28,28+8*n,0,0)+b''.join(tables)+data)
 with dest.open('rb') as f:translated=gettext.GNUTranslations(f)
 assert all(translated.gettext(k)==v for k,v in catalog.items() if k)
 rows[lang]=dict(keys=n-1,sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),roundtrip=True)
 print(lang,n-1,'keys, MO roundtrip PASS')
(OUT/'verification.json').write_text(json.dumps(rows,indent=2))
