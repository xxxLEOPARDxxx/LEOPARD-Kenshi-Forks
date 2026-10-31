"""Offline PE audit using complete manual export tables, independent of pefile."""
import argparse, hashlib, json, pathlib, struct
ROOT=pathlib.Path(__file__).resolve().parents[1]
class PE:
    def __init__(self,path):
        self.path=pathlib.Path(path);self.data=self.path.read_bytes();d=self.data
        self.pe=struct.unpack_from('<I',d,0x3c)[0];assert d[self.pe:self.pe+4]==b'PE\0\0'
        _,n,_,_,_,opt,_=struct.unpack_from('<HHIIIHH',d,self.pe+4)
        self.opt=self.pe+24; assert struct.unpack_from('<H',d,self.opt)[0]==0x20b
        self.base=struct.unpack_from('<Q',d,self.opt+24)[0]
        self.dirs=[struct.unpack_from('<II',d,self.opt+112+i*8) for i in range(16)]
        self.sections=[]
        for i in range(n):
            at=self.opt+opt+i*40
            name=d[at:at+8].rstrip(b'\0').decode();vs,va,rs,raw=struct.unpack_from('<IIII',d,at+8)
            self.sections.append((name,va,max(vs,rs),raw,rs))
    def off(self,rva):
        for name,va,size,raw,rs in self.sections:
            if va<=rva<va+size:
                assert rva-va<rs,(self.path,rva,'virtual bytes unavailable')
                return raw+rva-va
        raise AssertionError((self.path,hex(rva),'RVA outside sections'))
    def code(self,rva,n):
        at=self.off(rva);return self.data[at:at+n]
    def z(self,rva):
        at=self.off(rva);return self.data[at:self.data.index(b'\0',at)].decode('ascii')
    def exports(self):
        rva,size=self.dirs[0]
        if not rva:return {}
        fields=struct.unpack_from('<IIHHIIIIIII',self.data,self.off(rva));n=fields[7]
        pf,pn,po=map(self.off,fields[8:11]);out={}
        for i in range(n):
            name=self.z(struct.unpack_from('<I',self.data,pn+4*i)[0]);idx=struct.unpack_from('<H',self.data,po+2*i)[0]
            out[name]=struct.unpack_from('<I',self.data,pf+4*idx)[0]
        return out
    def imports(self):
        rva,size=self.dirs[1];out={}
        if not rva:return out
        at=self.off(rva)
        while True:
            oft,_,_,name,ft=struct.unpack_from('<5I',self.data,at);at+=20
            if not name:break
            thunk=self.off(oft or ft);names=[]
            while True:
                value=struct.unpack_from('<Q',self.data,thunk)[0];thunk+=8
                if not value:break
                names.append('#'+str(value&65535) if value>>63 else self.z(value+2))
            out[self.z(name).lower()]=names
        return out
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dll',type=pathlib.Path,default=pathlib.Path('D:/DEV/Kenshi/build/TauntExpansion/TauntExpansion.dll'));ap.add_argument('--output',type=pathlib.Path);args=ap.parse_args()
    manifest=json.loads((ROOT/'upstream_sha256.json').read_text())
    for path,digest in manifest.items():assert hashlib.sha256((ROOT/'upstream'/path).read_bytes()).hexdigest()==digest,path
    for name in ('TauntExpansion.mod','RE_Kenshi.json'):
        assert (ROOT/name).read_bytes()==(ROOT/'upstream/ModPackage/TauntExpansion'/name).read_bytes(),name
    game=pathlib.Path('D:/SteamLibrary/steamapps/common/Kenshi')
    lib=PE(game/'KenshiLib.dll');exports=lib.exports();stubs={}
    for name,rva in exports.items():
        b=lib.code(rva,6)
        if b[:2]==b'\xff\x25':stubs[name]=rva+6+struct.unpack_from('<i',b,2)[0]
    base=min(stubs.values());table=(game/'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes()
    names=[n for n in stubs if 'chooseBestAttackTarget' in n and '_NV_' not in n];assert len(names)==1,names
    index=(stubs[names[0]]-base)//8;rva=struct.unpack_from('<I',table,index*4)[0]
    # RE_Kenshi launches this executable image. The game-root executable differs
    # internally and must not be used to prove the runtime Taunt instruction.
    exe=PE(game/'RE_Kenshi/Kenshi_x64.exe');code=exe.code(rva,0x1200)
    pattern=bytes.fromhex('80 B8 2A 01 00 00 00 74 10 F3 0F 58 3D')
    offsets=[i for i in range(len(code)-len(pattern)+1) if code[i:i+len(pattern)]==pattern]
    assert len(offsets)==1,offsets
    site=rva+offsets[0]+9;instruction=exe.code(site,8)
    bonus_rva=site+8+struct.unpack_from('<i',instruction,4)[0]
    bonus=struct.unpack('<f',exe.code(bonus_rva,4))[0];assert bonus==2.0,bonus
    dll=PE(args.dll);imports=dll.imports();native=imports.get('kenshilib.dll',[])
    assert native and not(set(native)-set(exports)),set(native)-set(exports)
    loader=game/'RE_Kenshi.dll';loader_bytes=loader.read_bytes()
    # Verified Plugins.cpp::LoadPlugin calls GetProcAddress with this exact
    # MSVC C++ name. A C export can load and expose MCM yet never start.
    loader_entry='?startPlugin@@YAXXZ'
    assert loader_entry.encode()+b'\0' in loader_bytes,'installed RE_Kenshi loader entry contract changed'
    for required in (loader_entry,'?stopPlugin@@YAXXZ','MCM_Describe'):assert required in dll.exports(),required
    lifecycle=json.loads(pathlib.Path('D:/DEV/Kenshi/build/TauntExpansionTests/lifecycle_proof.json').read_text(encoding='utf-8-sig'))
    assert lifecycle['production_source_sha256']==hashlib.sha256((ROOT/'TauntExpansion.cpp').read_bytes()).hexdigest(),'lifecycle proof source changed; rerun tests'
    assert lifecycle['transaction_header_sha256']==hashlib.sha256((ROOT/'BonusTransaction.h').read_bytes()).hexdigest(),'lifecycle proof lock header changed; rerun tests'
    assert lifecycle['actual_extracted_startup_invoked_via_GetProcAddress'] and lifecycle['actual_extracted_LoadConfig_callback']
    assert lifecycle['early_describe_and_apply_before_startup'] and lifecycle['normal_startup_before_describe_and_apply']
    assert lifecycle['target_patch_and_hook_failure_callbacks_safe']
    report={'dll_sha256':hashlib.sha256(dll.data).hexdigest(),'dll_bytes':len(dll.data),'plugin_exports':sorted(dll.exports()),'loader_entry':loader_entry,'loader_dll_path':str(loader),'loader_dll_sha256':hashlib.sha256(loader_bytes).hexdigest(),'lifecycle_proof':lifecycle,'kenshilib_complete_export_count':len(exports),'kenshilib_import_count':len(native),'target_slot':index,'target_rva':hex(rva),'taunt_add_rva':hex(site),'taunt_add_bytes':instruction.hex(' '),'vanilla_bonus_rva':hex(bonus_rva),'vanilla_bonus':bonus,'scan_matches':len(offsets),'game_exe_path':str(exe.path),'game_exe_sha256':hashlib.sha256(exe.data).hexdigest(),'status':'PASS offline audit of exact RE loader entry, production lifecycle fixture and runtime executable image; live hook/behavior/FPS still require game test'}
    print(json.dumps(report,indent=2))
    if args.output:args.output.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
