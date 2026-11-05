"""Read-only actual PE contracts for the native transfer adapter; game never executes."""
from pathlib import Path
import hashlib,re,sys
sys.dont_write_bytecode=True
sys.path.insert(0,'D:/DEV/Kenshi/ours/SkillLevelUpInfo/tests/_tools')
import pefile
root=Path(__file__).resolve().parents[2]
files=[(Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe'),'504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'),(Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-keo-gog-native/re_kenshi/kenshi_GOG_x64.exe'),'2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681')]
images=[]
for p,sha in files:
 assert hashlib.sha256(p.read_bytes()).hexdigest()==sha
 images.append(pefile.PE(str(p),fast_load=True))
s=(root/'src/jobs/steel_jobs_transfer.cpp').read_text(encoding='utf-8-sig')
rows=re.findall(r'\{0x([A-Fa-f0-9]+)u,0x([A-Fa-f0-9]+)u,\{([^}]+)\},\{([^}]+)\}\}',s)
assert len(rows)==10
for a,b,x,y in rows:
 for pe,rva,raw in ((images[0],int(a,16),x),(images[1],int(b,16),y)):
  expected=bytes(int(v,16)for v in re.findall(r'0x([A-Fa-f0-9]+)',raw));assert len(expected)==32 and pe.get_data(rva,32)==expected
print('PASS 20 actual pinned Steam/GOG native transfer windows')
