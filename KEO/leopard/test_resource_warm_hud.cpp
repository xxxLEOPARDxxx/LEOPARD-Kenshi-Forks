// Game-free tests exercise the actual formatting, sampling and layout policy.
#include "resources/resource_warm_hud_policy.h"
#include <cstdio>
#include <cstring>
static unsigned checks=0;
#define C(x) do { ++checks; if (!(x)) { std::printf("FAIL %u: %s\n",__LINE__,#x);return 1; } } while(0)
const char* Identity(const char* s){return s;}
int main()
{
    using namespace resource_warm_hud;
    Snapshot s={};s.ramOn=true;s.gpuOn=true;s.ramBytes=4ULL<<30;s.ramCap=8ULL<<30;
    s.ramFiles=1250;s.ramPending=42;s.gpuBytes=256ULL<<20;s.gpuCap=1ULL<<30;
    s.gpuReady=20;s.gpuPending=5;s.gpuCreated=33;s.gpuAdopted=8;s.gpuSkipUploads=567;
    Sampler sampler;C(sampler.Observe(100,s.ramReadBytes)==0);
    s.ramReadBytes=512ULL<<20;C(sampler.Observe(600,s.ramReadBytes)==1024);
    Lines a=Format(s,1024,Identity);C(a.text[0].find("4.0/8.0 GiB")!=std::string::npos);
    C(a.text[0].find("1024 MiB/s")!=std::string::npos);C(a.text[0].find("42 remaining")!=std::string::npos);
    C(a.text[1].find("256/1024 MiB")!=std::string::npos);C(a.text[1].find("20 ready")!=std::string::npos);
    C(a.text[2].find("8 used")!=std::string::npos);C(a.text[2].find("567 uploads skipped")!=std::string::npos);
    C(a.text[0].find('%')==std::string::npos);C(a.text[2].find("Warming")!=std::string::npos);
    s.workPaused=true;C(Format(s,0,Identity).text[2].find("Paused")!=std::string::npos);
    s.workPaused=false;s.ramPending=s.gpuPending=0;C(Format(s,0,Identity).text[2].find("Ready")!=std::string::npos);
    s.ramActive=true;C(Format(s,0,Identity).text[2].find("Warming")!=std::string::npos);
    s.ramOn=s.gpuOn=false;a=Format(s,0,Identity);C(a.text[0]=="RAM warm: Off");C(a.text[1]=="VRAM standby: Off");C(a.text[2]=="Off");
    C(sampler.Observe(850,0)==0);C(sampler.Observe(1100,1ULL<<20)==4);
    sampler.Reset();C(sampler.Observe(0xfffffff0u,0)==0);C(sampler.Observe(0xf0u,1ULL<<20)==3.90625);
    C(sampler.Observe(0xf0u,2ULL<<20)==0); // no divide by zero
    Layout l=Place(1920,1080,680,24);C(l.left==16&&l.top==96&&l.width==716&&l.height==100);
    l=Place(320,180,680,24);C(l.left>=0&&l.top>=0&&l.left+l.width<=320&&l.top+l.height<=180);
    l=Place(1,1,680,24);C(l.width<=1&&l.height<=1&&l.left==0&&l.top==0);
    C(!Due(1100,1000,true));C(Due(1250,1000,true));C(Due(1,0,false));C(Due(0xf0,0xfffffff0u,true));
    std::printf("Resource warm HUD actual policy %u checks PASS\n",checks);return 0;
}
