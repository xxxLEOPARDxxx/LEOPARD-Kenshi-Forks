"""SHA-pinned actual-file native contracts. Neither input EXE is executed."""
from pathlib import Path
import argparse,hashlib,json,os,struct,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--steam-exe',type=Path,required=True);p.add_argument('--gog-exe',type=Path,required=True);a=p.parse_args()
R=Path('D:/DEV/Kenshi/forks/KEO');O=R/'build/steel-jobs-native';O.mkdir(parents=True,exist_ok=True)
d=json.loads((R/'leopard/steel_jobs/native_contract.json').read_text(encoding='utf8'))
for image,key in [(a.steam_exe,'steam_sha256'),(a.gog_exe,'gog_sha256')]:assert hashlib.sha256(image.read_bytes()).hexdigest()==d[key],f'{key} identity refused'
sys.path.insert(0,'D:/DEV/Kenshi/ours/SkillLevelUpInfo/tests/_tools');import pefile,capstone
images=[pefile.PE(str(q),fast_load=True)for q in[a.steam_exe,a.gog_exe]]
for i,im in enumerate(images):
 for row in d['code']:assert im.get_data(row['rva'][i],len(bytes.fromhex(row['bytes'][i])))==bytes.fromhex(row['bytes'][i]),row['name']
 for row in d['vtables']:
  vals=struct.unpack('<%dQ'%row['count'],im.get_data(row['rva'][i]-8,row['count']*8));assert list(v-im.OPTIONAL_HEADER.ImageBase for v in vals)==row['slots'][i]
  col=row['slots'][i][0];sig,off,cd,td,ch,selfr=struct.unpack('<6I',im.get_data(col,24));assert sig==1 and off==cd==0 and selfr==col
  name=im.get_data(td+16,80).split(b'\0')[0];assert name=={'EMPTY_VTABLE':b'.?AVTask_EmptyMachine@@','MOVE_VTABLE':b'.?AVTask_Move@@','BLANK_VTABLE':b'.?AVTask_Blank@@'}[row['name']]
# The factory's two switch arrays are the only layout-dependent operands in its
# common code body. Pin every case target, not just its entry prologue.
s,g=images
assert s.get_data(0x3303c8,0x122)==g.get_data(0x32ff58,0x122)
sj=struct.unpack('<108I',s.get_data(0x330218,432));gj=struct.unpack('<108I',g.get_data(0x32fda8,432))
assert all(b==v-0x470 and 0x32eb90<=v<0x330218 for v,b in zip(sj,gj))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
def shape(im,start,end):
 raw=bytearray(im.get_data(start,end-start));covered=0
 for q in md.disasm(raw,start):
  assert q.address==start+covered;covered+=q.size;r=q.address-start
  if q.group(capstone.CS_GRP_JUMP)or q.group(capstone.CS_GRP_CALL):
   for op in q.operands:
    if op.type==capstone.x86.X86_OP_IMM and not start<=op.imm<end:raw[r+q.imm_offset:r+q.imm_offset+q.imm_size]=b'\0'*q.imm_size
  for op in q.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:raw[r+q.disp_offset:r+q.disp_offset+q.disp_size]=b'\0'*q.disp_size
 assert covered==len(raw)
 return raw
