#include "CraftingInventoryBinding.h"

#include "CraftingSearchText.h"
#include "CraftingWindowDetection.h"

#include <kenshi/Building.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_InputManager.h>

#include <algorithm>
#include <sstream>
#include <vector>

// Minimal ownership interface stub to avoid including Platoon.h
// (it conflicts with Building.h enum declarations in this SDK drop).
class Ownerships
{
public:
    void getHomeFurnitureOfType(lektor<Building*>& out, BuildingFunction type);
    void getBuildingsWithFunction(lektor<Building*>& out, BuildingFunction bf);
};

void AddInventoryCandidateUnique(
    std::vector<InventoryCandidateInfo>* candidates,
    Inventory* inventory,
    const std::string& source,
    bool traderPreferred,
    bool visible,
    int priorityBias);

const char* ItemTypeNameForLog(itemType type);

void AddBuildingInventoryCandidate(
    Building* building,
    const char* sourceTag,
    const std::string& traderName,
    bool traderPreferred,
    int priorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates);

void AddInventoryCandidateUnique(
    std::vector<InventoryCandidateInfo>* candidates,
    Inventory* inventory,
    const std::string& source,
    bool traderPreferred,
    bool visible,
    int priorityBias)
{
    if (candidates == 0 || inventory == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        InventoryCandidateInfo& existing = (*candidates)[index];
        if (existing.inventory != inventory)
        {
            continue;
        }

        if (traderPreferred)
        {
            existing.traderPreferred = true;
        }
        if (visible)
        {
            existing.visible = true;
        }
        if (priorityBias > existing.priorityBias)
        {
            existing.priorityBias = priorityBias;
            existing.source = source;
        }
        return;
    }

    InventoryCandidateInfo info;
    info.inventory = inventory;
    info.source = source;
    info.traderPreferred = traderPreferred;
    info.visible = visible;
    info.priorityBias = priorityBias;
    candidates->push_back(info);
}

