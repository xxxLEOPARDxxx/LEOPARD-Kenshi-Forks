"""Read-only build/import and native stack ABI audit. Does not load the DLL."""
import argparse, hashlib, importlib.util, json, pathlib, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
GAME=pathlib.Path('D:/SteamLibrary/steamapps/common/Kenshi')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--dll',type=pathlib.Path,required=True);args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('qol_pe','D:/DEV/Kenshi/tools/qol_binary_check.py');pe_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(pe_module)
    pe=pe_module.PE(args.dll); assert pe.machine==0x8664
    assert 'MCM_Describe' in pe.exports()
    assert any('startPlugin' in name for name in pe.exports())
    imports=pe.imports(); native=[]
    for dll,names in imports.items():
        if dll.lower() not in ('kenshilib.dll','myguiengine_x64.dll','ogremain_x64.dll'): continue
        exported=pe_module.PE(GAME/dll).exports();missing=set(names)-exported
        assert not missing,(dll,sorted(missing))
        native += names
    assert not any('140' in name or 'vcruntime' in name.lower() for name in imports),imports.keys()
    assert 'MSVCP100.dll' in imports and 'MSVCR100.dll' in imports
    required_rotate = {'KenshiRotate_ApiVersion','KenshiRotate_IsRotated','KenshiRotate_CanRotate','KenshiRotate_SetRotated','KenshiRotate_RefreshVisuals'}
    raw=args.dll.read_bytes()
    assert all(name.encode('ascii')+b'\0' in raw for name in required_rotate), 'rotation consumer was compiled out'
    rotate_path=GAME/'mods/KenshiRotate/KenshiRotate.dll'
    if rotate_path.exists(): assert required_rotate <= pe_module.PE(rotate_path).exports()
    # Source path/offset verified against the installed SDK's Steam table.
    import pefile, capstone
    exe=GAME/'RE_Kenshi/Kenshi_x64.exe';p=pefile.PE(str(exe));dis=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    audit={}
    for name,rva,size in [('addQuantity',0x75D6E0,0x136),('sectionAddItem',0x74BD70,0x4E9),('canStackWith',0x75C930,0x90)]:
        audit[name]=['%x %s %s'%(i.address,i.mnemonic,i.op_str) for i in dis.disasm(p.get_data(rva,size),rva)]
    assert any('mov dword ptr [rdi + 0x12c], 1' in line for line in audit['sectionAddItem'])
    assert any('lea ecx, [rax + rcx - 1]' in line for line in audit['sectionAddItem'])
    assert any('mov dword ptr [rbp + 0x12c], eax' in line for line in audit['addQuantity'])
    report={'dll':str(args.dll),'bytes':args.dll.stat().st_size,'sha256':hashlib.sha256(args.dll.read_bytes()).hexdigest(),'native_imports_verified':len(native),'imports':sorted(imports),'exports':sorted(pe.exports()),'unpacked_exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'native_audit':audit}
    output=args.dll.parent/'StackSort-verification.json';output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(f'{len(native)} native imports verified; x64 VC10, startPlugin and MCM_Describe verified')
    print(f'{report["bytes"]} bytes; SHA-256 {report["sha256"]}')
    print(output)
if __name__=='__main__':main()
