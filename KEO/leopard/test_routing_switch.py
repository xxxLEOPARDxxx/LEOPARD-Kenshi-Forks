"""Local maintenance regression: compile actual configs and routing policies.

No game modules, live process operations or personal INI writes. The graph hook
body is extracted unchanged so the retained-hook fallback is executable too.
"""
from pathlib import Path
import os,re,subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/leopard-routing-tests'
OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
text=(ROOT/'src/pathfind/pathfind_hooks.cpp').read_text(encoding='utf-8')
a=text.index('char hook_csCheckFaceConn(');b=text.index('{',a);end=b+1;depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
(OUT/'connectivity.extract.h').write_text(text[a:end],encoding='utf-8')
# The switch is applied on both LoadConfig exits; a startup row must never be
# applied live to an already-patched process by the built-in settings panel.
config=(ROOT/'src/base/config.cpp').read_text(encoding='utf-8')
assert 'movement::ApplyRoutingSwitch(&LogDetail);' in config
assert config.count('FinalizeConfig();')==2
finalize=config[config.index('void FinalizeConfig()'):]
assert finalize.index('movement::ApplyRoutingSwitch(&LogDetail);')<finalize.index('GetSystemInfo(')
manifest=(ROOT/'src/plugin/hook_manifest_rows.inc').read_text(encoding='utf-8')
for row,want in (
    ('HOOK_CS_FIND_PATH','HOOK_WANT_PATHFIND_DIAG'),
    ('HOOK_CS_CHECK_FACE_CONN','HOOK_WANT_PATHFIND_DIAG'),
    ('HOOK_FIND_PATH_FULL','HOOK_WANT_FIND_PATH_FULL'),
    ('HOOK_REQUEST_PATH','HOOK_WANT_PATHFIND_DIAG'),
    ('HOOK_PATH_REQ_SUBMIT','HOOK_WANT_PATHFIND_DIAG'),
    ('HOOK_CS_FIND_PATH_FALLBACK','HOOK_WANT_PATHFIND_DIAG'),
):
    match=re.search(r'HOOK_ROW\('+row+r',([^\n]*)',manifest)
    assert match and want in match.group(1),(row,want)

sources=[ROOT/'leopard/routing_switch_tests.cpp',ROOT/'src/base/ini_text.cpp',
         ROOT/'src/plugin/hook_manifest_policy.cpp',ROOT/'src/pathfind/player_repath_tier_policy.cpp',ROOT/'src/zone/geometry/zone_geometry_cert.cpp']
sources += [ROOT/f'src/{p}/{p}_config.cpp' for p in ('zone','navmesh','pathfind','movement','fixes','inventory','planner','gui')]
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;{OUT}',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
for variant,defines in [('prod',[]),('dev',['/DKEO_DEBUG'])]:
    directory=OUT/variant;directory.mkdir(exist_ok=True)
    cmd=[VC/'bin/amd64/cl.exe','/nologo','/EHsc','/MD','/O2','/W3',*defines,
         '/Fo'+str(directory)+'/', '/Fe'+str(directory/'tests.exe'),*sources]
    p=subprocess.run([str(x) for x in cmd],cwd=ROOT,env=env,capture_output=True,text=True)
    (directory/'build.log').write_text(p.stdout+p.stderr,encoding='utf-8')
    if p.returncode:raise RuntimeError(p.stdout+p.stderr)
    p=subprocess.run([str(directory/'tests.exe')],capture_output=True,text=True)
    (directory/'run.log').write_text(p.stdout+p.stderr,encoding='utf-8')
    print(variant,p.stdout+p.stderr,end='')
    if p.returncode:raise SystemExit(p.returncode)

movement=(ROOT/'src/movement/movement_config.cpp').read_text(encoding='utf-8')
policy=(ROOT/'src/plugin/hook_manifest_policy.cpp').read_text(encoding='utf-8')
for name,original,needle,replacement,reason in (
    ('omit-hook-switch',ROOT/'src/movement/movement_config.cpp','pathfind::g_pathfindCfg.pathfindDiagEnabled = false;','','master OFF must suppress route-changing path hooks'),
    ('omit-graph-switch',ROOT/'src/movement/movement_config.cpp','pathfind::g_pathfindCfg.clusterGraphBypassMode = CGB_ORIGINAL;','','master OFF restores original connectivity verdict'),
    ('omit-repath-switch',ROOT/'src/movement/movement_config.cpp','pathfind::g_pathfindCfg.playerRepathTierEnabled = false;','','master OFF disables repath tier writes'),
    ('ignore-master-on',ROOT/'src/movement/movement_config.cpp','if (movement::g_movementCfg.routingEnabled)','if (false)','master ON preserves defaults and emits no override log'),
    ('force-hook-desire',ROOT/'src/plugin/hook_manifest_policy.cpp','in.pathfindDiag        = pathfind::g_pathfindCfg.pathfindDiagEnabled;','in.pathfindDiag        = true;','disabled routing hooks are never requested at startup'),
):
    source=movement if original.name=='movement_config.cpp' else policy
    assert source.count(needle)==1,(name,source.count(needle))
    directory=OUT/name;directory.mkdir(exist_ok=True);mutated=directory/original.name
    mutated.write_text(source.replace(needle,replacement),encoding='utf-8')
    changed=[mutated if p==original else p for p in sources]
    cmd=[VC/'bin/amd64/cl.exe','/nologo','/EHsc','/MD','/O2','/W3','/Fo'+str(directory)+'/', '/Fe'+str(directory/'tests.exe'),*changed]
    p=subprocess.run([str(x) for x in cmd],cwd=ROOT,env=env,capture_output=True,text=True)
    (directory/'build.log').write_text(p.stdout+p.stderr,encoding='utf-8')
    if p.returncode:raise RuntimeError(p.stdout+p.stderr)
    p=subprocess.run([str(directory/'tests.exe')],capture_output=True,text=True)
    (directory/'run.log').write_text(p.stdout+p.stderr,encoding='utf-8')
    assert p.returncode and 'FAIL '+reason in p.stdout+p.stderr,(name,p.stdout+p.stderr)
    print('Expected RED:',name,'->',reason)
print('5 compiled routing master-switch mutants rejected')