bool CollectTraderOwnershipInventoryCandidates(
    Character* trader,
    int captionScore,
    const char* sourcePrefix,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (trader == 0 || outCandidates == 0)
    {
        return false;
    }

    Ownerships* ownerships = trader->getOwnerships();
    if (ownerships == 0)
    {
        return false;
    }

    const std::string traderName = CharacterNameForLog(trader);
    const int scoreBias = captionScore > 0 ? (captionScore / 12) : 0;
    bool addedAny = false;

    bool traderInventoryVisible = false;
    TryResolveCharacterInventoryVisible(trader, &traderInventoryVisible);
    if (trader->inventory != 0)
    {
        std::stringstream selfSource;
        selfSource << (sourcePrefix == 0 ? "trader_owned" : sourcePrefix)
                   << ":" << traderName
                   << " self_inventory=true"
                   << " visible=" << (traderInventoryVisible ? "true" : "false")
                   << " items=" << InventoryItemCountForLog(trader->inventory);
        AddInventoryCandidateUnique(
            outCandidates,
            trader->inventory,
            selfSource.str(),
            true,
            traderInventoryVisible,
            4200 + scoreBias);
        addedAny = true;
    }

    lektor<Building*> shopFurniture;
    ownerships->getHomeFurnitureOfType(shopFurniture, BF_SHOP);
    if (shopFurniture.size() == 0)
    {
        ownerships->getBuildingsWithFunction(shopFurniture, BF_SHOP);
    }
    for (std::size_t index = 0; index < shopFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            shopFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_shop",
            traderName,
            true,
            3200 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    lektor<Building*> generalStorageFurniture;
    ownerships->getHomeFurnitureOfType(generalStorageFurniture, BF_GENERAL_STORAGE);
    for (std::size_t index = 0; index < generalStorageFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            generalStorageFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_storage",
            traderName,
            true,
            1800 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    lektor<Building*> resourceStorageFurniture;
    ownerships->getHomeFurnitureOfType(resourceStorageFurniture, BF_RESOURCE_STORAGE);
    for (std::size_t index = 0; index < resourceStorageFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            resourceStorageFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_resource",
            traderName,
            true,
            1500 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    return addedAny;
}

void CollectNearbyInventoryCandidates(
    const Ogre::Vector3& center,
    std::vector<InventoryCandidateInfo>* outCandidates,
    std::size_t* scannedObjectsOut,
    std::size_t* inventoryObjectsOut,
    float scanRadius,
    int maxObjectsPerType,
    int priorityBiasBase,
    const char* sourcePrefix)
{
    if (outCandidates == 0 || ou == 0)
    {
        return;
    }

    if (scannedObjectsOut != 0)
    {
        *scannedObjectsOut = 0;
    }
    if (inventoryObjectsOut != 0)
    {
        *inventoryObjectsOut = 0;
    }

    const itemType scanTypes[] = { BUILDING, CONTAINER, ITEM, SHOP_TRADER_CLASS };

    for (std::size_t typeIndex = 0; typeIndex < sizeof(scanTypes) / sizeof(scanTypes[0]); ++typeIndex)
    {
        const itemType scanType = scanTypes[typeIndex];
        lektor<RootObject*> nearbyObjects;
        ou->getObjectsWithinSphere(nearbyObjects, center, scanRadius, scanType, maxObjectsPerType, 0);
        if (!nearbyObjects.valid() || nearbyObjects.size() == 0)
        {
            continue;
        }

        for (lektor<RootObject*>::const_iterator iter = nearbyObjects.begin(); iter != nearbyObjects.end(); ++iter)
        {
            RootObject* object = *iter;
            if (object == 0)
            {
                continue;
            }

            if (scannedObjectsOut != 0)
            {
                ++(*scannedObjectsOut);
            }

            Inventory* inventory = object->getInventory();
            if (inventory == 0)
            {
                continue;
            }

            if (inventoryObjectsOut != 0)
            {
                ++(*inventoryObjectsOut);
            }

            const float distanceSq = object->getPosition().squaredDistance(center);
            const float radiusSq = scanRadius * scanRadius;
            int proximityBias = priorityBiasBase;
            if (radiusSq > 1.0f)
            {
                const float normalizedDistance = distanceSq / radiusSq;
                if (normalizedDistance <= 1.0f)
                {
                    proximityBias += static_cast<int>((1.0f - normalizedDistance) * 600.0f);
                }
                else
                {
                    proximityBias -= static_cast<int>((normalizedDistance - 1.0f) * 160.0f);
                }
            }

            std::stringstream src;
            src << (sourcePrefix == 0 ? "nearby" : sourcePrefix)
                << ":" << ItemTypeNameForLog(scanType)
                << ":" << RootObjectDisplayNameForLog(object)
                << " visible=" << (inventory->isVisible() ? "true" : "false")
                << " items=" << InventoryItemCountForLog(inventory)
                << " dist2=" << static_cast<int>(distanceSq)
                << " radius=" << static_cast<int>(scanRadius);
            AddInventoryCandidateUnique(
                outCandidates,
                inventory,
                src.str(),
                false,
                inventory->isVisible(),
                proximityBias);
        }
    }
}


const char* ItemTypeNameForLog(itemType type)
{
    switch (type)
    {
    case BUILDING:
        return "BUILDING";
    case CONTAINER:
        return "CONTAINER";
    case ITEM:
        return "ITEM";
    case SHOP_TRADER_CLASS:
        return "SHOP_TRADER_CLASS";
    default:
        return "OTHER";
    }
}

void AddBuildingInventoryCandidate(
    Building* building,
    const char* sourceTag,
    const std::string& traderName,
    bool traderPreferred,
    int priorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (building == 0 || outCandidates == 0)
    {
        return;
    }

    Inventory* inventory = building->getInventory();
    if (inventory == 0)
    {
        return;
    }

    std::stringstream source;
    source << (sourceTag == 0 ? "trader_owned" : sourceTag)
           << ":" << traderName
           << " owner=" << RootObjectDisplayNameForLog(building)
           << " visible=" << (inventory->isVisible() ? "true" : "false")
           << " items=" << InventoryItemCountForLog(inventory);
    AddInventoryCandidateUnique(
        outCandidates,
        inventory,
        source.str(),
        traderPreferred,
        inventory->isVisible(),
        priorityBias);
}


namespace
{
std::size_t CountNonEmptyKeysLocal(const std::vector<std::string>& keys)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        if (!keys[index].empty())
        {
            ++count;
        }
    }
    return count;
}

bool TryExtractSearchKeysFromInventorySection(InventorySection* section, std::vector<std::string>* outKeys)
{
    if (section == 0 || outKeys == 0)
    {
        return false;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
    if (sectionItems.empty())
    {
        return false;
    }

    std::vector<InventorySection::SectionItem> sortedItems(sectionItems.begin(), sectionItems.end());
    struct SectionItemTopLeftLess
    {
        bool operator()(const InventorySection::SectionItem& leftItem, const InventorySection::SectionItem& rightItem) const
        {
            if (leftItem.y != rightItem.y)
            {
                return leftItem.y < rightItem.y;
            }
            return leftItem.x < rightItem.x;
        }
    };
    std::sort(sortedItems.begin(), sortedItems.end(), SectionItemTopLeftLess());

    outKeys->clear();
    outKeys->reserve(sortedItems.size());
    for (std::size_t index = 0; index < sortedItems.size(); ++index)
    {
        Item* item = sortedItems[index].item;
        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        outKeys->push_back(key);
    }

    return !outKeys->empty();
}

bool TryExtractQuantityNameKeysFromInventorySection(
    InventorySection* section,
    std::vector<QuantityNameKey>* outKeys)
{
    if (section == 0 || outKeys == 0)
    {
        return false;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
    if (sectionItems.empty())
    {
        return false;
    }

    std::vector<InventorySection::SectionItem> sortedItems(sectionItems.begin(), sectionItems.end());
    struct SectionItemTopLeftLess
    {
        bool operator()(const InventorySection::SectionItem& leftItem, const InventorySection::SectionItem& rightItem) const
        {
            if (leftItem.y != rightItem.y)
            {
                return leftItem.y < rightItem.y;
            }
            return leftItem.x < rightItem.x;
        }
    };
    std::sort(sortedItems.begin(), sortedItems.end(), SectionItemTopLeftLess());

    outKeys->clear();
    outKeys->reserve(sortedItems.size());
    for (std::size_t index = 0; index < sortedItems.size(); ++index)
    {
        Item* item = sortedItems[index].item;
        if (item == 0)
        {
            continue;
        }

        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        if (key.empty())
        {
            continue;
        }

        QuantityNameKey hint;
        hint.quantity = item->quantity;
        hint.key = key;
        outKeys->push_back(hint);
    }

    return !outKeys->empty();
}

} // namespace

bool TryResolveTraderInventoryNameKeysFromTraderOwnership(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        CollectTraderOwnershipInventoryCandidates(
            captionTrader,
            captionScore,
            "trader_home_caption",
            &inventoryCandidates);
    }

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        CollectTraderOwnershipInventoryCandidates(
            dialogueTarget,
            0,
            "trader_home_dialogue",
            &inventoryCandidates);
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    std::size_t selfInventoryCandidateCount = 0;
    for (std::size_t candidateIndex = 0; candidateIndex < inventoryCandidates.size(); ++candidateIndex)
    {
        const std::string sourceLower = NormalizeSearchText(inventoryCandidates[candidateIndex].source);
        if (sourceLower.find("self inventory true") != std::string::npos)
        {
            ++selfInventoryCandidateCount;
        }
    }

    std::vector<std::string> aggregateKeys;
    std::vector<QuantityNameKey> aggregateQuantityKeys;
    for (std::size_t index = 0; index < inventoryCandidates.size(); ++index)
    {
        Inventory* inventory = inventoryCandidates[index].inventory;
        if (inventory == 0)
        {
            continue;
        }

        std::vector<std::string> inventoryKeys;
        if (TryExtractSearchKeysFromInventory(inventory, &inventoryKeys))
        {
            for (std::size_t keyIndex = 0; keyIndex < inventoryKeys.size(); ++keyIndex)
            {
                if (!inventoryKeys[keyIndex].empty())
                {
                    aggregateKeys.push_back(inventoryKeys[keyIndex]);
                }
            }
        }

        std::vector<QuantityNameKey> inventoryQuantityKeys;
        if (TryExtractQuantityNameKeysFromInventory(inventory, &inventoryQuantityKeys))
        {
            for (std::size_t q = 0; q < inventoryQuantityKeys.size(); ++q)
            {
                if (!inventoryQuantityKeys[q].key.empty())
                {
                    aggregateQuantityKeys.push_back(inventoryQuantityKeys[q]);
                }
            }
        }
    }

    std::vector<std::string> aggregateFallbackKeys;
    std::size_t aggregateAlignedMatchCount = 0;
    std::size_t aggregateNonEmptyCount = 0;
    bool aggregateAlignmentComputed = false;

    if (uiQuantities != 0 && !uiQuantities->empty() && !aggregateQuantityKeys.empty())
    {
        std::vector<std::string> aggregateAlignedNames;
        if (BuildAlignedInventoryNameHintsByQuantity(
                *uiQuantities,
                aggregateQuantityKeys,
                &aggregateAlignedNames))
        {
            aggregateAlignmentComputed = true;
            std::size_t alignedMatches = 0;
            for (std::size_t index = 0; index < aggregateAlignedNames.size(); ++index)
            {
                if (!aggregateAlignedNames[index].empty())
                {
                    ++alignedMatches;
                }
            }
            aggregateAlignedMatchCount = alignedMatches;

            for (std::size_t index = 0; index < aggregateAlignedNames.size(); ++index)
            {
                if (!aggregateAlignedNames[index].empty() || index >= uiQuantities->size())
                {
                    continue;
                }

                const int quantity = (*uiQuantities)[index];
                if (quantity <= 0)
                {
                    continue;
                }

                std::string fillHint = ResolveUniqueQuantityNameHint(aggregateQuantityKeys, quantity);
                if (fillHint.empty())
                {
                    fillHint = ResolveTopQuantityNameHints(aggregateQuantityKeys, quantity, 4);
                }
                if (!fillHint.empty())
                {
                    aggregateAlignedNames[index] = fillHint;
                }
            }

            aggregateFallbackKeys = aggregateAlignedNames;
            aggregateNonEmptyCount = CountNonEmptyKeysLocal(aggregateFallbackKeys);

            const bool strongAggregateAlignment =
                uiQuantities->size() > 0
                && (alignedMatches * 3 >= uiQuantities->size() * 2);
            if (strongAggregateAlignment)
            {
                outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
                if (outSource != 0)
                {
                    std::stringstream source;
                    source << "trader_owned_aggregate aligned_matches=" << alignedMatches
                           << "/" << uiQuantities->size()
                           << " non_empty=" << aggregateNonEmptyCount
                           << " ownership_keys=" << aggregateKeys.size()
                           << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                           << " self_inventory_candidates=" << selfInventoryCandidateCount
                           << " trader_owned_candidates=" << inventoryCandidates.size();
                    *outSource = source.str();
                }
                if (outQuantityKeys != 0)
                {
                    outQuantityKeys->assign(
                        aggregateQuantityKeys.begin(),
                        aggregateQuantityKeys.end());
                }
                return true;
            }
        }
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        if (!aggregateFallbackKeys.empty())
        {
            outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
            if (outSource != 0)
            {
                std::stringstream source;
                source << "trader_owned_aggregate_fallback aligned_matches=" << aggregateAlignedMatchCount
                       << "/" << (uiQuantities == 0 ? 0 : uiQuantities->size())
                       << " non_empty=" << aggregateNonEmptyCount
                       << " ownership_keys=" << aggregateKeys.size()
                       << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                       << " self_inventory_candidates=" << selfInventoryCandidateCount
                       << " trader_owned_candidates=" << inventoryCandidates.size();
                *outSource = source.str();
            }
            if (outQuantityKeys != 0)
            {
                outQuantityKeys->assign(
                    aggregateQuantityKeys.begin(),
                    aggregateQuantityKeys.end());
            }
            return true;
        }

        return false;
    }

    const std::size_t resolvedNonEmptyCount = CountNonEmptyKeysLocal(*outKeys);
    if (!aggregateFallbackKeys.empty() && aggregateAlignmentComputed)
    {
        if (resolvedNonEmptyCount == 0 && aggregateNonEmptyCount > 0)
        {
            outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
            if (outSource != 0)
            {
                std::stringstream source;
                source << "trader_owned_aggregate_coverage_fallback aligned_matches="
                       << aggregateAlignedMatchCount
                       << "/" << (uiQuantities == 0 ? 0 : uiQuantities->size())
                       << " non_empty=" << aggregateNonEmptyCount
                       << " ownership_keys=" << aggregateKeys.size()
                       << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                       << " self_inventory_candidates=" << selfInventoryCandidateCount
                       << " trader_owned_candidates=" << inventoryCandidates.size();
                *outSource = source.str();
            }
            if (outQuantityKeys != 0)
            {
                outQuantityKeys->assign(
                    aggregateQuantityKeys.begin(),
                    aggregateQuantityKeys.end());
            }
            return true;
        }
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " non_empty=" << resolvedNonEmptyCount
               << " self_inventory_candidates=" << selfInventoryCandidateCount
               << " trader_owned_candidates=" << inventoryCandidates.size();
        *outSource = source.str();
    }
    return true;
}

bool TryResolveTraderInventoryNameKeysFromNearbyObjects(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t scannedObjects = 0;
    std::size_t inventoryObjects = 0;
    bool hasFocusedTraderContext = false;

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t focusedScanned = 0;
        std::size_t focusedInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            dialogueTarget->getPosition(),
            &inventoryCandidates,
            &focusedScanned,
            &focusedInventoryObjects,
            220.0f,
            160,
            2200,
            "nearby_dialog");
        scannedObjects += focusedScanned;
        inventoryObjects += focusedInventoryObjects;

        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(dialogueTarget, &targetInventoryVisible);
        if (dialogueTarget->inventory != 0)
        {
            std::stringstream source;
            source << dialogueReason << " explicit_target_inv=true";
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                dialogueTarget->inventory,
                source.str(),
                dialogueTarget->isATrader(),
                targetInventoryVisible,
                1900);
        }
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t captionScanned = 0;
        std::size_t captionInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            captionTrader->getPosition(),
            &inventoryCandidates,
            &captionScanned,
            &captionInventoryObjects,
            260.0f,
            192,
            1300 + (captionScore / 4),
            "nearby_caption");
        scannedObjects += captionScanned;
        inventoryObjects += captionInventoryObjects;
    }

    if (!hasFocusedTraderContext)
    {
        Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
        const Ogre::Vector3 fallbackCenter =
            selectedCharacter != 0 ? selectedCharacter->getPosition() : ou->getCameraCenter();
        std::size_t fallbackScanned = 0;
        std::size_t fallbackInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            fallbackCenter,
            &inventoryCandidates,
            &fallbackScanned,
            &fallbackInventoryObjects,
            900.0f,
            512,
            0,
            "nearby_world");
        scannedObjects += fallbackScanned;
        inventoryObjects += fallbackInventoryObjects;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);

    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream line;
        line << *outSource
             << " nearby_focused=" << (hasFocusedTraderContext ? "true" : "false")
             << " nearby_candidates=" << inventoryCandidates.size()
             << " nearby_scanned=" << scannedObjects
             << " nearby_with_inventory=" << inventoryObjects;
        *outSource = line.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromNearbyShopCounters(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> nearbyCandidates;
    std::size_t scannedObjects = 0;
    std::size_t inventoryObjects = 0;
    bool hasFocusedTraderContext = false;

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t focusedScanned = 0;
        std::size_t focusedInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            dialogueTarget->getPosition(),
            &nearbyCandidates,
            &focusedScanned,
            &focusedInventoryObjects,
            360.0f,
            256,
            1400,
            "nearby_shop_dialog");
        scannedObjects += focusedScanned;
        inventoryObjects += focusedInventoryObjects;
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t captionScanned = 0;
        std::size_t captionInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            captionTrader->getPosition(),
            &nearbyCandidates,
            &captionScanned,
            &captionInventoryObjects,
            360.0f,
            256,
            1200 + (captionScore / 8),
            "nearby_shop_caption");
        scannedObjects += captionScanned;
        inventoryObjects += captionInventoryObjects;
    }

    if (!hasFocusedTraderContext)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> shopCounterCandidates;
    const int expected = static_cast<int>(expectedEntryCount);
    for (std::size_t index = 0; index < nearbyCandidates.size(); ++index)
    {
        const InventoryCandidateInfo& nearbyCandidate = nearbyCandidates[index];
        if (nearbyCandidate.inventory == 0)
        {
            continue;
        }

        const std::string sourceLower = NormalizeSearchText(nearbyCandidate.source);
        if (!IsShopCounterCandidateSource(sourceLower))
        {
            continue;
        }

        int priorityBias = nearbyCandidate.priorityBias + 1200;
        const int itemCount = static_cast<int>(InventoryItemCountForLog(nearbyCandidate.inventory));
        if (expected > 0)
        {
            const int diff = itemCount > expected ? itemCount - expected : expected - itemCount;
            priorityBias += 900 - diff * 140;
            if (diff <= 2)
            {
                priorityBias += 240;
            }
            else if (diff <= 5)
            {
                priorityBias += 100;
            }
        }

        std::stringstream focusedSource;
        focusedSource << nearbyCandidate.source
                      << " shop_counter_focus=true";
        AddInventoryCandidateUnique(
            &shopCounterCandidates,
            nearbyCandidate.inventory,
            focusedSource.str(),
            true,
            nearbyCandidate.visible,
            priorityBias);
    }

    if (shopCounterCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        shopCounterCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream line;
        line << *outSource
             << " nearby_shop_candidates=" << shopCounterCandidates.size()
             << " nearby_shop_scanned=" << scannedObjects
             << " nearby_shop_with_inventory=" << inventoryObjects;
        *outSource = line.str();
    }

    return true;
}

