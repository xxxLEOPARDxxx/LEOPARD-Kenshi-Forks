"""Bounded production carried-weight policy over owned native facts; no game calls."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/carried-weight-tests';OUT.mkdir(parents=True,exist_ok=True)
header=ROOT/'src/inventory/carried_weight_policy.h'
api=r'''#include <stdint.h>
namespace keo_inventory {struct WeightBody{float x,y,z,scale,height,frame,hunger;int robot;};struct WeightOps{void*ctx;float(*nativeWeight)(void*,uintptr_t);uintptr_t(*owner)(void*,uintptr_t);int(*count)(void*,uintptr_t);uintptr_t(*child)(void*,uintptr_t,int);uintptr_t(*carried)(void*,uintptr_t);uintptr_t(*inventory)(void*,uintptr_t);bool(*body)(void*,uintptr_t,WeightBody*);bool(*nativeBody)(void*,uintptr_t,uintptr_t,float*);};float CarriedWeightEvaluate(const WeightOps&,uintptr_t,float);float CarriedBodyFactor(const WeightBody&);}
'''
code=r'''
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <new>
API
using namespace keo_inventory;
static volatile LONG checks=0,allocs=0;
void*operator new(size_t n){InterlockedIncrement(&allocs);void*p=std::malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){std::free(p);}void*operator new[](size_t n){return ::operator new(n);}void operator delete[](void*p){::operator delete(p);}
void C(bool b,const char*s){InterlockedIncrement(&checks);if(!b){printf("FAIL %s\n",s);std::exit(1);}}
bool Near(float a,float b){return std::fabs(a-b)<0.002f;}
struct Inv{float base;uintptr_t who;int n;uintptr_t children[70];};struct Actor{uintptr_t held,inv;WeightBody body;};struct Fixture{Inv invs[90];Actor actors[90];int originals;bool readable;};
WeightBody Human(){WeightBody b={5,17,5,1,100,100,3,0};return b;}
float Native(void*c,uintptr_t i){Fixture*f=(Fixture*)c;++f->originals;return f->invs[i].base;}
uintptr_t Owner(void*c,uintptr_t i){return ((Fixture*)c)->invs[i].who;}
int Count(void*c,uintptr_t i){return ((Fixture*)c)->invs[i].n;}
uintptr_t Child(void*c,uintptr_t i,int k){return ((Fixture*)c)->invs[i].children[k];}
uintptr_t Carried(void*c,uintptr_t a){return ((Fixture*)c)->actors[a].held;}
uintptr_t Inventory(void*c,uintptr_t a){return ((Fixture*)c)->actors[a].inv;}
bool Body(void*c,uintptr_t a,WeightBody*b){*b=((Fixture*)c)->actors[a].body;return true;}
bool NativeBody(void*c,uintptr_t,uintptr_t,float*b){Fixture*f=(Fixture*)c;*b=30;return f->readable;}
WeightOps Ops(Fixture*f){WeightOps o={f,Native,Owner,Count,Child,Carried,Inventory,Body,NativeBody};return o;}
void Fresh(Fixture&f){std::memset(&f,0,sizeof(f));f.readable=true;for(int i=0;i<90;++i)f.actors[i].body=Human();f.invs[1].base=20;f.invs[1].who=1;f.actors[1].inv=1;}
void Rows(){Fixture f;Fresh(f);
 f.invs[1].base=0;f.actors[1].held=2;f.actors[2].inv=2;f.actors[2].body.hunger=0;
 C(Near(CarriedWeightEvaluate(Ops(&f),1,0),-9),"empty carrier inventory must preserve signed -9 body delta; native outer+30 gives21");
 f.invs[1].base=2;C(Near(CarriedWeightEvaluate(Ops(&f),1,2),-7),"low inventory preserves signed body delta without arbitrary clamp");
 Fresh(f);f.actors[1].held=2;f.actors[2].inv=2;f.invs[2].who=2;f.invs[2].base=3;f.actors[2].held=3;f.actors[3].inv=3;f.invs[3].who=3;f.invs[3].base=5;
 C(Near(CarriedWeightEvaluate(Ops(&f),1,20),58),"A carrying B carrying C includes C body30 once, B body stays native outer30");
 f.actors[3].body.hunger=0;C(Near(CarriedWeightEvaluate(Ops(&f),1,20),49),"nested carried child's hunger scales its full body, not only an unpaired delta");
 Fresh(f);f.invs[1].n=1;f.invs[1].children[0]=2;f.invs[2].base=10;f.actors[1].held=2;f.actors[2].inv=3;f.invs[3].base=30;
 C(Near(CarriedWeightEvaluate(Ops(&f),1,20),60),"missing loose contents and carried inventory count once; native body30 is not added twice");
 f.actors[2].body.hunger=0;C(Near(CarriedWeightEvaluate(Ops(&f),1,20),51),"starved body uses0.7 native body delta, inventory unchanged");f.actors[2].body=Human();f.actors[2].body.robot=1;f.actors[2].body.hunger=0;C(Near(CarriedWeightEvaluate(Ops(&f),1,20),66),"robot density1.2 ignores hunger");
 WeightBody b=Human();b.x*=4;C(Near(CarriedBodyFactor(b),2),"race hull sqrt(volume ratio)");b=Human();b.scale=2;C(Near(CarriedBodyFactor(b),std::sqrt(8.0f)),"animal model scale cubed before sqrt");b=Human();b.height=200;b.frame=400;C(Near(CarriedBodyFactor(b),std::sqrt(32.0f)),"Height cubed and Frame linear");b=Human();b.hunger=-10;C(Near(CarriedBodyFactor(b),.7f),"hunger low clamp");b.hunger=30;C(Near(CarriedBodyFactor(b),1),"hunger high clamp");b.x=-1;C(Near(CarriedBodyFactor(b),1),"invalid hull keeps neutral volume");b=Human();b.height=0;C(Near(CarriedBodyFactor(b),1),"missing slider staysneutral");unsigned nanBits=0x7fc00000;float nan;std::memcpy(&nan,&nanBits,4);b=Human();b.scale=nan;C(Near(CarriedBodyFactor(b),1),"NaN scale never poisons weight");b.hunger=nan;C(Near(CarriedBodyFactor(b),1),"NaN hunger neutral");
 Fresh(f);C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"empty noncarrier preserves exact native");f.invs[1].n=2;f.invs[1].children[0]=f.invs[1].children[1]=2;f.invs[2].base=10;C(CarriedWeightEvaluate(Ops(&f),1,20)==30&&f.originals==1,"duplicate looseinventory reference is counted once");f.invs[2].n=1;f.invs[2].children[0]=1;C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"inventory cycle refuses augmentation");Fresh(f);f.invs[1].n=5000;C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"item scan budget fails closed");Fresh(f);for(int i=1;i<15;++i){f.invs[i].n=1;f.invs[i].children[0]=i+1;f.invs[i+1].base=1;}C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"depth budget refuses without unbounded recursion");
 Fresh(f);f.actors[1].held=2;f.actors[2].inv=2;f.invs[2].base=15;f.readable=false;C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"unverified native body capture refuses whole augmentation");f.readable=true;f.invs[2].base=nan;C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"nonfinite child weight preserves baseline");f.invs[2].base=-1;C(CarriedWeightEvaluate(Ops(&f),1,20)==20,"negative native child refuses");
 Fresh(f);f.invs[1].n=1;f.invs[1].children[0]=2;f.invs[2].base=10;LONG before=allocs;for(int i=0;i<100;++i)C(CarriedWeightEvaluate(Ops(&f),1,20)==30,"repeat computes current native facts");C(allocs==before,"no hot-path heap allocation");}
int main(){Rows();printf("PASS %ld production carried weight controls\n",checks);return 0;}
'''
code=code.replace('API','#include "inventory/carried_weight_policy.h"' if header.exists() else api+'\nnamespace keo_inventory {float CarriedWeightEvaluate(const WeightOps&,uintptr_t,float native){return native;}float CarriedBodyFactor(const WeightBody&){return 1;}}')
cpp=OUT/'policy.cpp';cpp.write_text(code)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{ROOT}/src;{VC}/include;{SDK}/Include',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
for opt in ['Od','O2']:
 cmd=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(cpp)]
 if header.exists():cmd.append(str(ROOT/'src/inventory/carried_weight_policy.cpp'))
 exe=OUT/('policy-'+opt+'.exe');cmd+=['/Fo'+str(OUT)+'/', '/Fe'+str(exe)]
 r=subprocess.run(cmd,env=env,capture_output=True,text=True);(OUT/('build-'+opt+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,r.stdout.strip());(OUT/('run-'+opt+'.log')).write_text(r.stdout+r.stderr)
 if '--expect-red' in sys.argv:assert r.returncode==1 and ('missing loose contents' in r.stdout or 'empty carrier inventory' in r.stdout)
 else:assert r.returncode==0,r.stdout+r.stderr
