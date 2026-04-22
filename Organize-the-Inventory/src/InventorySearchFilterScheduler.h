#pragma once

namespace MyGUI
{
class Widget;
}

struct InventorySearchFilterRefreshResult
{
    InventorySearchFilterRefreshResult();

    bool attempted;
    bool applied;
    bool skipped;
};

InventorySearchFilterRefreshResult ApplyInventorySearchFilterIfNeeded(
    MyGUI::Widget* inventoryParent,
    bool forceShowAll);
void ClearInventorySearchFilterRefreshState();