bool TryExtractSearchKeysFromInventory(Inventory* inventory, std::vector<std::string>* outKeys)
{
    if (inventory == 0 || outKeys == 0)
    {
        return false;
    }

    outKeys->clear();

    lektor<InventorySection*>& allSections = inventory->getAllSections();
    if (allSections.valid() && allSections.size() > 0)
    {
        std::vector<std::string> mergedSectionKeys;
        for (uint32_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
        {
            InventorySection* section = allSections[sectionIndex];
            std::vector<std::string> sectionKeys;
            if (!TryExtractSearchKeysFromInventorySection(section, &sectionKeys))
            {
                continue;
            }

            for (std::size_t keyIndex = 0; keyIndex < sectionKeys.size(); ++keyIndex)
            {
                if (!sectionKeys[keyIndex].empty())
                {
                    mergedSectionKeys.push_back(sectionKeys[keyIndex]);
                }
            }
        }

        if (!mergedSectionKeys.empty())
        {
            outKeys->swap(mergedSectionKeys);
            return true;
        }
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid() || allItems.size() == 0)
    {
        return false;
    }

    outKeys->reserve(allItems.size());
    for (uint32_t index = 0; index < allItems.size(); ++index)
    {
        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(allItems[index]));
        outKeys->push_back(key);
    }

    return !outKeys->empty();
}

