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
    // Optional, click-local diagnostics. Never owns or dereferences consumed items.
    struct Stats
    {
        unsigned sources, findMiss, sourceInvalid, sourceCapacity, targetInvalid, targetCapacity,
                 targetFull, rejected, accepted, badUnits, chargeMismatch, removeReject, addOK, addFault;
        struct Pair { const void* source; const void* target; int sq,tq,cap,sw,sh,tw,th;
                      float full,left,targetFull,targetLeft; bool accepted; } samples[8];
        unsigned sampleCount;
        Stats():sources(0),findMiss(0),sourceInvalid(0),sourceCapacity(0),targetInvalid(0),
            targetCapacity(0),targetFull(0),rejected(0),accepted(0),badUnits(0),chargeMismatch(0),
            removeReject(0),addOK(0),addFault(0),sampleCount(0){}
    };
    InventorySection* section;
    Stats* stats;
    NativeMergeAdapter(InventorySection* s, Stats* trace=NULL) : section(s),stats(trace) {}
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
            // VC10 groups the SDK's overloaded virtual == declarations in a
            // different order from Native hand's vtable. Call the verified
            // nonvirtual hand comparison (all five fields, Native null rule).
            if (items[i].item && items[i].item->handle._NV_operator_equal(token)) return items[i].item;
        if (stats) ++stats->findMiss;
        return NULL;
    }
    bool HasMergeTarget(const Token& token)
    {
        Item* source = Find(token);
        if (stats) ++stats->sources;
        if (!source || source->quantity <= 0 || source->isEquipped)
        { if(stats) ++stats->sourceInvalid; return false; }
        // Valid existing stacks never need native splitting into new Items.
        if (source->quantity > source->isStackable(section))
        { if(stats) ++stats->sourceCapacity; return false; }
        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        for (size_t i = 0; i < items.size(); ++i)
        {
            Item* target = items[i].item;
            if (!target || target == source || target->quantity <= 0 || target->isEquipped)
            { if(stats) ++stats->targetInvalid; continue; }
            const int maxStack = target->isStackable(section);
            if (maxStack < 1 || target->quantity > maxStack)
            { if(stats) ++stats->targetCapacity; continue; }
            if (target->quantity == maxStack)
            {
                // A stack at its item-count limit can still accept charges.
                // Native addItem explicitly enters its merge path for a
                // partial open charge even when isStackable(section) == 1.
                const double full = (double)target->originalFullChargeAmount;
                const double left = (double)target->chargesLeft;
                if (!_finite(full) || !_finite(left) || full <= 0.0 || left < 0.0 || left >= full)
                { if(stats) ++stats->targetFull; continue; }
            }
            // Virtual isSameAs and the native ownership rule account for
            // quality/material/manufacturer/stolen states; no template-only key.
            const bool compatible=target->canStackWith(source);
            if(stats)
            {
                if(compatible) ++stats->accepted; else ++stats->rejected;
                if(stats->sampleCount<8 && source->manufacturerData==target->manufacturerData &&
                   source->materialData==target->materialData)
                {
                    Stats::Pair& p=stats->samples[stats->sampleCount++];
                    p.source=source;p.target=target;p.sq=source->quantity;p.tq=target->quantity;p.cap=maxStack;
                    p.sw=source->itemWidth;p.sh=source->itemHeight;p.tw=target->itemWidth;p.th=target->itemHeight;
                    p.full=source->originalFullChargeAmount;p.left=source->chargesLeft;
                    p.targetFull=target->originalFullChargeAmount;p.targetLeft=target->chargesLeft;p.accepted=compatible;
                }
            }
            if (compatible) return true;
        }
        return false;
    }
    int Reinsert(const Token& token)
    {
        Item* source = Find(token);
        if (!source) return 0;
        const double full = (double)source->originalFullChargeAmount;
        const double originalUnits = Units(source, full);
        if (!_finite(full) || !_finite(originalUnits) || originalUnits <= 0.0)
        { if(stats) ++stats->badUnits; return 0; }
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
            if ((double)target->originalFullChargeAmount != full)
            { if(stats) ++stats->chargeMismatch; return 0; }
            Credit credit; credit.key = target->handle; credit.units = Units(target, full);
            if (!_finite(credit.units)) { if(stats) ++stats->badUnits; return 0; }
            credits.push_back(credit);
        }
        if (!section->removeItem(source)) { if(stats) ++stats->removeReject; return 0; }
        try
        {
            // Never read source after this call: successful merge schedules its
            // destruction. On false the audited native branch retains a live
            // remainder, recovered by handle below.
            if (section->_NV_addItem(source, 1)) { if(stats) ++stats->addOK; return 1; }
        }
        catch (...) { }
        if(stats) ++stats->addFault;
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
