"""Check the optional order observers stay between the same startup barriers."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root/'LootScootExecuteHooksEntry.inl').read_text(encoding='utf-8-sig')
startup = source.split('__declspec(dllexport) void startPlugin()', 1)[1]
assert startup.count('StartupHooks::Install(') == 1, 'expected one opt-in batch'
start = startup.index('StartupHooks::Hook orderObservers[]')
apply = startup.index('StartupHooks::Install(')
world = startup.index('const bool worldHookInstalled')
assert startup.index('&PlayerInterface_updateUT_orig') < start < apply < world
assert startup.index('PlayerInterface_newPlayerTask_hook,', start, apply) < startup.index(
    'PlayerInterface_moveWaypoint_hook,', start, apply)
assert startup[start:apply].count('StartupHooks::Hook(') == 2
assert world < startup.index('lse_jobs_runtime::Install(') < startup.index(
    '&ContextMenu_showContextMenu_orig')
print('PASS optional order-hook batch retains mandatory startup barriers')
