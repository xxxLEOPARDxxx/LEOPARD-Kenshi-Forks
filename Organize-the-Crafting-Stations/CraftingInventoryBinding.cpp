#include "CraftingInventoryBinding.h"

#include "CraftingSearchText.h"
#include "CraftingWindowDetection.h"

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
}

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

namespace
{
InventoryGUI* ReadWidgetInventoryGuiPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    InventoryGUI** typed = widget->_getInternalData<InventoryGUI*>(false);
    if (typed != 0 && *typed != 0)
    {
        return *typed;
    }

    typed = widget->getUserData<InventoryGUI*>(false);
    if (typed != 0 && *typed != 0)
    {
        return *typed;
    }

    return 0;
}

void CollectWidgetInventoryGuiPointersRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::size_t* nodesVisited,
    std::vector<InventoryGUI*>* outPointers)
{
    if (widget == 0 || outPointers == 0 || nodesVisited == 0 || *nodesVisited >= maxNodes)
    {
        return;
    }

    ++(*nodesVisited);

    InventoryGUI* inventoryGui = ReadWidgetInventoryGuiPointer(widget);
    AddInventoryGuiPointerUnique(outPointers, inventoryGui);

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

        CollectWidgetInventoryGuiPointersRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            maxNodes,
            nodesVisited,
            outPointers);
    }
}
}

void CollectWidgetInventoryGuiPointers(
    MyGUI::Widget* rootWidget,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::vector<InventoryGUI*>* outPointers)
{
    if (rootWidget == 0 || outPointers == 0)
    {
        return;
    }

    std::size_t nodesVisited = 0;
    CollectWidgetInventoryGuiPointersRecursive(
        rootWidget,
        0,
        maxDepth,
        maxNodes,
        &nodesVisited,
        outPointers);

    MyGUI::Widget* current = rootWidget->getParent();
    for (std::size_t depth = 0; current != 0 && depth < 12; ++depth)
    {
        InventoryGUI* inventoryGui = ReadWidgetInventoryGuiPointer(current);
        AddInventoryGuiPointerUnique(outPointers, inventoryGui);
        current = current->getParent();
    }
}