bool TryExtractQuantityNameKeysFromInventory(
    Inventory* inventory,
    std::vector<QuantityNameKey>* outKeys)
{
    if (inventory == 0 || outKeys == 0)
    {
        return false;
    }

    outKeys->clear();

    lektor<InventorySection*>& allSections = inventory->getAllSections();
    if (allSections.valid() && allSections.size() > 0)
    {
        std::vector<QuantityNameKey> mergedSectionKeys;
        for (uint32_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
        {
            InventorySection* section = allSections[sectionIndex];
            std::vector<QuantityNameKey> sectionKeys;
            if (!TryExtractQuantityNameKeysFromInventorySection(section, &sectionKeys))
            {
                continue;
            }

            for (std::size_t keyIndex = 0; keyIndex < sectionKeys.size(); ++keyIndex)
            {
                if (!sectionKeys[keyIndex].key.empty())
                {
                    mergedSectionKeys.push_back(sectionKeys[keyIndex]);
                }
            }
        }

        if (!mergedSectionKeys.empty())
        {
            outKeys->swap(mergedSectionKeys);
            return true;
        }
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid() || allItems.size() == 0)
    {
        return false;
    }

    outKeys->reserve(allItems.size());
    for (uint32_t index = 0; index < allItems.size(); ++index)
    {
        Item* item = allItems[index];
        if (item == 0)
        {
            continue;
        }

        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        if (key.empty())
        {
            continue;
        }

        QuantityNameKey hint;
        hint.quantity = item->quantity;
        hint.key = key;
        outKeys->push_back(hint);
    }

    return !outKeys->empty();
}

std::string ResolveUniqueQuantityNameHint(const std::vector<QuantityNameKey>& keys, int quantity)
{
    if (quantity <= 0 || keys.empty())
    {
        return "";
    }

    std::string match;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const QuantityNameKey& hint = keys[index];
        if (hint.quantity != quantity || hint.key.empty())
        {
            continue;
        }

        if (match.empty())
        {
            match = hint.key;
            continue;
        }

        if (match != hint.key)
        {
            return "";
        }
    }

    return match;
}

