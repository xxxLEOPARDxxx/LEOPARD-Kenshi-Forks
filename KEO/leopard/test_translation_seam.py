"""Native KeoTr body extracted at the catalog boundary; host header is game-free.
This deliberately does not claim native LocaleManager/catalog integration tests.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task1/translation';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/base/keo_tr.cpp').read_text(encoding='utf-8')
a=source.index('const char* KeoTr(');b=source.index('{',a);e=b+1;depth=1
while depth:depth+=(source[e]=='{')-(source[e]=='}');e+=1
actual=source[a:e]
code=r"""
#include "base/keo_tr.h"
#include <cstdio>
#include <cstring>
bool translationReady=false, mainThread=true; bool IsMainThread(){return mainThread;}
unsigned calls=0;const char*Tr(const char*){++calls;return "translated";}
ACTUAL
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %s\n",#x);return 1;}}while(0)
int main(){CHECK(!std::strcmp(KeoTr("text"),"text")&&!calls);translationReady=true;mainThread=false;CHECK(!std::strcmp(KeoTr("text"),"text")&&!calls);mainThread=true;CHECK(!std::strcmp(KeoTr(NULL),"")&&!calls);CHECK(!std::strcmp(KeoTr(""),"")&&!calls);CHECK(!std::strcmp(KeoTr("Squad movement"),"translated")&&calls==1);const char*result=KeoTr("text");CHECK(!std::strcmp(result,"translated")&&calls==2);printf("Actual native translation boundary %u checks PASS\n",checks);return 0;}
""".replace('ACTUAL',actual)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
def run(name,text,passes=True):
 cpp=OUT/(name+'.cpp');cpp.write_text(text,encoding='utf-8');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(cpp),f'/Fo{OUT/name}.obj',f'/Fe{OUT/name}.exe'],env=env,capture_output=True,text=True)
 (OUT/(name+'.build.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 r=subprocess.run([str(OUT/(name+'.exe'))],capture_output=True,text=True);(OUT/(name+'.run.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if (r.returncode==0)!=passes:raise AssertionError(name+': '+r.stdout)
 print(name,r.stdout.strip())
run('actual',code)
for name,old,new in [('always-English','return Tr(english);','return english;'),('query-empty','if (!english || !*english)','if (!english)'),('query-null','if (!english || !*english)','if (english && !*english)')]:
 assert old in code;run(name,code.replace(old,new),False)
print('3 compiled translation-boundary mutants rejected')
