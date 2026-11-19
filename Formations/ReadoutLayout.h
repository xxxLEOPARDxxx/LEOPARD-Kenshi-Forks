#pragma once

namespace readout
{
    struct Rect {int x,y,width,height;};
    struct Size {int width,height;};

    constexpr int Padding=12;
    constexpr int MinWidth=128;

    inline bool Place(int nameLeft,int nameTop,int bountyTop,int textWidth,int textHeight,Rect& out)
    {
        if(nameLeft<-4096||nameTop<=0||nameTop>16384||
           textWidth<=0||textWidth>4096||textHeight<=0||textHeight>4096)return false;
        const int anchorTop=bountyTop>=0&&bountyTop<nameTop?bountyTop:nameTop;
        const int height=textHeight+2*Padding;
        if(height>anchorTop)return false;
        const int width=textWidth+2*Padding;
        out={nameLeft<0?0:nameLeft,anchorTop-height,width<MinWidth?MinWidth:width,height};
        return true;
    }
}
