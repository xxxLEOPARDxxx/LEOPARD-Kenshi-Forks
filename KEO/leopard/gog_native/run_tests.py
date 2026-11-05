import argparse,hashlib,os,pathlib,subprocess
p=argparse.ArgumentParser(description='Compile production profile tests; supplied game EXE is read only, never executed.');p.add_argument('--gog-exe',type=pathlib.Path,required=True);a=p.parse_args()
assert hashlib.sha256(a.gog_exe.read_bytes()).hexdigest()=='2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681','GOG fixture identity refused'
here=pathlib.Path(__file__).resolve().parent;root=here.parents[1];out=root/'build/gog-native-tests';out.mkdir(exist_ok=True)
vc=pathlib.Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=pathlib.Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+str(sdk/'Bin')+';'+env.get('PATH','')
def test(name,sources,args):
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/GL','/I'+str(root/'src'),'/I'+str(root/'src/game')]+[str(x)for x in sources]+['/Fe'+name+'.exe','/link','/LTCG']
 subprocess.run(cmd,cwd=out,env=env,check=True);subprocess.run([str(out/(name+'.exe'))]+args,cwd=out,check=True)
files=['game/gog_profile.cpp','pathfind/path_priority_publication.cpp','fixes/world/town_claim_policy.cpp','fixes/stitch/stitch_byte_guard_policy.cpp','fixes/physx/physx_query_guard_policy.cpp','inventory/dialogue_thunk_policy.cpp']
test('profile',[root/'src'/f for f in files]+[here/'profile_fixture.cpp'],[str(a.gog_exe.resolve())])
test('refusal',[root/'src/game/native_profile.cpp',root/'src/game/gog_profile.cpp',here/'runtime_refusal.cpp'],[])

# Actual production Resolve/Add/global loop and startup comparison, not profile self-enumeration.
import sys
subprocess.run([sys.executable,str(here/"test_resolve_registry.py")],cwd=root,check=True)
