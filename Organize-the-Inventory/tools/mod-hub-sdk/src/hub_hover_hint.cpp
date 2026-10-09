#include "hub_hover_hint.h"

#include <mygui/MyGUI_Widget.h>

namespace
{
const char* kToolTipUserStringKey = "ToolTip";
}

void HubHoverHint_Attach(MyGUI::Widget* widget, const char* hint_text)
{
    if (widget == 0 || hint_text == 0 || hint_text[0] == '\0')
    {
        return;
    }

    widget->setNeedMouseFocus(true);
    widget->setNeedToolTip(true);
    widget->setUserString(kToolTipUserStringKey, hint_text);
}

void HubHoverHint_Attach(MyGUI::Widget* widget, const std::string& hint_text)
{
    HubHoverHint_Attach(widget, hint_text.c_str());
}
