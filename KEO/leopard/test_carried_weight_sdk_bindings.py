"""Actual SDK declarations + unchanged Bindings; own stubs, no game/SDK execution."""
from pathlib import Path
import os,subprocess,sys
R=Path('D:/DEV/Kenshi/forks/KEO');O=R/'build/carried-weight-sdk';O.mkdir(parents=True,exist_ok=True)
source=(R/'src/inventory/carried_weight.cpp').read_text(encoding='utf8')
def body(name):
 start=source.index(name);at=source.index('{',start);depth=1;end=at+1
 while depth:
  if source[end]=='{':depth+=1
  elif source[end]=='}':depth-=1
  end+=1
 return source[start:end]
window=source[source.index('static const unsigned char kWeightSteam'):source.index('bool SdkAt(')]
sdkat=body('bool SdkAt(');bindings=body('bool Bindings(')
RED='--expect-red'in sys.argv
if '--old-control'in sys.argv:
 bindings=bindings.replace('&Character::_NV_getAge','&Character::getAge').replace('&Character::_NV_getRace','&Character::getRace')
 RED=True
code=r'''
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include "game/klib_bindings.h"
#include "base/klib_include.h"
#include <kenshi/Inventory.h>
#include <kenshi/Character.h>
#include "base/klib_include_end.h"
// These are owned link stubs with distinct bodies, never the actual game API.
float Inventory::getTotalWeight(){return 123.0f;}
float Character::getTotalCarryWeight(){return 234.0f;}
Character*Inventory::getCallbackCharacter()const{return (Character*)uintptr_t(0x1111);}
GameDataCopyStandalone*Character::getAppearanceData(){return (GameDataCopyStandalone*)uintptr_t(0x2222);}
float Character::_NV_getAge()const{return 345.0f;}
RaceData*Character::_NV_getRace()const{return (RaceData*)uintptr_t(0x3333);}
static uintptr_t gameBase;static bool gog=false;static int rejected=0,checks=0;static void*allowed[6];
static const uintptr_t steamRva[6]={0x746890,0x5C7F40,0x70C320,0xCD330,0x5E4EF0,0x5E1470};
static const uintptr_t gogRva[6]={0x7461C0,0x5C8250,0x70BC80,0xCD330,0x5E5200,0x5E1780};
template<typename T>static void*Direct(T f){return (void*&)f;}
intptr_t KlibRealAddressOf(void*function){for(unsigned i=0;i<6;++i)if(function==allowed[i])return gameBase+(gog?gogRva[i]:steamRva[i]);++rejected;return 0;}
bool NativeProfileIsGog(){return gog;}
static void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s rejected=%d\n",s,rejected);exit(1);}}
''' + window+sdkat+'\n'+bindings+r'''
int main(){allowed[0]=Direct(&Inventory::getTotalWeight);allowed[1]=Direct(&Character::getTotalCarryWeight);allowed[2]=Direct(&Inventory::getCallbackCharacter);allowed[3]=Direct(&Character::getAppearanceData);allowed[4]=Direct(&Character::_NV_getAge);allowed[5]=Direct(&Character::_NV_getRace);
 const unsigned char*age=(const unsigned char*)Direct(&Character::getAge);const unsigned char*race=(const unsigned char*)Direct(&Character::getRace);
 C(age!=allowed[4]&&race!=allowed[5],"compiler emits separate local virtual PMF thunks");
 C(!memcmp(age,"\x48\x8b\x01\xff\xa0\x90\x03\0\0",9)&&!memcmp(race,"\x48\x8b\x01\xff\xa0\xd0\0\0\0",9),"actual MSVC thunks match delivered F1880/F1890 bytes");
 C(!KlibRealAddress(&Character::getAge)&&!KlibRealAddress(&Character::getRace)&&rejected==2,"SDK-style boundary rejects virtual PMFs without executing them");
 std::vector<unsigned char>image(0x800000);gameBase=(uintptr_t)&image[0];
 for(unsigned p=0;p<2;++p){gog=p!=0;memcpy((void*)(gameBase+(gog?gogRva[0]:steamRva[0])),gog?kWeightGog:kWeightSteam,32);memcpy((void*)(gameBase+(gog?gogRva[1]:steamRva[1])),gog?kCarryGog:kCarrySteam,44);memcpy((void*)(gameBase+(gog?0x4443B:0x443C8)),gog?kThunkGog:kThunkSteam,5);
  rejected=0;uintptr_t weight=0;C(Bindings(&weight)&&!rejected&&weight==gameBase+(gog?gogRva[0]:steamRva[0]),"actual production Bindings admits only correct nonvirtual SDK aliases");
 }
 printf("PASS %d actual production weight SDK checks\n",checks);
}
'''
(O/'bindings.cpp').write_text(code,encoding='utf8');V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');S=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{V}/include;{S}/Include',LIB=f'{V}/lib/amd64;{S}/Lib/x64')
for opt in ('Od','O2'):
 exe=O/('bindings-'+opt+('-red'if RED else'')+'.exe')
 cmd=[str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MD','/DBOOST_ALL_NO_LIB','/I'+str(R/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include/ogre','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(O/'bindings.cpp'),'/Fo'+str(O/('bindings-'+opt+'.obj')),'/Fe'+str(exe)]
 p=subprocess.run(cmd,env=env,capture_output=True,text=True);(O/('build-'+opt+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');assert not p.returncode,p.stdout+p.stderr
 p=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,p.stdout.strip());(O/('run-'+opt+('-red'if RED else'')+'.log')).write_text(p.stdout+p.stderr,encoding='utf8')
 if RED:assert p.returncode==1 and'actual production Bindings admits'in p.stdout
 else:assert not p.returncode,p.stdout+p.stderr
