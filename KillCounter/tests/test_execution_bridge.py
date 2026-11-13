from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
with tempfile.TemporaryDirectory(prefix='killcounter-bridge-') as tmp:
 out=Path(tmp);exe=out/'bridge.exe'
 source=(root/'KillCounter.cpp').read_text(encoding='utf-8-sig')
 assert 'g_bridgeLifecycleReady=destructionReady && resetReady && updateReady;' in source
 assert source.index('DrainExecutionQueue();UpdatePendingKills();')>source.index('void GameWorld_update_hook(')
 assert 'ForgetExecutionCharacter(character);' in source
 assert 'KenshiLib::GetRealAddress(&GameWorld::_clearAndDestroyGameWorldStuff)' in source
 pending=source[source.index('static void UpdatePendingKills()'):source.index('// Save / load:')]
 (out/'production_pending_kills.inc').write_text(pending,encoding='utf-8')
 subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2','/MD','/I'+str(out),str(root/'tests/execution_bridge_fixture.cpp'),'/Fe'+str(exe),'/Fo'+str(out/'bridge.obj')],cwd=out,env=env,check=True)
 subprocess.run([str(exe)],check=True)
