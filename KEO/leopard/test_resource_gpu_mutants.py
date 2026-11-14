"""Run targeted negative controls against copied production sources, never edits them."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build' / 'resource_gpu_test' / 'mutants'
out.mkdir(parents=True, exist_ok=True)
store = (root / 'src/resources/resource_gpu_store.cpp').read_text(encoding='utf-8')
dds = (root / 'src/resources/resource_dds.cpp').read_text(encoding='utf-8')
cases = [
    ('rotated_layers', 'store', '&f[l]->data[p.offset]', '&f[(l+1)%6]->data[p.offset]', 'independent expected BC byte'),
    ('gate_omitted', 'store', 'if(!Read(workAllowed))continue;', 'if(false)continue;', 'closed work gate preserves queued job'),
    ('cap_omitted', 'store', 'return extra<=cap&&used<=cap-extra;', 'return true;', 'over cap refused'),
    ('adoption_charge_leaked', 'store', 'if(r->charged_){', 'if(false){', 'adoption detaches GPU budget'),
    ('wrong_native_mips', 'dds', 'result.levels=count-5;', 'result.levels=count-4;', 'native mip truncation'),
    ('victim_release_delayed', 'store', 'drop.reset();', 'static std::vector<ReadyPtr> held;held.push_back(drop);drop.reset();', 'reserve one additional accounting unit'),
    ('long_key_refused', 'store', 'j.key.size()>512', 'j.key.size()>256', 'realistic long native-name hash key accepted'),
    ('ram_pressure_ignored', 'store', 'return resource_cache::StandbyAllowed();', 'return true;', 'RAM pressure releases standby source pins'),
]
for name, kind, before, after, expected in cases:
    source = store if kind == 'store' else dds
    assert source.count(before) == 1, (name, source.count(before))
    mutant = out / (name + '.cpp')
    mutant.write_text(source.replace(before, after), encoding='utf-8')
    exe = out / (name + '.exe')
    if kind == 'store':
        fixture = out / (name + '-fixture.cpp')
        fixture.write_text((root / 'leopard/test_resource_gpu.cpp').read_text(encoding='utf-8').replace(
            '#include "../src/resources/resource_gpu_store.cpp"', '#include "' + mutant.as_posix() + '"'), encoding='utf-8')
        sources = ['/DRESOURCE_GPU_INCLUDE_STORE', str(fixture), 'src/resources/resource_dds.cpp']
    else:
        sources = ['leopard/test_resource_gpu_dds.cpp', str(mutant)]
    cmd = ['cl', '/nologo', '/EHsc', '/MD', '/O2', '/I', 'src'] + sources + [f'/Fo{out}\\', f'/Fe{exe}']
    if kind == 'store':
        cmd += ['/link', 'd3d11.lib', 'dxgi.lib']
    result = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    (out / (name + '-build.txt')).write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    result = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, timeout=20)
    (out / (name + '-test.txt')).write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode != 0 and expected in result.stdout, (name, result.returncode, result.stdout)
    print(name + ': expected RED verified')
print('All eight GPU/DDS production negative controls PASS')
