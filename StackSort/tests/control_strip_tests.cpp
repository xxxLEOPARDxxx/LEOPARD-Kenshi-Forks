#include <cstdio>
#include "ControlStripPlan.h"
static int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL strip %d: %s\n",__LINE__,#x);}}while(0)
static ControlStrip::Request Request(int width)
{
    ControlStrip::Request r;r.parent=ControlStrip::Rect(0,0,width,100);r.anchor=ControlStrip::Rect(width/3,60,width/3,24);r.preferredX=width/2-143;r.gap=2;
    int desired[]={30,180,40,30},minimum[]={18,0,24,18};
    for(int i=0;i<4;++i){r.desired[i]=desired[i];r.minimum[i]=minimum[i];}
    r.captionWidth[0]=146;r.captionWidth[1]=94;r.captionWidth[2]=42;return r;
}
static void Valid(const ControlStrip::Request& r,const std::vector<ControlStrip::Rect>& blocks,const ControlStrip::Plan& p)
{
    CHECK(p.fits);if(!p.fits)return;CHECK(p.x>=0&&p.x+p.total<=r.parent.w);
    CHECK(p.y==r.anchor.y);for(int i=0;i<4;++i)CHECK(p.width[i]>0);
    CHECK(p.width[1]>=r.captionWidth[p.caption]);
    for(size_t i=0;i<blocks.size();++i){const ControlStrip::Rect& b=blocks[i];
        CHECK(b.y+b.h<=p.y||b.y>=p.y+r.anchor.h||b.x+b.w<=p.x||b.x>=p.x+p.total);}
}
int main()
{
    std::vector<ControlStrip::Rect> blocks;
    ControlStrip::Request r=Request(650);ControlStrip::Plan p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.caption==0&&p.total==286&&p.x==r.preferredX);
    blocks.push_back(ControlStrip::Rect(500,60,24,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.caption==0);
    r=Request(260);blocks.clear();blocks.push_back(ControlStrip::Rect(198,60,24,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.caption==1);
    r=Request(180);blocks.clear();blocks.push_back(ControlStrip::Rect(140,60,24,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.caption==2);
    r=Request(320);blocks.clear();blocks.push_back(ControlStrip::Rect(0,60,80,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.x>=82);
    blocks.push_back(ControlStrip::Rect(285,60,35,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
    r=Request(200);r.preferredX=-200;blocks.clear();p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.x==0);
    r.preferredX=500;p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.x+p.total==200);
    r=Request(80);CHECK(!ControlStrip::Build(r,blocks).fits);
    r=Request(320);blocks.push_back(ControlStrip::Rect(0,60,320,24));CHECK(!ControlStrip::Build(r,blocks).fits);
    blocks.clear();blocks.push_back(ControlStrip::Rect(0,0,320,20));Valid(r,blocks,ControlStrip::Build(r,blocks));
    r.anchor.y=90;CHECK(!ControlStrip::Build(r,blocks).fits);
    std::printf("%d control-strip checks, %d failures\n",checks,failures);return failures?1:0;
}
