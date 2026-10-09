#ifndef EMC_HUB_HOVER_HINT_H
#define EMC_HUB_HOVER_HINT_H

#include <string>

namespace MyGUI
{
class Widget;
}

void HubHoverHint_Attach(MyGUI::Widget* widget, const char* hint_text);
void HubHoverHint_Attach(MyGUI::Widget* widget, const std::string& hint_text);

#endif