std::string ResolveTopQuantityNameHints(
    const std::vector<QuantityNameKey>& keys,
    int quantity,
    std::size_t maxHints)
{
    if (quantity <= 0 || keys.empty() || maxHints == 0)
    {
        return "";
    }

    struct NameScore
    {
        std::string key;
        int count;
    };

    std::vector<NameScore> scores;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const QuantityNameKey& quantityName = keys[index];
        if (quantityName.quantity != quantity || quantityName.key.empty())
        {
            continue;
        }

        bool merged = false;
        for (std::size_t existing = 0; existing < scores.size(); ++existing)
        {
            if (scores[existing].key == quantityName.key)
            {
                ++scores[existing].count;
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            NameScore score;
            score.key = quantityName.key;
            score.count = 1;
            scores.push_back(score);
        }
    }

    if (scores.empty())
    {
        return "";
    }

    struct NameScoreGreater
    {
        bool operator()(const NameScore& left, const NameScore& right) const
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }
            return left.key < right.key;
        }
    };
    std::sort(scores.begin(), scores.end(), NameScoreGreater());

    std::string merged;
    const std::size_t limit = scores.size() < maxHints ? scores.size() : maxHints;
    for (std::size_t index = 0; index < limit; ++index)
    {
        if (!merged.empty())
        {
            merged.push_back(' ');
        }
        merged.append(scores[index].key);
    }

    return merged;
}

