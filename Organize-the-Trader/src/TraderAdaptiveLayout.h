#pragma once
#include <string>
// Plain pixel arithmetic: measured text/skin sizes enter once on invalidation.
namespace TraderAdaptiveLayout
{
inline int ParseMoneyCaption(const std::string& caption)
{
    int value = 0;
    bool digits = false;
    for (std::size_t i = 0; i < caption.size(); ++i)
    {
        const char ch = caption[i];
        if (ch >= '0' && ch <= '9')
        {
            digits = true;
            if (value > (2147483647 - (ch - '0')) / 10) return -1;
            value = value * 10 + (ch - '0');
        }
    }
    return digits ? value : -1;
}
inline bool SafeMoneyPlate(bool ownsContent, bool isContent, int plateHeight, int parentHeight, bool trusted = false)
{
    return !ownsContent && !isContent && plateHeight > 0 && parentHeight > 0
        && (trusted || plateHeight <= parentHeight / 3);
}
inline int FitFont(int fontHeight, int naturalWidth, int available)
{
    if (naturalWidth <= available || naturalWidth <= 0) return fontHeight;
    const int fitted = fontHeight * (available > 2 ? available - 2 : 1) / naturalWidth;
    int floor = (fontHeight * 3 + 3) / 4;
    if (floor < 12) floor = 12;
    if (floor > fontHeight) floor = fontHeight;
    return fitted > floor ? fitted : floor;
}
struct SearchRow
{
    int editWidth, clientWidth, clearLeft, clearWidth, countLeft, countWidth;
    SearchRow(int width, int countNeed, int clearNeed)
    {
        if (width < 0) width = 0;
        countWidth = countNeed < width ? countNeed : width;
        if (countWidth < 0) countWidth = 0;
        int available = width - countWidth - (countWidth ? 8 : 0);
        clearWidth = clearNeed < available - 6 ? clearNeed : available - 6;
        if (clearWidth < 0) clearWidth = 0;
        editWidth = available - (clearWidth ? clearWidth + 6 : 0);
        if (editWidth < 0) editWidth = 0;
        clientWidth = editWidth > 16 ? editWidth - 16 : 0;
        clearLeft = clearWidth ? editWidth + 6 : editWidth;
        countLeft = width - countWidth;
    }
};
struct Rect
{
    int left, top, width, height;
    Rect(int l=0, int t=0, int w=0, int h=0) : left(l), top(t), width(w), height(h) {}
    bool operator==(const Rect& rhs) const
    { return left==rhs.left && top==rhs.top && width==rhs.width && height==rhs.height; }
};
inline Rect ProjectAlign(Rect rect, int dx, int dy, int horizontal, int vertical)
{
    if (horizontal == 3) rect.width += dx;
    else if (horizontal) rect.left += horizontal == 2 ? dx : dx / 2;
    if (vertical == 3) rect.height += dy;
    else if (vertical) rect.top += vertical == 2 ? dy : dy / 2;
    return rect;
}
struct Viewport
{
    bool bound;
    Rect baseline, written;
    int parentWidth, parentHeight;
    Viewport() : bound(false), parentWidth(0), parentHeight(0) {}
    Rect Update(const Rect& current, int parentW, int parentH, int horizontal, int vertical, int header, int footer)
    {
        const int dx = parentW - parentWidth, dy = parentH - parentHeight;
        if (bound && current == ProjectAlign(written, dx, dy, horizontal, vertical))
            baseline = ProjectAlign(baseline, dx, dy, horizontal, vertical);
        else
            baseline = current; // Native absolute layout reset supplies a new baseline.
        bound = true; parentWidth = parentW; parentHeight = parentH;
        written = baseline;
        written.top += header;
        written.height -= header + footer;
        if (written.height < 0) written.height = 0;
        return written;
    }
};
struct Footer
{
    int sortWidth, searchWidth, sortTop, searchTop, extraHeight;
    Footer(int width, int sortNeed, int searchNeed, int height)
    {
        const int gap = 8;
        extraHeight = sortNeed + gap + searchNeed > width ? height + gap : 0;
        sortTop = -extraHeight;
        searchTop = 0;
        sortWidth = extraHeight ? width : sortNeed;
        searchWidth = extraHeight ? width : width - sortWidth - gap;
    }
};
struct Header
{
    int traderTop, playerTop, dealTop, extraHeight;
    Header(int width, int traderNeed, int playerNeed, int dealNeed, int height)
    {
        traderTop = playerTop = dealTop = extraHeight = 0;
        if (dealNeed > 0)
        {
            const int leftSpace = (width - dealNeed) / 2;
            const int rightSpace = width - dealNeed - leftSpace;
            if (traderNeed + 12 > leftSpace || playerNeed + 12 > rightSpace)
                dealTop = extraHeight = height;
        }
        if (playerNeed > 0 && traderNeed + playerNeed + 12 > width)
        {
            playerTop = height;
            dealTop = dealNeed > 0 ? height * 2 : 0;
            extraHeight = dealNeed > 0 ? dealTop : playerTop;
        }
    }
};
}
