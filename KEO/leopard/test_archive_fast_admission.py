from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/archive_fast_admission_test';OUT.mkdir(parents=True,exist_ok=True)
text=(ROOT/'src/resources/resource_cache.cpp').read_text()
def function(marker):
 start=text.index(marker);brace=text.index('{',start);pos=brace+1;depth=1
 while depth:
  if text[pos]=='{':depth+=1
  elif text[pos]=='}':depth-=1
  pos+=1
 return text[start:pos]
(OUT/'archive_open_production.inc').write_text(function('std::string Path(')+'\n'+function('Ogre::DataStreamPtr* Open('))
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');dep=Path('D:/DEV/Kenshi/deps/KenshiLib_keo')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+str(ROOT.parents[1])+';'+env['PATH']
for mode in ('Od','O2'):
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/'+mode,'/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(ROOT/'src'),'/I'+str(OUT),'/I'+str(dep/'Include'),'/I'+str(Path('D:/DEV/Kenshi/deps/boost_1_60_0')),str(ROOT/'leopard/test_archive_fast_admission.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(OUT/(mode+'.exe')),'/link','/LIBPATH:'+str(dep/'Libraries/ogre'),'OgreMain_x64.lib']
 build=subprocess.run(args,env=env,cwd=ROOT,capture_output=True);(OUT/(mode+'-build.txt')).write_bytes(build.stdout+build.stderr)
 assert build.returncode==0,build.stdout.decode(errors='replace')
 run=subprocess.run([str(OUT/(mode+'.exe'))],env=env,cwd=ROOT,capture_output=True);(OUT/(mode+'-run.txt')).write_bytes(run.stdout+run.stderr)
 print(run.stdout.decode(errors='replace')+run.stderr.decode(errors='replace'),end='')
 if run.returncode:sys.exit(run.returncode)
