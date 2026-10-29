from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
root=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting\Source')
code=r'''
#include <cstdio>
#include "D:/DEV/Kenshi/KenshiLib-src/Include/kenshi/Enums.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRules.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRequiredItems.h"
BetterLootingRules g_betterLootingRules={};
void BetterLootingSaveRules(){}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){BetterLootingRequiredItems list;CHECK(list.Empty());
CHECK(list.Parse(" name:Animal Skin ; id:record-ID | Basic First Aid Kit\nname:Animal Skin"));
CHECK(list.MatchesName("animal skin"));CHECK(!list.MatchesName("Animal Skinning"));CHECK(!list.MatchesName("Fine Animal Skin"));
CHECK(list.MatchesId("record-ID"));CHECK(!list.MatchesId("record-id"));CHECK(!list.MatchesName("record-ID"));CHECK(list.MatchesName("Basic First Aid Kit"));
g_betterLootingRules.minValue=1000;g_betterLootingRules.minValuePerKg=500;g_betterLootingRules.allowStolen=false;
CHECK(EvaluateLootDecision(1,100,false,ITEM,ITEM_NO_FUNCTION,true,false)==SKIP_CATEGORY);
CHECK(EvaluateLootDecision(1,100,false,ITEM,ITEM_NO_FUNCTION,true,true)==LOOT);
CHECK(EvaluateLootDecision(1,100,true,ITEM,ITEM_NO_FUNCTION,true,true)==SKIP_STOLEN);
g_betterLootingRules.allowStolen=true;CHECK(EvaluateLootDecision(1,100,true,ITEM,ITEM_NO_FUNCTION,true,true)==LOOT);
std::string many;for(int i=0;i<129;++i){char n[32];sprintf_s(n,"id:item-%d;",i);many+=n;}CHECK(!list.Parse(many));CHECK(list.MatchesId("item-127"));CHECK(!list.MatchesId("item-128"));
CHECK(list.Parse(" ; | \n"));CHECK(list.Empty());
const std::string utf8="\xD0\xA8\xD0\xBA\xD1\x83\xD1\x80\xD0\xB0";CHECK(list.Parse("name:"+utf8));CHECK(list.MatchesName(utf8));CHECK(!list.MatchesName(utf8+"x"));
puts("PASS: exact names/IDs, prefixes, trim, separators, ASCII case and UTF-8; 128-entry bound; required items bypass only category/value/ratio, never theft policy");return 0;}
'''
panel=(root/'BetterLootingPanel.cpp').read_text(encoding='utf-8-sig')
panel='\n'.join(line for line in panel.splitlines() if not line.startswith('#include'))
code=code.replace('int main(){',panel+'\nint main(){',1)
code=code.replace('puts("PASS: exact names/IDs', 'g_betterLootingRules.requiredItems="name:Animal Skin";g_betterLootingRules.ignorePlayerStorage=true;g_betterLootingRules.pickupGroundItems=true;for(int i=0;i<5;++i){BetterLootingApplyPreset(i);CHECK(g_betterLootingRules.requiredItems=="name:Animal Skin");CHECK(g_betterLootingRules.ignorePlayerStorage);CHECK(g_betterLootingRules.pickupGroundItems);}\nputs("PASS: exact names/IDs',1)
p=base/'rules_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/wd4482',str(p),'/Fo'+str(base/'rules_test.obj'),'/Fe'+str(base/'rules_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'rules_test.exe')],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