namespace
{

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

bool TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    PruneSectionWidgetInventoryLinks();
    if (TraderState().binding.g_sectionWidgetInventoryLinks.empty())
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        return false;
    }

    std::vector<MyGUI::Widget*> ancestry;
    ancestry.reserve(24);
    for (MyGUI::Widget* current = entriesRoot; current != 0; current = current->getParent())
    {
        bool duplicate = false;
        for (std::size_t index = 0; index < ancestry.size(); ++index)
        {
            if (ancestry[index] == current)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            ancestry.push_back(current);
        }
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t matchedLinks = 0;
    std::size_t directEntriesMatches = 0;
    std::size_t directBackpackMatches = 0;

    for (std::size_t linkIndex = 0;
         linkIndex < TraderState().binding.g_sectionWidgetInventoryLinks.size();
         ++linkIndex)
    {
        const SectionWidgetInventoryLink& link =
            TraderState().binding.g_sectionWidgetInventoryLinks[linkIndex];
        if (link.sectionWidget == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        int matchedDepth = -1;
        for (std::size_t depth = 0; depth < ancestry.size(); ++depth)
        {
            if (ancestry[depth] == link.sectionWidget)
            {
                matchedDepth = static_cast<int>(depth);
                break;
            }
        }

        if (matchedDepth < 0)
        {
            continue;
        }

        ++matchedLinks;
        if (link.sectionWidget == entriesRoot)
        {
            ++directEntriesMatches;
        }
        if (link.sectionWidget == backpackContent)
        {
            ++directBackpackMatches;
        }

        int depthBonus = 1600 - matchedDepth * 90;
        if (depthBonus < 240)
        {
            depthBonus = 240;
        }

        const unsigned long long ageTicks =
            TraderState().core.g_updateTickCounter >= link.lastSeenTick
                ? TraderState().core.g_updateTickCounter - link.lastSeenTick
                : 0ULL;

        std::stringstream source;
        source << "section_widget_map"
               << " section=" << link.sectionName
               << " widget=" << link.widgetName
               << " depth=" << matchedDepth
               << " age_ticks=" << ageTicks
               << " items=" << link.itemCount;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            true,
            link.inventory->isVisible(),
            10800 + depthBonus);
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
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " section_widget_matches=" << matchedLinks
               << " direct_entries_matches=" << directEntriesMatches
               << " direct_backpack_matches=" << directBackpackMatches
               << " tracked_links=" << TraderState().binding.g_sectionWidgetInventoryLinks.size();
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromInventoryGuiMap(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    PruneInventoryGuiInventoryLinks();
    if (TraderState().binding.g_inventoryGuiInventoryLinks.empty())
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }

    MyGUI::Widget* scrollBackpackContent = FindAncestorByToken(backpackContent, "scrollview_backpack_content");
    if (scrollBackpackContent == 0)
    {
        scrollBackpackContent = FindWidgetInParentByToken(traderParent, "scrollview_backpack_content");
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);

    std::vector<InventoryGUI*> widgetInventoryGuis;
    CollectWidgetInventoryGuiPointers(backpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(scrollBackpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(entriesRoot, 3, 256, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(traderParent, 2, 192, &widgetInventoryGuis);
    if (owningWindow != 0)
    {
        CollectWidgetInventoryGuiPointers(owningWindow, 6, 2400, &widgetInventoryGuis);
    }
    if (widgetInventoryGuis.empty())
    {
        return false;
    }

    std::vector<std::uintptr_t> widgetPointerAliases;
    for (std::size_t pointerIndex = 0; pointerIndex < widgetInventoryGuis.size(); ++pointerIndex)
    {
        CollectPointerAliasesFromRawPointer(widgetInventoryGuis[pointerIndex], &widgetPointerAliases);
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t matchedLinks = 0;
    std::size_t inferredMatches = 0;
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

        const bool guiMatch =
            HasInventoryGuiPointer(widgetInventoryGuis, link.inventoryGui)
            || HasPointerAlias(widgetPointerAliases, link.inventoryGui);
        if (!guiMatch)
        {
            continue;
        }

        ++matchedLinks;
        const unsigned long long ageTicks =
            TraderState().core.g_updateTickCounter >= link.lastSeenTick
                ? TraderState().core.g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        std::stringstream source;
        source << "inventory_gui_map"
               << " owner=" << link.ownerName
               << " age_ticks=" << ageTicks
               << " items=" << link.itemCount
               << " visible=" << (link.inventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            true,
            link.inventory->isVisible(),
            12600);
    }

    for (std::size_t guiIndex = 0; guiIndex < widgetInventoryGuis.size(); ++guiIndex)
    {
        InventoryGUI* widgetGui = widgetInventoryGuis[guiIndex];
        Inventory* inferredInventory = 0;
        std::size_t resolvedOffset = 0;
        InventoryGuiBackPointerKind resolvedKind = InventoryGuiBackPointerKind_DirectInventory;
        RootObject* resolvedOwner = 0;
        if (!TryResolveInventoryFromInventoryGuiBackPointerOffsets(
                widgetGui,
                &inferredInventory,
                &resolvedOffset,
                &resolvedKind,
                &resolvedOwner))
        {
            continue;
        }

        ++inferredMatches;
        RootObject* owner = resolvedOwner;
        if (owner == 0)
        {
            owner = inferredInventory->getOwner();
            if (owner == 0)
            {
                owner = inferredInventory->getCallbackObject();
            }
        }

        Character* ownerCharacter = dynamic_cast<Character*>(owner);
        const bool ownerTrader = ownerCharacter != 0 && ownerCharacter->isATrader();
        const std::size_t itemCount = InventoryItemCountForLog(inferredInventory);
        InventoryGUI* directResolvedGui = TryGetInventoryGuiSafe(inferredInventory);
        const bool guiExactMatch = directResolvedGui != 0 && directResolvedGui == widgetGui;
        std::stringstream source;
        source << "inventory_gui_offset"
               << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
               << " items=" << itemCount
               << " gui_exact=" << (guiExactMatch ? "true" : "false")
               << " visible=" << (inferredInventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            inferredInventory,
            source.str(),
            ownerTrader,
            inferredInventory->isVisible(),
            12350);

        std::stringstream resolutionSignature;
        resolutionSignature << widgetGui
                            << "|" << inferredInventory
                            << "|" << static_cast<int>(resolvedKind)
                            << "|" << resolvedOffset
                            << "|" << itemCount;
        if (resolutionSignature.str() != TraderState().binding.g_lastInventoryGuiBackPointerResolutionSignature)
        {
            std::stringstream line;
            line << "inventory gui back-pointer resolved"
                 << " inv_gui=" << widgetGui
                 << " inventory=" << inferredInventory
                 << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
                 << " owner=" << RootObjectDisplayNameForLog(owner)
                 << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
                 << " gui_exact=" << (guiExactMatch ? "true" : "false")
                 << " items=" << itemCount;
            LogBindingDebugLine(line.str());
            TraderState().binding.g_lastInventoryGuiBackPointerResolutionSignature = resolutionSignature.str();
        }
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
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " gui_map_matches=" << matchedLinks
               << " gui_offset_matches=" << inferredMatches
               << " tracked_gui_links=" << TraderState().binding.g_inventoryGuiInventoryLinks.size()
               << " learned_gui_offsets=" << TraderState().binding.g_inventoryGuiBackPointerOffsets.size()
               << " widget_gui_ptrs=" << widgetInventoryGuis.size()
               << " widget_aliases=" << widgetPointerAliases.size();
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromRecentRefreshedInventories(
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

    PruneRecentlyRefreshedInventories();
    if (TraderState().binding.g_recentRefreshedInventories.empty())
    {
        return false;
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore);
    Inventory* captionTraderInventory = captionTrader == 0 ? 0 : captionTrader->inventory;

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    for (std::size_t index = 0;
         index < TraderState().binding.g_recentRefreshedInventories.size();
         ++index)
    {
        const RefreshedInventoryLink& link =
            TraderState().binding.g_recentRefreshedInventories[index];
        if (link.inventory == 0)
        {
            continue;
        }

        int priorityBias = 9800;
        if (link.ownerTrader)
        {
            priorityBias += 1200;
        }
        if (link.ownerSelected)
        {
            priorityBias -= 1800;
        }
        if (link.visible)
        {
            priorityBias += 180;
        }

        if (expectedEntryCount > 0)
        {
            const int expected = static_cast<int>(expectedEntryCount);
            const int count = static_cast<int>(link.itemCount);
            const int diff = count > expected ? count - expected : expected - count;
            priorityBias += 1200 - diff * 120;
            if (diff <= 1)
            {
                priorityBias += 400;
            }
            else if (diff <= 3)
            {
                priorityBias += 180;
            }
        }

        if (captionTraderInventory != 0 && link.inventory == captionTraderInventory)
        {
            priorityBias += 2600 + (captionScore / 2);
        }

        const unsigned long long ageTicks =
            TraderState().core.g_updateTickCounter >= link.lastSeenTick
                ? TraderState().core.g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        if (ageTicks > 0)
        {
            const int agePenalty = static_cast<int>(ageTicks > 1200ULL ? 1200ULL : ageTicks);
            priorityBias -= agePenalty;
        }

        std::stringstream source;
        source << "recent_refresh"
               << " owner=" << link.ownerName
               << " owner_trader=" << (link.ownerTrader ? "true" : "false")
               << " owner_selected=" << (link.ownerSelected ? "true" : "false")
               << " visible=" << (link.visible ? "true" : "false")
               << " items=" << link.itemCount
               << " age_ticks=" << ageTicks;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            link.ownerTrader,
            link.visible,
            priorityBias);
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
               << " refresh_candidates=" << inventoryCandidates.size()
               << " tracked_recent=" << TraderState().binding.g_recentRefreshedInventories.size();
        *outSource = source.str();
    }

    return true;
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

void LogInventoryBindingDiagnostics(std::size_t expectedEntryCount)
{
    if (!ShouldLogBindingDebug())
    {
        return;
    }

    if (ou == 0 || ou->player == 0)
    {
        LogWarnLine("inventory binding diagnostics: GameWorld/player unavailable");
        return;
    }

    std::stringstream header;
    header << "inventory binding diagnostics begin expected_entries=" << expectedEntryCount
           << " gui_display_object=" << RootObjectDisplayNameForLog(ou->guiDisplayObject.getRootObject());
    LogWarnLine(header.str());

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    RootObject* mouseTargetRoot = ou->player->mouseRightTarget;
    Character* mouseTargetCharacter = mouseTargetRoot == 0 ? 0 : mouseTargetRoot->getHandle().getCharacter();

    std::stringstream selectedLine;
    selectedLine << "binding selected_character=" << CharacterNameForLog(selectedCharacter)
                 << " selected_inventory_items=" << InventoryItemCountForLog(
                        selectedCharacter == 0 ? 0 : selectedCharacter->inventory)
                 << " mouse_target_root=" << RootObjectDisplayNameForLog(mouseTargetRoot)
                 << " mouse_target_character=" << CharacterNameForLog(mouseTargetCharacter)
                 << " mouse_target_inventory_items=" << InventoryItemCountForLog(
                        mouseTargetCharacter == 0 ? 0 : mouseTargetCharacter->inventory);
    LogWarnLine(selectedLine.str());

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    std::size_t index = 0;
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* playerChar = *iter;
        if (playerChar == 0)
        {
            continue;
        }

        Dialogue* dialogue = playerChar->dialogue;
        Character* target = dialogue == 0 ? 0 : dialogue->getConversationTarget().getCharacter();

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = dialogue != 0 && !dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = (target != 0) && (playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer);

        std::stringstream line;
        line << "binding player_char[" << index << "]=" << CharacterNameForLog(playerChar)
             << " inv_visible=" << (playerInventoryVisible ? "true" : "false")
             << " inv_items=" << InventoryItemCountForLog(playerChar->inventory)
             << " has_dialogue=" << (dialogue != 0 ? "true" : "false")
             << " dialog_active=" << (dialogActive ? "true" : "false")
             << " engaged=" << (engaged ? "true" : "false")
             << " target=" << CharacterNameForLog(target)
             << " target_trader=" << (target != 0 && target->isATrader() ? "true" : "false")
             << " target_inv_visible=" << (targetInventoryVisible ? "true" : "false")
             << " target_inv_items=" << InventoryItemCountForLog(target == 0 ? 0 : target->inventory);
        LogWarnLine(line.str());

        ++index;
        if (index >= 12)
        {
            break;
        }
    }

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    std::size_t activeCount = 0;
    std::size_t activeTraders = 0;
    std::size_t activeVisibleInventories = 0;
    std::size_t activeExactCountInventories = 0;
    std::size_t loggedCandidates = 0;

    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0)
        {
            continue;
        }

        ++activeCount;

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);
        const bool isTrader = candidate->isATrader();
        const std::size_t inventoryItemCount = InventoryItemCountForLog(candidate->inventory);
        const bool exactCountMatch = inventoryItemCount == expectedEntryCount && inventoryItemCount > 0;

        if (isTrader)
        {
            ++activeTraders;
        }
        if (inventoryVisible)
        {
            ++activeVisibleInventories;
        }
        if (exactCountMatch)
        {
            ++activeExactCountInventories;
        }

        if (loggedCandidates < 12 && (isTrader || inventoryVisible || exactCountMatch))
        {
            std::stringstream line;
            line << "binding active_char[" << loggedCandidates << "]=" << CharacterNameForLog(candidate)
                 << " trader=" << (isTrader ? "true" : "false")
                 << " inv_visible=" << (inventoryVisible ? "true" : "false")
                 << " inv_items=" << inventoryItemCount
                 << " exact_items_match=" << (exactCountMatch ? "true" : "false");
            LogWarnLine(line.str());
            ++loggedCandidates;
        }
    }

    std::stringstream summary;
    summary << "binding active_chars_summary total=" << activeCount
            << " traders=" << activeTraders
            << " inv_visible=" << activeVisibleInventories
            << " inv_items_match_expected=" << activeExactCountInventories;
    LogWarnLine(summary.str());

    Character* selectedForCenter = ou->player->selectedCharacter.getCharacter();
    Ogre::Vector3 nearbyCenter = selectedForCenter != 0 ? selectedForCenter->getPosition() : ou->getCameraCenter();
    std::vector<InventoryCandidateInfo> nearbyCandidates;
    std::size_t nearbyScannedObjects = 0;
    std::size_t nearbyInventoryObjects = 0;
    CollectNearbyInventoryCandidates(
        nearbyCenter,
        &nearbyCandidates,
        &nearbyScannedObjects,
        &nearbyInventoryObjects);

    std::stringstream nearbySummary;
    nearbySummary << "binding nearby_inventory_summary scanned_objects=" << nearbyScannedObjects
                  << " with_inventory=" << nearbyInventoryObjects
                  << " candidates=" << nearbyCandidates.size();
    LogWarnLine(nearbySummary.str());

    std::size_t nearbyLogged = 0;
    for (std::size_t nearbyIndex = 0;
         nearbyIndex < nearbyCandidates.size() && nearbyLogged < 12;
         ++nearbyIndex)
    {
        const InventoryCandidateInfo& candidate = nearbyCandidates[nearbyIndex];
        std::vector<std::string> keys;
        const bool hasKeys = TryExtractSearchKeysFromInventory(candidate.inventory, &keys);

        std::stringstream line;
        line << "binding nearby_candidate[" << nearbyLogged << "]"
             << " key_count=" << (hasKeys ? keys.size() : 0)
             << " visible=" << (candidate.visible ? "true" : "false")
             << " trader_preferred=" << (candidate.traderPreferred ? "true" : "false")
             << " source=\"" << TruncateForLog(candidate.source, 220) << "\"";
        if (hasKeys && !keys.empty())
        {
            line << " key0=\"" << TruncateForLog(keys[0], 48) << "\"";
        }
        LogWarnLine(line.str());
        ++nearbyLogged;
    }

    LogWarnLine("inventory binding diagnostics end");
}

bool TryResolveAndCacheTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    TraderPanelInventoryBinding* outBinding,
    std::string* outStatus)
{
    if (outBinding != 0)
    {
        outBinding->traderParent = 0;
        outBinding->entriesRoot = 0;
        outBinding->inventory = 0;
        outBinding->stage.clear();
        outBinding->source.clear();
        outBinding->expectedEntryCount = 0;
        outBinding->nonEmptyKeyCount = 0;
        outBinding->lastSeenTick = 0;
    }
    if (outStatus != 0)
    {
        outStatus->clear();
    }

    if (traderParent == 0 || entriesRoot == 0 || expectedEntryCount == 0)
    {
        if (outStatus != 0)
        {
            *outStatus = "panel_or_entries_missing";
        }
        return false;
    }

    TraderPanelInventoryBinding cached;
    if (TryGetTraderPanelInventoryBinding(traderParent, entriesRoot, expectedEntryCount, &cached))
    {
        if (outBinding != 0)
        {
            *outBinding = cached;
        }
        if (outStatus != 0)
        {
            *outStatus = "cached";
        }
        return true;
    }

    std::vector<std::string> keys;
    std::vector<QuantityNameKey> quantityKeys;
    std::string source;
    Inventory* selectedInventory = 0;

    if (TryResolveTraderInventoryNameKeysFromInventoryGuiMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeysLocal(keys);
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;
        if (selectedInventory != 0 && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "inventory_gui_map",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_inventory_gui_map";
                }
                return true;
            }
        }
    }

    keys.clear();
    quantityKeys.clear();
    source.clear();
    selectedInventory = 0;
    if (TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeysLocal(keys);
        const int directEntriesMatches =
            ExtractTaggedIntValue(source, "direct_entries_matches=");
        const int directBackpackMatches =
            ExtractTaggedIntValue(source, "direct_backpack_matches=");
        const bool directSectionMatch =
            directEntriesMatches > 0 || directBackpackMatches > 0;
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;

        if (selectedInventory != 0 && directSectionMatch && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "section_widget",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_section_widget";
                }
                return true;
            }
        }
    }

    keys.clear();
    quantityKeys.clear();
    source.clear();
    selectedInventory = 0;
    if (TryResolveTraderInventoryNameKeysFromWidgetBindings(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeysLocal(keys);
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;
        const std::string sourceLower = NormalizeSearchText(source);
        const bool hasGuiMatchTag =
            sourceLower.find("widget gui match") != std::string::npos;

        if (selectedInventory != 0 && hasGuiMatchTag && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "widget",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_widget";
                }
                return true;
            }
        }
    }

    if (outStatus != 0)
    {
        *outStatus = "high_confidence_binding_not_found";
    }
    return false;
}
