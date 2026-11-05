"""Actual L2 banking alarm stays independent of detailed reporting."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task3-error-alarm';OUT.mkdir(exist_ok=True)
s=(ROOT/'src/navmesh/cache/nm_cache_report.cpp').read_text(encoding='utf-8');a=s.index('static void CheckL2BankingError(');b=s.index('{',a);e=b+1;d=1
while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
actual=s[a:e]
code=r'''
#include <Windows.h>
#include <sstream>
#include <string>
#include <cstdio>
namespace navmesh{struct State{volatile LONG nmCacheMissCount,nmL2ZeroFaceSkip,nmDiskWriteCount;};State g_nmCache={};}
bool bypass=false,latched=false;unsigned errors=0;
bool L2Bypassed(){return bypass;}bool L2NotBanking(long misses,long writes){return writes==0&&misses>=8;}
bool L2NoBankFirstReport(){bool fresh=!latched;latched=true;return fresh;}void LogError(const std::string&){++errors;}
ACTUAL
unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %s\n",#x);return 1;}}while(0)
int main(){CheckL2BankingError();C(!errors&&!latched);navmesh::g_nmCache.nmCacheMissCount=8;
 bypass=true;CheckL2BankingError();C(!errors&&!latched);bypass=false;
 navmesh::g_nmCache.nmL2ZeroFaceSkip=8;CheckL2BankingError();C(!errors&&!latched);navmesh::g_nmCache.nmL2ZeroFaceSkip=0;
 navmesh::g_nmCache.nmDiskWriteCount=1;CheckL2BankingError();C(!errors&&!latched);navmesh::g_nmCache.nmDiskWriteCount=0;
 CheckL2BankingError();C(errors==1&&latched);for(int i=0;i<1000;++i)CheckL2BankingError();C(errors==1);
 printf("Actual Debug-independent L2 banking alarm %u checks PASS\n",checks);return 0;}
'''.replace('ACTUAL',actual)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
cpp=OUT/'actual.cpp';cpp.write_text(code,encoding='utf-8');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(cpp),'/Fo'+str(OUT/'actual.obj'),'/Fe'+str(OUT/'actual.exe')],env=env,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
r=subprocess.run([str(OUT/'actual.exe')],capture_output=True,text=True);(OUT/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');assert not r.returncode
# Verify the actual every-frame owner admits the error alarm at its original
# reporting cadence before returning for the Debug gate.
owner=s[s.index('void LogNavMeshCacheStats('):]
assert owner.index('lastNMLogTime = now;')<owner.index('CheckL2BankingError();')<owner.index('if (!DetailedLogEnabled()) return;')<owner.index('CacheStatsWindowCtx ctx;')
print('Actual owner cadence/error-before-report admission PASS')
