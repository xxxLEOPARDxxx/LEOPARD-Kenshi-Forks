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
