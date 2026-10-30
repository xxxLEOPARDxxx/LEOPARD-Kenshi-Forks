#pragma once
#include <algorithm>
#include <climits>
#include <vector>
namespace ControlStrip
{
struct Rect { int x,y,w,h; Rect(int X=0,int Y=0,int W=0,int H=0):x(X),y(Y),w(W),h(H){} };
struct Request
{
    Rect parent, anchor;
    int preferredX, gap;
    int desired[4]; // back, sort, direction, forward
    int minimum[4];
    int captionWidth[3]; // full, compact, numeric; including skin padding
    int leftClampInset;
    bool centerOnAnchor;
    int trailingWidth; // optional cooperating checkbox; zero for our four controls
    Request():leftClampInset(0),centerOnAnchor(false),trailingWidth(0){}
};
struct Plan
{
    bool fits;
    int x,y,width[4],caption,total;
    Plan():fits(false),x(0),y(0),caption(0),total(0) { for(int i=0;i<4;++i) width[i]=0; }
};
struct Interval { int left,right; Interval(int l,int r):left(l),right(r){} };
inline Plan Build(const Request& req,const std::vector<Rect>& obstacles)
{
    Plan best;
    if(req.parent.w<=0 || req.parent.h<=0 || req.anchor.h<=0 || req.anchor.y<0 ||
       req.anchor.y>req.parent.h-req.anchor.h || req.gap<0 || req.gap>50 ||
       req.trailingWidth<0 || req.trailingWidth>10000 || obstacles.size()>128) return best;
    const int spacing=(req.trailingWidth?4:3)*req.gap+req.trailingWidth;
    std::vector<Interval> free;
    free.push_back(Interval(0,req.parent.w));
    for(size_t b=0;b<obstacles.size();++b)
    {
        const Rect& r=obstacles[b];
        if(r.w<=0 || r.h<=0 || (long long)r.y+r.h<=req.anchor.y ||
           r.y>=(long long)req.anchor.y+req.anchor.h) continue;
        const int left=(int)(std::max)(0LL,(long long)r.x-req.gap);
        const int right=(int)(std::min)((long long)req.parent.w,(long long)r.x+r.w+req.gap);
        if(left>=right) continue;
        for(size_t i=0;i<free.size();)
        {
            const Interval part=free[i];
            if(right<=part.left || left>=part.right) { ++i;continue; }
            free.erase(free.begin()+i);
            if(part.left<left) { free.insert(free.begin()+i,Interval(part.left,left));++i; }
            if(right<part.right) { free.insert(free.begin()+i,Interval(right,part.right));++i; }
        }
    }
    for(int mode=0;mode<3;++mode)
    {
        long long closest=LLONG_MAX;
        for(size_t i=0;i<free.size();++i)
        {
            int widths[4];long long minimum=spacing;
            for(int k=0;k<4;++k)
            {
                widths[k]=k==1?req.captionWidth[mode]:req.minimum[k];
                if(widths[k]<1 || widths[k]>10000) return Plan();
                minimum+=widths[k];
            }
            const int available=free[i].right-free[i].left;
            if(minimum>available) continue;
            long long desired=spacing;
            for(int k=0;k<4;++k) desired+=(std::max)(widths[k],req.desired[k]);
            // A right-side neighbor such as KEO's checkbox must not make us
            // fill its entire left interval and pin the toolbar to the panel
            // edge. Keep readable measured minima so preferredX can survive.
            const bool rightNeighborClamp=!req.centerOnAnchor && free[i].right<req.parent.w &&
                (long long)req.preferredX+desired>free[i].right;
            int extra=available-(int)minimum;
            const int priority[4]={1,2,0,3};
            for(int p=0;p<4 && !rightNeighborClamp;++p)
            {
                int k=priority[p];
                int add=(std::min)(extra,(std::max)(0,req.desired[k]-widths[k]));
                widths[k]+=add;extra-=add;
            }
            int total=spacing;for(int k=0;k<4;++k) total+=widths[k];
            // Backpack centering uses the actual fitted width, including KEO,
            // rather than the configured width of just our four controls.
            const int preferred=req.centerOnAnchor ? req.anchor.x+(req.anchor.w-total)/2 : req.preferredX;
            const int spare=available-total;
            const int inset=preferred<free[i].left ?
                (std::min)(spare,(std::max)(0,(std::min)(50,req.leftClampInset))) : 0;
            const int x=(std::max)(free[i].left+inset,(std::min)(preferred,free[i].right-total));
            const long long delta=(long long)x-preferred;
            const long long distance=delta<0?-delta:delta;
            if(distance>=closest) continue;
            closest=distance;best.fits=true;best.x=x;best.y=req.anchor.y;best.caption=mode;best.total=total;
            for(int k=0;k<4;++k) best.width[k]=widths[k];
        }
        if(best.fits) return best; // keep complete words whenever they fit
    }
    return best;
}
}
