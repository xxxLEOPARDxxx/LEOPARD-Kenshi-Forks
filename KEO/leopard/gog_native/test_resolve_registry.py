"""Execute extracted production Resolve/Add/global loop and startup guard, not a reconstructed registry.
Supplied game code is never executed. SDK targets come from the actual official paired RVA tables.
"""
from pathlib import Path
import argparse,collections,hashlib,os,struct,subprocess
p=argparse.ArgumentParser();p.add_argument('--rva-dir',type=Path,default=Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/RVAs'));p.add_argument('--skip-mutant',action='store_true');p.add_argument('--check-old-profile',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];out=root/'build/gog-native-registry';out.mkdir(exist_ok=True)
s=(root/'src/game/klib_bindings.cpp').read_text(encoding='utf8');gs=(root/'src/game/klib_globals.cpp').read_text(encoding='utf8')
def body(text,marker):
 start=text.index('{',text.index(marker));at=start+1;depth=1
 while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
 return text[start:at]
resolve=body(s,'static void Resolve()');add=body(s,'static void Add(');glob=body(gs,'uintptr_t KlibGlobalAddress(')
init=body(s,'bool InitKlibBindings(');guard=body(init,'for (size_t i = 0; i < s_count; ++i)')
st=(a.rva_dir/'Steam_1.0.65.br').read_bytes();gt=(a.rva_dir/'GOG_1.0.65.br').read_bytes();assert len(st)==len(gt)==7630*4
assert hashlib.sha256(gt).hexdigest()=='a2a6394e95312a89e3d3f6842fbfda9c16e7d79749f1ee796fbd6168600bf347'
pairs=collections.defaultdict(set)
for x,y in zip(struct.unpack('<7630I',st),struct.unpack('<7630I',gt)):
 if x and y:pairs[x].add(y)
# Do not choose from an ambiguous symbol. This fails if Resolve newly consumes one.
rows='\n'.join('{0x%Xu,0x%Xu},'%(x,next(iter(ys)))for x,ys in sorted(pairs.items())if len(ys)==1)
code=r'''#include "game/gog_profile.h"
#include "game/klib_address_policy.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <new>
struct Pair {uintptr_t steam,gog;};static const Pair sdk[]={SDK_ROWS};
static std::vector<unsigned char> owned(0x232A000u);static uintptr_t gameBase=(uintptr_t)&owned[0];
static uintptr_t maskedKey=0;static KlibAddressEntry s_entries[160];static size_t s_count=0;static bool s_overflow=false;static uintptr_t s_globals[9]={0};
static uintptr_t OfficialSdkTarget(uintptr_t steam){for(size_t i=0;i<sizeof(sdk)/sizeof(sdk[0]);++i)if(sdk[i].steam==steam)return gameBase+sdk[i].gog;return 0;}
struct FakeWorld {char a[0x18];void*physics;char b[0x4B0-0x20];void*navmesh;char c[0x580-0x4B8];void*player;};
struct FakeRender{char a[8];void*render;};struct FakeShared{char a[8];void*townList;};
static_assert(offsetof(FakeWorld,physics)==0x18,"physics offset");static_assert(offsetof(FakeWorld,navmesh)==0x4B0,"navmesh offset");static_assert(offsetof(FakeWorld,player)==0x580,"player offset");
static FakeWorld*ou;static void*options;static FakeRender*au;static FakeShared*shou;
uintptr_t KlibGlobalAddress(uintptr_t rva) GLOBAL_BODY
uintptr_t KlibWeatherAddress(bool mainThread){return OfficialSdkTarget(mainThread?0x9E9540:0x9E80A0);}
static void Add(const char*name,uintptr_t legacy,uintptr_t impl,uintptr_t resolved) ADD_BODY
// Native member-pointer expressions are discarded by preprocessing; all runtime
// calls from this body are owned fixtures. The actual Add/global array/loop run.
#define Bind(name,legacy,impl,...) Add(name,legacy,impl,OfficialSdkTarget(impl))
static void Resolve() RESOLVE_BODY
#undef Bind
uintptr_t NativeProfileAddress(uintptr_t base,uintptr_t key){uintptr_t out=0;if(key==maskedKey)return 0;return GogProfile::Absolute(base,key,&out)?out:0;}
static void Log(const char*line){puts(line);}typedef void(*KlibLogFn)(const char*);
static bool StartupMatches(){uintptr_t base=gameBase;bool ok=!s_overflow;KlibLogFn log=Log;
for(size_t i=0;i<s_count;++i) STARTUP_GUARD
return ok;}
static unsigned checks=0;static bool Check(bool x,const char*why){++checks;if(!x){printf("FAIL %s\n",why);return false;}return true;}
int main(){
 ou=new((void*)OfficialSdkTarget(0x21330B0))FakeWorld;options=(void*)OfficialSdkTarget(0x2132440);au=new((void*)OfficialSdkTarget(0x21322B0))FakeRender;shou=new((void*)OfficialSdkTarget(0x2133098))FakeShared;
 Resolve();bool ok=Check(!s_overflow&&s_count==107,"actual Resolve captures98 functions and9 globals");
 size_t auIndex=160,nested=160;unsigned globals=0;
 for(size_t i=0;i<s_count;++i){ok=Check(s_entries[i].resolved!=0,"official SDK target is nonzero")&&ok;if(s_entries[i].implementationRva>=0x2000000)++globals;if(strcmp(s_entries[i].name,"au")==0)auIndex=i;if(strcmp(s_entries[i].name,"au.render")==0)nested=i;}
 ok=Check(globals==9&&auIndex<107&&nested<107,"actual global names and loop captured")&&ok;
 // This positive assertion is RED on the released missing-au table.
 ok=Check(StartupMatches(),"complete actual Resolve is admitted by actual production startup guard")&&ok;
 maskedKey=0x21322B0;ok=Check(!StartupMatches(),"lost au key refuses startup")&&ok;
 maskedKey=0x21322B8;ok=Check(!StartupMatches(),"lost nested global key refuses startup")&&ok;maskedKey=0;
 if(auIndex<107){uintptr_t save=s_entries[auIndex].resolved;s_entries[auIndex].resolved=save+8;ok=Check(!StartupMatches(),"wrong SDK pointer refuses startup")&&ok;s_entries[auIndex].resolved=0;ok=Check(!StartupMatches(),"null SDK pointer refuses startup")&&ok;s_entries[auIndex].resolved=save;}
 printf("%s %u checks,107 actual production Resolve entries; no game code executed\n",ok?"PASS":"FAIL",checks);return ok?0:1;
}
'''
for old,new in [('SDK_ROWS',rows),('GLOBAL_BODY',glob),('ADD_BODY',add),('RESOLVE_BODY',resolve),('STARTUP_GUARD',guard)]:assert old in code;code=code.replace(old,new)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');w=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(w/'Include'),LIB=str(vc/'lib/amd64')+';'+str(w/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env.get('PATH','')
def run(name,text,expected=0,profile_source=None):
 cpp=out/(name+'.cpp');cpp.write_text(text,encoding='utf8');cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/GL','/I'+str(root/'src'),str(cpp),str(profile_source or root/'src/game/gog_profile.cpp'),'/Fe'+name+'.exe','/link','/LTCG'];subprocess.run(cmd,cwd=out,env=env,check=True);result=subprocess.run([str(out/(name+'.exe'))],cwd=out,capture_output=True,text=True);(out/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf8');print(result.stdout);assert result.returncode==expected,(name,result.returncode,expected)
run('actual-resolve',code)
if not a.skip_mutant:
 bad=code.replace('if (!s_entries[i].resolved || s_entries[i].resolved != NativeProfileAddress(base,s_entries[i].implementationRva))','if (false)');assert bad!=code;run('mutant-bypass',bad,1);print('PASS compiled startup-bypass negative control')

if a.check_old_profile:
 old=out/'old-e608-profile';old.mkdir(exist_ok=True)
 for name in ('gog_profile.cpp','gog_profile.h','gog_profile_data.inc'):
  data=subprocess.check_output(['git','-C',str(root),'show','e6082d6264b68daaa4297ede7f6fe1cc5f9fb4f4:src/game/'+name]);(old/name).write_bytes(data)
 run('old-profile-RED',code,1,old/'gog_profile.cpp')
 assert 'MISMATCH au ours=0x0'in(out/'old-profile-RED.log').read_text(encoding='utf8')
 print('PASS old released profile RED reproduces missing-au startup refusal')
