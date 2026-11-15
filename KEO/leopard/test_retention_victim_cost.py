"""Compile/run the actual production Oldest bodies with counted metadata reads."""
from pathlib import Path
import os, subprocess, json, sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/retention-victim-cost';OUT.mkdir(parents=True,exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=str(vc/'bin/amd64')+';'+str(sdk/'Bin')+';'+env['PATH']
def body(raw, mark):
    a=raw.index(mark);p=raw.index('{',a)+1;depth=1
    while depth:
        depth+=(raw[p]=='{')-(raw[p]=='}');p+=1
    return raw[a:p]
parts=[]
for name,filename,mapname,member,capacity in [('geometry','resource_geometry.cpp','Models','models',2048),('texture','resource_texture_retention.cpp','Textures','textures',1024)]:
    raw=(ROOT/'src/resources'/filename).read_text()
    indexed='state->victims.Oldest()' in raw
    parts.append('namespace '+name+' {\n'+f'''struct Entry {{Touch touch;unsigned victimSlot;Entry():victimSlot(~0u){{}}}};
typedef std::map<const int*,Entry> {mapname};
struct State {{{mapname} {member};'''+(f'resource_retention::VictimIndex<{mapname},{capacity}> victims;State():victims({member}){{}}' if indexed else '')+'''};
static State storage;static State*state=&storage;
'''+f'static void Release({mapname}::iterator i){{'+('state->victims.Remove(i);' if indexed else '')+f'state->{member}.erase(i);}}\n'+body(raw,'static void Oldest()')+f'''
int Test(){{
    static int keys[{capacity}];
    for(unsigned n=0;n<{capacity};++n){{Entry e;e.touch.value=n;{mapname}::iterator i=state->{member}.insert(std::make_pair(&keys[n],e)).first;'''+('state->victims.Insert(i);' if indexed else '')+f'''}}
    comparisons=0;for(unsigned n=0;n<32;++n)Oldest();
    std::cout<<"{name} comparisons="<<comparisons<<" remaining="<<state->{member}.size()<<"\\n";
    return comparisons<=2048&&state->{member}.size()=={capacity}-32?0:1;
}}
}}
''')
source='''#include <map>
#include <iostream>
static unsigned comparisons=0;
struct Touch {unsigned __int64 value;bool operator<(const Touch&o)const{++comparisons;return value<o.value;}};
'''+('#include "resources/retention_victim_index.h"\n' if any('VictimIndex' in p for p in parts) else '')+'\n'.join(parts)+'\nint main(){int a=geometry::Test(),b=texture::Test();return a||b;}\n'
fixture=OUT/'cost.cpp';fixture.write_text(source)
cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),str(fixture),'/Fo'+str(OUT)+'\\','/Fe'+str(OUT/'cost.exe')]
b=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True);(OUT/'build.log').write_bytes(b.stdout+b.stderr)
if b.returncode:print((b.stdout+b.stderr).decode(errors='replace'));sys.exit(2)
r=subprocess.run([str(OUT/'cost.exe')],cwd=ROOT,env=env,capture_output=True)
print(r.stdout.decode(errors='replace'),end='');(OUT/'run.log').write_bytes(r.stdout+r.stderr)
if '--baseline' in sys.argv:
    (ROOT/'leopard/retention-victim-cost-baseline.json').write_text(json.dumps({'exit':r.returncode,'output':r.stdout.decode()},indent=2)+'\n')
sys.exit(r.returncode)
