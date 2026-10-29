from pathlib import Path
import os,subprocess,sys
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
src=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting\Source\BetterLooting.cpp').read_text(encoding='utf-8-sig')
def structure(name):
 start=src.index('struct '+name+'\n');opening=src.index('{',start);depth=1;i=opening+1
 while depth:depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i+1]
code=r'''
#include <cstdlib>
#include <cstdio>
struct RootObject{};static int allocations=0;static bool badNested=false;
template<class T>struct lektor{T*stuff;unsigned count,capacity;lektor():stuff(NULL),count(0),capacity(0){}void clear(){count=0;}void fill(unsigned n){if(n>capacity){stuff=(T*)malloc(n*sizeof(T));capacity=n;++allocations;}count=n;}};
'''+structure('BetterLootingRadiusScratch')+'\n'+structure('BetterLootingRadiusScratchUse')+r'''
BetterLootingRadiusScratch* last=NULL;
void current(bool early){static BetterLootingRadiusScratch scratch;last=&scratch;BetterLootingRadiusScratchUse use(scratch);if(!use.acquired)return;scratch.characters.fill(256);scratch.buildings.fill(256);{BetterLootingRadiusScratchUse nested(scratch);if(nested.acquired||scratch.characters.count!=256||scratch.buildings.count!=256)badNested=true;}if(early)return;}
void baseline(){lektor<RootObject*> characters,buildings;characters.fill(256);buildings.fill(256);}
int main(int argc,char**){for(int i=0;i<10000;++i){if(argc>1)baseline();else current((i&1)!=0);}printf("scan allocations after 10000 calls: %d\n",allocations);if(allocations>2)return 1;if(last==NULL||last->busy||last->characters.count||last->buildings.count||badNested)return 2;puts("PASS: two retained buffers, clear on every/early exit, nested use cannot overwrite an outer scan");return 0;}
'''
p=base/'scratch_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(p),'/Fo'+str(base/'scratch_test.obj'),'/Fe'+str(base/'scratch_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'scratch_test.exe')]+sys.argv[1:],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