x=shape(s,0x32eb90,0x330218);y=shape(g,0x32e720,0x32fda8)
for at,n in [(0x70,3),(0x77,3)]:x[at:at+n]=y[at:at+n]
assert x==y,'factory instruction/register/member/control-flow shape changed'
assert shape(s,0x332000,0x33206d)==shape(g,0x331b90,0x331bfd)
assert shape(s,0x60df70,0x60e628)==shape(g,0x60e280,0x60e938)
print('PASS actual EXE code, RTTI,109-byte move ctor,1720-byte registration and108 factory cases')
code=r'''
#include "game/gog_jobs_profile.h"
#include <Windows.h>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Image {std::vector<unsigned char>raw;IMAGE_NT_HEADERS64*nt;IMAGE_SECTION_HEADER*sections;uintptr_t base;bool gog,relocate;uintptr_t corrupt,refuse;};
static int checks=0;
static void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
static bool Read(void*c,uintptr_t r,void*out,size_t n){Image*i=(Image*)c;if(r==i->refuse)return false;for(unsigned k=0;k<i->nt->FileHeader.NumberOfSections;++k){IMAGE_SECTION_HEADER&s=i->sections[k];if(r>=s.VirtualAddress&&r-s.VirtualAddress<=s.SizeOfRawData&&n<=s.SizeOfRawData-(r-s.VirtualAddress)){
 size_t at=s.PointerToRawData+r-s.VirtualAddress;if(at>i->raw.size()||n>i->raw.size()-at)return false;memcpy(out,&i->raw[at],n);
 if(i->relocate&&n==8){for(int t=JobsProfile::EMPTY_VTABLE;t<=JobsProfile::BLANK_VTABLE;++t){uintptr_t a=JobsProfile::Rva(i->gog,(JobsProfile::Key)t)-8;if(r>=a&&r<a+JobsProfile::TableCount((JobsProfile::Key)t)*8)*(uintptr_t*)out+=i->base-i->nt->OptionalHeader.ImageBase;}}
 if(r==i->corrupt)((unsigned char*)out)[0]^=1;return true;}}return false;}
static void Run(const char*file,bool gog){Image i={};i.gog=gog;std::ifstream f(file,std::ios::binary);i.raw.assign(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());C(i.raw.size()>1000000,"actual fixture file");i.nt=(IMAGE_NT_HEADERS64*)(&i.raw[0]+((IMAGE_DOS_HEADER*)&i.raw[0])->e_lfanew);i.sections=IMAGE_FIRST_SECTION(i.nt);i.base=i.nt->OptionalHeader.ImageBase;
 C(JobsProfile::Validate(gog,i.base,Read,&i),"all actual windows/vtables accepted");C(JobsProfile::ValidateForInstall(gog,i.base,Read,&i),"actual install contract accepted");
 C(!JobsProfile::Validate(gog,0,Read,&i)&&!JobsProfile::Validate(gog,i.base,0,&i),"invalid input refused");
 for(int k=0;k<JobsProfile::EMPTY_VTABLE;++k){i.corrupt=JobsProfile::Rva(gog,(JobsProfile::Key)k);C(!JobsProfile::Validate(gog,i.base,Read,&i),"every changed actual code window refused");C(JobsProfile::ValidateForInstall(gog,i.base,Read,&i)==(k==JobsProfile::CAPTION||k==JobsProfile::CONTEXT_SHOW),"install ignores only our prior caption/context detours");i.corrupt=0;i.refuse=JobsProfile::Rva(gog,(JobsProfile::Key)k);C(!JobsProfile::Validate(gog,i.base,Read,&i),"reader failure refused");i.refuse=0;}
 for(int k=JobsProfile::EMPTY_VTABLE;k<=JobsProfile::BLANK_VTABLE;++k)for(unsigned j=0;j<JobsProfile::TableCount((JobsProfile::Key)k);++j){i.corrupt=JobsProfile::Rva(gog,(JobsProfile::Key)k)-8+j*8;C(!JobsProfile::Validate(gog,i.base,Read,&i)&&!JobsProfile::ValidateForInstall(gog,i.base,Read,&i),"every RTTI/deleting/body/finish vtable slot changed is refused");i.corrupt=0;}
 C(!JobsProfile::Rva(gog,(JobsProfile::Key)-1)&&!JobsProfile::Rva(gog,JobsProfile::KEY_COUNT),"unknown key no fallback");
 i.base+=0x20000000;i.relocate=true;C(JobsProfile::Validate(gog,i.base,Read,&i),"real ASLR relocation normalized");i.relocate=false;C(!JobsProfile::Validate(gog,i.base,Read,&i),"wrong base refused");
}
int main(int argc,char**argv){C(argc==3,"two read-only fixtures");Run(argv[1],false);Run(argv[2],true);printf("PASS %d production native-profile checks\n",checks);}
'''
(O/'profile.cpp').write_text(code,encoding='utf8');V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');S=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{V}/include;{S}/Include',LIB=f'{V}/lib/amd64;{S}/Lib/x64')
cmd=[str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2','/MD','/I'+str(R/'src'),str(O/'profile.cpp'),str(R/'src/game/gog_jobs_profile.cpp'),'/Fo'+str(O)+'/', '/Fe'+str(O/'profile.exe')]
q=subprocess.run(cmd,env=env,capture_output=True,text=True);(O/'build.log').write_text(q.stdout+q.stderr,encoding='utf8');assert not q.returncode,q.stdout+q.stderr
q=subprocess.run([str(O/'profile.exe'),str(a.steam_exe.resolve()),str(a.gog_exe.resolve())],capture_output=True,text=True);print(q.stdout);(O/'run.log').write_text(q.stdout+q.stderr,encoding='utf8');assert not q.returncode,q.stdout+q.stderr
