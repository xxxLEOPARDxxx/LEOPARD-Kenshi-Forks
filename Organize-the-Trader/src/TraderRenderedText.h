#pragma once
#include <string>
#include <vector>
// A composed EditBox can have no root text at all. Its public Client skin
// owns the actual ISubWidgetText. Keep this adapter independent of MyGUI ABI.
namespace TraderRenderedText
{
template<class Widget>
Widget* Resolve(Widget* widget)
{
    Widget* chain[4];
    int count = 0;
    while (widget != 0 && count < 4)
    {
        bool repeated = false;
        for (int i = 0; i < count; ++i) if (chain[i] == widget) repeated = true;
        if (repeated) break;
        chain[count++] = widget;
        widget = widget->getClientWidget();
    }
    // EditBox::initialiseOverride first takes root text, then overrides it
    // with the Client skin text when present. Match that public contract.
    while (count > 0)
    {
        Widget* candidate = chain[--count];
        if (candidate->getSubWidgetText() != 0) return candidate;
    }
    return 0;
}
inline int OuterWidth(int naturalText, int outerWidth, int textRegionWidth)
{
    int chrome = outerWidth - textRegionWidth;
    if (chrome < 0) chrome = 0;
    return naturalText + chrome + 4;
}
template<class Caption>
struct MeasureCache
{
    bool valid;
    std::string font;
    int height, widest;
    size_t refreshes, captionSets, widthReads;
    std::vector<Caption> captions;
    MeasureCache() : valid(false), height(0), widest(0), refreshes(0), captionSets(0), widthReads(0) {}
    template<class Renderer>
    int Get(const std::string& name, int fontHeight, const std::vector<Caption>& items, Renderer& renderer)
    {
        if (valid && name == font && fontHeight == height && items == captions) return widest;
        if (!renderer.Begin(name, fontHeight)) return -1;
        ++refreshes;
        int maximum = 0;
        for (size_t i = 0; i < items.size(); ++i)
        {
            const int width = renderer.CaptionWidth(items[i]);
            ++captionSets; ++widthReads;
            if (width > maximum) maximum = width;
        }
        font = name; height = fontHeight; captions = items; widest = maximum; valid = true;
        return widest;
    }
    void Reset() { valid = false; }
};
}
