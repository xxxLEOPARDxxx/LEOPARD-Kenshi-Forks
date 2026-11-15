from pathlib import Path
import os,re,subprocess,sys,importlib.util,hashlib,struct,json
s=Path(__file__).resolve().parents[1];out=s/'build/purge-cycle';out.mkdir(exist_ok=True)
spec=importlib.util.spec_from_file_location('pe',s/'tools/kenshilib/check_imports.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
pe=p.PE(s.parents[1]/'RE_Kenshi/Kenshi_x64.exe')
assert hashlib.sha256(pe.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
calls=[(0xA0EB67,4),(0xA0EB79,4),(0xA0EBB4,6),(0xA0EC02,6),(0xA0EC50,6),(0xA0EC9E,6)]
imports={};directory=pe.offset(pe.directories[1][0])
while True:
    original,stamp,chain,name,first=struct.unpack_from('<5I',pe.data,directory)
    if not any([original,stamp,chain,name,first]):break
    off=pe.offset(original or first);index=0
    while True:
        thunk=struct.unpack_from('<Q',pe.data,off+8*index)[0]
        if not thunk:break
        symbol='#'+str(thunk&0xffff) if thunk&(1<<63) else pe.string(thunk+2)
        imports[first+8*index]=(pe.string(name),symbol);index+=1
    directory+=20
def imported_call(at):
    code=pe.data[pe.offset(at):pe.offset(at)+6];assert code[:2]==b'\xff\x15'
    return imports[at+6+struct.unpack_from('<i',code,2)[0]]
assert 'TextureManager' in imported_call(0xA0EB59)[1]
assert 'MeshManager' in imported_call(0xA0EB6B)[1]
for at,n in calls:
    raw=pe.data[pe.offset(at):pe.offset(at)+n]
    if n==4:assert raw==bytes.fromhex('41ff5068')
    else:assert 'unloadUnreferencedResourcesInGroup' in imported_call(at)[1]
group_literals=[0xA0EB93,0xA0EBE1,0xA0EC2F,0xA0EC7D]
groups=[]
for at in group_literals:
    raw=pe.data[pe.offset(at):pe.offset(at)+7];assert raw[:3]==bytes.fromhex('488d15')
    groups.append(pe.string(at+7+struct.unpack_from('<i',raw,3)[0]))
assert groups==['General','foliage','Landscape','Overlaymaps']
(out/'native-callers.json').write_text(json.dumps(dict(exeSha256=hashlib.sha256(pe.data).hexdigest(),
    managerCallers=[hex(x[0]) for x in calls[:2]],groupCallers=[hex(x[0]) for x in calls[2:]],groups=groups),indent=2))
fills=[]
for at,n in calls:
    # Include surrounding32 bytes ending at return, independently of fixture.
    start=at+n-32;b=pe.data[pe.offset(start):pe.offset(start)+32]
    fills.append('{const unsigned char b[]={'+','.join(hex(v) for v in b)+'};memcpy(&image['+hex(start)+'],b,sizeof(b));}')
fixture=(s/'leopard/test_ogre_purge_wrappers.cpp').read_text()
production=(s/'src/render/ogre_purge_skip.cpp').read_text()
production=re.sub(r'^(?:#include|#pragma intrinsic)[^\n]*\n','',production,flags=re.M)
cpp=out/'actual.cpp';cpp.write_text(fixture.replace('// PRODUCTION_BODY',production).replace('// SITE_BYTES: runner fills known caller windows from the pinned disk EXE.','\n'.join(fills)))
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
for opt in ['Od','O2']:
    args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+opt,'/I'+str(s/'src'),str(cpp),str(s/'src/render/ogre_purge_policy.cpp'),'/Fo'+str(out)+'/','/Fe'+str(out/('actual-'+opt+'.exe'))]
    b=subprocess.run(args,cwd=s,env=env,capture_output=True);(out/('build-'+opt+'.txt')).write_bytes(b.stdout+b.stderr);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
    r=subprocess.run([str(out/('actual-'+opt+'.exe'))],cwd=s,capture_output=True);(out/('run-'+opt+'.txt')).write_bytes(r.stdout+r.stderr);print(r.stdout.decode(),end='');assert r.returncode==0,'behavior failed, see build/purge-cycle/run-'+opt+'.txt'
    # Existing pure arithmetic controls remain unchanged and still pass.
    args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+opt,'/I'+str(s/'src'),'/I'+str(s/'tools/tests'),str(s/'tools/tests/ogre_purge_units.cpp'),str(s/'src/render/ogre_purge_policy.cpp'),'/Fo'+str(out)+'/','/Fe'+str(out/('policy-'+opt+'.exe'))]
    b=subprocess.run(args,cwd=s,env=env,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
    r=subprocess.run([str(out/('policy-'+opt+'.exe'))],capture_output=True);(out/('policy-'+opt+'.txt')).write_bytes(r.stdout+r.stderr);assert r.returncode==0;r.stdout and print(r.stdout.decode(),end='')
for name,before,after in [
    ('shared-clock','const int channel=PurgeCallerIndex(caller,group);','int channel=PurgeCallerIndex(caller,group);if(channel>=0)channel=0;'),
    ('skip-unknown-caller','if(channel<0)return true;','if(channel<0)return false;'),
    ('ignore-partial-hook','if(!IsMainThread()||!s_installed)return true;','if(!IsMainThread())return true;'),
    ('ignore-hook-kind','&&s_callers[i].group==group','')]:
    assert production.count(before)==1
    mutant=out/(name+'.cpp');mutant.write_text(fixture.replace('// PRODUCTION_BODY',production.replace(before,after)).replace('// SITE_BYTES: runner fills known caller windows from the pinned disk EXE.','\n'.join(fills)))
    args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/I'+str(s/'src'),str(mutant),str(s/'src/render/ogre_purge_policy.cpp'),'/Fo'+str(out)+'/','/Fe'+str(out/(name+'.exe'))]
    b=subprocess.run(args,cwd=s,env=env,capture_output=True);assert b.returncode==0,(b.stdout+b.stderr).decode(errors='replace')
    r=subprocess.run([str(out/(name+'.exe'))],capture_output=True);(out/(name+'-red.txt')).write_bytes(r.stdout+r.stderr);assert r.returncode!=0,name+' survived';print(name+': expected RED')
print('PASS native six-caller contracts, Od/O2 wrappers+policy, four compiled negative controls')
