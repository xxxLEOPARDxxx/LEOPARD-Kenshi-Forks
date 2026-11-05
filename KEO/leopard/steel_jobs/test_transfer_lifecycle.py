"""Actual module fault/quarantine/reset code over synchronous owned native boundaries."""
from pathlib import Path
import os,subprocess,runpy
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/'leopard/steel_jobs/test_transfer.py'),run_name='transfer_core_verification')
out=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-steel-jobs/transfer-tests')
s=(root/'src/jobs/steel_jobs_transfer.cpp').read_text(encoding='utf-8-sig')
def function(marker):
 a=s.index(marker);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""#include <windows.h>
#define main TransferCoreFixtureMain
#include "fixture.cpp"
#undef main
namespace keo_jobs {
static volatile LONG s_transferFault=0,s_transferBusy=0,s_resetPending=0;
static void*s_quarantined=0;static uint64_t s_quarantineEpoch=0;
static F native;static uint64_t epoch=41;static bool duringReset=false,throws=false;
static bool CaptureSafe(void*,void*a,void*b,void*i,StockTransferSnapshot*s){return Snap(&native,a,b,i,s);}
static bool NativeValid(void*,const StockTransferSnapshot&s){return native.live&&s.epoch==epoch;}
static void*DetachSafe(void*,void*a,void*i,int n){return Detach(&native,a,i,n);}
static int OfferSafe(void*,void*a,void*i){if(throws)throw 1;if(duringReset){++epoch;ResetStockTransfer();}return Offer(&native,a,i);}
static bool RestoreSafe(void*,void*a,void*i,const StockTransferSnapshot&s){return Restore(&native,a,i,s);}
static bool MeasureSafe(void*,void*a,void*b,const StockTransferSnapshot&s,double*x,double*y){return Measure(&native,a,b,s,x,y);}
RESET_TICKET
TRANSFER
RESET
}
int main(){using namespace keo_jobs;native=Fresh();native.move=0;native.offerMode=0;native.restores=false;
 StockTransferResult r=TransferStockItem((void*)1,(void*)2,(void*)3);C(r.fault&&s_transferFault&&s_quarantined==(void*)3&&s_quarantineEpoch==41,"module retains ONE exact opaque native ticket");int offers=native.offerCalls;
 for(int i=0;i<100;++i)TransferStockItem((void*)1,(void*)2,(void*)4);C(native.offerCalls==offers&&s_quarantined==(void*)3,"sticky quarantine never grows/retries/drop original");
 int restores=native.restoreCalls;ResetStockTransfer();C(!s_quarantined&&!s_quarantineEpoch&&!s_transferFault&&native.restoreCalls==restores,"world reset discards old opaque ticket without native dereference");
 native=Fresh();s_transferBusy=1;r=TransferStockItem((void*)1,(void*)2,(void*)3);C(!r.fault&&!native.detachCalls,"nonblocking concurrent attempt unchanged");s_transferBusy=0;
 native=Fresh();throws=true;r=TransferStockItem((void*)1,(void*)2,(void*)3);throws=false;C(r.fault&&native.restoreCalls==1&&!s_transferBusy,"native C++ fault restores remaining original and releases transaction scope");ResetStockTransfer();
 native=Fresh();native.offerMode=0;duringReset=true;r=TransferStockItem((void*)1,(void*)2,(void*)3);duringReset=false;C(r.fault&&!s_transferBusy&&!s_quarantined&&!s_transferFault&&native.restoreCalls==0,"reset during native callback invalidates old world; no stale restore/dereference");
 printf("PASS %d production quarantine/lifecycle controls\n",checks);}
"""
for marker,text in [('RESET_TICKET',function('static void ResetTransferTicket()')),('TRANSFER',function('StockTransferResult TransferStockItem(void*src')),('RESET',function('void ResetStockTransfer()'))]:code=code.replace(marker,text)
(out/'lifecycle.cpp').write_text(code,encoding='utf-8')
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{root}/src;{vc}/include;{sdk}/Include',LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
for opt in ('Od','O2'):
 exe=out/('lifecycle-'+opt+'.exe');cmd=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(out/'lifecycle.cpp'),str(out/'core.cpp'),'/Fo'+str(out)+'/','/Fe'+str(exe)]
 r=subprocess.run(cmd,env=env,capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(opt,r.stdout.strip());assert r.returncode==0,r.stdout+r.stderr
