#pragma once

#include "InventoryBinding.h"

namespace MyGUI
{
class Widget;
}

class Inventory;
class InventoryGUI;

void RegisterInventoryGuiInventoryLink(InventoryGUI* inventoryGui, Inventory* inventory);

bool CollectBoundBackpackEntriesForContent(
    MyGUI::Widget* backpackContent,
    std::vector<InventoryBoundEntry>* outEntries,
    std::string* outReason);
