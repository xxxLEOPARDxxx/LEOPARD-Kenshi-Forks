#pragma once

#include <string>
#include <vector>

namespace MyGUI
{
class Widget;
}

class Inventory;
class Item;

struct InventoryBoundEntry
{
    InventoryBoundEntry()
        : widget(0)
        , item(0)
        , quantity(0)
    {
    }

    MyGUI::Widget* widget;
    Item* item;
    int quantity;
    std::string sectionName;
};

void RegisterInventorySectionWidgetLink(
    MyGUI::Widget* sectionWidget,
    Inventory* inventory,
    const std::string& sectionName);

void CollectLikelyInventoryEntryWidgets(
    MyGUI::Widget* rootWidget,
    std::vector<MyGUI::Widget*>* outWidgets);

bool CollectBoundInventoryEntriesForRoot(
    MyGUI::Widget* inventoryRoot,
    std::vector<InventoryBoundEntry>* outEntries,
    std::string* outReason);

Inventory* ResolveBoundInventoryForRoot(MyGUI::Widget* inventoryRoot);
