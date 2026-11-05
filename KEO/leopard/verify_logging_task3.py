"""Verify the frozen Task3 handoff and native candidate contents, without game calls."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'leopard/logging-task3-20261103.json').read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for name,want in manifest['frozenSource'].items():
 p=ROOT/name;assert p.is_file() and sha(p)==want,name
for variant,candidate in manifest['candidates'].items():
 folder=Path(candidate['folder']);actual={p.relative_to(folder).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p)} for p in folder.rglob('*') if p.is_file()}
 assert actual==candidate['files'],variant
 assert not any(p.endswith('.po') for p in actual),variant
 assert {p for p in actual if p.startswith('locale/')}=={f'locale/{x}/LC_MESSAGES/keo.mo' for x in ('en_GB','ru_RU','zh_CN')},variant
 assert (folder/'README.md').read_bytes()==(ROOT/'README.md').read_bytes()
 assert (folder/'KEO.ini').read_bytes()==(ROOT/'KEO.ini').read_bytes()
 assert '# Debug=false' in (folder/'KEO.ini').read_text(encoding='utf-8')
print(f"Frozen Task3 source {len(manifest['frozenSource'])} files and DEV/PROD candidate composition PASS")
