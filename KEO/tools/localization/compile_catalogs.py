"""Compile exactly KEO's EN/RU/ZH source PO catalogs to runtime MO only.
Stdlib-only, deterministic GNU gettext revision 0; rejects missing/empty,
duplicate, fuzzy or changed format placeholders before writing output.
"""
from pathlib import Path
import argparse,ast,gettext,io,re,struct
ROOT=Path(__file__).resolve().parents[2]
LANGUAGES=('en_GB','ru_RU','zh_CN')
def read_po(path):
    entries={};key=None;text='';field=None
    def commit():
        if key is not None:
            if key in entries:raise ValueError(f'{path}: duplicate key {key!r}')
            entries[key]=text
    for raw in path.read_text(encoding='utf-8-sig').splitlines():
        line=raw.strip()
        if line.startswith('#,') and 'fuzzy' in line:raise ValueError(f'{path}: fuzzy translation')
        if line.startswith('msgid '):
            commit();key=ast.literal_eval(line[6:]);text='';field='id'
        elif line.startswith('msgstr '):text=ast.literal_eval(line[7:]);field='str'
        elif line.startswith('"'):
            part=ast.literal_eval(line)
            if field=='id':key+=part
            elif field=='str':text+=part
        elif line and not line.startswith('#'):raise ValueError(f'{path}: unsupported PO syntax {line}')
    commit();return entries
def specs(s):return re.findall(r'%(?!%)(?:[-+#0]*\d*(?:\.\d+)?[hlLzjt]*)?[diouxXeEfgGaAcspn]',s.replace('%%',''))
def compile_mo(entries):
    pairs=sorted((k.encode('utf-8'),v.encode('utf-8')) for k,v in entries.items())
    n=len(pairs);offset=28+16*n;originals=b'';translated=b'';ot=[];tt=[]
    for k,v in pairs:ot.append((len(k),offset+len(originals)));originals+=k+b'\0'
    offset+=len(originals)
    for k,v in pairs:tt.append((len(v),offset+len(translated)));translated+=v+b'\0'
    return struct.pack('<7I',0x950412de,0,n,28,28+8*n,0,0)+b''.join(struct.pack('<2I',*x) for x in ot+tt)+originals+translated

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    catalogs={lang:read_po(ROOT/f'locale/{lang}/LC_MESSAGES/keo.po') for lang in LANGUAGES}
    keys=set(catalogs['en_GB'])
    for lang,entries in catalogs.items():
        if set(entries)!=keys:raise ValueError(f'{lang}: keys differ from EN')
        for k,v in entries.items():
            if not v:raise ValueError(f'{lang}: empty translation {k!r}')
            if specs(k)!=specs(v):raise ValueError(f'{lang}: format mismatch {k!r}')
        data=compile_mo(entries);loaded=gettext.GNUTranslations(io.BytesIO(data))
        assert all(loaded.gettext(k)==v for k,v in entries.items() if k)
        target=args.output/lang/'LC_MESSAGES/keo.mo';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        print(f'{lang}: {len(entries)-1} keys, {len(data)} bytes -> {target}')
if __name__=='__main__':main()
