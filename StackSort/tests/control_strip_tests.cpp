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
    // Screenshot 2026-10-30 14:34: backpack outer~390px; KEO checkbox
    // starts at local271. Expanding controls to fill269px forced x=0.
    // Keep full translated words at their measured minimum and permit x=21.
    r=Request(390);r.anchor=ControlStrip::Rect(61,60,206,24);r.preferredX=21;
    blocks.clear();blocks.push_back(ControlStrip::Rect(271,60,24,24));
    p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
    CHECK(p.caption==0);CHECK(p.width[1]==r.captionWidth[0]);CHECK(p.total==212);CHECK(p.x==21);
    // A roomy row does not compact or change configured control widths.
    blocks[0].x=350;p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
    CHECK(p.caption==0&&p.total==286&&p.width[1]==180&&p.x==21);
    // Body row from the same screenshot: armor occupies the left side and
    // absorbs a mere preferredX +5 unless a small post-clamp inset is kept.
    r=Request(650);r.anchor=ControlStrip::Rect(420,60,160,24);r.preferredX=317;
    blocks.clear();blocks.push_back(ControlStrip::Rect(216,60,134,24));
    p=ControlStrip::Build(r,blocks);const int oldBodyX=p.x;
    r.preferredX+=5;r.leftClampInset=5;p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
    CHECK(p.x==oldBodyX+5);
    r.parent.w=p.x+p.total-3;p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
    CHECK(p.x>=oldBodyX&&p.x<oldBodyX+5); // inset yields to the right parent edge
    // Backpack: center the fitted whole row (including optional KEO), using
    // measured RU/EN/ZH full-caption widths. Body's legacy branch stays above.
    const int fullWidths[]={146,99,95};
    for(int lang=0;lang<3;++lang)for(int keo=0;keo<2;++keo)
    {
        r=Request(390);r.anchor=ControlStrip::Rect(20,60,350,24); // grid, not old Arrange(61,206)
        r.centerOnAnchor=true;r.trailingWidth=keo?24:0;r.captionWidth[0]=fullWidths[lang];blocks.clear();
        p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);
        CHECK(p.caption==0);CHECK(abs(2*p.x+p.total-(2*r.anchor.x+r.anchor.w))<=1);
        CHECK(p.total==286+(keo?26:0));
        blocks.push_back(ControlStrip::Rect(0,60,30,24));p=ControlStrip::Build(r,blocks);Valid(r,blocks,p);CHECK(p.x>=32);
    }
    r=Request(80);r.centerOnAnchor=true;r.trailingWidth=24;blocks.clear();CHECK(!ControlStrip::Build(r,blocks).fits);
    std::printf("%d control-strip checks, %d failures\n",checks,failures);return failures?1:0;
}
