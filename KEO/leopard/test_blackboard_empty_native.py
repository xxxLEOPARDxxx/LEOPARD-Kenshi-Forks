"""Pinned existing file/SDK/phase contracts; no process or engine execution."""
from pathlib import Path
import os,re,hashlib,importlib.util,struct
ROOT=Path(__file__).resolve().parents[1];GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
spec=importlib.util.spec_from_file_location('pe',ROOT/'tools/kenshilib/check_imports.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
exe=m.PE(GAME/'RE_Kenshi/Kenshi_x64.exe');sdk=m.PE(GAME/'KenshiLib.dll');checks=0
def check(x):
 global checks
 assert x;checks+=1
def read(r,n):return exe.data[exe.offset(r):exe.offset(r)+n]
def call(r):
 b=read(r,5);check(b[0]in(0xe8,0xe9));return r+5+struct.unpack('<i',b[1:])[0]
check(hashlib.sha256(exe.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
check(hashlib.sha256(sdk.data).hexdigest()=='5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b')
check(hashlib.sha256(read(0x26a2e0,0x1f8)).hexdigest()=='7e3385e963170a255b0a26a249a292f02849c05ec8e955d54c657ddab3033530')
check(hashlib.sha256(read(0x2e74b0,0x92)).hexdigest()=='30ebf545389b5a29dd4cadc836e7bb882c37f1be578dc7344f849b17c01030d1')
src=(ROOT/'src/fixes/world/blackboard_empty.cpp').read_text()
arrays={n:bytes(int(v.strip(),16)for v in vals.split(','))for n,vals in re.findall(r'static const unsigned char (\w+)\[\]=\{([^}]+)\}',src)}
proofs=re.findall(r'\{(0x[0-9a-f]+),(\w+),sizeof\(\2\)\}',src);check(len(proofs)==7)
for r,n in proofs:check(read(int(r,16),len(arrays[n]))==arrays[n])
eo=sdk.offset(sdk.directories[0][0]);names=sdk.offset(sdk.u32(eo+32));ords=sdk.offset(sdk.u32(eo+36));funcs=sdk.offset(sdk.u32(eo+28));exports={sdk.string(sdk.u32(names+i*4)):sdk.u32(funcs+4*sdk.u16(ords+i*2))for i in range(sdk.u32(eo+24))}
table=struct.unpack('<7630I',(GAME/'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes())
for symbol,index,target in [('?update@Blackboard@@QEAAXM@Z',1143,0x26a2e0),('?updateMT@FactionManager@@QEAAXM@Z',2994,0x2e74b0)]:check(exports[symbol]==0x15b46+6*index and table[index]==target)
# Exact call/thunk identity, not just an address with matching first byte.
check(call(0x788485)==0x234a7 and call(0x234a7)==0x2e74b0)
check(call(0x7eb1b8)==0x16cde and call(0x16cde)==0x26a2e0)
# Allocation-only native empty route: +158 map, size+20; no iterator => skip
# timer/erase loop, ten local handles => one array allocation and one free.
check(read(0x26a325,5)==bytes.fromhex('b90a000000'))
check(call(0x26a349)==0xed5634 and call(0x26a4b1)==0xed562e)
check(read(0x26a391,7)==bytes.fromhex('4981c458010000'))
check(read(0x26a3a8,8)==bytes.fromhex('49837c2420007418'))
check(read(0x26a3d3,9)==bytes.fromhex('4885db0f84b1000000'))
check(read(0x26a48d,4)==bytes.fromhex('85ed7418'))
# Sequential native phase: two completion joins before chars/factions, then
# the background restart. Full bodies/writer review is in adjacent report.
check(call(0x787eca)==0x4f8ae and call(0x787ed3)==0x2e843)
check(call(0x78844d)==0x4f8ae and call(0x788456)==0x2e843)
check(call(0x788467)==0x4b628 and call(0x788476)==0xb4c4)
check(call(0x7884a9)==0x49071)
print('PASS',checks,'pinned native allocation/owner-phase/ABI contracts; files only')
