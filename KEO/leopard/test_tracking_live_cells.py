from pathlib import Path
import os, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/tracking_live_cells_test'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/movement/tracking.cpp').read_text()
header=(ROOT/'src/movement/tracking.h').read_text()
def function(text,marker):
    start=text.index(marker); brace=text.index('{',start); depth=1; pos=brace+1
    while depth:
        if text[pos]=='{':depth+=1
        elif text[pos]=='}':depth-=1
        pos+=1
    return text[start:pos]
structure=function(header,'struct WatchedCharacter')+';\n'
functions='\n'.join(function(source,marker) for marker in
    ['static int EnqueueAheadOfMover(', 'void PollActiveMovers(', 'int CollectMoverRetainZones('])
(OUT/'tracking_struct.inc').write_text(structure)
(OUT/'tracking_functions.inc').write_text(functions)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for mode in ('Od','O2'):
    args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+mode,
          '/I'+str(ROOT/'src'),'/I'+str(OUT),str(ROOT/'leopard/test_tracking_live_cells.cpp'),
          str(ROOT/'src/movement/mover_policy.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(OUT/(mode+'.exe'))]
    build=subprocess.run(args,env=env,cwd=ROOT,capture_output=True)
    (OUT/(mode+'-build.txt')).write_bytes(build.stdout+build.stderr)
    assert build.returncode==0,build.stdout.decode(errors='replace')
    run=subprocess.run([str(OUT/(mode+'.exe'))],capture_output=True)
    (OUT/(mode+'-run.txt')).write_bytes(run.stdout+run.stderr)
    print(run.stdout.decode(errors='replace')+run.stderr.decode(errors='replace'),end='')
    if run.returncode:sys.exit(run.returncode)
