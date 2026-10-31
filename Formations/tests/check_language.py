"""Compile the actual language reader against local/Workshop path fixtures."""
from pathlib import Path
import subprocess, tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'Formations.cpp').read_text(encoding='utf-8-sig')
start=source.index('    static int CurrentLang()')
brace=source.index('{',start);depth=1;end=brace+1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
function=source[start:end]
out=root.parents[2]/'build/FormationsLanguageCheck'
out.mkdir(parents=True,exist_ok=True)
code=r'''
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
enum {EN,RU,ZH};
static char g_dir[MAX_PATH];
static std::wstring fixtureExe;
static DWORD FixtureModuleFileNameW(HMODULE module,LPWSTR out,DWORD size){
 if(module!=NULL || fixtureExe.size()>=size)return 0;
 wcscpy_s(out,size,fixtureExe.c_str());return (DWORD)fixtureExe.size();}
#define GetModuleFileNameW FixtureModuleFileNameW
''' + function + r'''
int wmain(int argc,wchar_t** argv){
 std::filesystem::path root=argv[1];int failed=0,checks=0;
 auto check=[&](const char* name,int expected){++checks;int got=CurrentLang();
  if(got!=expected){++failed;printf("FAIL %s: got %d expected %d\n",name,got,expected);}};
 auto config=[&](const char* value){FILE* f=NULL;_wfopen_s(&f,(root/L"game/settings.cfg").c_str(),L"w");fputs(value,f);fclose(f);};
 std::filesystem::create_directories(root/L"game/mods/Formations");
 std::filesystem::create_directories(root/L"steamapps/workshop/content/233860/123/Formations");
 std::filesystem::create_directories(root/L"game/RE_Kenshi");
 auto dll=[&](const wchar_t* dir){std::string path=(root/dir).string()+"\\";strcpy_s(g_dir,path.c_str());};
 fixtureExe=(root/L"game/Kenshi_x64.exe").make_preferred().wstring();
 config("language=ru_RU\n");dll(L"game/mods/Formations");check("local Russian",RU);
 dll(L"steamapps/workshop/content/233860/123/Formations");check("Workshop Russian",RU);
 config("language=zh_CN\n");fixtureExe=(root/L"game/RE_Kenshi/Kenshi_x64.exe").make_preferred().wstring();check("RE_Kenshi Chinese",ZH);
 config("language=en_GB\n");check("English",EN);
 std::filesystem::remove(root/L"game/settings.cfg");check("missing settings",EN);
 printf("%d language checks, %d failed\n",checks,failed);return failed;
}
'''
(out/'language.cpp').write_text(code,encoding='utf-8')
vc=Path(r'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
bat=out/'build_check.cmd'
bat.write_text('@echo off\ncall "'+str(vc)+'" >nul\ncl /nologo /EHsc /std:c++17 /MT "'+str(out/'language.cpp')+'" /Fo"'+str(out/'language.obj')+'" /Fe"'+str(out/'language.exe')+'"\n',encoding='utf-8')
subprocess.run(['cmd.exe','/c',str(bat)],check=True,cwd=out)
with tempfile.TemporaryDirectory(prefix='formations-language-') as fixture:
    subprocess.run([str(out/'language.exe'),fixture],check=True)