bool BuildAlignedInventoryNameHintsByQuantity(
    const std::vector<int>& uiQuantities,
    const std::vector<QuantityNameKey>& inventoryQuantityNameKeys,
    std::vector<std::string>* outAlignedNames)
{
    if (outAlignedNames == 0)
    {
        return false;
    }

    outAlignedNames->assign(uiQuantities.size(), "");
    if (uiQuantities.empty() || inventoryQuantityNameKeys.empty())
    {
        return false;
    }

    const std::size_t n = uiQuantities.size();
    const std::size_t m = inventoryQuantityNameKeys.size();
    const int gapScore = -2;
    const int mismatchScore = -3;
    const int matchScore = 8;

    std::vector<int> dp((n + 1) * (m + 1), 0);
    std::vector<unsigned char> dir((n + 1) * (m + 1), 0);

    for (std::size_t i = 1; i <= n; ++i)
    {
        dp[i * (m + 1)] = static_cast<int>(i) * gapScore;
        dir[i * (m + 1)] = 1;
    }
    for (std::size_t j = 1; j <= m; ++j)
    {
        dp[j] = static_cast<int>(j) * gapScore;
        dir[j] = 2;
    }

    for (std::size_t i = 1; i <= n; ++i)
    {
        for (std::size_t j = 1; j <= m; ++j)
        {
            const int uiQuantity = uiQuantities[i - 1];
            const int inventoryQuantity = inventoryQuantityNameKeys[j - 1].quantity;
            const bool quantityMatch = uiQuantity > 0 && uiQuantity == inventoryQuantity;

            const int diag = dp[(i - 1) * (m + 1) + (j - 1)] + (quantityMatch ? matchScore : mismatchScore);
            const int up = dp[(i - 1) * (m + 1) + j] + gapScore;
            const int left = dp[i * (m + 1) + (j - 1)] + gapScore;

            unsigned char bestDir = 0;
            int bestScore = diag;
            if (up > bestScore)
            {
                bestScore = up;
                bestDir = 1;
            }
            if (left > bestScore)
            {
                bestScore = left;
                bestDir = 2;
            }

            if (quantityMatch && diag == bestScore)
            {
                bestDir = 0;
            }

            dp[i * (m + 1) + j] = bestScore;
            dir[i * (m + 1) + j] = bestDir;
        }
    }

    std::size_t i = n;
    std::size_t j = m;
    std::size_t matchedCount = 0;
    while (i > 0 || j > 0)
    {
        unsigned char step = 0;
        if (i > 0 && j > 0)
        {
            step = dir[i * (m + 1) + j];
        }
        else if (i > 0)
        {
            step = 1;
        }
        else
        {
            step = 2;
        }

        if (step == 0 && i > 0 && j > 0)
        {
            const int uiQuantity = uiQuantities[i - 1];
            const QuantityNameKey& quantityName = inventoryQuantityNameKeys[j - 1];
            if (uiQuantity > 0
                && uiQuantity == quantityName.quantity
                && !quantityName.key.empty())
            {
                (*outAlignedNames)[i - 1] = quantityName.key;
                ++matchedCount;
            }
            --i;
            --j;
            continue;
        }

        if (step == 1 && i > 0)
        {
            --i;
            continue;
        }

        if (j > 0)
        {
            --j;
        }
    }

    return matchedCount > 0;
}
bool TryResolveInventoryNameKeysFromCandidates(
    const std::vector<InventoryCandidateInfo>& candidates,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory)
{
    if (outKeys == 0 || candidates.empty())
    {
        return false;
    }
    if (outSelectedInventory != 0)
    {
        *outSelectedInventory = 0;
    }

    const std::string& normalizedQuery = TraderState().search.g_searchQueryNormalized;

    Inventory* bestInventory = 0;
    std::vector<std::string> bestKeys;
    std::vector<QuantityNameKey> bestQuantityKeys;
    std::string bestSource;
    int bestScore = -1000000;
    int bestAlignedMatches = 0;
    int bestAlignedTotal = 0;
    int bestQueryMatches = 0;
    int bestNonEmptyCount = 0;
    Inventory* bestCoverageInventory = 0;
    std::vector<std::string> bestCoverageKeys;
    std::vector<QuantityNameKey> bestCoverageQuantityKeys;
    std::string bestCoverageSource;
    int bestCoverageScore = -1000000;
    int bestCoverageAlignedMatches = 0;
    int bestCoverageAlignedTotal = 0;
    int bestCoverageQueryMatches = 0;
    int bestCoverageNonEmptyCount = 0;
    bool usedCoverageFallback = false;

    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const InventoryCandidateInfo& candidate = candidates[index];
        if (candidate.inventory == 0)
        {
            continue;
        }

        std::vector<std::string> keys;
        if (!TryExtractSearchKeysFromInventory(candidate.inventory, &keys))
        {
            continue;
        }
        std::vector<QuantityNameKey> quantityKeys;
        TryExtractQuantityNameKeysFromInventory(candidate.inventory, &quantityKeys);
        int candidateAlignedMatches = 0;
        int candidateAlignedTotal = 0;
        int candidateQueryMatches = 0;

        const int keyCount = static_cast<int>(keys.size());
        const int nonEmptyKeyCount = static_cast<int>(CountNonEmptyKeysLocal(keys));
        const int emptyKeyCount = keyCount - nonEmptyKeyCount;
        const int expected = static_cast<int>(expectedEntryCount);
        const int diff =
            nonEmptyKeyCount > expected
                ? nonEmptyKeyCount - expected
                : expected - nonEmptyKeyCount;
        const bool lowCoverage = expected > 0 && nonEmptyKeyCount * 2 < expected;
        const bool noUsableNames = nonEmptyKeyCount == 0;

        int score = 0;
        score += nonEmptyKeyCount * 28;
        score -= emptyKeyCount * 140;
        score -= diff * 28;
        if (diff == 0)
        {
            score += 2200;
        }
        else if (diff <= 1)
        {
            score += 900;
        }
        else if (lowCoverage)
        {
            score -= 2500;
        }
        if (candidate.traderPreferred)
        {
            score += 420;
        }
        if (candidate.visible)
        {
            score += 220;
        }
        if (noUsableNames)
        {
            score -= 5400;
        }

        score += candidate.priorityBias;

        const std::string sourceLower = NormalizeSearchText(candidate.source);
        if (sourceLower.find("nearby") != std::string::npos)
        {
            score += 120;
        }
        if (sourceLower.find("root candidate") != std::string::npos)
        {
            score -= 140;
        }

        if (!normalizedQuery.empty())
        {
            for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
            {
                if (keys[keyIndex].find(normalizedQuery) != std::string::npos)
                {
                    ++candidateQueryMatches;
                }
            }

            if (candidateQueryMatches > 0)
            {
                const int queryMatchBoost = lowCoverage ? 140 : 520;
                score += candidateQueryMatches * queryMatchBoost;
                if (candidateQueryMatches >= 2 && !lowCoverage)
                {
                    score += 260;
                }
                if (sourceLower.find("nearby") != std::string::npos)
                {
                    score += lowCoverage ? 40 : 220;
                }
            }
            else if (normalizedQuery.size() >= 2)
            {
                score -= 980;
            }
        }

        if (uiQuantities != 0 && !uiQuantities->empty() && !quantityKeys.empty())
        {
            int sequenceAlignedMatches = 0;
            {
                std::vector<std::string> alignedNames;
                if (BuildAlignedInventoryNameHintsByQuantity(*uiQuantities, quantityKeys, &alignedNames))
                {
                    for (std::size_t alignedIndex = 0; alignedIndex < alignedNames.size(); ++alignedIndex)
                    {
                        if (!alignedNames[alignedIndex].empty())
                        {
                            ++sequenceAlignedMatches;
                        }
                    }
                }
            }
            candidateAlignedMatches = sequenceAlignedMatches;
            candidateAlignedTotal = static_cast<int>(uiQuantities->size());
            score += sequenceAlignedMatches * 180;
            if (uiQuantities->size() >= 10
                && sequenceAlignedMatches * 3 < static_cast<int>(uiQuantities->size()))
            {
                score -= 2800;
            }

            const std::size_t quantityCompareCount =
                uiQuantities->size() < quantityKeys.size()
                    ? uiQuantities->size()
                    : quantityKeys.size();

            int exactQuantityPositionMatches = 0;
            for (std::size_t q = 0; q < quantityCompareCount; ++q)
            {
                const int uiQuantity = (*uiQuantities)[q];
                if (uiQuantity > 0 && quantityKeys[q].quantity == uiQuantity)
                {
                    ++exactQuantityPositionMatches;
                }
            }
            score += exactQuantityPositionMatches * 220;

            std::vector<int> uiSorted;
            uiSorted.reserve(uiQuantities->size());
            for (std::size_t q = 0; q < uiQuantities->size(); ++q)
            {
                const int uiQuantity = (*uiQuantities)[q];
                if (uiQuantity > 0)
                {
                    uiSorted.push_back(uiQuantity);
                }
            }
            std::vector<int> candidateSorted;
            candidateSorted.reserve(quantityKeys.size());
            for (std::size_t q = 0; q < quantityKeys.size(); ++q)
            {
                const int candidateQuantity = quantityKeys[q].quantity;
                if (candidateQuantity > 0)
                {
                    candidateSorted.push_back(candidateQuantity);
                }
            }

            std::sort(uiSorted.begin(), uiSorted.end());
            std::sort(candidateSorted.begin(), candidateSorted.end());

            std::size_t uiIndex = 0;
            std::size_t candidateIndex = 0;
            int multisetQuantityMatches = 0;
            while (uiIndex < uiSorted.size() && candidateIndex < candidateSorted.size())
            {
                if (uiSorted[uiIndex] == candidateSorted[candidateIndex])
                {
                    ++multisetQuantityMatches;
                    ++uiIndex;
                    ++candidateIndex;
                }
                else if (uiSorted[uiIndex] < candidateSorted[candidateIndex])
                {
                    ++uiIndex;
                }
                else
                {
                    ++candidateIndex;
                }
            }
            score += multisetQuantityMatches * 40;
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestInventory = candidate.inventory;
            bestSource = candidate.source;
            bestKeys.swap(keys);
            bestQuantityKeys.swap(quantityKeys);
            bestAlignedMatches = candidateAlignedMatches;
            bestAlignedTotal = candidateAlignedTotal;
            bestQueryMatches = candidateQueryMatches;
            bestNonEmptyCount = nonEmptyKeyCount;
        }

        const bool coverageCandidate = expected < 8 || !lowCoverage;
        if (coverageCandidate && score > bestCoverageScore)
        {
            bestCoverageScore = score;
            bestCoverageInventory = candidate.inventory;
            bestCoverageSource = candidate.source;
            bestCoverageKeys = keys;
            bestCoverageQuantityKeys = quantityKeys;
            bestCoverageAlignedMatches = candidateAlignedMatches;
            bestCoverageAlignedTotal = candidateAlignedTotal;
            bestCoverageQueryMatches = candidateQueryMatches;
            bestCoverageNonEmptyCount = nonEmptyKeyCount;
        }
    }

    if (bestInventory == 0 || bestKeys.empty())
    {
        return false;
    }

    if (expectedEntryCount >= 8
        && bestNonEmptyCount * 2 < static_cast<int>(expectedEntryCount)
        && bestCoverageInventory != 0
        && !bestCoverageKeys.empty())
    {
        bestInventory = bestCoverageInventory;
        bestSource = bestCoverageSource;
        bestKeys.swap(bestCoverageKeys);
        bestQuantityKeys.swap(bestCoverageQuantityKeys);
        bestScore = bestCoverageScore;
        bestAlignedMatches = bestCoverageAlignedMatches;
        bestAlignedTotal = bestCoverageAlignedTotal;
        bestQueryMatches = bestCoverageQueryMatches;
        bestNonEmptyCount = bestCoverageNonEmptyCount;
        usedCoverageFallback = true;
    }

    if (bestKeys.size() > expectedEntryCount)
    {
        bestKeys.resize(expectedEntryCount);
    }
    if (bestQuantityKeys.size() > expectedEntryCount)
    {
        bestQuantityKeys.resize(expectedEntryCount);
    }

    outKeys->swap(bestKeys);
    if (outSource != 0)
    {
        std::stringstream source;
        source << bestSource;
        if (bestAlignedTotal > 0)
        {
            source << " aligned_matches=" << bestAlignedMatches
                   << "/" << bestAlignedTotal;
        }
        if (!normalizedQuery.empty())
        {
            source << " query_matches=" << bestQueryMatches;
        }
        if (usedCoverageFallback)
        {
            source << " coverage_fallback=true";
        }
        source << " non_empty=" << bestNonEmptyCount;
        *outSource = source.str();
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->swap(bestQuantityKeys);
    }
    if (outSelectedInventory != 0)
    {
        *outSelectedInventory = bestInventory;
    }
    return true;
}
