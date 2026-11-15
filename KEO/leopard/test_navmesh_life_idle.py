from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/navmesh-life-idle';OUT.mkdir(parents=True,exist_ok=True)
text=(ROOT/'src/fixes/streaming/navmesh_life.cpp').read_text()
a=text.index('void NavMeshLifeOnAdd(');b=text.index('\nvoid InstallNavMeshLife(',a)
body=text[a:b]
fixture=r'''
#include <windows.h>
#include <cstdio>
namespace fixes { struct Cfg { bool navMeshLifeEnabled; } g_fixesCfg; }
static bool detail;static unsigned samples,beats,forwards,checks;static volatile LONG s_adds,s_rets;
static bool DetailedLogEnabled(){return detail;}
static int Sample(const unsigned char*,const void*,const char*,int){++samples;return 0;}
static void MaybeHeartbeat(){++beats;}
static __int64 Original(__int64 c,__int64 i,__int64 g){++forwards;return c+i+g;}
static __int64 (*orig_removeInstance)(__int64,__int64,__int64)=Original;
// BODY
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 fixes::g_fixesCfg.navMeshLifeEnabled=true;detail=false;
 NavMeshLifeOnAdd((void*)1,2);C(samples==0&&beats==0&&s_adds==0);
 C(hook_removeInstance(1,2,3)==6);C(samples==0&&beats==0&&s_rets==0&&forwards==1);
 detail=true;NavMeshLifeOnAdd((void*)1,2);C(samples==1&&beats==1&&s_adds==1);
 C(hook_removeInstance(1,2,3)==6);C(samples==2&&beats==2&&s_rets==1&&forwards==2);
 detail=false;NavMeshLifeOnAdd((void*)1,2);C(hook_removeInstance(1,2,3)==6);C(samples==2&&beats==2&&forwards==3);
 detail=true;fixes::g_fixesCfg.navMeshLifeEnabled=false;NavMeshLifeOnAdd((void*)1,2);C(samples==2&&beats==2);
 std::printf("PASS %u actual lifecycle diagnostic controls\n",checks);return 0;
}
'''.replace('// BODY',body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for opt in ('Od','O2'):
 cpp=OUT/(opt+'.cpp');cpp.write_text(fixture);exe=OUT/(opt+'.exe')
 p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(cpp),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'samples==0' in p.stderr;print('expected RED');break
 assert p.returncode==0
