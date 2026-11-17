"""Compile semantic mutations; only ordinary CHECK exits count as detection."""
from pathlib import Path
import os, subprocess, json

root = Path(__file__).resolve().parents[1]
text = (root / 'src/fixes/world/bone_import.cpp').read_text()
mutations = {
    'signed_vertex_key': ('pair.key=Read<unsigned>(assignment,0)', 'pair.key=Read<int>(assignment,0)'),
    'incomplete_record': ('std::memcpy(pair.record,assignment,12)', 'std::memcpy(pair.record,assignment,8)'),
    'skip_dirty': ('*((unsigned char*)self+0xf8)=1', '*((unsigned char*)self+0xf8)=0'),
    'lower_instead_of_upper': ('left=key<Read<uintptr_t>(cursor,0x18)', 'left=key<=Read<uintptr_t>(cursor,0x18)'),
    'accept_shared': ('||Read<unsigned char>(self,0)', '||false'),
    'dirty_before_link': ('void*out=NULL;link(map,&out,left,parent,node);', 'void*out=NULL;*((unsigned char*)self+0xf8)=1;link(map,&out,left,parent,node);'),
}
results = {}
for name, (old, new) in mutations.items():
    assert text.count(old) == 1, name
    folder = root / 'build/bone_import_controls' / name
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'bone_import_mutant.cpp'
    source.write_text(text.replace(old, new))
    env = dict(os.environ)
    env['BONE_IMPORT_SOURCE'] = str(source)
    run = subprocess.run(['cmd.exe', '/c', 'leopard\\test_bone_import.cmd'], cwd=root,
                         env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = run.stdout.decode(errors='replace')
    (folder / 'result.log').write_text(output)
    detected = run.returncode == 1 and 'FAIL line ' in output and 'OGRE Shutdown' in output
    results[name] = {'exitCode': run.returncode, 'ordinaryCheckExit': detected}
    print(name, 'PASS' if detected else 'FAIL', run.returncode)
    if not detected:
        raise SystemExit(output[-3500:])
(root / 'build/bone_import_controls/results.json').write_text(json.dumps(results, indent=2))
