"""VC10 host execution of actual shared MO loader and KeoTr admission.
The fake LocaleManager is the explicit native boundary; no game is loaded.
"""
from pathlib import Path
import os,subprocess,shutil,struct,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/task3-catalog-host';OUT.mkdir(exist_ok=True)
subprocess.run([sys.executable,str(ROOT/'tools/localization/compile_catalogs.py'),'--output',str(OUT/'locale')],check=True)
(OUT/'kenshi').mkdir(exist_ok=True)
(OUT/'kenshi/LocaleInfo.h').write_text('''#pragma once
#include <string>
struct LocaleInfo{std::string id;}; extern unsigned localeQueries; extern LocaleInfo fakeLocale;
struct LocaleManager{static LocaleManager*getInstance(){++localeQueries;static LocaleManager m;return &m;}LocaleInfo*getCurrentLocale(){return &fakeLocale;}};
''')
(OUT/'Debug.h').write_text('''#pragma once
#include <string>
extern unsigned details,errors;extern std::string lastDetail;
inline void DebugLog(const std::string&s){++details;lastDetail=s;}
inline void ErrorLog(const std::string&){++errors;}
''')
good=(OUT/'locale/en_GB/LC_MESSAGES/keo.mo').read_bytes();n=struct.unpack_from('<I',good,8)[0]
# Valid big-endian metadata/tables with unchanged string bytes.
end=28+16*n;big=b''.join(good[i:i+4][::-1] for i in range(0,end,4))+good[end:];(OUT/'big.mo').write_bytes(big)
mutants=[b'',good[:27],b'BAD!'+good[4:],good[:8]+struct.pack('<I',0xffffffff)+good[12:]]
for pos,value in [(12,len(good)),(16,len(good)),(28+8,0xffffffff),(28+12,0xffffffff)]:
 b=bytearray(good);struct.pack_into('<I',b,pos,value);mutants.append(bytes(b))
# Unterminated final translation.
b=bytearray(good);b[-1]=65;mutants.append(bytes(b))
# Valid tables with aliased payloads must respect record/decoded-text budgets.
count=65537;off=28+16*count
mutants.append(struct.pack('<7I',0x950412de,0,count,28,28+8*count,0,0)+struct.pack('<2I',1,off)*(2*count)+b'A\0')
count=100;payload=b'A'*(100*1024)+b'\0';off=28+16*count
mutants.append(struct.pack('<7I',0x950412de,0,count,28,28+8*count,0,0)+struct.pack('<2I',len(payload)-1,off)*(2*count)+payload)
for i,b in enumerate(mutants):(OUT/f'bad{i}.mo').write_bytes(b)
bad=OUT/'badlocale/en_GB/LC_MESSAGES';bad.mkdir(parents=True,exist_ok=True);(bad/'keo.mo').write_bytes(mutants[-1])
s=(ROOT/'src/base/keo_tr.cpp').read_text(encoding='utf-8');a=s.index('const char* KeoTr(');b=s.index('{',a);e=b+1;d=1
while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
actual=s[a:e]
code=r'''
#include <string>
#include <cstdio>
#include <cstring>
unsigned localeQueries=0,details=0,errors=0;std::string lastDetail;
#define KLOC_DOMAIN "keo"
#include "base/Localization.h"
LocaleInfo fakeLocale;
bool mainThread=true,translationReady=false;bool IsMainThread(){return mainThread;}
ACTUAL
unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL %s\n",#x);return 1;}}while(0)
int main(){
 C(!KLocLog::Current().enabled);C(!strcmp(KeoTr(NULL),""));C(!strcmp(KeoTr(""),""));
 C(!strcmp(KeoTr("Detailed log"),"Detailed log")&&!localeQueries&&!KLoc::Initialised());
 translationReady=true;mainThread=false;C(!strcmp(KeoTr("Detailed log"),"Detailed log")&&!localeQueries&&!KLoc::Initialised());mainThread=true;
 // Actual loader accepts both byte orders and rejects malformed rows atomically.
 C(KLoc::LoadMo("big.mo"));size_t count=KLoc::Catalog().size();C(count==68);
 for(int i=0;i<11;++i){char path[32];sprintf_s(path,"bad%d.mo",i);C(!KLoc::LoadMo(path)&&KLoc::Catalog().size()==count);}
 C(!KLoc::LoadDomain("badlocale","en_GB")&&errors==1&&!details);
 KLoc::Catalog().clear();fakeLocale.id="ru";
 const char*saved=KeoTr("Detailed log");C(strcmp(saved,"Detailed log")&&KLoc::Initialised()&&localeQueries==1&&!details);
 std::string expected=saved;KLocLog::SetEnabled(true);C(details==1&&lastDetail.find("strings loaded=68")!=std::string::npos&&localeQueries==1);
 C(saved==KeoTr("Detailed log")&&expected==saved);KLocLog::SetEnabled(true);C(details==1);
 KLocLog::SetEnabled(false);KLocLog::SetEnabled(true);C(details==2&&saved==KeoTr("Detailed log")&&localeQueries==1);
 KLocLog::SetEnabled(false);KLoc::Catalog()["fmt %s"]="fmt %d";C(!strcmp(TrFmt("fmt %s"),"fmt %s")&&errors==2&&!KLocLog::Current().enabled);
 fakeLocale.id="zh_CN";C(expected==KeoTr("Detailed log")&&localeQueries==1);
 C(!strcmp(KeoTr("Missing key"),"Missing key"));
 printf("Actual shared catalog/admission/late-summary %u checks PASS\n",checks);return 0;
}
'''.replace('ACTUAL',actual)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{OUT};{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
cpp=OUT/'actual.cpp';cpp.write_text(code,encoding='utf-8');r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(cpp),'/Fo'+str(OUT/'actual.obj'),'/Fe'+str(OUT/'actual.exe')],env=env,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
r=subprocess.run([str(OUT/'actual.exe')],cwd=OUT,capture_output=True,text=True);(OUT/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');assert not r.returncode
