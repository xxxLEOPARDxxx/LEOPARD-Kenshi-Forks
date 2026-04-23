#pragma once

namespace MyGUI
{
class Widget;
}

bool ApplyInventorySearchFilterToParent(MyGUI::Widget* inventoryParent, bool forceShowAll);
void ClearInventorySearchFilterState();
void ClearInventorySearchFilterStateWithoutRestoringEntries();
