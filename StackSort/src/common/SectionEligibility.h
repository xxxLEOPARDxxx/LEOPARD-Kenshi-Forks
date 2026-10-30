#pragma once
#include "SortPolicy.h"
#include <algorithm>
// Requires complete InventorySection from the native adapter's inventory
// header. Keeping the policy header independent also permits fault-model tests.
// Game thread only. The solver receives dimensions, never this object.
inline bool StackSort_EligibleSection(const InventorySection* section)
{
    if (!section) return false;
    const int w = (std::min)(section->width, (int)section->content.nRows);
    const int h = (std::min)(section->height, (int)section->content.nCols);
    if (!SortPolicy::SortableGrid(w, h, section->itemsLimit,
            section->isAnEquippedItemSection, section->enabled)) return false;
    // A single permitted resource whose footprint fills the storage has no
    // alternative layout even when its skin reports more than one grid cell.
    if (section->veryLimitedSlot.size() == 1)
    {
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        bool alternative = false;
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].item && items[i].w > 0 && items[i].h > 0 &&
                (w / items[i].w) * (h / items[i].h) >= 2) alternative = true;
        if (!alternative) return false;
    }
    return true;
}
