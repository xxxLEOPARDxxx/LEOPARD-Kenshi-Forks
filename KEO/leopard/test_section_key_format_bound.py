from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/section-key-format-bound';OUT.mkdir(parents=True,exist_ok=True)
text=(ROOT/'src/fixes/streaming/section_key_ring.cpp').read_text();a=text.index('size_t SectionKeyRingFormat(');b=text.index('\nvoid SectionKeyRingHeartbeatLine(',a);body=text[a:b]
fixture=r'''
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "base/fixed_log_buf.h"
#include "fixes/streaming/section_key_ring.h"
typedef FlbExternal SkBuf;
static const int kRingSize=8,kSiteCount=2;
static SectionKeyEntry s_ring[2][8];static volatile LONG s_next[2];static unsigned reads,formatted,checks;
static LONG Read(volatile LONG*p){++reads;return *p;}
static void FormatEntry(SkBuf*o,const SectionKeyEntry&e){++formatted;FlbStr(o," row=");FlbDec(o,e.seq);}
// BODY
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 char out[512];s_next[0]=s_next[1]=100000;reads=formatted=0;
 size_t n=SectionKeyRingFormat(out,sizeof(out),4);
 C(reads<=20&&formatted==0);C(n<sizeof(out)&&out[n]==0);
 s_ring[0][(100000-1)%8].seq=100000;reads=formatted=0;
 n=SectionKeyRingFormat(out,sizeof(out),4);C(reads<=20&&formatted==1);C(strstr(out," row=100000")!=NULL);
 s_ring[0][(99999-1)%8].seq=99999;reads=formatted=0;
 n=SectionKeyRingFormat(out,sizeof(out),1);C(formatted==1&&strstr(out," row=99999")==NULL);
 reads=formatted=0;SectionKeyRingFormat(out,sizeof(out),0);C(reads<=4&&formatted==0);
 s_next[0]=s_next[1]=0;reads=0;n=SectionKeyRingFormat(out,sizeof(out),4);C(strstr(out,"recorded no call")!=NULL&&reads<=4);
 C(SectionKeyRingFormat(NULL,0,4)==0);C(SectionKeyRingFormat(out,1,4)==0&&out[0]==0);
 std::printf("PASS %u actual section crash formatter controls\n",checks);return 0;
}
'''.replace('// BODY',body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
for opt in ('Od','O2'):
 cpp=OUT/(opt+'.cpp');cpp.write_text(fixture);exe=OUT/(opt+'.exe')
 p=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(cpp),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True);assert p.returncode==0,(p.stdout+p.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True);print((p.stdout+p.stderr).decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'reads<=20' in p.stderr;print('expected RED');break
 assert p.returncode==0
