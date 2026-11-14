#pragma once
#include <string>
#include <sstream>
namespace resource_forward {
struct Cells {int x[12],y[12];unsigned count;Cells():count(0){}};
inline void Add(Cells&c,int x,int y){
    if(x<0||x>63||y<0||y>63||c.count>=12)return;
    for(unsigned i=0;i<c.count;++i)if(c.x[i]==x&&c.y[i]==y)return;
    c.x[c.count]=x;c.y[c.count++]=y;
}
inline void Candidates(int x,int y,int destX,int destY,bool moving,Cells&c){
    c.count=0;if(x<0||x>63||y<0||y>63)return;
    if(moving){int dx=(destX>x)-(destX<x),dy=(destY>y)-(destY<y);
        Add(c,x+2*dx,y+2*dy);Add(c,x+2*dx+dy,y+2*dy-dx);Add(c,x+2*dx-dy,y+2*dy+dx);
    }
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx||dy)Add(c,x+dx,y+dy);
}
inline bool CanWarm(double now,double lastTick,double frameSeconds,double blockedUntil){
    return now>=blockedUntil && now-lastTick>=0.05 && frameSeconds>=0 && frameSeconds<0.020;
}
inline std::string TerrainKey(const std::string&sid,int type,const std::string paths[6]){
    unsigned __int64 hash=14695981039346656037ULL;
    for(unsigned i=0;i<6;++i){for(size_t j=0;j<paths[i].size();++j){hash^=static_cast<unsigned char>(paths[i][j]);hash*=1099511628211ULL;}hash^=0;hash*=1099511628211ULL;}
    std::ostringstream out;out<<"Terrain"<<type<<"_"<<sid<<"|"<<std::hex<<hash;return out.str();
}
}
