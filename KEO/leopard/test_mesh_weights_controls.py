"""Compile ordinary CHECK-negative controls against production-header mutations."""
from pathlib import Path
import os, subprocess, json

root = Path(__file__).resolve().parents[1]
source = (root / 'src/fixes/world/mesh_weights_linear.h').read_text()
mutations = {
    'skip_normalization': ('if(distance>1.1920928955078125e-7f)', 'if(false)'),
    'wrong_reciprocal': ('const float factor=1.0f/total', 'const float factor=total'),
    'wrong_sum': ('total+=it->second.weight', 'total-=it->second.weight'),
    'wrong_maximum': ('*out=maximum;', '*out=4;'),
    'accept_zero_group': ('if(!count||!nonzero)', 'if(!count)'),
    'accept_extra_keys': ('if(it!=end)return false;', 'if(false)return false;'),
}
results = {}
for name, (old, new) in mutations.items():
    assert source.count(old) == 1, name
    folder = root / 'build/mesh_weights_controls' / name
    header = folder / 'fixes/world/mesh_weights_linear.h'
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(source.replace(old, new))
    env = dict(os.environ)
    env['CL'] = '/I"' + str(folder) + '"'
    run = subprocess.run(['cmd.exe', '/c', 'leopard\\test_mesh_weights.cmd'], cwd=root,
                         env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = run.stdout.decode(errors='replace')
    (folder / 'result.log').write_text(text)
    # Compiler errors, crashes, timeouts and loader failures do not count as
    # a killed mutant: the actual fixture must report an ordinary failed check.
    good = run.returncode == 1 and 'FAIL line ' in text and 'OGRE Shutdown' in text
    results[name] = {'exitCode': run.returncode, 'ordinaryCheckExit': good}
    print(name, 'PASS' if good else 'FAIL', run.returncode)
    if not good:
        raise SystemExit(text[-3000:])
(root / 'build/mesh_weights_controls/results.json').write_text(json.dumps(results, indent=2))
