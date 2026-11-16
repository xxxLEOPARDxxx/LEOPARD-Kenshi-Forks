from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/keo-resource-cache-20261114/build/equipment-field-test-20261116');out.mkdir(exist_ok=True)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');e=dict(os.environ,INCLUDE=f'{vc}/include;{sdk}/Include;D:/DEV/Kenshi/deps/KenshiLib_keo/Include;D:/DEV/Kenshi/deps/KenshiLib_keo/Include/ogre;D:/DEV/Kenshi/deps/boost_1_60_0',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
red='--red'in sys.argv
for opt in (['O2']if red else['Od','O2']):
 exe=out/('fields-'+opt+'.exe');args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+opt,'/D_ITERATOR_DEBUG_LEVEL=0',str(root/'leopard/equipment_field_tests.cpp')]
 args+=['/DBASELINE_FORWARD']if red else[str(root/'src/fixes/world/equipment_field_lookup.cpp'),'/DOGRE_STATIC_LIB']
 args+=['/Fo'+str(out)+'/', '/Fe'+str(exe)];p=subprocess.run(args,cwd=out,env=e,capture_output=True,text=True);(out/('build-'+opt+('-red'if red else'')+'.txt')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(exe)]+(['bench']if opt=='O2'and not red else[]),cwd=out,capture_output=True,text=True);print(p.stdout+p.stderr,end='');(out/('run-'+opt+('-red'if red else'')+'.txt')).write_text(p.stdout+p.stderr)
 if red:assert p.returncode and 'FAIL constant slot hit' in p.stdout
 else:assert not p.returncode

if not red:
 source=(root/'src/fixes/world/equipment_field_lookup.cpp').read_text()
 mutations={
 'wrong-hash':source.replace('0x28253c9a37862ULL','0x12345678ULL'),
 'mapped-instead-of-pair':source.replace('return &*found;','return reinterpret_cast<IntPair*>(&found->second);'),
 'ignore-owner-thread':source.replace('enabled&&onOwnerThread&&','enabled&&'),
 'double-native-fallback':source.replace('return original(map,key);','original(map,key);return original(map,key);')}
 for name,changed in mutations.items():
  assert changed!=source
  f=out/('mutant-'+name+'.cpp');f.write_text(changed);exe=out/('mutant-'+name+'.exe')
  args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DOGRE_STATIC_LIB','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(root/'src/fixes/world'),str(root/'leopard/equipment_field_tests.cpp'),str(f),'/Fo'+str(out)+'/', '/Fe'+str(exe)]
  p=subprocess.run(args,cwd=out,env=e,capture_output=True,text=True);(out/('mutant-'+name+'-build.txt')).write_text(p.stdout+p.stderr);assert not p.returncode,p.stdout+p.stderr
  p=subprocess.run([str(exe)],cwd=out,capture_output=True,text=True);(out/('mutant-'+name+'-run.txt')).write_text(p.stdout+p.stderr);assert p.returncode==1 and 'FAIL 'in p.stdout,(name,p.returncode,p.stdout,p.stderr);print('KILLED '+name+': '+p.stdout.strip())
