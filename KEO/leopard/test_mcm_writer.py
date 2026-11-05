"""Actual existing flat atomic INI writer, owned fixture files only."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/mcm-writer-tests';OUT.mkdir(exist_ok=True)
s=(ROOT/'src/render/render_live.cpp').read_text()
def body(marker):
 a=s.index(marker);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
actual='\n'.join(body(x) for x in ['static bool ReadWholeFile(', 'static DWORD WriteReplacing(', 'static std::string IniPath(', 'static bool ReadIni(', 'static bool WriteIniIfChanged(', 'static std::string FirstIniEol(', 'static bool HasIniSection(', 'bool SaveIniEntries('])
code=r'''
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include "base/ini_text.h"
#include "base/ini_names.h"
#include "base/log_admission.h"
std::string dir;bool throwLog=false,detailed=false;unsigned checks=0,errors=0;
bool DetailedLogEnabled(){return detailed;}void LogError(const std::string&){++errors;}
std::string GetDLLDirectory(){return dir;}void LogMsg(const std::string&){if(throwLog)throw std::runtime_error("fake success log allocation failure");}
ACTUAL
void C(bool ok,const char*why){++checks;if(!ok){printf("FAIL %s\n",why);throw 1;}}
std::string Text(){std::string s;C(ReadWholeFile(IniPath(),&s),"fixture read");return s;}
int main(int argc,char**argv){try{C(argc==2,"owned directory argument");dir=argv[1];std::string p=IniPath();const std::string original="; owner\r\nunknown=keep\r\npreload=true\r\npreload=true\r\n";FILE*f=NULL;fopen_s(&f,p.c_str(),"wb");C(f!=NULL,"owned source open");C(fwrite(original.data(),1,original.size(),f)==original.size()&&fclose(f)==0,"owned source written");
 IniEntry e;e.key="preload";e.value="false";e.kind=INI_BOOL;e.append=true;std::vector<IniEntry>entries(1,e);
 HANDLE lock=CreateFileA(p.c_str(),GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);C(lock!=INVALID_HANDLE_VALUE,"exclusive read-failure lock");C(!SaveIniEntries(entries,NULL,"MCM"),"actual read sharing failure reported");CloseHandle(lock);C(Text()==original,"read failure keeps original");
 std::string tmp=p+".tmp";C(CreateDirectoryA(tmp.c_str(),NULL)!=FALSE,"owned temp path blocked by directory");C(!SaveIniEntries(entries,NULL,"MCM"),"actual temp write failure reported");C(Text()==original,"temp write failure keeps original");RemoveDirectoryA(tmp.c_str());
 lock=CreateFileA(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);C(lock!=INVALID_HANDLE_VALUE,"replace-failure lock");C(!SaveIniEntries(entries,NULL,"MCM"),"actual atomic replace failure reported");CloseHandle(lock);C(Text()==original&&GetFileAttributesA(tmp.c_str())==INVALID_FILE_ATTRIBUTES,"replace failure keeps original and cleans temp");
 C(errors==3,"all three actual writer failures logged while Debug is OFF");detailed=true;
 C(SaveIniEntries(entries,NULL,"MCM"),"atomic flat save succeeds");std::string saved=Text();C(saved.find("; owner\r\nunknown=keep\r\n")==0&&saved.find("preload=false\r\npreload=false\r\n")!=std::string::npos,"comments EOL unknown and duplicate semantics retained");C(SaveIniEntries(entries,NULL,"MCM")&&Text()==saved,"no-change save stable");
 entries[0].value="true";throwLog=true;bool threw=false,ok=false;try{ok=SaveIniEntries(entries,NULL,"MCM");}catch(...){threw=true;}throwLog=false;C(!threw&&ok&&Text()==original,"success logging failure cannot hide a completed atomic commit");
 C(DeleteFileA(p.c_str())!=FALSE,"owned source cleanup");printf("PASS actual flat atomic writer %u checks\n",checks);return 0;}catch(...){return 1;}}
'''.replace('ACTUAL',actual)
cpp=OUT/'writer.cpp';cpp.write_text(code)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(cpp),str(ROOT/'src/base/ini_text.cpp'),'/Fo'+str(OUT)+'/', '/Fe'+str(OUT/'writer.exe')],env=env,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
owned=OUT/'owned';owned.mkdir(exist_ok=True)
r=subprocess.run([str(OUT/'writer.exe'),owned.as_posix()+'/'],capture_output=True,text=True);(OUT/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');assert not r.returncode
