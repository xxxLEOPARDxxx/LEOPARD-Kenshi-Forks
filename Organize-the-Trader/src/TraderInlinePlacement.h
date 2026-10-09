#pragma once
namespace TraderInlinePlacement
{
struct Rect
{
    int left, top, width, height;
    Rect(int x, int y, int w, int h) : left(x), top(y), width(w), height(h) {}
};
inline Rect Place(int parentWidth, const Rect& money)
{
    const int left = money.left + money.width + 8;
    const int available = parentWidth - left - 16;
    return Rect(left, money.top, available > 0 ? available : 0, money.height);
}
}
