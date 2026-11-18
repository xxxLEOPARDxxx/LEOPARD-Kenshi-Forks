"""Production admission against saved native windows, without starting Kenshi.

The Steam windows are also checked against the actual SHA-pinned file. GOG uses
the existing reviewed contract snapshot; its EXE need not be on this machine.
"""
from pathlib import Path
import hashlib, json, os, struct, subprocess, sys

root = Path(__file__).resolve().parents[2]
out = root / 'build/steel-jobs-shared-context'
out.mkdir(parents=True, exist_ok=True)
contract = json.loads((root / 'leopard/steel_jobs/native_contract.json').read_text())
sys.path.insert(0, 'D:/DEV/Kenshi/ours/SkillLevelUpInfo/tests/_tools')
import pefile
native = Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
assert hashlib.sha256(native.read_bytes()).hexdigest() == contract['steam_sha256']
pe = pefile.PE(str(native), fast_load=True)
for row in contract['code']:
    expected = bytes.fromhex(row['bytes'][0])
    assert pe.get_data(row['rva'][0], len(expected)) == expected
for row in contract['vtables']:
    actual = struct.unpack('<%dQ' % row['count'], pe.get_data(row['rva'][0]-8, row['count']*8))
    assert [v-pe.OPTIONAL_HEADER.ImageBase for v in actual] == row['slots'][0]

def codes(platform):
    return ',\n'.join('{%d,%d,{%s}}' % (r['rva'][platform], len(bytes.fromhex(r['bytes'][platform])),
        ','.join(str(v) for v in bytes.fromhex(r['bytes'][platform]))) for r in contract['code'])

def tables(platform):
    return ',\n'.join('{%d,%d,{%s}}' % (r['rva'][platform]-8, r['count'],
        ','.join(str(v) for v in r['slots'][platform])) for r in contract['vtables'])

code = r'''
#include "game/gog_jobs_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Window {uintptr_t rva;unsigned n;unsigned char bytes[32];};
struct Table {uintptr_t rva;unsigned n;uintptr_t entries[8];};
static const Window windows[2][11]={WINDOWS};
static const Table tables[2][3]={TABLES};
static int checks=0;
static void Check(bool ok,const char*what){++checks;if(!ok){printf("FAIL %s\n",what);exit(1);}}
struct Image {unsigned platform;uintptr_t base,shared,corrupt,unreadable;unsigned char opcode;uintptr_t secondShared;};
static bool Read(void*ctx,uintptr_t rva,void*out,size_t n){Image&i=*(Image*)ctx;
 if(rva==i.unreadable)return false;
 for(unsigned k=0;k<11;++k){const Window&w=windows[i.platform][k];if(rva!=w.rva||n!=w.n)continue;
  memcpy(out,w.bytes,n);unsigned char*b=(unsigned char*)out;
  if(rva==i.shared||rva==i.secondShared){b[0]=i.opcode;b[1]=0x12;b[2]=0x34;b[3]=0x56;b[4]=0;}
  if(i.corrupt>=rva&&i.corrupt-rva<n)b[i.corrupt-rva]^=1;return true;
 }
 for(unsigned k=0;k<3;++k){const Table&t=tables[i.platform][k];if(n!=8||rva<t.rva||rva-t.rva>=t.n*8||(rva-t.rva)%8)continue;
  uintptr_t v=i.base+t.entries[(rva-t.rva)/8];if(rva==i.corrupt)v^=1;memcpy(out,&v,8);return true;
 }return false;
}
static bool Valid(Image&i){return JobsProfile::Validate(i.platform!=0,i.base,Read,&i);}
int main(){for(unsigned p=0;p<2;++p){Image i={p,0x7FF600000000ULL,0,0,0,0xE9};
 Check(Valid(i),"unchanged native contract admitted");i.shared=windows[p][JobsProfile::CONTEXT_SHOW].rva;
 Check(Valid(i),"existing chained context-menu hook must not disable independent waypoint jobs");
 for(unsigned k=5;k<32;++k){i.corrupt=i.shared+k;Check(!Valid(i),"changed shared native tail refused");}i.corrupt=0;
 i.unreadable=i.shared;Check(!Valid(i),"unreadable shared window refused");i.unreadable=0;
 i.opcode=0xE8;Check(!Valid(i),"arbitrary patched entry refused");i.opcode=0xE9;
 i.shared=0;Check(Valid(i),"initial task metadata contract before later plugins");
 i.shared=windows[p][JobsProfile::FACTORY].rva;
 Check(JobsProfile::ValidateForInstall(p!=0,i.base,Read,&i),"later chained task factory must remain installable after other plugins load");
 Check(Valid(i),"an existing factory hook is chained, not called as an unmodified native body");
 i.secondShared=windows[p][JobsProfile::CONTEXT_SHOW].rva;
 Check(Valid(i)&&JobsProfile::ValidateForInstall(p!=0,i.base,Read,&i),"context and factory hooks coexist without disabling independent route jobs");
 for(unsigned k=5;k<32;++k){i.corrupt=i.shared+k;Check(!Valid(i)&&!JobsProfile::ValidateForInstall(p!=0,i.base,Read,&i),"every changed factory tail byte refused");}i.corrupt=0;
 i.unreadable=i.shared;Check(!Valid(i)&&!JobsProfile::ValidateForInstall(p!=0,i.base,Read,&i),"unreadable shared factory entry refused");i.unreadable=0;
 i.opcode=0xE8;Check(!Valid(i)&&!JobsProfile::ValidateForInstall(p!=0,i.base,Read,&i),"arbitrary non-hook factory patch refused");i.opcode=0xE9;
 i.secondShared=0;
 for(unsigned k=0;k<11;++k){if(k==JobsProfile::CONTEXT_SHOW||k==JobsProfile::FACTORY)continue;i.shared=windows[p][k].rva;Check(!Valid(i),"shared-chain exception never applies to direct native dependencies");}i.shared=0;
 for(unsigned k=0;k<11;++k){i.corrupt=windows[p][k].rva;Check(!Valid(i),"all other changed entries still refused");}i.corrupt=0;
 for(unsigned k=0;k<3;++k)for(unsigned j=0;j<tables[p][k].n;++j){i.corrupt=tables[p][k].rva+j*8;Check(!Valid(i),"every changed native vtable slot refused");}i.corrupt=0;
 i.base+=0x20000000;Check(Valid(i),"ASLR base does not change admission");
 }printf("PASS %d shared-context native-admission checks\n",checks);}
'''.replace('WINDOWS', '{'+codes(0)+'},{'+codes(1)+'}').replace('TABLES', '{'+tables(0)+'},{'+tables(1)+'}')
(out/'shared_context.cpp').write_text(code)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ, INCLUDE=f'{vc}/include;{sdk}/Include', LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
for opt in ['Od','O2']:
    binary=out/f'shared_context_{opt}.exe'
    cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT','/I'+str(root/'src'),str(out/'shared_context.cpp'),str(root/'src/game/gog_jobs_profile.cpp'),'/Fo'+str(out)+'/', '/Fe'+str(binary)]
    q=subprocess.run(cmd,env=env,capture_output=True,text=True)
    (out/f'compile-{opt}.log').write_text(q.stdout+q.stderr)
    assert q.returncode==0,q.stdout+q.stderr
    q=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
    (out/f'run-{opt}.log').write_text(q.stdout+q.stderr)
    print(opt,q.stdout.strip())
    assert q.returncode==0,q.stdout+q.stderr
