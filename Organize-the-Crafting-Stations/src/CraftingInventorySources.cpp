#include "CraftingInventoryBinding.h"

#include "CraftingSearchText.h"
#include "CraftingWindowDetection.h"

#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <Windows.h>

#include <sstream>
#include <vector>

namespace
{
template <typename T>
T* ReadWidgetUserDataPointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->getUserData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetInternalDataPointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->_getInternalData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetAnyDataPointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T* pointerInternal = ReadWidgetInternalDataPointerLocal<T>(widget);
    if (pointerInternal != 0)
    {
        return pointerInternal;
    }

    T* pointerUser = ReadWidgetUserDataPointerLocal<T>(widget);
    if (pointerUser != 0)
    {
        return pointerUser;
    }

    return 0;
}

Item* ResolveWidgetItemPointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Item* item = ReadWidgetAnyDataPointerLocal<Item>(widget);
    if (item != 0)
    {
        return item;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointerLocal<InventoryItemBase>(widget);
    if (itemBase == 0)
    {
        return 0;
    }

    return dynamic_cast<Item*>(itemBase);
}

RootObjectBase* ResolveWidgetObjectBasePointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    RootObjectBase* objectBase = ReadWidgetAnyDataPointerLocal<RootObjectBase>(widget);
    if (objectBase != 0)
    {
        return objectBase;
    }

    RootObject* object = ReadWidgetAnyDataPointerLocal<RootObject>(widget);
    if (object != 0)
    {
        return object;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointerLocal<InventoryItemBase>(widget);
    if (itemBase != 0)
    {
        return itemBase;
    }

    return 0;
}

