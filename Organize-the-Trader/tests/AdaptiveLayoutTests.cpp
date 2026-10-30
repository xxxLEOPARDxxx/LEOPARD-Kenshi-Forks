#include "../src/TraderAdaptiveLayout.h"
#include <cstdlib>
static int checks = 0;
void Check(bool ok) { ++checks; if (!ok) std::abort(); }
#include <cstdio>
int main()
{
    using namespace TraderAdaptiveLayout;
    Check(ParseMoneyCaption("c.21,021") == 21021);
    Check(ParseMoneyCaption("c.0") == 0);
    Check(ParseMoneyCaption("c.2,147,483,647") == 2147483647);
    Check(ParseMoneyCaption("c.2,147,483,648") == -1);
    Check(ParseMoneyCaption("Money:") == -1);
    Check(SafeMoneyPlate(false, false, 40, 600));
    Check(!SafeMoneyPlate(true, false, 600, 600));
    Check(!SafeMoneyPlate(false, true, 40, 600));
    Check(!SafeMoneyPlate(false, false, 300, 600));
    Check(SafeMoneyPlate(false, false, 40, 60, true));
    Check(!SafeMoneyPlate(true, false, 40, 60, true));
    Check(!SafeMoneyPlate(false, true, 40, 60, true));
    Header noDeal(300, 180, 180, 0, 32);
    Check(noDeal.playerTop == 32 && noDeal.extraHeight == 32);
    Header asymmetric(400, 200, 60, 80, 32);
    Check(asymmetric.traderTop == 0 && asymmetric.playerTop == 0 && asymmetric.dealTop == 32);
    Header captured(669, 163, 200, 80, 54);
    Check(captured.dealTop == 0 && captured.extraHeight == 0);
    Check(FitFont(40, 600, 300) == 30);
    Check(FitFont(10, 600, 1) == 10);
    Check(FitFont(16, 600, 1) == 12);
    Viewport a, b;
    Rect original(10, 20, 300, 100), other(10, 20, 300, 70);
    Rect shrunk = a.Update(original, 400, 200, 0, 0, 80, 40);
    Rect mixed = b.Update(other, 400, 200, 0, 0, 80, 40);
    Check(shrunk.height == 0 && mixed.height == 0);
    Check(a.Update(shrunk, 400, 200, 0, 0, 80, 40) == shrunk);
    Check(a.Update(shrunk, 400, 200, 0, 0, 0, 0) == original);
    Check(b.Update(mixed, 400, 200, 0, 0, 0, 0) == other);
    Viewport resized;
    Rect first = resized.Update(original, 400, 200, 3, 3, 80, 40);
    Rect aligned = ProjectAlign(first, 100, 100, 3, 3);
    Rect second = resized.Update(aligned, 500, 300, 3, 3, 80, 40);
    Check(second.height == 80 && second.top == 100);
    Rect restored = resized.Update(second, 500, 300, 3, 3, 0, 0);
    Check(restored == Rect(10, 20, 400, 200));
    // Native explicit resize discards our old margins; adopt its absolute baseline.
    Rect reset(10, 30, 500, 400);
    Rect reapplied = resized.Update(reset, 600, 500, 3, 3, 80, 40);
    Check(reapplied == Rect(10, 110, 500, 280));
    Check(resized.Update(reapplied, 600, 500, 3, 3, 0, 0) == reset);
    for (int width = 0; width <= 1000; width += 10)
    for (int countWidth = 0; countWidth <= 2000; countWidth += 20)
    {
        SearchRow row(width, countWidth, 32);
        Check(row.editWidth >= 0 && row.clientWidth >= 0 && row.countLeft >= 0);
        Check(row.clearLeft + row.clearWidth <= width && row.countLeft + row.countWidth <= width);
    }
    Footer small(680, 278, 320, 32);
    Check(small.sortWidth == 278 && small.searchWidth == 394 && small.extraHeight == 0);
    Footer wrap(480, 278, 320, 32);
    Check(wrap.extraHeight == 40 && wrap.sortTop == -40 && wrap.searchTop == 0);
    Check(wrap.sortWidth == 480 && wrap.searchWidth == 480);
    for (int width = 360; width <= 2000; width += 17)
    for (int font = 12; font <= 40; font += 2)
    {
        const int sort = font * 15, search = font * 18;
        Footer f(width, sort, search, font + 12);
        Check(f.sortWidth > 0 && f.searchWidth > 0);
        Check(f.sortWidth <= width && f.searchWidth <= width);
        const int fitted = FitFont(font, font * 12, f.sortWidth - font * 3);
        Check(fitted >= (font < 12 ? font : 12) && fitted >= (font * 3 + 3) / 4);
        if (!f.extraHeight) Check(f.sortWidth + 8 + f.searchWidth == width);
        else Check(f.sortTop + font + 12 < f.searchTop);
        Header h(width, font * 10, font * 10, font * 9, font + 12);
        Check(h.playerTop >= h.traderTop && h.dealTop >= h.traderTop);
    }
    std::printf("PASS: %d adaptive layout assertions\n", checks);
}
