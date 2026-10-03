#pragma once

#include <string>

namespace TraderLayoutRecognition
{
// Internal layout names are not translated. Keep this independent of captions,
// item population, money text, mouse focus and the current dialogue target.
struct Markers
{
    bool arrange, scroll, backpack, data, equipment;
    Markers() : arrange(false), scroll(false), backpack(false), data(false), equipment(false) {}
    bool isTrader() const { return arrange && scroll && backpack && data && !equipment; }
};

template <typename Widget>
void Collect(Widget* widget, Markers& markers, bool root = true)
{
    if (widget == 0 || (!root && widget->getTypeName() == "Window"))
        return;

    const std::string& name = widget->getName();
    // Kenshi can prefix names when loading more than one layout instance.
    const bool scroll = name.find("scrollview_backpack_content") != std::string::npos;
    markers.arrange |= name.find("ArrangeButton") != std::string::npos;
    markers.scroll |= scroll;
    markers.backpack |= !scroll && name.find("backpack_content") != std::string::npos;
    markers.data |= name.find("datapanel") != std::string::npos;
    markers.equipment |= name.find("OpenBagButton") != std::string::npos
        || name.find("InventoryMainTitle") != std::string::npos;
    for (std::size_t i = 0; i < widget->getChildCount(); ++i)
        Collect(widget->getChildAt(i), markers, false);
}

template <typename Widget>
bool IsTrader(Widget* root)
{
    Markers markers;
    Collect(root, markers);
    return markers.isTrader();
}
}
