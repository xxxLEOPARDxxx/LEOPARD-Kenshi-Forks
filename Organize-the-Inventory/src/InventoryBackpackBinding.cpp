#include "InventoryBackpackBinding.h"

#include "InventoryCore.h"
#include "InventorySearchText.h"
#include "InventoryWindowDetection.h"

#include <kenshi/GameData.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <Windows.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace
{
std::string g_lastBackpackBindingSignature;
std::string g_lastBackpackBindingTraceSignature;
std::string g_lastInventoryGuiBindingSignature;
std::string g_lastInventoryGuiOffsetLearningSignature;

struct InventoryGuiInventoryLink
{
    InventoryGuiInventoryLink()
        : inventoryGui(0)
        , inventory(0)
        , itemCount(0)
        , lastSeenTick(0)
    {
    }

    InventoryGUI* inventoryGui;
    Inventory* inventory;
    std::string ownerName;
    std::size_t itemCount;
    DWORD lastSeenTick;
};

enum InventoryGuiBackPointerKind
{
    InventoryGuiBackPointerKind_DirectInventory,
    InventoryGuiBackPointerKind_OwnerObject,
    InventoryGuiBackPointerKind_CallbackObject,
};

struct InventoryGuiBackPointerOffset
{
    InventoryGuiBackPointerOffset()
        : offset(0)
        , kind(InventoryGuiBackPointerKind_DirectInventory)
        , hits(0)
    {
    }

    std::size_t offset;
    InventoryGuiBackPointerKind kind;
    std::size_t hits;
};

std::vector<InventoryGuiInventoryLink> g_inventoryGuiInventoryLinks;
std::vector<InventoryGuiBackPointerOffset> g_inventoryGuiBackPointerOffsets;

struct BackpackInventoryCandidate
{
    BackpackInventoryCandidate()
        : inventory(0)
        , priorityBias(0)
    {
    }

    Inventory* inventory;
    int priorityBias;
    std::string source;
};

struct EquippedBackpackCandidate
{
    EquippedBackpackCandidate()
        : inventory(0)
        , item(0)
        , sectionItemCount(0)
        , sectionQuantity(0)
        , score(-2147483647)
    {
    }

    Inventory* inventory;
    Item* item;
    std::string sectionName;
    std::size_t sectionItemCount;
    std::size_t sectionQuantity;
    int score;
};

DWORD CurrentBindingTick()
{
    return GetTickCount();
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

std::size_t InventoryItemCountForLog(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    __try
    {
        return static_cast<std::size_t>(inventory->getNumItems());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void LogBackpackBindingTrace(
    MyGUI::Widget* backpackContent,
    const std::string& captionNormalized,
    std::size_t uiEntries,
    std::size_t uiQuantity,
    Inventory* resolvedInventory,
    const std::string& reason,
    const std::vector<std::string>& selectedRows,
    const std::vector<std::string>& finalRows,
    std::size_t pairedEntries)
{
    if (!ShouldLogBindingDebug())
    {
        return;
    }

    std::stringstream signature;
    signature << SafeWidgetName(backpackContent)
              << "|" << captionNormalized
              << "|" << uiEntries
              << "|" << uiQuantity
              << "|" << resolvedInventory
              << "|" << reason
              << "|" << pairedEntries;
    for (std::size_t index = 0; index < selectedRows.size(); ++index)
    {
        signature << "|s:" << selectedRows[index];
    }
    for (std::size_t index = 0; index < finalRows.size(); ++index)
    {
        signature << "|f:" << finalRows[index];
    }

    if (signature.str() == g_lastBackpackBindingTraceSignature)
    {
        return;
    }
    g_lastBackpackBindingTraceSignature = signature.str();

    std::stringstream line;
    line << "inventory backpack binding trace begin"
         << " content=" << SafeWidgetName(backpackContent)
         << " caption=\"" << captionNormalized << "\""
         << " ui_entries=" << uiEntries
         << " ui_quantity=" << uiQuantity
         << " resolved_inventory=" << resolvedInventory
         << " paired_entries=" << pairedEntries
         << " reason=\"" << reason << "\""
         << " selected_candidates=" << selectedRows.size()
         << " final_candidates=" << finalRows.size();
    LogBindingDebugLine(line.str());

    for (std::size_t index = 0; index < selectedRows.size(); ++index)
    {
        std::stringstream candidateLine;
        candidateLine << "inventory backpack binding trace selected[" << index << "] "
                      << selectedRows[index];
        LogBindingDebugLine(candidateLine.str());
    }

    for (std::size_t index = 0; index < finalRows.size(); ++index)
    {
        std::stringstream candidateLine;
        candidateLine << "inventory backpack binding trace final[" << index << "] "
                      << finalRows[index];
        LogBindingDebugLine(candidateLine.str());
    }

    LogBindingDebugLine("inventory backpack binding trace end");
}

bool IsInventoryPointerValidSafe(Inventory* inventory)
{
    if (inventory == 0)
    {
        return false;
    }

    __try
    {
        inventory->getNumItems();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
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

bool WidgetCoordTopLeftLess(MyGUI::Widget* left, MyGUI::Widget* right)
{
    if (left == 0 || right == 0)
    {
        return left < right;
    }

    const MyGUI::IntCoord leftCoord = left->getCoord();
    const MyGUI::IntCoord rightCoord = right->getCoord();
    if (leftCoord.top != rightCoord.top)
    {
        return leftCoord.top < rightCoord.top;
    }
    if (leftCoord.left != rightCoord.left)
    {
        return leftCoord.left < rightCoord.left;
    }
    return left < right;
}

std::string ResolveBackpackItemCaption(Item* item)
{
    if (item == 0)
    {
        return "";
    }

    if (!item->displayName.empty())
    {
        return item->displayName;
    }

    return item->getName();
}

int ComputeCaptionMatchScore(const std::string& captionNormalized, Item* item)
{
    if (captionNormalized.empty() || item == 0)
    {
        return 0;
    }

    const std::string itemCaptionNormalized =
        NormalizeInventorySearchText(ResolveBackpackItemCaption(item));
    if (itemCaptionNormalized.empty())
    {
        return 0;
    }

    if (captionNormalized == itemCaptionNormalized)
    {
        return 6000;
    }

    if (captionNormalized.find(itemCaptionNormalized) != std::string::npos
        || itemCaptionNormalized.find(captionNormalized) != std::string::npos)
    {
        return 3200;
    }

    return 0;
}

bool SectionItemTopLeftLess(
    const InventorySection::SectionItem& leftItem,
    const InventorySection::SectionItem& rightItem)
{
    if (leftItem.y != rightItem.y)
    {
        return leftItem.y < rightItem.y;
    }
    if (leftItem.x != rightItem.x)
    {
        return leftItem.x < rightItem.x;
    }
    return leftItem.item < rightItem.item;
}

void AddInventoryCandidateUnique(
    std::vector<BackpackInventoryCandidate>* candidates,
    Inventory* inventory,
    int priorityBias,
    const std::string& source)
{
    if (candidates == 0 || inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        BackpackInventoryCandidate& existing = (*candidates)[index];
        if (existing.inventory != inventory)
        {
            continue;
        }

        if (priorityBias > existing.priorityBias)
        {
            existing.priorityBias = priorityBias;
            existing.source = source;
        }
        return;
    }

    BackpackInventoryCandidate candidate;
    candidate.inventory = inventory;
    candidate.priorityBias = priorityBias;
    candidate.source = source;
    candidates->push_back(candidate);
}

void PruneInventoryGuiInventoryLinks()
{
    if (g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    const DWORD now = CurrentBindingTick();
    std::vector<InventoryGuiInventoryLink> kept;
    kept.reserve(g_inventoryGuiInventoryLinks.size());
    for (std::size_t index = 0; index < g_inventoryGuiInventoryLinks.size(); ++index)
    {
        const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (now >= link.lastSeenTick && now - link.lastSeenTick > 60000UL)
        {
            continue;
        }

        kept.push_back(link);
    }

    g_inventoryGuiInventoryLinks.swap(kept);
}

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

void AddInventoryGuiPointerUnique(
    std::vector<InventoryGUI*>* pointers,
    InventoryGUI* pointer)
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
    AddInventoryGuiPointerUnique(outPointers, ReadWidgetInventoryGuiPointer(widget));

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
        AddInventoryGuiPointerUnique(outPointers, ReadWidgetInventoryGuiPointer(current));
        current = current->getParent();
    }
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

const char* InventoryGuiBackPointerKindLabel(InventoryGuiBackPointerKind kind)
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
        if (hit.offset == offset && hit.kind == kind)
        {
            ++hit.hits;
            return;
        }
    }

    InventoryGuiBackPointerOffset hit;
    hit.offset = offset;
    hit.kind = kind;
    hit.hits = 1;
    hits->push_back(hit);
}

void LearnInventoryGuiBackPointerOffsets()
{
    PruneInventoryGuiInventoryLinks();
    if (g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    const std::size_t kMaxScanOffset = 0x400;
    std::vector<InventoryGuiBackPointerOffset> offsetHits;
    std::size_t validatedLinks = 0;
    for (std::size_t linkIndex = 0; linkIndex < g_inventoryGuiInventoryLinks.size(); ++linkIndex)
    {
        const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[linkIndex];
        if (link.inventoryGui == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
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

    g_inventoryGuiBackPointerOffsets.swap(offsetHits);

    std::stringstream signature;
    for (std::size_t index = 0; index < g_inventoryGuiBackPointerOffsets.size(); ++index)
    {
        const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
        signature << InventoryGuiBackPointerKindLabel(learned.kind)
                  << ":" << learned.offset
                  << ":" << learned.hits << "|";
    }

    if (signature.str() != g_lastInventoryGuiOffsetLearningSignature)
    {
        std::stringstream line;
        if (g_inventoryGuiBackPointerOffsets.empty())
        {
            line << "inventory gui back-pointer offsets not learned"
                 << " tracked_links=" << g_inventoryGuiInventoryLinks.size()
                 << " validated_links=" << validatedLinks
                 << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
        }
        else
        {
            line << "inventory gui back-pointer offsets learned";
            const std::size_t previewCount =
                g_inventoryGuiBackPointerOffsets.size() < 8
                    ? g_inventoryGuiBackPointerOffsets.size()
                    : 8;
            for (std::size_t index = 0; index < previewCount; ++index)
            {
                const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
                line << " offset" << index << "=0x"
                     << std::hex << std::uppercase << learned.offset
                     << std::dec
                     << "(" << InventoryGuiBackPointerKindLabel(learned.kind)
                     << ",hits=" << learned.hits << ")";
            }
            line << " tracked_links=" << g_inventoryGuiInventoryLinks.size()
                 << " validated_links=" << validatedLinks
                 << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
        }
        LogBindingDebugLine(line.str());
        g_lastInventoryGuiOffsetLearningSignature = signature.str();
    }
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
    if (g_inventoryGuiBackPointerOffsets.empty())
    {
        return false;
    }

    for (std::size_t index = 0; index < g_inventoryGuiBackPointerOffsets.size(); ++index)
    {
        const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
        const void* value = 0;
        if (!TryReadPointerValueSafe(inventoryGui, learned.offset, &value) || value == 0)
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

        *outInventory = candidateInventory;
        if (outOffset != 0)
        {
            *outOffset = learned.offset;
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

    return false;
}

void CollectWidgetChainInventoryCandidates(
    MyGUI::Widget* widget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::size_t maxDepth,
    std::vector<BackpackInventoryCandidate>* outCandidates)
{
    if (widget == 0 || outCandidates == 0)
    {
        return;
    }

    MyGUI::Widget* current = widget;
    for (std::size_t depth = 0; current != 0 && depth <= maxDepth; ++depth)
    {
        Inventory* inventory = ResolveInventoryWidgetInventoryPointer(current);
        if (inventory != 0)
        {
            std::stringstream source;
            source << (sourcePrefix == 0 ? "widget" : sourcePrefix)
                   << ":depth=" << depth;
            AddInventoryCandidateUnique(
                outCandidates,
                inventory,
                basePriorityBias - static_cast<int>(depth) * 80,
                source.str());
        }

        current = current->getParent();
    }
}

Inventory* ResolveSelectedEquippedBackpackInventory(
    const std::string& backpackCaptionNormalized,
    std::size_t expectedEntryCount,
    std::size_t expectedQuantity,
    std::string* outSource,
    std::size_t* outSectionItemCount,
    std::size_t* outSectionQuantity,
    std::vector<std::string>* outCandidateRows)
{
    if (outSource != 0)
    {
        outSource->clear();
    }
    if (outSectionItemCount != 0)
    {
        *outSectionItemCount = 0;
    }
    if (outSectionQuantity != 0)
    {
        *outSectionQuantity = 0;
    }
    if (outCandidateRows != 0)
    {
        outCandidateRows->clear();
    }

    if (ou == 0 || ou->player == 0)
    {
        return 0;
    }

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    if (selectedCharacter == 0 || selectedCharacter->inventory == 0)
    {
        return 0;
    }

    InventorySection* backSection = selectedCharacter->inventory->getSection("back");
    InventorySection* backpackAttachSection =
        selectedCharacter->inventory->getSection("backpack_attach");

    EquippedBackpackCandidate bestCandidate;
    lektor<InventorySection*>& allSections = selectedCharacter->inventory->getAllSections();
    for (std::size_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
    {
        InventorySection* carrierSection = allSections[static_cast<uint32_t>(sectionIndex)];
        if (carrierSection == 0)
        {
            continue;
        }

        const Ogre::vector<InventorySection::SectionItem>::type& carrierItems = carrierSection->getItems();
        for (std::size_t itemIndex = 0; itemIndex < carrierItems.size(); ++itemIndex)
        {
            Item* backpackItem = carrierItems[itemIndex].item;
            if (backpackItem == 0)
            {
                continue;
            }

            Inventory* backpackInventory = backpackItem->getInventory();
            if (backpackInventory == 0 || !IsInventoryPointerValidSafe(backpackInventory))
            {
                continue;
            }

            InventorySection* backpackContentSection =
                backpackInventory->getSection("backpack_content");
            if (backpackContentSection == 0)
            {
                continue;
            }

            const Ogre::vector<InventorySection::SectionItem>::type& backpackItems =
                backpackContentSection->getItems();
            std::size_t backpackQuantity = 0;
            for (std::size_t backpackIndex = 0; backpackIndex < backpackItems.size(); ++backpackIndex)
            {
                Item* nestedItem = backpackItems[backpackIndex].item;
                backpackQuantity += static_cast<std::size_t>(
                    nestedItem != 0 && nestedItem->quantity > 0 ? nestedItem->quantity : 1);
            }

            int score = 9000;
            const int captionScore =
                ComputeCaptionMatchScore(backpackCaptionNormalized, backpackItem);
            score += captionScore;
            if (carrierSection == backSection || carrierSection == backpackAttachSection)
            {
                score += 1200;
            }

            const int entryDiff =
                static_cast<int>(backpackItems.size()) - static_cast<int>(expectedEntryCount);
            if (entryDiff == 0)
            {
                score += 2600;
            }
            else
            {
                score -= std::abs(entryDiff) * 900;
            }

            const int quantityDiff =
                static_cast<int>(backpackQuantity) - static_cast<int>(expectedQuantity);
            if (quantityDiff == 0)
            {
                score += 1600;
            }
            else
            {
                score -= std::abs(quantityDiff) * 120;
            }

            if (outCandidateRows != 0)
            {
                std::stringstream row;
                row << "item=\"" << ResolveBackpackItemCaption(backpackItem) << "\""
                    << " inventory=" << backpackInventory
                    << " section=";
                if (carrierSection == backSection)
                {
                    row << "back";
                }
                else if (carrierSection == backpackAttachSection)
                {
                    row << "backpack_attach";
                }
                else
                {
                    row << "other";
                }
                row << " caption_score=" << captionScore
                    << " section_items=" << backpackItems.size()
                    << " section_quantity=" << backpackQuantity
                    << " entry_diff=" << entryDiff
                    << " quantity_diff=" << quantityDiff
                    << " score=" << score;
                outCandidateRows->push_back(row.str());
            }

            if (bestCandidate.inventory != 0 && score <= bestCandidate.score)
            {
                continue;
            }

            bestCandidate.inventory = backpackInventory;
            bestCandidate.item = backpackItem;
            if (carrierSection == backSection)
            {
                bestCandidate.sectionName = "back";
            }
            else if (carrierSection == backpackAttachSection)
            {
                bestCandidate.sectionName = "backpack_attach";
            }
            else
            {
                bestCandidate.sectionName = "other";
            }
            bestCandidate.sectionItemCount = backpackItems.size();
            bestCandidate.sectionQuantity = backpackQuantity;
            bestCandidate.score = score;
        }
    }

    if (bestCandidate.inventory == 0)
    {
        return 0;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << "selected_backpack_item"
               << ":section=" << bestCandidate.sectionName
               << ":caption_score="
               << ComputeCaptionMatchScore(backpackCaptionNormalized, bestCandidate.item);
        *outSource = source.str();
    }
    if (outSectionItemCount != 0)
    {
        *outSectionItemCount = bestCandidate.sectionItemCount;
    }
    if (outSectionQuantity != 0)
    {
        *outSectionQuantity = bestCandidate.sectionQuantity;
    }

    return bestCandidate.inventory;
}

void CollectVisibleBackpackEntryWidgets(
    MyGUI::Widget* entriesRoot,
    std::vector<MyGUI::Widget*>* outWidgets,
    std::size_t* outQuantity)
{
    if (outWidgets == 0)
    {
        return;
    }

    outWidgets->clear();
    if (outQuantity != 0)
    {
        *outQuantity = 0;
    }

    if (entriesRoot == 0)
    {
        return;
    }

    const std::size_t childCount = entriesRoot->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0 || !child->getInheritedVisible())
        {
            continue;
        }

        int quantity = 0;
        if (!TryResolveInventoryItemQuantityFromWidget(child, &quantity) || quantity <= 0)
        {
            continue;
        }

        outWidgets->push_back(child);
        if (outQuantity != 0)
        {
            *outQuantity += static_cast<std::size_t>(quantity);
        }
    }

    if (outWidgets->empty())
    {
        CollectLikelyInventoryEntryWidgets(entriesRoot, outWidgets);
        if (outQuantity != 0)
        {
            *outQuantity = 0;
            for (std::size_t index = 0; index < outWidgets->size(); ++index)
            {
                int quantity = 0;
                if (TryResolveInventoryItemQuantityFromWidget((*outWidgets)[index], &quantity) && quantity > 0)
                {
                    *outQuantity += static_cast<std::size_t>(quantity);
                }
            }
        }
    }

    std::sort(outWidgets->begin(), outWidgets->end(), WidgetCoordTopLeftLess);
}

bool TryCollectBackpackSectionItems(
    Inventory* inventory,
    std::vector<InventorySection::SectionItem>* outItems,
    std::size_t* outQuantity)
{
    if (outItems == 0)
    {
        return false;
    }

    outItems->clear();
    if (outQuantity != 0)
    {
        *outQuantity = 0;
    }

    if (inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return false;
    }

    InventorySection* section = inventory->getSection("backpack_content");
    if (section == 0)
    {
        return false;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& rawItems = section->getItems();
    if (rawItems.empty())
    {
        return false;
    }

    outItems->assign(rawItems.begin(), rawItems.end());
    std::sort(outItems->begin(), outItems->end(), SectionItemTopLeftLess);
    if (outQuantity != 0)
    {
        for (std::size_t index = 0; index < outItems->size(); ++index)
        {
            Item* item = (*outItems)[index].item;
            *outQuantity += static_cast<std::size_t>(
                item != 0 && item->quantity > 0 ? item->quantity : 1);
        }
    }

    return true;
}

Inventory* ResolveBestBackpackInventory(
    MyGUI::Widget* backpackContent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    std::size_t expectedQuantity,
    std::string* outSource,
    std::string* outReason,
    std::vector<std::string>* outSelectedCandidateRows,
    std::vector<std::string>* outFinalCandidateRows)
{
    if (outSource != 0)
    {
        outSource->clear();
    }
    if (outReason != 0)
    {
        outReason->clear();
    }
    if (outSelectedCandidateRows != 0)
    {
        outSelectedCandidateRows->clear();
    }
    if (outFinalCandidateRows != 0)
    {
        outFinalCandidateRows->clear();
    }

    MyGUI::Window* backpackWindow = FindOwningWindow(backpackContent);
    const std::string backpackCaptionNormalized = NormalizeInventorySearchText(
        backpackWindow == 0 ? "" : backpackWindow->getCaption().asUTF8());

    std::vector<BackpackInventoryCandidate> candidates;
    CollectWidgetChainInventoryCandidates(entriesRoot, "entries_root", 9800, 6, &candidates);
    if (backpackContent != entriesRoot)
    {
        CollectWidgetChainInventoryCandidates(backpackContent, "backpack_content", 9400, 6, &candidates);
    }

    std::string selectedBackpackSource;
    Inventory* selectedBackpackInventory = ResolveSelectedEquippedBackpackInventory(
        backpackCaptionNormalized,
        expectedEntryCount,
        expectedQuantity,
        &selectedBackpackSource,
        0,
        0,
        outSelectedCandidateRows);
    if (selectedBackpackInventory != 0)
    {
        AddInventoryCandidateUnique(
            &candidates,
            selectedBackpackInventory,
            12000,
            selectedBackpackSource.empty() ? "selected_equipped_backpack" : selectedBackpackSource);
    }

    PruneInventoryGuiInventoryLinks();
    std::vector<InventoryGUI*> widgetInventoryGuis;
    CollectWidgetInventoryGuiPointers(backpackContent, 4, 320, &widgetInventoryGuis);
    if (entriesRoot != backpackContent)
    {
        CollectWidgetInventoryGuiPointers(entriesRoot, 3, 256, &widgetInventoryGuis);
    }
    if (backpackWindow != 0)
    {
        CollectWidgetInventoryGuiPointers(backpackWindow, 6, 2400, &widgetInventoryGuis);
    }

    for (std::size_t guiIndex = 0; guiIndex < widgetInventoryGuis.size(); ++guiIndex)
    {
        InventoryGUI* widgetGui = widgetInventoryGuis[guiIndex];
        bool matchedTrackedLink = false;
        for (std::size_t linkIndex = 0; linkIndex < g_inventoryGuiInventoryLinks.size(); ++linkIndex)
        {
            const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[linkIndex];
            if (link.inventoryGui != widgetGui || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
            {
                continue;
            }

            matchedTrackedLink = true;
            std::stringstream source;
            source << "inventory_gui_link"
                   << " owner=" << link.ownerName
                   << " items=" << link.itemCount;
            AddInventoryCandidateUnique(&candidates, link.inventory, 12800, source.str());

            if (outFinalCandidateRows != 0)
            {
                std::stringstream row;
                row << "widget_gui=" << widgetGui
                    << " tracked_link_inventory=" << link.inventory
                    << " owner=" << link.ownerName
                    << " items=" << link.itemCount;
                outFinalCandidateRows->push_back(row.str());
            }
        }

        Inventory* guiResolvedInventory = 0;
        std::size_t resolvedOffset = 0;
        InventoryGuiBackPointerKind resolvedKind = InventoryGuiBackPointerKind_DirectInventory;
        RootObject* resolvedOwner = 0;
        if (TryResolveInventoryFromInventoryGuiBackPointerOffsets(
                widgetGui,
                &guiResolvedInventory,
                &resolvedOffset,
                &resolvedKind,
                &resolvedOwner))
        {
            std::stringstream source;
            source << "inventory_gui_backptr"
                   << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
                   << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
                   << " owner=" << RootObjectDisplayNameForLog(resolvedOwner);
            AddInventoryCandidateUnique(&candidates, guiResolvedInventory, 12400, source.str());

            if (outFinalCandidateRows != 0)
            {
                std::stringstream row;
                row << "widget_gui=" << widgetGui
                    << " resolved_inventory=" << guiResolvedInventory
                    << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
                    << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
                    << " owner=" << RootObjectDisplayNameForLog(resolvedOwner);
                outFinalCandidateRows->push_back(row.str());
            }
        }
        else if (!matchedTrackedLink && outFinalCandidateRows != 0)
        {
            std::stringstream row;
            row << "widget_gui=" << widgetGui
                << " unresolved=true"
                << " tracked_gui_links=" << g_inventoryGuiInventoryLinks.size()
                << " learned_offsets=" << g_inventoryGuiBackPointerOffsets.size();
            outFinalCandidateRows->push_back(row.str());
        }
    }

    if (candidates.empty())
    {
        if (outReason != 0)
        {
            std::stringstream reason;
            reason << "no_inventory_candidates"
                   << " widget_guis=" << widgetInventoryGuis.size()
                   << " tracked_gui_links=" << g_inventoryGuiInventoryLinks.size()
                   << " learned_gui_offsets=" << g_inventoryGuiBackPointerOffsets.size();
            *outReason = reason.str();
        }
        return 0;
    }

    Inventory* bestInventory = 0;
    int bestScore = -2147483647;
    std::size_t bestSectionItemCount = 0;
    std::size_t bestSectionQuantity = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const BackpackInventoryCandidate& candidate = candidates[index];
        std::vector<InventorySection::SectionItem> sectionItems;
        std::size_t sectionQuantity = 0;
        if (!TryCollectBackpackSectionItems(candidate.inventory, &sectionItems, &sectionQuantity))
        {
            if (outFinalCandidateRows != 0)
            {
                std::stringstream row;
                row << "inventory=" << candidate.inventory
                    << " source=" << candidate.source
                    << " priority_bias=" << candidate.priorityBias
                    << " invalid_backpack_content_section=true";
                outFinalCandidateRows->push_back(row.str());
            }
            continue;
        }

        int score = candidate.priorityBias + 10000;
        const int entryDiff = static_cast<int>(sectionItems.size()) - static_cast<int>(expectedEntryCount);
        if (entryDiff == 0)
        {
            score += 3200;
        }
        else
        {
            score -= std::abs(entryDiff) * 900;
        }

        const int quantityDiff = static_cast<int>(sectionQuantity) - static_cast<int>(expectedQuantity);
        if (quantityDiff == 0)
        {
            score += 1600;
        }
        else
        {
            score -= std::abs(quantityDiff) * 120;
        }

        if (outFinalCandidateRows != 0)
        {
            std::stringstream row;
            row << "inventory=" << candidate.inventory
                << " source=" << candidate.source
                << " priority_bias=" << candidate.priorityBias
                << " section_items=" << sectionItems.size()
                << " section_quantity=" << sectionQuantity
                << " entry_diff=" << entryDiff
                << " quantity_diff=" << quantityDiff
                << " score=" << score;
            outFinalCandidateRows->push_back(row.str());
        }

        if (bestInventory == 0 || score > bestScore)
        {
            bestInventory = candidate.inventory;
            bestScore = score;
            bestSectionItemCount = sectionItems.size();
            bestSectionQuantity = sectionQuantity;
            if (outSource != 0)
            {
                *outSource = candidate.source;
            }
        }
    }

    if (bestInventory == 0)
    {
        if (outReason != 0)
        {
            *outReason = "no_backpack_content_section_candidate";
        }
        return 0;
    }

    if (outReason != 0)
    {
        std::stringstream reason;
        reason << "source=" << (outSource == 0 ? "" : *outSource)
               << " ui_entries=" << expectedEntryCount
               << " ui_quantity=" << expectedQuantity
               << " section_items=" << bestSectionItemCount
               << " section_quantity=" << bestSectionQuantity;
        if (!backpackCaptionNormalized.empty())
        {
            reason << " caption=\"" << backpackCaptionNormalized << "\"";
        }
        *outReason = reason.str();
    }

    return bestInventory;
}
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
    for (std::size_t index = 0; index < g_inventoryGuiInventoryLinks.size(); ++index)
    {
        InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui != inventoryGui)
        {
            continue;
        }

        link.inventory = inventory;
        link.ownerName = ownerName;
        link.itemCount = itemCount;
        link.lastSeenTick = CurrentBindingTick();

        std::stringstream signature;
        signature << inventoryGui << "|" << inventory
                  << "|" << ownerName << "|" << itemCount;
        if (signature.str() != g_lastInventoryGuiBindingSignature)
        {
            std::stringstream line;
            line << "inventory layout gui binding"
                 << " inv_gui=" << inventoryGui
                 << " owner=" << ownerName
                 << " inv_items=" << itemCount;
            LogBindingDebugLine(line.str());
            g_lastInventoryGuiBindingSignature = signature.str();
        }
        return;
    }

    InventoryGuiInventoryLink link;
    link.inventoryGui = inventoryGui;
    link.inventory = inventory;
    link.ownerName = ownerName;
    link.itemCount = itemCount;
    link.lastSeenTick = CurrentBindingTick();
    g_inventoryGuiInventoryLinks.push_back(link);

    std::stringstream signature;
    signature << inventoryGui << "|" << inventory
              << "|" << ownerName << "|" << itemCount;
    if (signature.str() != g_lastInventoryGuiBindingSignature)
    {
        std::stringstream line;
        line << "inventory layout gui binding"
             << " inv_gui=" << inventoryGui
             << " owner=" << ownerName
             << " inv_items=" << itemCount;
        LogBindingDebugLine(line.str());
        g_lastInventoryGuiBindingSignature = signature.str();
    }
}

bool CollectBoundBackpackEntriesForContent(
    MyGUI::Widget* backpackContent,
    std::vector<InventoryBoundEntry>* outEntries,
    std::string* outReason,
    Inventory** outInventory)
{
    if (outEntries == 0)
    {
        return false;
    }

    outEntries->clear();
    if (outReason != 0)
    {
        outReason->clear();
    }
    if (outInventory != 0)
    {
        *outInventory = 0;
    }

    if (backpackContent == 0)
    {
        return false;
    }

    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        entriesRoot = backpackContent;
    }

    std::vector<MyGUI::Widget*> entryWidgets;
    std::size_t uiQuantity = 0;
    CollectVisibleBackpackEntryWidgets(entriesRoot, &entryWidgets, &uiQuantity);
    MyGUI::Window* backpackWindow = FindOwningWindow(backpackContent);
    const std::string backpackCaptionNormalized = NormalizeInventorySearchText(
        backpackWindow == 0 ? "" : backpackWindow->getCaption().asUTF8());
    if (entryWidgets.empty())
    {
        if (outReason != 0)
        {
            *outReason = "no_visible_backpack_entry_widgets";
        }
        LogBackpackBindingTrace(
            backpackContent,
            backpackCaptionNormalized,
            0,
            0,
            0,
            "no_visible_backpack_entry_widgets",
            std::vector<std::string>(),
            std::vector<std::string>(),
            0);
        return false;
    }

    std::string inventorySource;
    std::string inventoryReason;
    std::vector<std::string> selectedCandidateRows;
    std::vector<std::string> finalCandidateRows;
    Inventory* inventory = ResolveBestBackpackInventory(
        backpackContent,
        entriesRoot,
        entryWidgets.size(),
        uiQuantity,
        &inventorySource,
        &inventoryReason,
        &selectedCandidateRows,
        &finalCandidateRows);
    if (inventory == 0)
    {
        if (outReason != 0)
        {
            *outReason = inventoryReason;
        }
        LogBackpackBindingTrace(
            backpackContent,
            backpackCaptionNormalized,
            entryWidgets.size(),
            uiQuantity,
            0,
            inventoryReason,
            selectedCandidateRows,
            finalCandidateRows,
            0);
        return false;
    }
    if (outInventory != 0)
    {
        *outInventory = inventory;
    }

    std::vector<InventorySection::SectionItem> sortedItems;
    std::size_t sectionQuantity = 0;
    if (!TryCollectBackpackSectionItems(inventory, &sortedItems, &sectionQuantity))
    {
        if (outReason != 0)
        {
            *outReason = "selected_inventory_missing_backpack_content_section";
        }
        LogBackpackBindingTrace(
            backpackContent,
            backpackCaptionNormalized,
            entryWidgets.size(),
            uiQuantity,
            inventory,
            "selected_inventory_missing_backpack_content_section",
            selectedCandidateRows,
            finalCandidateRows,
            0);
        return false;
    }

    const std::size_t pairCount =
        entryWidgets.size() < sortedItems.size() ? entryWidgets.size() : sortedItems.size();
    for (std::size_t index = 0; index < pairCount; ++index)
    {
        InventoryBoundEntry entry;
        entry.widget = entryWidgets[index];
        entry.item = sortedItems[index].item;
        entry.quantity =
            entry.item != 0 && entry.item->quantity > 0 ? entry.item->quantity : 1;
        entry.sectionName = "backpack_content";
        outEntries->push_back(entry);
    }

    if (outReason != 0)
    {
        std::stringstream reason;
        reason << inventoryReason
               << " paired_entries=" << outEntries->size();
        *outReason = reason.str();
    }

    if (outEntries->empty())
    {
        LogBackpackBindingTrace(
            backpackContent,
            backpackCaptionNormalized,
            entryWidgets.size(),
            uiQuantity,
            inventory,
            outReason == 0 ? "paired_entries=0" : *outReason,
            selectedCandidateRows,
            finalCandidateRows,
            0);
    }

    if (ShouldLogBindingDebug())
    {
        std::stringstream signature;
        signature << backpackContent
                  << "|" << inventory
                  << "|" << inventoryReason
                  << "|" << outEntries->size();
        if (signature.str() != g_lastBackpackBindingSignature)
        {
            g_lastBackpackBindingSignature = signature.str();

            std::stringstream line;
            line << "inventory backpack binding resolved"
                 << " content=" << SafeWidgetName(backpackContent)
                 << " inventory=" << inventory
                 << " ui_entries=" << entryWidgets.size()
                 << " ui_quantity=" << uiQuantity
                 << " section_items=" << sortedItems.size()
                 << " section_quantity=" << sectionQuantity
                 << " paired_entries=" << outEntries->size()
                 << " reason=\"" << inventoryReason << "\"";
            LogBindingDebugLine(line.str());
        }
    }

    return !outEntries->empty();
}

bool IsInventoryOwnedByInventoryContext(Inventory* inventory, Inventory* contextInventory)
{
    if (inventory == 0
        || contextInventory == 0
        || !IsInventoryPointerValidSafe(inventory)
        || !IsInventoryPointerValidSafe(contextInventory))
    {
        return false;
    }

    if (inventory == contextInventory)
    {
        return true;
    }

    RootObject* contextOwner = 0;
    RootObject* contextCallbackObject = 0;
    TryGetInventoryOwnerPointersSafe(contextInventory, &contextOwner, &contextCallbackObject);

    RootObject* owner = 0;
    RootObject* callbackObject = 0;
    TryGetInventoryOwnerPointersSafe(inventory, &owner, &callbackObject);
    if ((contextOwner != 0 && (owner == contextOwner || callbackObject == contextOwner))
        || (contextCallbackObject != 0
            && (owner == contextCallbackObject || callbackObject == contextCallbackObject)))
    {
        return true;
    }

    lektor<InventorySection*>& allSections = contextInventory->getAllSections();
    for (std::size_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
    {
        InventorySection* section = allSections[static_cast<uint32_t>(sectionIndex)];
        if (section == 0)
        {
            continue;
        }

        const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
        for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
        {
            Item* item = items[itemIndex].item;
            if (item == 0)
            {
                continue;
            }

            if (owner == item || callbackObject == item)
            {
                return true;
            }

            if (item->getInventory() == inventory)
            {
                return true;
            }
        }
    }

    return false;
}
