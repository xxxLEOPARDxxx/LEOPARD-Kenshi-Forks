"""Join explicit body-review coverage; inventory alone is not a code review."""
from pathlib import Path
import hashlib,json,re,sys
ROOT=Path(__file__).resolve().parents[1]
target=ROOT/'leopard/keo-full-audit-coverage-20261115.json'
reports=sorted((ROOT/'leopard').glob('full-audit-*-20261115.md'))
covered={}
for report in reports:
 for path in set(re.findall(r'src/[A-Za-z0-9_/.-]+\.(?:cpp|h|inc)\b',report.read_text(encoding='utf-8'))):
  covered.setdefault(path,[]).append(report.name)
# Ordered focused follow-ups override only their actually re-reviewed paths.
# Earlier snapshots remain historical evidence, not hashes of later bodies.
current={}
for name in ('hook-batch-cache-review-20261115.json','base-squad-empty-review-20261115.json','retention-victim-index-review-20261116.json','skeleton-reserve-review-20261116.json','model-hud-review-20261116.json','geometry-description-transfer-review-20261116.json','morph-pose-review-20261116.json'):
 followup=ROOT/'leopard'/name
 if followup.exists():
  for row in json.loads(followup.read_text()):
   path=row['path'];current[path]=row;covered.setdefault(path,[]).append(row['report'])
for path,row in current.items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==row['sha256'],path
root_report=ROOT/'leopard/full-audit-fixes-world-streaming-20261115.json'
for row in json.loads(root_report.read_text()):
 path=row['path']
 if path not in current:assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==row['sha256'],path
 covered.setdefault(path,[]).append('full-audit-fixes-world-streaming-20261115.md')
actual={p.relative_to(ROOT).as_posix():p for p in (ROOT/'src').rglob('*') if p.suffix in ('.h','.cpp','.inc')}
expected=743 if (ROOT/'leopard/morph-pose-review-20261116.json').exists() else 741 if (ROOT/'leopard/model-hud-review-20261116.json').exists() else 739 if (ROOT/'leopard/skeleton-reserve-review-20261116.json').exists() else 737 if (ROOT/'leopard/retention-victim-index-review-20261116.json').exists() else (736 if (ROOT/'leopard/base-squad-empty-review-20261115.json').exists() else (734 if current else 733))
assert len(actual)==expected,len(actual)
assert not set(actual)-set(covered),sorted(set(actual)-set(covered))
rows=[dict(path=k,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),lines=len(p.read_bytes().splitlines()),reports=sorted(set(covered[k])),kind='generated table structurally validated' if k=='src/game/gog_profile_data.inc' else 'body/declarations read') for k,p in sorted(actual.items())]
result=dict(files=len(rows),lines=sum(x['lines'] for x in rows),coverage=rows)
if '--write' in sys.argv:target.write_text(json.dumps(result,indent=2),encoding='utf-8')
else:assert json.loads(target.read_text())==result,'source/coverage changed after snapshot'
print('PASS explicit review coverage:',result['files'],'source files,',result['lines'],'physical lines')
