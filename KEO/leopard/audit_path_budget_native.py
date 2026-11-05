"""Offline504b request identity/owner/budget proof; never loads/calls game code."""
from pathlib import Path
import sys,hashlib,struct,importlib.util
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('keo_pe',ROOT/'tools/kenshilib/check_imports.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
path=Path(sys.argv[1]) if len(sys.argv)>1 else Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
p=m.PE(path);assert hashlib.sha256(p.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e','unsupported offline engine identity'
def at(rva,n):return p.data[p.offset(rva):p.offset(rva)+n]
windows={0x3aede3:'4c8dbb800000004c897c2430',0x3aee9f:'4c897c244088442438',0x3aeed6:'e8e7cec8ff',0x3aee7a:'488d4b08e88e35c8ff48',0x3aee88:'488bc8e80701c7ff84c07404b001eb0232c084c00f94c0',0x3aada8:'e823a99300',0xce56f8:'488bf2',0xce5a30:'488b869c000000',0xce5a4f:'b80000020048897df8440f4ec0b8',0xce5a5c:'b80002090085db897d00c7',0x790b30:'4883ec28488b01ff505833c9483988500200000f95c04883c428c3'}
for rva,want in windows.items():assert at(rva,len(bytes.fromhex(want)))==bytes.fromhex(want),hex(rva)
def target(rva,opcode):
 assert at(rva,1)==opcode
 return rva+5+struct.unpack('<i',at(rva+1,4))[0]
assert target(0x3aeed6,b'\xe8')==0x3bdc2 and target(0x3bdc2,b'\xe9')==0x3aabf0
assert target(0x3aee7e,b'\xe8')==0x32411 and target(0x32411,b'\xe9')==0x7974f0
assert target(0x3aee8b,b'\xe8')==0x1ef97 and target(0x1ef97,b'\xe9')==0x790b30
assert target(0x3aada8,b'\xe8')==0xce56d0
print('504b PASS: 11 exact windows/7relativeedges; same request+128 nintharg, NPC inverse of resolved-player, character-onlyreturn3AADAD, original budget pair9C/A0')
