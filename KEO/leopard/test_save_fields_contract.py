"""Pinned native CPU parser contract; disk files only, no game execution."""
from pathlib import Path
import hashlib,json,re,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,'D:/DEV/Kenshi/tools/_native_python')
import pefile
game=pefile.PE('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
ogre=pefile.PE('D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll')
assert hashlib.sha256(game.__data__).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
assert (game.FILE_HEADER.TimeDateStamp,game.OPTIONAL_HEADER.SizeOfImage)==(0x65d604d7,0x232c000)
source=(ROOT/'src/fixes/world/save_field_key_patch.cpp').read_text(encoding='utf-8')
arrays={n:bytes(int(v,16)for v in data.split(','))for n,data in re.findall(r'static const unsigned char ((?:valueProof|proof)\d+)\[\]=\{([^}]+)\}',source)}
rows=re.findall(r'\{(0x[0-9a-f]+),((?:valueProof|proof)\d+),sizeof\(\2\)\}',source)
expected=[(0x6c8fd0,673),(0x6c9640,672),(0x6c9fd0,676),(0x6ca320,697),(0x6ca690,697),(0x6c9990,713),(0x69cd0,298),(0x4d7c5,5),(0x6c133e,75),(0x6c16c0,171),(0x69b80,259),(0x2808d,5),(0x6bc130,209),(0x4e8c3,5)]
assert len(rows)==len(expected)
for (rva,name),(want,length)in zip(rows,expected):
 assert int(rva,16)==want and len(arrays[name])==length
 assert game.get_data(want,length)==arrays[name],name
# Native assign implementations match including short path, only addresses differ.
a=bytearray(game.get_data(0x69cd0,298));b=bytearray(ogre.get_data(0xdad0,298))
for offset in (0x73,0x80,0x86,0x9f,0xfc):a[offset:offset+4]=b'\0'*4;b[offset:offset+4]=b'\0'*4
assert a==b
def target(rva,opcode):
 data=game.get_data(rva,5);assert data[0]==opcode
 return rva+5+int.from_bytes(data[1:],'little',signed=True)
assert target(0x2808d,0xe9)==0x69b80 and target(0x4e8c3,0xe9)==0x6bc130
assert target(0x6c9bf8,0xe8)==0x2808d and target(0x6c9a6d,0xe8)==0x4e8c3
start=source.index('struct Proof{');admit=source.index('static bool Admit(HMODULE game)',start);brace=source.index('{',admit);depth=0
for end in range(brace,len(source)):
 depth+=(source[end]=='{')-(source[end]=='}')
 if depth==0:break
extracted=source[start:end+1];assert 'void Install'not in extracted
out=ROOT/'build/save_fields_integration';out.mkdir(parents=True,exist_ok=True)
(out/'admission.inc').write_text('// Verbatim production contract after pinned disk validation.\n'+extracted+'\n',encoding='utf-8')
report={'sourceSha256':hashlib.sha256(source.encode()).hexdigest(),'proofRegions':len(rows),'proofBytes':sum(n for _,n in expected),'assignProxyNormalizedBytes':298,'noGameProcessAccess':True}
(out/'contract.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('PASS native parser contract:',report)
