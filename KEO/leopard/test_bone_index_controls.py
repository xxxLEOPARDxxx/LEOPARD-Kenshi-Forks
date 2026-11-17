"""Compile semantic controls; an ordinary native CHECK exit is required."""
from pathlib import Path
import os, subprocess, json

root = Path(__file__).resolve().parents[1]
text = (root / 'src/fixes/world/bone_index.cpp').read_text()
mutations = {
    'truncate_bone': ('Read<unsigned short>(node,0x24)', 'Read<unsigned char>(node,0x24)'),
    'wrong_rank': ('forward[bone]=(unsigned short)rank;', 'forward[bone]=(unsigned short)(rank+1);'),
    'erase_old_holes': ('unsigned rank=0;', 'unsigned rank=0;for(unsigned k=0;k<=maximum;++k)forward[k]=0;'),
    'skip_first_zero': ('size_t i=begin;i<target', 'size_t i=begin+1;i<target'),
    'wrong_growth_order': ('Resize(blendToBone,unique,grow);Resize(boneToBlend,size_t(maximum)+1,grow);', 'Resize(boneToBlend,size_t(maximum)+1,grow);Resize(blendToBone,unique,grow);'),
    'accept_alias': ('||boneToBlend==blendToBone', '||false'),
}
results = {}
for name, (old, new) in mutations.items():
    assert text.count(old) == 1, name
    folder = root / 'build/bone_index_controls' / name
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'bone_index_mutant.cpp'
    source.write_text(text.replace(old, new))
    env = dict(os.environ, BONE_INDEX_SOURCE=str(source))
    run = subprocess.run(['cmd.exe', '/c', 'leopard\\test_bone_index.cmd'], cwd=root,
                         env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = run.stdout.decode(errors='replace')
    (folder / 'result.log').write_text(output)
    detected = run.returncode == 1 and 'FAIL line ' in output and 'OGRE Shutdown' in output
    results[name] = {'exitCode': run.returncode, 'ordinaryCheckExit': detected}
    print(name, 'PASS' if detected else 'FAIL', run.returncode)
    if not detected:
        raise SystemExit(output[-3500:])
(root / 'build/bone_index_controls/results.json').write_text(json.dumps(results, indent=2))
