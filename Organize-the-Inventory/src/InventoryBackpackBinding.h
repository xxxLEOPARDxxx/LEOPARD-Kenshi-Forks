#pragma once

#include "InventoryBinding.h"

namespace MyGUI
{
class Widget;
}

class Inventory;
class InventoryGUI;

void RegisterInventoryGuiInventoryLink(InventoryGUI* inventoryGui, Inventory* inventory);

bool IsInventoryOwnedByInventoryContext(Inventory* inventory, Inventory* contextInventory);

bool CollectBoundBackpackEntriesForContent(
    MyGUI::Widget* backpackContent,
    std::vector<InventoryBoundEntry>* outEntries,
    std::string* outReason,
    Inventory** outInventory = 0);
