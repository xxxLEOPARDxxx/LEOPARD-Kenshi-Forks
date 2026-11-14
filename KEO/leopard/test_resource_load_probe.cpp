#define RESOURCE_LOAD_PROBE_TEST
#include <windows.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
static bool enabled=true,onMain=true,throwsNative=false,sehNative=false;
static int clocks=0,mainChecks=0,nativeCalls=0;static void*seen=NULL;
static LONGLONG tick=0,nativeDuration=3000;static double seconds=0;
static std::string output;
bool DetailedLogEnabled(){return enabled;}
bool IsMainThread(){++mainChecks;return onMain;}
LONGLONG QpcNow(){++clocks;return tick;}
long QpcDeltaUs(LONGLONG a,LONGLONG b){return b<=a?0:(long)(b-a);}
double ElapsedSec(){return seconds;}
void LogMsg(const std::string&s){output+=s;output+='\n';}
void Native(void*p){++nativeCalls;seen=p;tick+=nativeDuration;if(sehNative)RaiseException(0xE0426E01,0,0,NULL);if(throwsNative)throw 17;}
#include "../src/resources/resource_load_probe.cpp"
static bool CatchNativeSeh(void*p){__try{resource_load_probe_detail::Factory(p);}__except(GetExceptionCode()==0xE0426E01?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}return false;}
static int checks=0;
void Check(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
int main(){
 using namespace resource_load_probe_detail;
 originalFactory=&Native;originalResource=&Native;
 void*self=reinterpret_cast<void*>(0x1234);
 Factory(self);Check(nativeCalls==1&&seen==self,"factory forwards once and exact self");
 Check(factory.calls==1&&factory.totalUs==3000&&factory.maxUs==3000&&factory.over2ms==1&&factory.over20ms==0,"factory elapsed accounting");
 nativeDuration=25000;Resource(self);Check(nativeCalls==2&&resource.calls==1&&resource.totalUs==25000&&resource.over20ms==1,"resource native call and independent accounting");
 nativeDuration=1000;Factory(self);Check(factory.calls==2&&factory.totalUs==4000&&factory.maxUs==3000&&factory.over2ms==1,"sum max and thresholds aggregate");
 enabled=false;int c=clocks,m=mainChecks;Factory(self);Check(nativeCalls==4&&clocks==c&&mainChecks==m&&factory.calls==2,"disabled forwards with no timing or thread probe");
 enabled=true;onMain=false;Resource(self);Check(nativeCalls==5&&clocks==c&&resource.calls==1,"off main forwards with no timing/stat mutation");onMain=true;
 throwsNative=true;bool thrown=false;c=clocks;try{Factory(self);}catch(int n){thrown=n==17;}Check(thrown&&nativeCalls==6&&clocks==c+1&&factory.calls==2,"native exception propagates once without completed sample");throwsNative=false;
 nativeDuration=-100;Factory(self);Check(factory.calls==3&&factory.totalUs==4000&&factory.maxUs==3000,"negative duration clamps");
 nativeDuration=2000;Factory(self);Check(factory.over2ms==1,"over2ms strict boundary");nativeDuration=20000;Resource(self);Check(resource.over20ms==1&&resource.over2ms==2,"over20ms strict boundary");
 factory.calls=~0ULL;factory.totalUs=~0ULL-100;factory.over2ms=~0ULL;factory.over20ms=~0ULL;nativeDuration=25000;Factory(self);Check(factory.calls==~0ULL&&factory.totalUs==~0ULL&&factory.over2ms==~0ULL&&factory.over20ms==~0ULL,"cumulative counters saturate");
 sehNative=true;c=clocks;int beforeNative=nativeCalls;unsigned __int64 beforeSamples=factory.calls;Check(CatchNativeSeh(self)&&nativeCalls==beforeNative+1&&clocks==c+1&&factory.calls==beforeSamples,"native SEH propagates to caller without completed sample");sehNative=false;
 seconds=0;ResourceLoadProbeReport();Check(output.empty(),"first report establishes cadence");seconds=9.99;ResourceLoadProbeReport();Check(output.empty(),"report waits ten seconds");seconds=10;ResourceLoadProbeReport();Check(output.find("factoryDrain")!=std::string::npos&&output.find("resourceFinish")!=std::string::npos&&output.find("inclusiveElapsed")!=std::string::npos,"summary labels preserve attribution scope");
 size_t size=output.size();seconds=19.9;ResourceLoadProbeReport();Check(output.size()==size,"report rate bounded");enabled=false;seconds=30;ResourceLoadProbeReport();Check(output.size()==size,"debug off report silent");
 printf("PASS %d actual-production-wrapper controls\n",checks);return 0;
}
