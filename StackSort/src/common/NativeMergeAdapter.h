#pragma once
#include <cmath>
#include <float.h>
#include <climits>
// Native addQuantity is arithmetic only: it leaves a zero-quantity source in
// the section. Reinsert a detached source through the game's section addItem
// instead, which owns stack compatibility, charge transfer and source cleanup.
// Steam 1.0.65: addQuantity 75D6E0; section addItem 74BD70. addItem's amount is
// additional to item->quantity-1, so 1 means the entire existing source ONCE.
struct NativeMergeAdapter
{
    typedef hand Token;
    InventorySection* section;
    NativeMergeAdapter(InventorySection* s) : section(s) {}
    struct Credit
    {
        Token key;
        double units;
    };
    static double Units(const Item* item, double full)
    {
        if (full <= 0.0) return (double)item->quantity;
        return ((double)item->quantity - 1.0) * full + (double)item->chargesLeft;
    }
    // Native addItem temporarily normalizes source.quantity=1. On an
    // allocation failure the stack-local remainder is gone, so source's fields
    // alone cannot recover it. Derive only the part not credited to live targets.
    bool RestoreUncredited(Item* remainder, double originalUnits, double full,
                          const std::vector<Credit>& credits)
    {
        double remaining = originalUnits;
        for (size_t i = 0; i < credits.size(); ++i)
        {
            Item* target = Find(credits[i].key);
            if (!target) return false;
            const double delta = Units(target, full) - credits[i].units;
            if (!_finite(delta) || delta < -0.001) return false;
            remaining -= delta;
        }
        if (!_finite(remaining) || remaining < -0.001 || remaining > originalUnits + 0.001) return false;
        if (remaining <= 0.001) { remainder->quantity = 0; return true; }
        const double unitsPerItem = full > 0.0 ? full : 1.0;
        const double count = std::ceil(remaining / unitsPerItem - 1e-9);
        if (count < 1.0 || count > INT_MAX) return false;
        remainder->quantity = (int)count;
        if (full > 0.0) remainder->chargesLeft = (float)(remaining - (count - 1.0) * full);
        return true;
    }
    void Snapshot(std::vector<Token>& out)
    {
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        out.reserve(items.size());
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].item) out.push_back(items[i].item->handle);
    }
    Item* Find(const Token& token)
    {
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].item && items[i].item->handle == token) return items[i].item;
        return NULL;
    }
    bool HasMergeTarget(const Token& token)
    {
        Item* source = Find(token);
        if (!source || source->quantity <= 0 || source->isEquipped) return false;
        // Valid existing stacks never need native splitting into new Items.
        if (source->quantity > source->isStackable(section)) return false;
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        for (size_t i = 0; i < items.size(); ++i)
        {
            Item* target = items[i].item;
            if (!target || target == source || target->quantity <= 0 || target->isEquipped) continue;
            const int maxStack = target->isStackable(section);
            if (maxStack < 1 || target->quantity > maxStack) continue;
            if (target->quantity == maxStack)
            {
                // A stack at its item-count limit can still accept charges.
                // Native addItem explicitly enters its merge path for a
                // partial open charge even when isStackable(section) == 1.
                const double full = (double)target->originalFullChargeAmount;
                const double left = (double)target->chargesLeft;
                if (!_finite(full) || !_finite(left) || full <= 0.0 || left < 0.0 || left >= full) continue;
            }
            // Virtual isSameAs and the native ownership rule account for
            // quality/material/manufacturer/stolen states; no template-only key.
            if (target->canStackWith(source)) return true;
        }
        return false;
    }
    int Reinsert(const Token& token)
    {
        Item* source = Find(token);
        if (!source) return 0;
        const double full = (double)source->originalFullChargeAmount;
        const double originalUnits = Units(source, full);
        if (!_finite(full) || !_finite(originalUnits) || originalUnits <= 0.0) return 0;
        // Allocated before the native mutation window. Every target that native
        // compatibility permits is recorded, including already-full stacks.
        std::vector<Credit> credits;
        credits.reserve(section->getItems().size());
        int x = 0, y = 0;
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        for (size_t i = 0; i < items.size(); ++i)
        {
            Item* target = items[i].item;
            if (target == source) { x = items[i].x; y = items[i].y; continue; }
            if (!target || !target->canStackWith(source)) continue;
            if ((double)target->originalFullChargeAmount != full) return 0;
            Credit credit; credit.key = target->handle; credit.units = Units(target, full);
            if (!_finite(credit.units)) return 0;
            credits.push_back(credit);
        }
        if (!section->removeItem(source)) return 0;
        try
        {
            // Never read source after this call: successful merge schedules its
            // destruction. On false the audited native branch retains a live
            // remainder, recovered by handle below.
            if (section->_NV_addItem(source, 1)) return 1;
        }
        catch (...) { }
        // The handle is the only authority after a possibly consuming call.
        // Do not recreate an item when the native outcome is ambiguous.
        Item* remainder = token.getItem();
        if (remainder && (section->hasItem(remainder) || remainder->inventorySection.empty()) &&
            RestoreUncredited(remainder, originalUnits, full, credits))
        {
            if (remainder->quantity > 0 && !section->hasItem(remainder) &&
                section->canItemGoHere(remainder, x, y)) section->_NV__addItem(remainder, x, y);
        }
        ErrorLog("StackSort: native consolidation refused or threw; remainder recovered where possible, sorting stopped");
        return -1;
    }
};
