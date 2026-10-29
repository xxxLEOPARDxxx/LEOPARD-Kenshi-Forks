from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
src=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting\Source\BetterLooting.cpp').read_text(encoding='utf-8-sig')
def extract(name):
 start=src.index(name+'(')
 while src.find(';',start)<src.find('{',start):start=src.index(name+'(',start+len(name)+1)
 start=src.rfind('\n',0,start)+1;opening=src.index('{',start);depth=1;i=opening+1
 while depth:depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i]
code=r'''
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "D:/DEV/Kenshi/KenshiLib-src/Include/kenshi/Enums.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRules.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRequiredItems.h"
BetterLootingRules g_betterLootingRules={};BetterLootingRequiredItems g_requiredLoot;bool g_requiredLootTextComplete=true,g_betterLootingDebugLog=false,g_depositByRules=false;
namespace HoldKey{struct Key{int vk;Key():vk(0){}};Key Parse(const char*){return Key();}}HoldKey::Key g_depositKey;
std::string configFile;std::string BetterLootingIniPath(){return configFile;}void BlTrace(const char*){}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
'''
for name in ['LoadBetterLootingRules','BetterLootingWriteBool','BetterLootingSaveRules']:
 code+='\n'+extract(name)
code+=r'''
int main(int argc,char**argv){CHECK(argc==2);configFile=argv[1];DeleteFileA(configFile.c_str());
CHECK(WritePrivateProfileStringA("Other","Keep","untouched",configFile.c_str()));
std::string original(9000,'x');CHECK(WritePrivateProfileStringA("Rules","RequiredItems",original.c_str(),configFile.c_str()));CHECK(LoadBetterLootingRules());CHECK(!g_requiredLootTextComplete);CHECK(g_requiredLoot.Empty());CHECK(g_betterLootingRules.ignorePlayerStorage);CHECK(!g_betterLootingRules.pickupGroundItems);BetterLootingSaveRules();char readback[12000]={};GetPrivateProfileStringA("Rules","RequiredItems","",readback,sizeof(readback),configFile.c_str());CHECK(original==readback);
std::string maximum(8191,'x');CHECK(WritePrivateProfileStringA("Rules","RequiredItems",maximum.c_str(),configFile.c_str()));CHECK(LoadBetterLootingRules());CHECK(g_requiredLootTextComplete);CHECK(g_betterLootingRules.requiredItems==maximum);
const std::string utf8="name:\xE5\x85\xBD\xE7\x9A\xAE; name:\xD0\xA8\xD0\xBA\xD1\x83\xD1\x80\xD0\xB0;id:skin-id";
CHECK(WritePrivateProfileStringA("Rules","RequiredItems",utf8.c_str(),configFile.c_str()));CHECK(WritePrivateProfileStringA("Behavior","IgnorePlayerStorage","0",configFile.c_str()));CHECK(WritePrivateProfileStringA("Behavior","PickupGroundItems","1",configFile.c_str()));CHECK(LoadBetterLootingRules());CHECK(g_requiredLootTextComplete);CHECK(g_betterLootingRules.requiredItems==utf8);CHECK(!g_betterLootingRules.ignorePlayerStorage);CHECK(g_betterLootingRules.pickupGroundItems);CHECK(g_requiredLoot.MatchesId("skin-id"));BetterLootingSaveRules();GetPrivateProfileStringA("Rules","RequiredItems","",readback,sizeof(readback),configFile.c_str());CHECK(utf8==readback);GetPrivateProfileStringA("Other","Keep","",readback,sizeof(readback),configFile.c_str());CHECK(std::string(readback)=="untouched");DeleteFileA(configFile.c_str());
puts("PASS: real Windows INI load/save preserves unrelated keys and oversized whitelist; UTF-8 ru/zh names round-trip; new defaults and live reload correct; no installed INI touched");return 0;}
'''
p=base/'config_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/wd4482',str(p),'/Fo'+str(base/'config_test.obj'),'/Fe'+str(base/'config_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'config_test.exe'),str(base/'temporary_config.ini')],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
