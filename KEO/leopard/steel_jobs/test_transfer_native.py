"""Read-only actual PE contracts for the native transfer adapter; game never executes."""
from pathlib import Path
import hashlib,re,struct,sys,zipfile
sys.dont_write_bytecode=True
sys.path.insert(0,'D:/DEV/Kenshi/ours/SkillLevelUpInfo/tests/_tools')
import pefile
root=Path(__file__).resolve().parents[2]
game=Path('D:/SteamLibrary/steamapps/common/Kenshi')
steam=(game/'RE_Kenshi/Kenshi_x64.exe').read_bytes()
assert hashlib.sha256(steam).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
with zipfile.ZipFile(game/'DropMeFiles_VfFSw.zip') as archive:
 gog=[body for entry in archive.infolist() if entry.filename.lower().endswith('kenshi_gog_x64.exe')
      for body in [archive.read(entry)] if hashlib.sha256(body).hexdigest()=='2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681']
assert len(gog)==1,'The stock GOG profile requires the exact RE_Kenshi-compatible executable'
images=[pefile.PE(data=steam,fast_load=True),pefile.PE(data=gog[0],fast_load=True)]
s=(root/'src/jobs/steel_jobs_transfer.cpp').read_text(encoding='utf-8-sig')
rows=re.findall(r'\{0x([A-Fa-f0-9]+)u,0x([A-Fa-f0-9]+)u,\{([^}]+)\},\{([^}]+)\}\}',s)
assert len(rows)==10
for a,b,x,y in rows:
 for pe,rva,raw in ((images[0],int(a,16),x),(images[1],int(b,16),y)):
  expected=bytes(int(v,16)for v in re.findall(r'0x([A-Fa-f0-9]+)',raw));assert len(expected)==32 and pe.get_data(rva,32)==expected
assert '35fc2b168db724d6371b0c0cf9d8cab06f13b5fa277b1f3a64d3d44a194805a9' in s, 'Published StackSort refused'
assert 'StockSlot(' in s,'Native inventory virtuals are entry thunks, not body addresses'
roles=((3,1),(5,0),(8,6),(10,5),(2,3),(3,2))
vtables=s[s.index('static const StockVtables kStockVtables'):s.index('static bool StockSlot(')]
profiles=[tuple(int(v,16) for v in re.findall(r'0x([a-f0-9]+)u',row,re.I))
          for row in re.findall(r'\{(0x[^}]+)\}',vtables)]
assert len(profiles)==2 and all(len(profile)==8 for profile in profiles)
for flavor,pe in enumerate(images):
 base=pe.OPTIONAL_HEADER.ImageBase
 for j,(slot,row) in enumerate(roles):
  table=profiles[flavor][j>=4]
  thunk=profiles[flavor][2+j]
  assert struct.unpack('<Q',pe.get_data(table+slot*8,8))[0]==base+thunk
  branch=pe.get_data(thunk,5)
  assert branch[0]==0xe9 and thunk+5+struct.unpack_from('<i',branch,1)[0]==int(rows[row][flavor],16)
  assert thunk!=int(rows[row][flavor],16),'The old raw-slot comparison must fail'
print('PASS 20 actual Steam/GOG bodies and 12 vtable thunk/branch contracts')
