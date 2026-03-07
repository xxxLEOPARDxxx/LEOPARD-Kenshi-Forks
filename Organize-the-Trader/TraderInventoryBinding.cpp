#include "TraderInventoryBinding.h"

#include "TraderSearchText.h"
#include "TraderWindowDetection.h"

#include <core/Functions.h>
#include <kenshi/Building.h>
#include <kenshi/Character.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Dialogue.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

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

bool IsDescendantOf(MyGUI::Widget* widget, MyGUI::Widget* ancestor)
{
    if (widget == 0 || ancestor == 0)
    {
        return false;
    }

    MyGUI::Widget* current = widget;
    while (current != 0)
    {
        if (current == ancestor)
        {
            return true;
        }
        current = current->getParent();
    }

    return false;
}

std::string RootObjectDisplayNameForLog(RootObject* object)
{
    if (object == 0)
    {
        return "<null>";
    }

    if (!object->displayName.empty())
    {
        return object->displayName;
    }

    const std::string objectName = object->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (object->data != 0 && !object->data->name.empty())
    {
        return object->data->name;
    }

    return "<unnamed>";
}

std::size_t AbsoluteDiffSize(std::size_t left, std::size_t right)
{
    return left > right ? left - right : right - left;
}

InventoryGUI* TryGetInventoryGuiSafe(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    __try
    {
        return inventory->getInventoryGUI();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool IsInventoryPointerValidSafe(Inventory* inventory)
{
    if (inventory == 0)
    {
        return false;
    }

    __try
    {
        inventory->isVisible();
        inventory->getOwner();
        inventory->getAllItems();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsRootObjectPointerValidSafe(RootObject* object)
{
    if (object == 0)
    {
        return false;
    }

    __try
    {
        object->getInventory();
        object->data;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

Inventory* TryGetRootObjectInventorySafe(RootObject* object)
{
    if (object == 0)
    {
        return 0;
    }

    __try
    {
        return object->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool TryGetInventoryOwnerPointersSafe(
    Inventory* inventory,
    RootObject** outOwner,
    RootObject** outCallbackObject)
{
    if (outOwner != 0)
    {
        *outOwner = 0;
    }
    if (outCallbackObject != 0)
    {
        *outCallbackObject = 0;
    }

    if (inventory == 0)
    {
        return false;
    }

    __try
    {
        if (outOwner != 0)
        {
            *outOwner = inventory->getOwner();
        }
        if (outCallbackObject != 0)
        {
            *outCallbackObject = inventory->getCallbackObject();
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (outOwner != 0)
        {
            *outOwner = 0;
        }
        if (outCallbackObject != 0)
        {
            *outCallbackObject = 0;
        }
        return false;
    }
}

bool IsShopCounterCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    return sourceLower.find("shop counter") != std::string::npos
        || sourceLower.find("owner=shop counter") != std::string::npos
        || sourceLower.find("trader_furniture_shop") != std::string::npos;
}

bool IsTraderAnchoredCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("self inventory true") != std::string::npos
        || sourceLower.find("trader home caption") != std::string::npos
        || sourceLower.find("caption trader") != std::string::npos
        || sourceLower.find("dialog target") != std::string::npos
        || sourceLower.find("widget") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader true") != std::string::npos;
}

bool IsRiskyCoverageFallbackSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader false") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("nearby shop") != std::string::npos
        || sourceLower.find("nearby caption") != std::string::npos
        || sourceLower.find("nearby dialog") != std::string::npos
        || sourceLower.find("nearby world") != std::string::npos
        || sourceLower.find("root candidate") != std::string::npos;
}

std::string StripInventorySourceDiagnostics(const std::string& source)
{
    static const char* kSuffixTokens[] =
    {
        " aligned_matches=",
        " query_matches=",
        " coverage_fallback=",
        " non_empty=",
        " nearby_shop_candidates=",
        " nearby_shop_scanned=",
        " nearby_shop_with_inventory=",
        " nearby_focused=",
        " nearby_candidates=",
        " nearby_scanned=",
        " nearby_with_inventory="
    };

    std::size_t cut = source.size();
    for (std::size_t i = 0; i < sizeof(kSuffixTokens) / sizeof(kSuffixTokens[0]); ++i)
    {
        const std::size_t tokenPos = source.find(kSuffixTokens[i]);
        if (tokenPos != std::string::npos && tokenPos < cut)
        {
            cut = tokenPos;
        }
    }

    while (cut > 0 && source[cut - 1] == ' ')
    {
        --cut;
    }
    return source.substr(0, cut);
}

std::string BuildKeysetSourceId(const std::string& source)
{
    return NormalizeSearchText(StripInventorySourceDiagnostics(source));
}

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

bool TryResolveTraderInventoryNameKeysFromWindowCaption(
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

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    if (owningWindow == 0)
    {
        return false;
    }

    const std::string windowCaption = owningWindow->getCaption().asUTF8();
    const std::string normalizedCaption = NormalizeSearchText(windowCaption);
    if (normalizedCaption.empty())
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t captionMatchCount = 0;

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        const std::string characterName = CharacterNameForLog(candidate);
        const std::string normalizedCharacterName = NormalizeSearchText(characterName);
        const int captionBias = ComputeCaptionNameMatchBias(normalizedCaption, normalizedCharacterName);
        if (captionBias <= 0)
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);

        std::stringstream src;
        src << "caption_trader:" << characterName
            << " visible=" << (inventoryVisible ? "true" : "false")
            << " caption_score=" << captionBias;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            candidate->inventory,
            src.str(),
            true,
            inventoryVisible,
            captionBias);
        ++captionMatchCount;
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved || outSource == 0)
    {
        return resolved;
    }

    std::stringstream source;
    source << *outSource
           << " caption_matches=" << captionMatchCount
           << " caption=\"" << TruncateForLog(windowCaption, 48) << "\"";
    *outSource = source.str();
    return true;
}

bool TryResolveTraderInventoryNameKeysFromDialogue(
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

    std::vector<Character*> playerCharacters;
    Character* selected = ou->player->selectedCharacter.getCharacter();
    if (selected != 0)
    {
        playerCharacters.push_back(selected);
    }

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (candidate == 0 || candidate == selected)
        {
            continue;
        }
        playerCharacters.push_back(candidate);
    }

    if (playerCharacters.empty())
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    for (std::size_t charIndex = 0; charIndex < playerCharacters.size(); ++charIndex)
    {
        Character* playerChar = playerCharacters[charIndex];
        if (playerChar == 0)
        {
            continue;
        }

        Dialogue* dialogue = playerChar->dialogue;
        if (dialogue == 0)
        {
            continue;
        }

        Character* target = dialogue->getConversationTarget().getCharacter();
        if (target == 0)
        {
            continue;
        }

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = !dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer;
        const bool targetIsTrader = target->isATrader();
        if (!dialogActive && !targetIsTrader)
        {
            continue;
        }
        if (!engaged && !targetIsTrader && !playerInventoryVisible && !targetInventoryVisible)
        {
            continue;
        }

        if (target->inventory != 0)
        {
            std::stringstream src;
            src << "dialog_target:" << RootObjectDisplayNameForLog(target)
                << " trader=" << (target->isATrader() ? "true" : "false")
                << " visible=" << (targetInventoryVisible ? "true" : "false");
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                target->inventory,
                src.str(),
                target->isATrader(),
                targetInventoryVisible);
        }

        if (playerChar->inventory != 0)
        {
            std::stringstream src;
            src << "dialog_player:" << RootObjectDisplayNameForLog(playerChar)
                << " trader=" << (playerChar->isATrader() ? "true" : "false")
                << " visible=" << (playerInventoryVisible ? "true" : "false");
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                playerChar->inventory,
                src.str(),
                false,
                playerInventoryVisible);
        }
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

bool TryResolveTraderInventoryNameKeysFromActiveCharacters(
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

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    std::vector<InventoryCandidateInfo> inventoryCandidates;

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* character = *it;
        if (character == 0 || character->inventory == 0)
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(character, &inventoryVisible);

        const bool isSelected = character == selectedCharacter;
        const bool isTrader = character->isATrader();

        std::stringstream src;
        src << "active_char:" << CharacterNameForLog(character)
            << " trader=" << (isTrader ? "true" : "false")
            << " visible=" << (inventoryVisible ? "true" : "false")
            << " selected=" << (isSelected ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            character->inventory,
            src.str(),
            isTrader || isSelected,
            inventoryVisible || isSelected);
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

void RegisterRecentlyRefreshedInventory(Inventory* inventory)
{
    if (inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    Character* ownerCharacter = dynamic_cast<Character*>(owner);
    Character* selectedCharacter =
        (ou == 0 || ou->player == 0) ? 0 : ou->player->selectedCharacter.getCharacter();
    const bool ownerSelected = ownerCharacter != 0 && ownerCharacter == selectedCharacter;
    const bool ownerTrader = ownerCharacter != 0 && ownerCharacter->isATrader();
    const std::string ownerName = RootObjectDisplayNameForLog(owner);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    const bool visible = inventory->isVisible();

    std::vector<RefreshedInventoryLink>& refreshedInventories =
        TraderState().binding.g_recentRefreshedInventories;

    for (std::size_t index = 0; index < refreshedInventories.size(); ++index)
    {
        RefreshedInventoryLink& link = refreshedInventories[index];
        if (link.inventory != inventory)
        {
            continue;
        }

        link.itemCount = itemCount;
        link.visible = visible;
        link.ownerTrader = ownerTrader;
        link.ownerSelected = ownerSelected;
        link.ownerName = ownerName;
        link.lastSeenTick = TraderState().core.g_updateTickCounter;
        return;
    }

    RefreshedInventoryLink link;
    link.inventory = inventory;
    link.itemCount = itemCount;
    link.visible = visible;
    link.ownerTrader = ownerTrader;
    link.ownerSelected = ownerSelected;
    link.ownerName = ownerName;
    link.lastSeenTick = TraderState().core.g_updateTickCounter;
    refreshedInventories.push_back(link);
}

void PruneRecentlyRefreshedInventories()
{
    std::vector<RefreshedInventoryLink>& refreshedInventories =
        TraderState().binding.g_recentRefreshedInventories;
    if (refreshedInventories.empty())
    {
        return;
    }

    std::vector<RefreshedInventoryLink> kept;
    kept.reserve(refreshedInventories.size());
    for (std::size_t index = 0; index < refreshedInventories.size(); ++index)
    {
        const RefreshedInventoryLink& link = refreshedInventories[index];
        if (link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (TraderState().core.g_updateTickCounter > link.lastSeenTick
            && TraderState().core.g_updateTickCounter - link.lastSeenTick > 3000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    refreshedInventories.swap(kept);
}

void AddInventoryGuiPointerUnique(std::vector<InventoryGUI*>* pointers, InventoryGUI* pointer)
{
    if (pointers == 0 || pointer == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < pointers->size(); ++index)
    {
        if ((*pointers)[index] == pointer)
        {
            return;
        }
    }

    pointers->push_back(pointer);
}

bool HasInventoryGuiPointer(
    const std::vector<InventoryGUI*>& pointers,
    InventoryGUI* pointer)
{
    if (pointer == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < pointers.size(); ++index)
    {
        if (pointers[index] == pointer)
        {
            return true;
        }
    }

    return false;
}

void AddPointerAliasUnique(std::vector<std::uintptr_t>* aliases, std::uintptr_t value)
{
    if (aliases == 0 || value == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < aliases->size(); ++index)
    {
        if ((*aliases)[index] == value)
        {
            return;
        }
    }

    aliases->push_back(value);
}

void CollectPointerAliasesFromRawPointer(const void* rawPointer, std::vector<std::uintptr_t>* aliases)
{
    if (rawPointer == 0 || aliases == 0)
    {
        return;
    }

    const std::uintptr_t rawValue = reinterpret_cast<std::uintptr_t>(rawPointer);
    AddPointerAliasUnique(aliases, rawValue);

    __try
    {
        const void* firstDeref = *reinterpret_cast<const void* const*>(rawPointer);
        AddPointerAliasUnique(aliases, reinterpret_cast<std::uintptr_t>(firstDeref));
        if (firstDeref != 0)
        {
            const void* secondDeref = *reinterpret_cast<const void* const*>(firstDeref);
            AddPointerAliasUnique(aliases, reinterpret_cast<std::uintptr_t>(secondDeref));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool HasPointerAlias(
    const std::vector<std::uintptr_t>& aliases,
    const void* pointerValue)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointerValue);
    if (value == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < aliases.size(); ++index)
    {
        if (aliases[index] == value)
        {
            return true;
        }
    }

    return false;
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
std::size_t CountNonEmptyKeys(const std::vector<std::string>& keys)
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

bool IsPanelBindingConfidentForExpected(
    const TraderPanelInventoryBinding& binding,
    std::size_t expectedEntryCount)
{
    if (binding.inventory == 0 || !IsInventoryPointerValidSafe(binding.inventory))
    {
        return false;
    }

    const std::size_t baselineExpected =
        expectedEntryCount > 0 ? expectedEntryCount : binding.expectedEntryCount;
    if (baselineExpected == 0 || binding.nonEmptyKeyCount == 0)
    {
        return false;
    }

    if (AbsoluteDiffSize(binding.expectedEntryCount, baselineExpected) > 6)
    {
        return false;
    }

    if (baselineExpected >= 8 && binding.nonEmptyKeyCount * 2 < baselineExpected)
    {
        return false;
    }

    return true;
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

bool TryReadPointerValueSafe(const void* base, std::size_t offset, const void** outValue)
{
    if (base == 0 || outValue == 0)
    {
        return false;
    }

    *outValue = 0;
    __try
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(base);
        *outValue = *reinterpret_cast<const void* const*>(bytes + offset);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *outValue = 0;
        return false;
    }
}

const char* InventoryGuiBackPointerKindLabelInternal(InventoryGuiBackPointerKind kind)
{
    switch (kind)
    {
    case InventoryGuiBackPointerKind_DirectInventory:
        return "inventory";
    case InventoryGuiBackPointerKind_OwnerObject:
        return "owner";
    case InventoryGuiBackPointerKind_CallbackObject:
        return "callback";
    default:
        return "unknown";
    }
}

int InventoryGuiBackPointerKindPriority(InventoryGuiBackPointerKind kind)
{
    switch (kind)
    {
    case InventoryGuiBackPointerKind_DirectInventory:
        return 0;
    case InventoryGuiBackPointerKind_OwnerObject:
        return 1;
    case InventoryGuiBackPointerKind_CallbackObject:
        return 2;
    default:
        return 3;
    }
}

void RecordInventoryGuiBackPointerOffsetHit(
    std::vector<InventoryGuiBackPointerOffset>* hits,
    std::size_t offset,
    InventoryGuiBackPointerKind kind)
{
    if (hits == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < hits->size(); ++index)
    {
        InventoryGuiBackPointerOffset& hit = (*hits)[index];
        if (hit.offset != offset || hit.kind != kind)
        {
            continue;
        }

        ++hit.hits;
        return;
    }

    InventoryGuiBackPointerOffset hit;
    hit.offset = offset;
    hit.kind = kind;
    hit.hits = 1;
    hits->push_back(hit);
}

void LearnInventoryGuiBackPointerOffsets()
{
    if (TraderState().binding.g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    const std::size_t kMaxScanOffset = 0x400;
    std::vector<InventoryGuiBackPointerOffset> offsetHits;
    std::size_t validatedLinks = 0;
    for (std::size_t linkIndex = 0;
         linkIndex < TraderState().binding.g_inventoryGuiInventoryLinks.size();
         ++linkIndex)
    {
        const InventoryGuiInventoryLink& link =
            TraderState().binding.g_inventoryGuiInventoryLinks[linkIndex];
        if (link.inventoryGui == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        InventoryGUI* resolvedGui = TryGetInventoryGuiSafe(link.inventory);
        if (resolvedGui == 0 || resolvedGui != link.inventoryGui)
        {
            continue;
        }

        ++validatedLinks;

        RootObject* owner = 0;
        RootObject* callbackObject = 0;
        TryGetInventoryOwnerPointersSafe(link.inventory, &owner, &callbackObject);

        for (std::size_t offset = sizeof(void*); offset <= kMaxScanOffset; offset += sizeof(void*))
        {
            const void* value = 0;
            if (!TryReadPointerValueSafe(link.inventoryGui, offset, &value) || value == 0)
            {
                continue;
            }

            if (value == link.inventory)
            {
                RecordInventoryGuiBackPointerOffsetHit(
                    &offsetHits,
                    offset,
                    InventoryGuiBackPointerKind_DirectInventory);
            }

            if (owner != 0 && value == owner)
            {
                Inventory* ownerInventory = TryGetRootObjectInventorySafe(owner);
                if (ownerInventory == link.inventory)
                {
                    RecordInventoryGuiBackPointerOffsetHit(
                        &offsetHits,
                        offset,
                        InventoryGuiBackPointerKind_OwnerObject);
                }
            }

            if (callbackObject != 0 && callbackObject != owner && value == callbackObject)
            {
                Inventory* callbackInventory = TryGetRootObjectInventorySafe(callbackObject);
                if (callbackInventory == link.inventory)
                {
                    RecordInventoryGuiBackPointerOffsetHit(
                        &offsetHits,
                        offset,
                        InventoryGuiBackPointerKind_CallbackObject);
                }
            }
        }
    }

    if (offsetHits.empty())
    {
        TraderState().binding.g_inventoryGuiBackPointerOffsets.clear();
        std::stringstream signature;
        signature << "none|tracked=" << TraderState().binding.g_inventoryGuiInventoryLinks.size()
                  << "|validated=" << validatedLinks
                  << "|scan=0x" << std::hex << std::uppercase << kMaxScanOffset;
        if (signature.str() != TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature)
        {
            std::stringstream line;
            line << "inventory gui back-pointer offsets not learned"
                 << " tracked_links=" << TraderState().binding.g_inventoryGuiInventoryLinks.size()
                 << " validated_links=" << validatedLinks
                 << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
            if (ShouldLogBindingDebug())
            {
                LogWarnLine(line.str());
            }
            TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature = signature.str();
        }
        return;
    }

    std::sort(
        offsetHits.begin(),
        offsetHits.end(),
        [](const InventoryGuiBackPointerOffset& left, const InventoryGuiBackPointerOffset& right) -> bool
        {
            if (left.hits != right.hits)
            {
                return left.hits > right.hits;
            }
            const int leftPriority = InventoryGuiBackPointerKindPriority(left.kind);
            const int rightPriority = InventoryGuiBackPointerKindPriority(right.kind);
            if (leftPriority != rightPriority)
            {
                return leftPriority < rightPriority;
            }
            return left.offset < right.offset;
        });

    TraderState().binding.g_inventoryGuiBackPointerOffsets.swap(offsetHits);

    std::stringstream signature;
    for (std::size_t index = 0;
         index < TraderState().binding.g_inventoryGuiBackPointerOffsets.size();
         ++index)
    {
        const InventoryGuiBackPointerOffset& learned =
            TraderState().binding.g_inventoryGuiBackPointerOffsets[index];
        signature << InventoryGuiBackPointerKindLabelInternal(learned.kind)
                  << ":" << learned.offset
                  << ":" << learned.hits << "|";
    }

    if (signature.str() != TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature)
    {
        std::stringstream line;
        line << "inventory gui back-pointer offsets learned";
        const std::size_t previewCount =
            TraderState().binding.g_inventoryGuiBackPointerOffsets.size() < 8
                ? TraderState().binding.g_inventoryGuiBackPointerOffsets.size()
                : 8;
        for (std::size_t index = 0; index < previewCount; ++index)
        {
            const InventoryGuiBackPointerOffset& learned =
                TraderState().binding.g_inventoryGuiBackPointerOffsets[index];
            line << " offset" << index << "=0x"
                 << std::hex << std::uppercase << learned.offset
                 << std::dec
                 << "(" << InventoryGuiBackPointerKindLabelInternal(learned.kind)
                 << ",hits=" << learned.hits << ")";
        }
        line << " tracked_links=" << TraderState().binding.g_inventoryGuiInventoryLinks.size()
             << " validated_links=" << validatedLinks
             << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
        LogBindingDebugLine(line.str());
        TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature = signature.str();
    }
}
} // namespace

const char* InventoryGuiBackPointerKindLabel(InventoryGuiBackPointerKind kind)
{
    return InventoryGuiBackPointerKindLabelInternal(kind);
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

void ClearTraderPanelInventoryBindings()
{
    TraderState().binding.g_traderPanelInventoryBindings.clear();
    TraderState().binding.g_lastPanelBindingSignature.clear();
    TraderState().binding.g_lastPanelBindingRefusedSignature.clear();
    TraderState().binding.g_lastPanelBindingProbeSignature.clear();
}

bool TryGetTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    TraderPanelInventoryBinding* outBinding)
{
    if (traderParent == 0 || entriesRoot == 0)
    {
        return false;
    }

    PruneTraderPanelInventoryBindings();
    for (std::size_t index = 0;
         index < TraderState().binding.g_traderPanelInventoryBindings.size();
         ++index)
    {
        TraderPanelInventoryBinding& binding =
            TraderState().binding.g_traderPanelInventoryBindings[index];
        if (binding.traderParent != traderParent || binding.entriesRoot != entriesRoot)
        {
            continue;
        }

        if (!IsPanelBindingConfidentForExpected(binding, expectedEntryCount))
        {
            return false;
        }

        binding.lastSeenTick = TraderState().core.g_updateTickCounter;
        if (outBinding != 0)
        {
            *outBinding = binding;
        }
        return true;
    }

    return false;
}

void LogPanelBindingProbeOnce(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    const std::string& status,
    const TraderPanelInventoryBinding* binding)
{
    if (!ShouldLogBindingDebug())
    {
        return;
    }

    if (traderParent == 0 || entriesRoot == 0 || expectedEntryCount == 0)
    {
        return;
    }

    std::stringstream signature;
    signature << traderParent
              << "|" << entriesRoot
              << "|" << expectedEntryCount
              << "|" << status;
    if (signature.str() == TraderState().binding.g_lastPanelBindingProbeSignature)
    {
        return;
    }
    TraderState().binding.g_lastPanelBindingProbeSignature = signature.str();

    PruneSectionWidgetInventoryLinks();
    PruneInventoryGuiInventoryLinks();

    MyGUI::Widget* matchedSectionWidget = 0;
    Inventory* matchedSectionInventory = 0;
    std::string matchedSectionName;
    for (std::size_t index = 0;
         index < TraderState().binding.g_sectionWidgetInventoryLinks.size();
         ++index)
    {
        const SectionWidgetInventoryLink& link =
            TraderState().binding.g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (link.sectionWidget == entriesRoot || IsDescendantOf(entriesRoot, link.sectionWidget))
        {
            matchedSectionWidget = link.sectionWidget;
            matchedSectionInventory = link.inventory;
            matchedSectionName = link.sectionName;
            break;
        }
    }

    std::stringstream line;
    line << "panel binding probe"
         << " panel_root=" << SafeWidgetName(traderParent)
         << "(" << traderParent << ")"
         << " entries_root=" << SafeWidgetName(entriesRoot)
         << "(" << entriesRoot << ")"
         << " expected_entries=" << expectedEntryCount
         << " status=" << status
         << " section_widget=" << SafeWidgetName(matchedSectionWidget)
         << "(" << matchedSectionWidget << ")"
         << " section_name=\"" << matchedSectionName << "\""
         << " section_inventory=" << matchedSectionInventory
         << " section_items=" << InventoryItemCountForLog(matchedSectionInventory)
         << " tracked_section_links=" << TraderState().binding.g_sectionWidgetInventoryLinks.size()
         << " tracked_gui_links=" << TraderState().binding.g_inventoryGuiInventoryLinks.size();

    if (binding != 0 && binding->inventory != 0 && IsInventoryPointerValidSafe(binding->inventory))
    {
        line << " resolved_inventory=" << binding->inventory
             << " resolved_items=" << InventoryItemCountForLog(binding->inventory)
             << " resolved_stage=" << binding->stage
             << " resolved_source=\"" << TruncateForLog(binding->source, 180) << "\"";
        LogInfoLine(line.str());

        std::vector<std::string> keys;
        if (TryExtractSearchKeysFromInventory(binding->inventory, &keys) && !keys.empty())
        {
            std::stringstream preview;
            preview << "panel binding probe key preview "
                    << BuildKeyPreviewForLog(keys, 14);
            LogInfoLine(preview.str());
        }
    }
    else
    {
        line << " resolved_inventory=0x0";
        LogWarnLine(line.str());
    }
}

void RegisterTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    Inventory* inventory,
    const char* stage,
    const std::string& source,
    std::size_t expectedEntryCount,
    std::size_t nonEmptyKeyCount)
{
    if (traderParent == 0
        || entriesRoot == 0
        || inventory == 0
        || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    TraderPanelInventoryBinding updated;
    updated.traderParent = traderParent;
    updated.entriesRoot = entriesRoot;
    updated.inventory = inventory;
    updated.stage = stage == 0 ? "unknown" : stage;
    updated.source = source;
    updated.expectedEntryCount = expectedEntryCount;
    updated.nonEmptyKeyCount = nonEmptyKeyCount;
    updated.lastSeenTick = TraderState().core.g_updateTickCounter;

    bool replaced = false;
    for (std::size_t index = 0;
         index < TraderState().binding.g_traderPanelInventoryBindings.size();
         ++index)
    {
        TraderPanelInventoryBinding& binding =
            TraderState().binding.g_traderPanelInventoryBindings[index];
        if (binding.traderParent != traderParent || binding.entriesRoot != entriesRoot)
        {
            continue;
        }

        binding = updated;
        replaced = true;
        break;
    }
    if (!replaced)
    {
        TraderState().binding.g_traderPanelInventoryBindings.push_back(updated);
    }

    std::stringstream signature;
    signature << traderParent
              << "|" << entriesRoot
              << "|" << inventory
              << "|stage=" << updated.stage
              << "|expected=" << expectedEntryCount
              << "|non_empty=" << nonEmptyKeyCount;
    if (signature.str() != TraderState().binding.g_lastPanelBindingSignature)
    {
        std::stringstream line;
        line << "panel inventory binding updated"
             << " stage=" << updated.stage
             << " expected=" << expectedEntryCount
             << " non_empty=" << nonEmptyKeyCount
             << " parent=" << SafeWidgetName(traderParent)
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " source=\"" << TruncateForLog(source, 180) << "\"";
        LogInfoLine(line.str());
        TraderState().binding.g_lastPanelBindingSignature = signature.str();
    }
}

void PruneTraderPanelInventoryBindings()
{
    if (TraderState().binding.g_traderPanelInventoryBindings.empty())
    {
        return;
    }

    std::vector<TraderPanelInventoryBinding> kept;
    kept.reserve(TraderState().binding.g_traderPanelInventoryBindings.size());
    for (std::size_t index = 0;
         index < TraderState().binding.g_traderPanelInventoryBindings.size();
         ++index)
    {
        const TraderPanelInventoryBinding& binding =
            TraderState().binding.g_traderPanelInventoryBindings[index];
        if (binding.traderParent == 0
            || binding.entriesRoot == 0
            || binding.inventory == 0
            || !IsInventoryPointerValidSafe(binding.inventory))
        {
            continue;
        }

        if (TraderState().core.g_updateTickCounter > binding.lastSeenTick
            && TraderState().core.g_updateTickCounter - binding.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(binding);
    }

    TraderState().binding.g_traderPanelInventoryBindings.swap(kept);
}

void RegisterSectionWidgetInventoryLink(
    MyGUI::Widget* sectionWidget,
    Inventory* inventory,
    const std::string& sectionName)
{
    if (sectionWidget == 0 || inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    const std::string widgetName = SafeWidgetName(sectionWidget);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    for (std::size_t index = 0;
         index < TraderState().binding.g_sectionWidgetInventoryLinks.size();
         ++index)
    {
        SectionWidgetInventoryLink& link =
            TraderState().binding.g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget != sectionWidget)
        {
            continue;
        }

        link.inventory = inventory;
        link.sectionName = sectionName;
        link.widgetName = widgetName;
        link.itemCount = itemCount;
        link.lastSeenTick = TraderState().core.g_updateTickCounter;
        return;
    }

    SectionWidgetInventoryLink link;
    link.sectionWidget = sectionWidget;
    link.inventory = inventory;
    link.sectionName = sectionName;
    link.widgetName = widgetName;
    link.itemCount = itemCount;
    link.lastSeenTick = TraderState().core.g_updateTickCounter;
    TraderState().binding.g_sectionWidgetInventoryLinks.push_back(link);
}

void PruneSectionWidgetInventoryLinks()
{
    if (TraderState().binding.g_sectionWidgetInventoryLinks.empty())
    {
        return;
    }

    std::vector<SectionWidgetInventoryLink> kept;
    kept.reserve(TraderState().binding.g_sectionWidgetInventoryLinks.size());
    for (std::size_t index = 0;
         index < TraderState().binding.g_sectionWidgetInventoryLinks.size();
         ++index)
    {
        const SectionWidgetInventoryLink& link =
            TraderState().binding.g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (TraderState().core.g_updateTickCounter > link.lastSeenTick
            && TraderState().core.g_updateTickCounter - link.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    TraderState().binding.g_sectionWidgetInventoryLinks.swap(kept);
}

void RegisterInventoryGuiInventoryLink(InventoryGUI* inventoryGui, Inventory* inventory)
{
    if (inventoryGui == 0 || inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    const std::string ownerName = RootObjectDisplayNameForLog(owner);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    for (std::size_t index = 0;
         index < TraderState().binding.g_inventoryGuiInventoryLinks.size();
         ++index)
    {
        InventoryGuiInventoryLink& link =
            TraderState().binding.g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui != inventoryGui)
        {
            continue;
        }

        link.inventory = inventory;
        link.ownerName = ownerName;
        link.itemCount = itemCount;
        link.lastSeenTick = TraderState().core.g_updateTickCounter;

        std::stringstream signature;
        signature << inventoryGui << "|" << inventory
                  << "|" << ownerName << "|" << itemCount;
        if (signature.str() != TraderState().binding.g_lastInventoryGuiBindingSignature)
        {
            std::stringstream line;
            line << "inventory layout gui binding"
                 << " inv_gui=" << inventoryGui
                 << " owner=" << ownerName
                 << " inv_items=" << itemCount;
            LogBindingDebugLine(line.str());
            TraderState().binding.g_lastInventoryGuiBindingSignature = signature.str();
        }
        return;
    }

    InventoryGuiInventoryLink link;
    link.inventoryGui = inventoryGui;
    link.inventory = inventory;
    link.ownerName = ownerName;
    link.itemCount = itemCount;
    link.lastSeenTick = TraderState().core.g_updateTickCounter;
    TraderState().binding.g_inventoryGuiInventoryLinks.push_back(link);

    std::stringstream signature;
    signature << inventoryGui << "|" << inventory
              << "|" << ownerName << "|" << itemCount;
    if (signature.str() != TraderState().binding.g_lastInventoryGuiBindingSignature)
    {
        std::stringstream line;
        line << "inventory layout gui binding"
             << " inv_gui=" << inventoryGui
             << " owner=" << ownerName
             << " inv_items=" << itemCount;
        LogBindingDebugLine(line.str());
        TraderState().binding.g_lastInventoryGuiBindingSignature = signature.str();
    }
}

void PruneInventoryGuiInventoryLinks()
{
    if (TraderState().binding.g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    std::vector<InventoryGuiInventoryLink> kept;
    kept.reserve(TraderState().binding.g_inventoryGuiInventoryLinks.size());
    for (std::size_t index = 0;
         index < TraderState().binding.g_inventoryGuiInventoryLinks.size();
         ++index)
    {
        const InventoryGuiInventoryLink& link =
            TraderState().binding.g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (TraderState().core.g_updateTickCounter > link.lastSeenTick
            && TraderState().core.g_updateTickCounter - link.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    TraderState().binding.g_inventoryGuiInventoryLinks.swap(kept);
}

void ClearInventoryGuiInventoryLinks()
{
    TraderState().binding.g_inventoryGuiInventoryLinks.clear();
    TraderState().binding.g_lastInventoryGuiBindingSignature.clear();
}

bool TryResolveInventoryFromInventoryGuiBackPointerOffsets(
    InventoryGUI* inventoryGui,
    Inventory** outInventory,
    std::size_t* outOffset,
    InventoryGuiBackPointerKind* outKind,
    RootObject** outResolvedOwner)
{
    if (outInventory == 0 || inventoryGui == 0)
    {
        return false;
    }

    *outInventory = 0;
    if (outOffset != 0)
    {
        *outOffset = 0;
    }
    if (outKind != 0)
    {
        *outKind = InventoryGuiBackPointerKind_DirectInventory;
    }
    if (outResolvedOwner != 0)
    {
        *outResolvedOwner = 0;
    }

    LearnInventoryGuiBackPointerOffsets();
    if (TraderState().binding.g_inventoryGuiBackPointerOffsets.empty())
    {
        return false;
    }

    for (std::size_t index = 0;
         index < TraderState().binding.g_inventoryGuiBackPointerOffsets.size();
         ++index)
    {
        const InventoryGuiBackPointerOffset& learned =
            TraderState().binding.g_inventoryGuiBackPointerOffsets[index];
        const std::size_t offset = learned.offset;
        const void* value = 0;
        if (!TryReadPointerValueSafe(inventoryGui, offset, &value) || value == 0)
        {
            continue;
        }

        Inventory* candidateInventory = 0;
        RootObject* resolvedOwner = 0;
        if (learned.kind == InventoryGuiBackPointerKind_DirectInventory)
        {
            candidateInventory = reinterpret_cast<Inventory*>(const_cast<void*>(value));
            if (!IsInventoryPointerValidSafe(candidateInventory))
            {
                candidateInventory = 0;
            }
        }
        else
        {
            resolvedOwner = reinterpret_cast<RootObject*>(const_cast<void*>(value));
            if (!IsRootObjectPointerValidSafe(resolvedOwner))
            {
                continue;
            }

            candidateInventory = TryGetRootObjectInventorySafe(resolvedOwner);
            if (!IsInventoryPointerValidSafe(candidateInventory))
            {
                continue;
            }

            RootObject* inventoryOwner = 0;
            RootObject* inventoryCallbackObject = 0;
            TryGetInventoryOwnerPointersSafe(
                candidateInventory,
                &inventoryOwner,
                &inventoryCallbackObject);

            if (resolvedOwner != inventoryOwner && resolvedOwner != inventoryCallbackObject)
            {
                continue;
            }
        }

        if (candidateInventory == 0 || !IsInventoryPointerValidSafe(candidateInventory))
        {
            continue;
        }

        if (outInventory != 0)
        {
            *outInventory = candidateInventory;
        }
        if (outOffset != 0)
        {
            *outOffset = offset;
        }
        if (outKind != 0)
        {
            *outKind = learned.kind;
        }
        if (outResolvedOwner != 0)
        {
            *outResolvedOwner = resolvedOwner;
        }
        return true;
    }

    std::stringstream signature;
    signature << inventoryGui << "|miss|" << TraderState().binding.g_inventoryGuiBackPointerOffsets.size();
    if (signature.str() != TraderState().binding.g_lastInventoryGuiBackPointerResolutionFailureSignature)
    {
        std::stringstream line;
        line << "inventory gui back-pointer unresolved"
             << " inv_gui=" << inventoryGui
             << " learned_offsets=" << TraderState().binding.g_inventoryGuiBackPointerOffsets.size();
        if (ShouldLogBindingDebug())
        {
            LogWarnLine(line.str());
        }
        TraderState().binding.g_lastInventoryGuiBackPointerResolutionFailureSignature = signature.str();
    }

    return false;
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
        const int nonEmptyKeyCount = static_cast<int>(CountNonEmptyKeys(keys));
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
