"""Production transfer orchestration over owned inventories; no game calls."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[2]
out=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-steel-jobs/transfer-tests');out.mkdir(parents=True,exist_ok=True)
source=root/'src/jobs/steel_jobs_transfer.cpp';header=root/'src/jobs/steel_jobs_transfer.h'
api=r"""#include <stdint.h>
namespace keo_jobs {struct StockTransferSnapshot{uintptr_t section,asset;uint64_t epoch;int x,y,quantity;double sourceMeasure,destMeasure,originalMeasure,unit;};struct StockTransferResult{double movedUnits;bool fault;void*quarantineItem;uint64_t epoch;};struct StockTransferOps{void*ctx;bool(*snapshot)(void*,void*,void*,void*,StockTransferSnapshot*);bool(*valid)(void*,const StockTransferSnapshot&);void*(*detach)(void*,void*,void*,int);int(*offer)(void*,void*,void*);bool(*restore)(void*,void*,void*,const StockTransferSnapshot&);bool(*measure)(void*,void*,void*,const StockTransferSnapshot&,double*,double*);};StockTransferResult TransferStockItemWithOps(const StockTransferOps&,void*,void*,void*){StockTransferResult r={0,false,0,0};return r;}}
"""
code=r"""#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
API
using namespace keo_jobs;
static int checks=0,allocs=0;static double measureDrift=0;
void*operator new(size_t n){++allocs;void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){free(p);}
void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
struct F{double src,dst,original,move,unit;bool readable,live,restores,counts,detachOk;int offerMode,detachCalls,offerCalls,restoreCalls;};
bool Snap(void*c,void*,void*,void*,StockTransferSnapshot*s){F&f=*(F*)c;if(!f.readable)return false;s->section=7;s->asset=8;s->epoch=41;s->x=3;s->y=4;s->quantity=10;s->sourceMeasure=f.src;s->destMeasure=f.dst;s->originalMeasure=f.original;s->unit=f.unit;return true;}
bool Valid(void*c,const StockTransferSnapshot&){return ((F*)c)->live;}
void*Detach(void*c,void*,void*i,int){F&f=*(F*)c;++f.detachCalls;if(!f.detachOk)return 0;f.src-=f.original;return i;}
int Offer(void*c,void*,void*){F&f=*(F*)c;++f.offerCalls;f.dst+=f.move;return f.offerMode;}
bool Restore(void*c,void*,void*,const StockTransferSnapshot&s){F&f=*(F*)c;++f.restoreCalls;C(s.x==3&&s.y==4&&s.section==7,"restore exact original section/coordinates");if(!f.restores)return false;f.src+=f.original-f.move;return true;}
bool Measure(void*c,void*,void*,const StockTransferSnapshot&,double*a,double*b){F&f=*(F*)c;*a=f.src;*b=f.dst+measureDrift;return f.counts;}
StockTransferOps Ops(F*f){StockTransferOps o={f,Snap,Valid,Detach,Offer,Restore,Measure};return o;}
F Fresh(){F f={100,20,10,10,1,true,true,true,true,true,1,0,0,0};return f;}
int main(){F f=Fresh();StockTransferResult r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.movedUnits==10&&!r.fault&&!r.quarantineItem,"full native transfer conserves original stack");C(f.detachCalls==1&&f.offerCalls==1&&!f.restoreCalls,"one detach/offer only");
 f=Fresh();f.offerMode=0;f.move=0;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(!r.movedUnits&&!r.fault&&f.src==100&&f.dst==20&&f.restoreCalls==1,"ordinary rejection restores original unchanged");
 f=Fresh();f.offerMode=0;f.move=3;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.movedUnits==3&&!r.fault&&f.src==97&&f.dst==23,"partial false reports progress after exact restore");
 f=Fresh();f.original=50;f.unit=100;f.move=25;f.offerMode=0;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.movedUnits==.25&&!r.fault,"charges preserve fractional units instead of quantity-only false alarm");
 f=Fresh();f.readable=false;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(!r.movedUnits&&!r.fault&&!f.detachCalls,"snapshot refusal unchanged");f=Fresh();f.live=false;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(!f.detachCalls&&!f.offerCalls,"old epoch never detaches");
 f=Fresh();f.detachOk=false;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(!f.offerCalls&&!f.restoreCalls&&!r.movedUnits,"failed native detach never offers");
 f=Fresh();f.move=0;f.offerMode=0;f.restores=false;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.fault&&r.quarantineItem==(void*)3&&r.epoch==41&&f.offerCalls==1,"failed restore retains exact original for epoch quarantine; no retry");
 f=Fresh();f.counts=false;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.fault&&!r.movedUnits,"unverifiable conservation never claims progress");
 f=Fresh();f.src=1000000;f.dst=1000000;measureDrift=1;r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(r.fault,"large balances cannot hide a lost or duplicated complete unit");measureDrift=0;
 int before=allocs;for(int i=0;i<100;++i){f=Fresh();r=TransferStockItemWithOps(Ops(&f),(void*)1,(void*)2,(void*)3);C(!r.fault,"repeated owned transaction");}C(allocs==before,"orchestrator no heap allocation");printf("PASS %d production transfer controls\n",checks);return 0;}
"""
if source.exists():
 (out/'core.cpp').write_text(source.read_text(encoding='utf-8-sig').split('// Native adapter.')[0],encoding='utf-8');api='#include "jobs/steel_jobs_transfer.h"'
code=code.replace('API',api);(out/'fixture.cpp').write_text(code,encoding='utf-8')
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{root}/src;{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
for opt in ('Od','O2'):
 cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(out/'fixture.cpp')]+([str(out/'core.cpp')]if source.exists()else[])+['/Fo'+str(out)+'/','/Fe'+str(out/(opt+'.exe'))]
 r=subprocess.run(cmd,env=env,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(out/(opt+'.exe'))],capture_output=True,text=True);print(opt,r.stdout.strip());assert r.returncode==0,r.stdout+r.stderr
