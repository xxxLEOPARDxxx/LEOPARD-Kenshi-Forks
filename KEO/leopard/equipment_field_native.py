"""Pinned offline equipment field callsites and ABI evidence; no process/game APIs."""
from pathlib import Path
import hashlib,importlib.util,json,struct,sys
root=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('pe_reader',root/'tools/kenshilib/check_imports.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
exe=Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe');pe=m.PE(exe);assert hashlib.sha256(pe.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
def data(a,n):return pe.data[pe.offset(a):pe.offset(a)+n]
def target(a):assert data(a,1)==b'\xe9';return a+5+struct.unpack('<i',data(a+1,4))[0]
def call(a,want):assert data(a,1)==b'\xe8';assert a+5+struct.unpack('<i',data(a+1,4))[0]==want
for thunk,want in [(0x480fe,0x6cef0),(0x4f886,0x6c780),(0x23ebb,0xb0a70),(0x13f3e,0x6a6b0),(0x505e7,0x6a290)]:assert target(thunk)==want
for a in [0x535bac,0x537c96,0x537ede]:call(a,0x480fe);assert data(a+5,3).endswith(b'\x28')
# Literal supplied to the stock std::string constructor before each call.
for a in [0x535b7d,0x537c69,0x537ebb]:assert data(a,3)==b'\x48\x8d\x15';literal=a+7+struct.unpack('<i',data(a+3,4))[0];assert data(literal,5)==b'slot\0'
assert data(0x6cf6c,4)==bytes.fromhex('498d4310')
assert data(0x6a6da,6)==bytes.fromhex('41bbb979379e') # 0x9e3779b9 character combine constant
assert data(0x6a2ac,4)==bytes.fromhex('4c8b7918') # bucket_count +18
assert data(0x6a2c0,5)==bytes.fromhex('4883792000') # size +20
assert data(0x6a2ed,4)==bytes.fromhex('488b4138') # buckets +38
assert data(0x6a320,4)==bytes.fromhex('488b4b08') # cached node hash +8
report={'format':'KEO_EQUIPMENT_FIELD_OFFLINE_CONTRACT','version':1,'gameSha256':hashlib.sha256(pe.data).hexdigest(),'processRead':False,'gameRun':False,'pairAddressReturn':True,'mappedValueOffset':40,'nodeValueOffset':16,'mapBytes':64,'mapBucketCountOffset':24,'mapSizeOffset':32,'mapBucketsOffset':56,'slotRawHash':'0x28253c9a37862','slotMixedHash':'0xaf7187ef7256aa87','slotCallsites':[{'rva':hex(a),'bytes':data(a,5).hex(),'aligned8Offset':a%8}for a in [0x535bac,0x537c96,0x537ede]],'thunks':{hex(a):hex(target(a))for a in [0x480fe,0x4f886,0x23ebb,0x13f3e,0x505e7]},'windows':[{'rva':hex(a),'bytes':data(a,n).hex()}for a,n in [(0x6cef0,32),(0x6cf22,82),(0x6a6b0,192),(0x6a290,277)]],'sdkHeaders':{str(p.relative_to(Path('D:/DEV/Kenshi/deps'))).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path('D:/DEV/Kenshi/deps/KenshiLib_keo/Include/kenshi/util/OgreUnordered.h'),Path('D:/DEV/Kenshi/deps/boost_1_60_0/boost/unordered/detail/table.hpp'),Path('D:/DEV/Kenshi/deps/boost_1_60_0/boost/unordered/detail/buckets.hpp'),Path('D:/DEV/Kenshi/deps/boost_1_60_0/boost/functional/hash/hash.hpp')]}}
out=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/keo-resource-cache-20261114/build/equipment-field-test-20261116/native-contract.json');out.write_text(json.dumps(report,indent=2)+'\n');print('PASS pinned offline equipment callsite/hash/layout checks');print(out)
