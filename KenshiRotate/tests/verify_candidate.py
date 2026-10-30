"""Offline PE/import and hash-bound rotation-map profile; never loads a DLL."""
from pathlib import Path
import hashlib, importlib.util, json, sys, gettext
sys.path.insert(0, 'D:/DEV/Kenshi/tools')
sys.path.insert(0, 'D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/qol-review/pythonlibs')
from qol_binary_check import PE
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86_const import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

module = Path(__file__).resolve().parents[1]
build = Path('D:/DEV/Kenshi/build/KenshiRotate')
game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
path = build/'KenshiRotate.dll'
p = PE(path)
assert p.machine == 0x8664
base = p.u64(p.u32(0x3c)+48)
table = p.offset(p.u32(p.dirs))
names, ordinals, funcs = (p.offset(p.u32(table+x)) for x in (32,36,28))
exports = {p.string(p.u32(names+4*i)): p.u32(funcs+4*p.u16(ordinals+2*i)) for i in range(p.u32(table+24))}
required = ['?startPlugin@@YAXXZ','MCM_Describe','KenshiRotate_ApiVersion',
 'KenshiRotate_RefreshVisuals','KenshiRotate_IsRotated','KenshiRotate_CanRotate','KenshiRotate_SetRotated']
assert all(n in exports for n in required), exports
checked = 0
for dll, imports in p.imports().items():
    if dll.lower() in {'kenshilib.dll','ogremain_x64.dll','myguiengine_x64.dll'}:
        missing = set(imports)-PE(game/dll).exports()
        assert not missing, (dll, sorted(missing))
        checked += len(imports)
assert {'MSVCP100.dll','MSVCR100.dll'} <= set(p.imports()), p.imports().keys()
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
def instructions(rva, size):
    return list(md.disasm(p.data[p.offset(rva):p.offset(rva)+size], base+rva))
entry = exports['KenshiRotate_IsRotated']
body = next(i.operands[0].imm-base for i in instructions(entry,64)
            if i.mnemonic == 'call' and i.operands[0].type == X86_OP_IMM)
code = instructions(body,256)
def rip_rva(i, op): return i.address+i.size+op.mem.disp-base
count = next(rip_rva(i,i.operands[0]) for i in code if i.mnemonic=='cmp'
    and i.operands[0].type==X86_OP_MEM and i.operands[0].mem.base==X86_REG_RIP and i.operands[0].size==8)
head = next(rip_rva(i,i.operands[1]) for i in code if i.mnemonic=='mov' and len(i.operands)==2
    and i.operands[1].type==X86_OP_MEM and i.operands[1].mem.base==X86_REG_RIP and i.operands[1].size==8)
assert count == head+8, (head,count)
assert any(i.mnemonic=='lea' and len(i.operands)==2 and i.operands[1].type==X86_OP_MEM
    and i.operands[1].mem.disp==0x58 for i in code), 'Item::handle offset changed'
profile = {
 'module':str(path),'sha256':hashlib.sha256(p.data).hexdigest(),'bytes':len(p.data),
 'image_base':hex(base),'is_rotated_export_rva':hex(entry),'is_rotated_body_rva':hex(body),
 'code_guards':[{'rva':hex(r),'bytes':p.data[p.offset(r):p.offset(r)+n].hex()} for r,n in ((entry,35),(body,25))],
 'set_head_pointer_rva':hex(head),'set_count_qword_rva':hex(count),
 'item_handle_offset':'0x58','node':{'left':0,'parent':8,'right':16,'hand':24,'color':56,'is_nil':57},
 'hand_fields':{'type':8,'container':12,'containerSerial':16,'index':20,'serial':24},
 'comparator':'unsigned lexicographic (container,containerSerial,index,serial); both NULL type11 equal',
 'synchronization':'No mutex/atomic: bounded revalidated RPM is observed, not atomic. Any mismatch/unreadable/cycle/limit => UNKNOWN.',
 'saved_tag':'GameData serialization state bdata[rotated]; not live Item field or shared prototype data',
 'native_imports_verified':checked,'exports':{n:hex(exports[n]) for n in required},
}
out=build/'KenshiRotate-rotation-read-profile.json'
out.write_text(json.dumps(profile,indent=2)+'\n',encoding='utf-8')
spec=importlib.util.spec_from_file_location('po2mo','D:/DEV/Kenshi/ours/tools/po2mo.py')
po2mo=importlib.util.module_from_spec(spec);spec.loader.exec_module(po2mo)
key='Log rotations and manual stack checks in RE_Kenshi_log.txt. Helps diagnose item ownership and incompatible stacks; does not change items.'
catalogs={}
for lang in ('en_GB','ru_RU','zh_CN'):
    entries=po2mo.parse_po(str(module/'locale'/lang/'LC_MESSAGES/kenshi_rotate.po'))
    target=build/'locale'/lang/'LC_MESSAGES/kenshi_rotate.mo';target.parent.mkdir(parents=True,exist_ok=True)
    po2mo.write_mo(entries,str(target))
    with target.open('rb') as f: translated=gettext.GNUTranslations(f)
    assert translated.gettext(key)==entries[key] and entries[key]
    catalogs[lang]={'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'entries':len(entries),'tooltip':entries[key]}
print(json.dumps({'dll':str(path),'sha256':profile['sha256'],'bytes':profile['bytes'],
 'native_imports_verified':checked,'profile':str(out),'catalogs':catalogs},ensure_ascii=False,indent=2))