Inventory* ResolveWidgetInventoryPointerLocal(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Inventory* inventory = ReadWidgetAnyDataPointerLocal<Inventory>(widget);
    if (inventory != 0)
    {
        return inventory;
    }

    Item* item = ResolveWidgetItemPointerLocal(widget);
    if (item != 0)
    {
        inventory = item->getInventory();
        if (inventory != 0)
        {
            return inventory;
        }
    }

    InventorySection* section = ReadWidgetAnyDataPointerLocal<InventorySection>(widget);
    if (section != 0)
    {
        Inventory* sectionInventory = section->getInventory();
        if (sectionInventory != 0)
        {
            return sectionInventory;
        }
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointerLocal(widget);
    RootObject* object = dynamic_cast<RootObject*>(objectBase);
    if (object != 0)
    {
        return object->getInventory();
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = handValue->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = (*handPointer)->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    return 0;
}

Inventory* TryGetInventoryFromItemSafeLocal(Item* item)
{
    if (item == 0)
    {
        return 0;
    }

    __try
    {
        return item->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

Item* TryGetSelectedObjectItemSafeLocal(PlayerInterface* player)
{
    if (player == 0)
    {
        return 0;
    }

    __try
    {
        if (player->selectedObject.isValid())
        {
            return player->selectedObject.getItem();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    return 0;
}

Item* TryGetMouseTargetItemSafeLocal(PlayerInterface* player)
{
    if (player == 0 || player->mouseRightTarget == 0)
    {
        return 0;
    }

    __try
    {
        return player->mouseRightTarget->getHandle().getItem();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void AddSelectedItemInventoryCandidateLocal(
    Item* item,
    const char* origin,
    std::vector<InventoryCandidateInfo>* inventoryCandidates)
{
    if (item == 0 || inventoryCandidates == 0)
    {
        return;
    }

    Inventory* inventory = TryGetInventoryFromItemSafeLocal(item);
    if (inventory == 0)
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    std::stringstream source;
    source << "selected_item:" << (origin == 0 ? "unknown" : origin)
           << ":" << TruncateForLog(ResolveCanonicalItemName(item), 48)
           << " owner=" << RootObjectDisplayNameForLog(owner)
           << " visible=" << (inventory->isVisible() ? "true" : "false")
           << " owner_items=" << InventoryItemCountForLog(owner == 0 ? 0 : owner->getInventory());
    AddInventoryCandidateUnique(
        inventoryCandidates,
        inventory,
        source.str(),
        true,
        inventory->isVisible(),
        5200);
}

void UpdateHoveredInventoryCacheLocal(
    Inventory* inventory,
    MyGUI::Widget* hoveredWidget,
    const char* sourceTag)
{
    if (inventory == 0)
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    std::stringstream signature;
    signature << inventory
              << "|owner=" << RootObjectDisplayNameForLog(owner)
              << "|items=" << InventoryItemCountForLog(inventory);
    if (sourceTag != 0)
    {
        signature << "|source=" << sourceTag;
    }

    TraderState().binding.g_cachedHoveredWidgetInventory = inventory;
    if (signature.str() != TraderState().binding.g_cachedHoveredWidgetInventorySignature)
    {
        std::stringstream line;
        line << "hovered inventory cached"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " inventory_items=" << InventoryItemCountForLog(inventory)
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " hovered_widget=" << SafeWidgetName(hoveredWidget);
        LogBindingDebugLine(line.str());
        TraderState().binding.g_cachedHoveredWidgetInventorySignature = signature.str();
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

void CollectWidgetChainInventoryCandidates(
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (rootWidget == 0 || outCandidates == 0)
    {
        return;
    }

    MyGUI::Widget* current = rootWidget;
    for (int depth = 0; current != 0 && depth < 12; ++depth)
    {
        Inventory* inventory = ResolveWidgetInventoryPointerLocal(current);
        if (inventory != 0)
        {
            RootObject* owner = inventory->getOwner();
            if (owner == 0)
            {
                owner = inventory->getCallbackObject();
            }

            std::stringstream source;
            source << (sourcePrefix == 0 ? "widget_inventory" : sourcePrefix)
                   << " root=" << SafeWidgetName(rootWidget)
                   << " via=" << SafeWidgetName(current)
                   << " depth=" << depth
                   << " owner=" << RootObjectDisplayNameForLog(owner)
                   << " visible=" << (inventory->isVisible() ? "true" : "false")
                   << " items=" << InventoryItemCountForLog(inventory);
            AddInventoryCandidateUnique(
                outCandidates,
                inventory,
                source.str(),
                true,
                inventory->isVisible(),
                basePriorityBias - depth * 90);
        }

        current = current->getParent();
    }
}

namespace
{
void CollectWidgetTreeInventoryCandidatesRecursive(
    MyGUI::Widget* widget,
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::size_t* nodesVisited,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (widget == 0
        || rootWidget == 0
        || outCandidates == 0
        || nodesVisited == 0
        || *nodesVisited >= maxNodes)
    {
        return;
    }

    ++(*nodesVisited);

    Inventory* inventory = ResolveWidgetInventoryPointerLocal(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }

        std::stringstream source;
        source << (sourcePrefix == 0 ? "widget_tree" : sourcePrefix)
               << " root=" << SafeWidgetName(rootWidget)
               << " via=" << SafeWidgetName(widget)
               << " depth=" << depth
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " visible=" << (inventory->isVisible() ? "true" : "false")
               << " items=" << InventoryItemCountForLog(inventory);
        AddInventoryCandidateUnique(
            outCandidates,
            inventory,
            source.str(),
            true,
            inventory->isVisible(),
            basePriorityBias - static_cast<int>(depth) * 40);
    }

    if (depth >= maxDepth)
    {
        return;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (*nodesVisited >= maxNodes)
        {
            break;
        }

        CollectWidgetTreeInventoryCandidatesRecursive(
            widget->getChildAt(childIndex),
            rootWidget,
            sourcePrefix,
            basePriorityBias,
            depth + 1,
            maxDepth,
            maxNodes,
            nodesVisited,
            outCandidates);
    }
}
} // namespace

void CollectWidgetTreeInventoryCandidates(
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (rootWidget == 0 || outCandidates == 0)
    {
        return;
    }

    std::size_t nodesVisited = 0;
    CollectWidgetTreeInventoryCandidatesRecursive(
        rootWidget,
        rootWidget,
        sourcePrefix,
        basePriorityBias,
        0,
        maxDepth,
        maxNodes,
        &nodesVisited,
        outCandidates);
}

bool TryResolveTraderInventoryNameKeysFromWidgetBindings(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    if (backpackContent != 0)
    {
        CollectWidgetChainInventoryCandidates(
            backpackContent,
            "widget_backpack",
            7600,
            &inventoryCandidates);
    }

    MyGUI::Widget* scrollBackpackContent = FindAncestorByToken(backpackContent, "scrollview_backpack_content");
    if (scrollBackpackContent == 0)
    {
        scrollBackpackContent = FindWidgetInParentByToken(traderParent, "scrollview_backpack_content");
    }
    if (scrollBackpackContent != 0)
    {
        CollectWidgetChainInventoryCandidates(
            scrollBackpackContent,
            "widget_scroll",
            7400,
            &inventoryCandidates);
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot != 0)
    {
        CollectWidgetChainInventoryCandidates(
            entriesRoot,
            "widget_entries",
            7800,
            &inventoryCandidates);

        CollectWidgetTreeInventoryCandidates(
            entriesRoot,
            "widget_tree_entries",
            8600,
            7,
            1200,
            &inventoryCandidates);
    }

    CollectWidgetChainInventoryCandidates(
        traderParent,
        "widget_parent",
        7000,
        &inventoryCandidates);
    CollectWidgetTreeInventoryCandidates(
        traderParent,
        "widget_tree_parent",
        8200,
        6,
        1200,
        &inventoryCandidates);

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);

    std::vector<InventoryGUI*> widgetInventoryGuis;
    CollectWidgetInventoryGuiPointers(backpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(scrollBackpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(entriesRoot, 3, 256, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(traderParent, 2, 192, &widgetInventoryGuis);
    if (owningWindow != 0)
    {
        CollectWidgetInventoryGuiPointers(owningWindow, 6, 2400, &widgetInventoryGuis);
        CollectWidgetTreeInventoryCandidates(
            owningWindow,
            "widget_tree_window",
            8000,
            6,
            1600,
            &inventoryCandidates);
    }

    std::vector<std::uintptr_t> widgetPointerAliases;
    for (std::size_t pointerIndex = 0; pointerIndex < widgetInventoryGuis.size(); ++pointerIndex)
    {
        CollectPointerAliasesFromRawPointer(widgetInventoryGuis[pointerIndex], &widgetPointerAliases);
    }

    std::vector<InventoryCandidateInfo> ownershipCandidates;

    const std::string normalizedCaption =
        owningWindow == 0 ? "" : NormalizeSearchText(owningWindow->getCaption().asUTF8());

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* character = *it;
        if (character == 0 || !character->isATrader() || character->inventory == 0)
        {
            continue;
        }

        int captionBias = 0;
        if (!normalizedCaption.empty())
        {
            captionBias = ComputeCaptionNameMatchBias(
                normalizedCaption,
                NormalizeSearchText(CharacterNameForLog(character)));
            if (captionBias <= 0)
            {
                continue;
            }
        }

        CollectTraderOwnershipInventoryCandidates(
            character,
            captionBias,
            "widget_trader",
            &ownershipCandidates);
    }

    std::size_t guiMatchedCandidateCount = 0;
    for (std::size_t candidateIndex = 0; candidateIndex < ownershipCandidates.size(); ++candidateIndex)
    {
        const InventoryCandidateInfo& ownershipCandidate = ownershipCandidates[candidateIndex];
        InventoryGUI* candidateGui = TryGetInventoryGuiSafe(ownershipCandidate.inventory);
        const bool guiMatch =
            HasInventoryGuiPointer(widgetInventoryGuis, candidateGui)
            || HasPointerAlias(widgetPointerAliases, candidateGui)
            || HasPointerAlias(widgetPointerAliases, ownershipCandidate.inventory);
        if (!guiMatch)
        {
            continue;
        }

        ++guiMatchedCandidateCount;

        std::stringstream source;
        source << "widget_gui_match:" << ownershipCandidate.source
               << " gui_matched=true";
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            ownershipCandidate.inventory,
            source.str(),
            true,
            ownershipCandidate.visible,
            ownershipCandidate.priorityBias + 6800);
    }

    if (inventoryCandidates.empty())
    {
        if (!TraderState().binding.g_loggedWidgetInventoryCandidatesMissing)
        {
            std::stringstream line;
            line << "widget inventory candidate scan found none"
                 << " parent=" << SafeWidgetName(traderParent)
                 << " has_backpack="
                 << (backpackContent == 0 ? "false" : "true")
                 << " has_scroll="
                 << (scrollBackpackContent == 0 ? "false" : "true")
                 << " has_entries="
                 << (entriesRoot == 0 ? "false" : "true")
                 << " widget_gui_ptrs=" << widgetInventoryGuis.size()
                 << " widget_aliases=" << widgetPointerAliases.size()
                 << " ownership_candidates=" << ownershipCandidates.size()
                 << " gui_matches=" << guiMatchedCandidateCount;
            LogBindingDebugLine(line.str());
            TraderState().binding.g_loggedWidgetInventoryCandidatesMissing = true;
        }
        return false;
    }
    TraderState().binding.g_loggedWidgetInventoryCandidatesMissing = false;

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved || outSource == 0)
    {
        return resolved;
    }

    std::stringstream line;
    line << *outSource
         << " widget_candidates=" << inventoryCandidates.size()
         << " widget_gui_ptrs=" << widgetInventoryGuis.size()
         << " widget_aliases=" << widgetPointerAliases.size()
         << " ownership_candidates=" << ownershipCandidates.size()
         << " gui_matches=" << guiMatchedCandidateCount;
    *outSource = line.str();
    return true;
}

bool TryResolveTraderInventoryNameKeysFromSelectedItemHandles(
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

    Item* selectedItem = TryGetSelectedObjectItemSafeLocal(ou->player);
    AddSelectedItemInventoryCandidateLocal(selectedItem, "selected_object", &inventoryCandidates);

    Item* mouseTargetItem = TryGetMouseTargetItemSafeLocal(ou->player);
    AddSelectedItemInventoryCandidateLocal(mouseTargetItem, "mouse_target", &inventoryCandidates);

    if (inventoryCandidates.empty())
    {
        return false;
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

bool TryResolveTraderInventoryNameKeysFromHoveredWidget(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    MyGUI::Widget* hovered = inputManager == 0 ? 0 : inputManager->getMouseFocusWidget();
    const bool hoveredInsideEntries =
        hovered != 0 && entriesRoot != 0 && IsDescendantOf(hovered, entriesRoot);
    if (hoveredInsideEntries)
    {
        CollectWidgetChainInventoryCandidates(
            hovered,
            "hovered_entry_chain",
            9800,
            &inventoryCandidates);
        CollectWidgetTreeInventoryCandidates(
            hovered,
            "hovered_entry_tree",
            10200,
            4,
            240,
            &inventoryCandidates);

        Inventory* bestHoveredInventory = 0;
        std::size_t bestHoveredInventoryItems = 0;
        for (std::size_t index = 0; index < inventoryCandidates.size(); ++index)
        {
            Inventory* inventory = inventoryCandidates[index].inventory;
            if (inventory == 0)
            {
                continue;
            }

            const std::size_t itemCount = InventoryItemCountForLog(inventory);
            if (bestHoveredInventory == 0
                || itemCount > bestHoveredInventoryItems
                || (itemCount == bestHoveredInventoryItems
                    && inventoryCandidates[index].priorityBias > 0))
            {
                bestHoveredInventory = inventory;
                bestHoveredInventoryItems = itemCount;
            }
        }

        if (bestHoveredInventory != 0)
        {
            UpdateHoveredInventoryCacheLocal(bestHoveredInventory, hovered, "hovered_entry");
        }
    }

    if (TraderState().binding.g_cachedHoveredWidgetInventory != 0
        && !IsInventoryPointerValidSafe(TraderState().binding.g_cachedHoveredWidgetInventory))
    {
        TraderState().binding.g_cachedHoveredWidgetInventory = 0;
        TraderState().binding.g_cachedHoveredWidgetInventorySignature.clear();
    }

    if (TraderState().binding.g_cachedHoveredWidgetInventory != 0)
    {
        RootObject* owner = TraderState().binding.g_cachedHoveredWidgetInventory->getOwner();
        if (owner == 0)
        {
            owner = TraderState().binding.g_cachedHoveredWidgetInventory->getCallbackObject();
        }

        std::stringstream source;
        source << "hovered_cached"
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " visible=" << (TraderState().binding.g_cachedHoveredWidgetInventory->isVisible() ? "true" : "false")
               << " items=" << InventoryItemCountForLog(TraderState().binding.g_cachedHoveredWidgetInventory)
               << " hovered_inside_entries=" << (hoveredInsideEntries ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            TraderState().binding.g_cachedHoveredWidgetInventory,
            source.str(),
            true,
            TraderState().binding.g_cachedHoveredWidgetInventory->isVisible(),
            9400);
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
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " hovered_candidates=" << inventoryCandidates.size()
               << " hovered_inside_entries=" << (hoveredInsideEntries ? "true" : "false");
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderQuantityNameKeysFromCaption(
    MyGUI::Widget* traderParent,
    std::vector<QuantityNameKey>* outKeys,
    std::string* outSource)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    outKeys->clear();
    if (outSource != 0)
    {
        outSource->clear();
    }

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    const std::string windowCaption = owningWindow == 0 ? "" : owningWindow->getCaption().asUTF8();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    Character* bestCharacter = 0;
    bool bestVisible = false;
    int bestScore = -1000000;

    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);

        const std::string characterName = CharacterNameForLog(candidate);
        const bool captionMatchesCharacter =
            !windowCaption.empty()
            && !characterName.empty()
            && ContainsAsciiCaseInsensitive(windowCaption, characterName.c_str());

        const int itemCount = static_cast<int>(InventoryItemCountForLog(candidate->inventory));
        int score = 0;
        score += 300;
        if (inventoryVisible)
        {
            score += 120;
        }
        if (captionMatchesCharacter)
        {
            score += 500;
        }
        score += itemCount;

        if (score > bestScore)
        {
            bestScore = score;
            bestCharacter = candidate;
            bestVisible = inventoryVisible;
        }
    }

    if (bestCharacter == 0 || bestCharacter->inventory == 0)
    {
        return false;
    }

    if (!TryExtractQuantityNameKeysFromInventory(bestCharacter->inventory, outKeys))
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << "caption_trader:" << CharacterNameForLog(bestCharacter)
               << " visible=" << (bestVisible ? "true" : "false")
               << " item_count=" << InventoryItemCountForLog(bestCharacter->inventory);
        *outSource = source.str();
    }

    return true;
}
