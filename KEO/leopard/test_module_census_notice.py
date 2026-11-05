"""Run the real startup census against owned fake entries, never a live process.
The 48-entry curated cap remains independent from the 256-entry raw crash table.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/module-census-tests'
OUT.mkdir(parents=True, exist_ok=True)
s = (ROOT / 'src/plugin/crash_record.cpp').read_text(encoding='utf-8')
a = s.index('void SnapshotModuleBases()')
b = s.index('{', a)
e, depth = b + 1, 1
while depth:
    depth += (s[e] == '{') - (s[e] == '}')
    e += 1
body = s[a:e]
prefix = r'''
#include <Windows.h>
#include <TlHelp32.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "diag/module_bases.h"
#include "check.h"
ModuleBaseEntry g_moduleBases[kMaxModuleBases],g_rawModuleBases[256];
int g_moduleBaseCount=0,g_rawModuleCount=0;
const int kRawModuleCap=256;
bool g_rawModuleListValid=false,debug=false;
unsigned cursor=0,total=152,details=0,errors=0,formats=0,closes=0;
bool DetailedLogEnabled(){return debug;}
void LogDetail(const char*line){++details;Check(std::strstr(line,"152 modules, kept 48")!=NULL,"summary keeps actual count and original cap");}
void LogError(const char*){++errors;}
BOOL FakeSelf(DWORD,LPCSTR,HMODULE*out){*out=(HMODULE)(uintptr_t)(0x100000+150*0x10000);return TRUE;}
HMODULE FakeRe(LPCSTR){return (HMODULE)(uintptr_t)(0x100000+151*0x10000);}
HANDLE FakeSnapshot(DWORD,DWORD){return (HANDLE)1;}
DWORD FakePid(){return 42;}
void Entry(MODULEENTRY32*e){std::memset(e,0,sizeof(*e));e->dwSize=sizeof(*e);sprintf_s(e->szModule,sizeof(e->szModule),"owned%u.dll",cursor);e->modBaseAddr=(BYTE*)(uintptr_t)(0x100000+cursor*0x10000);e->modBaseSize=0x10000;}
BOOL FakeFirst(HANDLE,MODULEENTRY32*e){cursor=0;if(!total)return FALSE;Entry(e);return TRUE;}
BOOL FakeNext(HANDLE,MODULEENTRY32*e){if(++cursor>=total)return FALSE;Entry(e);return TRUE;}
BOOL FakeClose(HANDLE){++closes;return TRUE;}
int Format(char*out,size_t cap,size_t,const char*fmt,...){++formats;va_list ap;va_start(ap,fmt);int n=vsnprintf_s(out,cap,_TRUNCATE,fmt,ap);va_end(ap);return n;}
#define GetModuleHandleExA FakeSelf
#define GetModuleHandleA FakeRe
#define CreateToolhelp32Snapshot FakeSnapshot
#define GetCurrentProcessId FakePid
#define Module32First FakeFirst
#define Module32Next FakeNext
#define CloseHandle FakeClose
#define _snprintf_s Format
'''
suffix = r'''
int main(){
 for(int on=0;on<2;++on){debug=on!=0;SnapshotModuleBases();
 Check(g_rawModuleListValid&&g_rawModuleCount==152,"all owned raw census entries retained regardless of log");
 Check(g_moduleBaseCount==48,"original curated cap unchanged");
 Check(g_moduleBases[0].base==0x100000+150*0x10000&&g_moduleBases[1].base==0x100000+151*0x10000,"self and RE stay pinned beyond normal curated fill");
 Check(details==(unsigned)on&&errors==0,"optional curated truncation detail only, never error");
 Check(formats==(unsigned)on,"OFF avoids optional formatting before producer");
 Check(ResolveModuleForAddress(g_rawModuleBases,g_rawModuleCount,0x100000+149*0x10000+7,NULL)==149,"raw attribution survives curated truncation");}
 Check(closes==2,"each snapshot closes its owned handle");return CheckExit("actual_module_census");}
'''
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = dict(os.environ, INCLUDE=f'{vc}/include;{sdk}/Include;{ROOT}/src;{ROOT}/tools/tests', LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')

def run(name, actual, green):
    folder = OUT / name
    folder.mkdir(exist_ok=True)
    cpp = folder / 'actual.cpp'
    cpp.write_text(prefix + actual + suffix, encoding='utf-8')
    exe = folder / 'actual.exe'
    cmd = [str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/D_MBCS', str(cpp), str(ROOT / 'src/diag/module_bases.cpp'), '/Fe' + str(exe), '/link', '/MACHINE:X64']
    p = subprocess.run(cmd, cwd=str(folder), env=env, capture_output=True, text=True)
    (folder / 'build.log').write_text(p.stdout + p.stderr, encoding='utf-8')
    assert p.returncode == 0, (name, p.stdout, p.stderr)
    p = subprocess.run([str(exe)], capture_output=True, text=True)
    (folder / 'run.log').write_text(p.stdout + p.stderr, encoding='utf-8')
    assert (p.returncode == 0) == green, (name, p.returncode, p.stdout)
    print(name + ': ' + ('PASS' if green else 'negative control rejected'))

run('production', body, True)
run('severity-mutant', body.replace('LogDetail(line)', 'LogError(line)'), False)
run('producer-mutant', body.replace(' && DetailedLogEnabled()', ''), False)
