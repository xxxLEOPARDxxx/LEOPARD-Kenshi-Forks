#pragma once

#include <string>

namespace MyGUI
{
class Widget;
}

class Inventory;
class Item;

std::string NormalizeInventorySearchText(const std::string& text);
std::string BuildInventoryItemSearchText(MyGUI::Widget* itemWidget);
std::string BuildInventoryItemSearchTextFromResolvedItem(MyGUI::Widget* itemWidget, Item* resolvedItem);
bool InventorySearchTextMatchesQuery(
    const std::string& searchableTextNormalized,
    const std::string& normalizedQuery);
bool TryResolveInventoryItemQuantityFromWidget(MyGUI::Widget* itemWidget, int* outQuantity);
Inventory* ResolveInventoryWidgetInventoryPointer(MyGUI::Widget* widget);
Item* ResolveInventoryWidgetItemPointer(MyGUI::Widget* widget);
