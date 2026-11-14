#pragma once
#include "resources/resource_warm_hud.h"
#include <string>
#include <cstdio>
namespace resource_warm_hud {
enum { HorizontalPad=18, VerticalPad=14 };
typedef const char* (*Translate)(const char*);
struct Lines { std::string text[3]; };
inline std::string Count(unsigned __int64 n) { char b[32];sprintf_s(b,"%I64u",n);return b; }
inline Lines Format(const Snapshot& s,double rate,Translate tr)
{
    Lines result;char b[96];
    result.text[0]=std::string(tr("RAM warm"))+": ";
    if (s.ramOn) {
        sprintf_s(b,"%.1f/%.1f GiB | %.0f MiB/s | ",double(s.ramBytes)/1073741824.,double(s.ramCap)/1073741824.,rate);
        result.text[0]+=b;result.text[0]+=Count(s.ramPending)+" "+tr("remaining")+" | "+Count(s.ramFiles)+" "+tr("files");
    } else result.text[0]+=tr("Off");
    result.text[1]=std::string(tr("VRAM standby"))+": ";
    if (s.gpuOn) {
        sprintf_s(b,"%.0f/%.0f MiB | ",double(s.gpuBytes)/1048576.,double(s.gpuCap)/1048576.);
        result.text[1]+=b;result.text[1]+=Count(s.gpuReady)+" "+tr("ready")+" | "+Count(s.gpuPending)+" "+tr("remaining");
    } else result.text[1]+=tr("Off");
    const bool busy=(s.ramOn&&(s.ramPending||s.ramActive))||(s.gpuOn&&s.gpuPending);
    result.text[2]=tr(!s.ramOn&&!s.gpuOn?"Off":busy?(s.workPaused?"Paused":"Warming"):"Ready");
    if (s.gpuOn) result.text[2]+=" | "+Count(s.gpuCreated)+" "+tr("created")+" | "+Count(s.gpuAdopted)+" "+tr("used")+" | "+Count(s.gpuSkipUploads)+" "+tr("uploads skipped");
    return result;
}
struct Sampler {
    unsigned at;unsigned __int64 bytes;bool sampled;
    Sampler():at(0),bytes(0),sampled(false){}
    void Reset() {at=0;bytes=0;sampled=false;}
    double Observe(unsigned now,unsigned __int64 total) {
        const unsigned elapsed=now-at;
        const double rate=sampled&&elapsed&&total>=bytes?double(total-bytes)*1000./double(elapsed)/1048576.:0.;
        at=now;bytes=total;sampled=true;return rate;
    }
};
struct Layout { int left,top,width,height; };
inline int Minimum(int a,int b) {return a<b?a:b;}
inline int Maximum(int a,int b) {return a>b?a:b;}
inline Layout Place(int viewWidth,int viewHeight,int textWidth,int lineHeight) {
    Layout r={};viewWidth=Maximum(0,viewWidth);viewHeight=Maximum(0,viewHeight);
    r.width=Minimum(viewWidth,Maximum(0,textWidth)+2*HorizontalPad);
    r.height=Minimum(viewHeight,Maximum(0,lineHeight)*3+2*VerticalPad);
    r.left=Minimum(16,viewWidth-r.width);r.top=Minimum(96,viewHeight-r.height);return r;
}
inline bool Due(unsigned now,unsigned at,bool sampled) { return !sampled||unsigned(now-at)>=250; }
}
