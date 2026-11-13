from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy()
env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
with tempfile.TemporaryDirectory(prefix='lse-effects-') as temporary:
    directory=Path(temporary)
    exe=directory/'effects.exe'
    subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',
                    '/I'+str(root),str(root/'tests/execution_effects_fixture.cpp'),
                    '/Fe'+str(exe),'/Fo'+str(directory/'effects.obj')],
                   cwd=directory,env=env,check=True)
    subprocess.run([str(exe)],check=True)
