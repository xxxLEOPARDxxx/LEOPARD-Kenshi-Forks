#pragma once

#include "TraderCore.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Ogre
{
class Vector3;
}

struct QuantityNameKey
{
    int quantity;
    std::string key;
};

struct InventoryCandidateInfo
{
    Inventory* inventory;
    std::string source;
    bool traderPreferred;
    bool visible;
    int priorityBias;
};

bool IsDescendantOf(MyGUI::Widget* widget, MyGUI::Widget* ancestor);
std::string RootObjectDisplayNameForLog(RootObject* object);
std::size_t AbsoluteDiffSize(std::size_t left, std::size_t right);
InventoryGUI* TryGetInventoryGuiSafe(Inventory* inventory);
bool IsInventoryPointerValidSafe(Inventory* inventory);
bool IsRootObjectPointerValidSafe(RootObject* object);
Inventory* TryGetRootObjectInventorySafe(RootObject* object);
bool TryGetInventoryOwnerPointersSafe(
    Inventory* inventory,
    RootObject** outOwner,
    RootObject** outCallbackObject);
bool IsShopCounterCandidateSource(const std::string& sourceLower);
bool IsTraderAnchoredCandidateSource(const std::string& sourceLower);
bool IsRiskyCoverageFallbackSource(const std::string& sourceLower);
std::string StripInventorySourceDiagnostics(const std::string& source);
std::string BuildKeysetSourceId(const std::string& source);
void AddInventoryCandidateUnique(
    std::vector<InventoryCandidateInfo>* candidates,
    Inventory* inventory,
    const std::string& source,
    bool traderPreferred,
    bool visible,
    int priorityBias = 0);
bool CollectTraderOwnershipInventoryCandidates(
    Character* trader,
    int captionScore,
    const char* sourcePrefix,
    std::vector<InventoryCandidateInfo>* outCandidates);
void CollectNearbyInventoryCandidates(
    const Ogre::Vector3& center,
    std::vector<InventoryCandidateInfo>* outCandidates,
    std::size_t* scannedObjectsOut,
    std::size_t* inventoryObjectsOut,
    float scanRadius = 900.0f,
    int maxObjectsPerType = 512,
    int priorityBiasBase = 0,
    const char* sourcePrefix = "nearby");
bool TryResolveTraderInventoryNameKeysFromWindowCaption(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys);
bool TryResolveTraderInventoryNameKeysFromDialogue(
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys);
bool TryResolveTraderInventoryNameKeysFromActiveCharacters(
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys);
void RegisterRecentlyRefreshedInventory(Inventory* inventory);
void PruneRecentlyRefreshedInventories();
void AddInventoryGuiPointerUnique(std::vector<InventoryGUI*>* pointers, InventoryGUI* pointer);
bool HasInventoryGuiPointer(
    const std::vector<InventoryGUI*>& pointers,
    InventoryGUI* pointer);
void AddPointerAliasUnique(std::vector<std::uintptr_t>* aliases, std::uintptr_t value);
void CollectPointerAliasesFromRawPointer(const void* rawPointer, std::vector<std::uintptr_t>* aliases);
bool HasPointerAlias(
    const std::vector<std::uintptr_t>& aliases,
    const void* pointerValue);

void ClearTraderPanelInventoryBindings();
void PruneTraderPanelInventoryBindings();
bool TryGetTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    TraderPanelInventoryBinding* outBinding);
bool TryExtractSearchKeysFromInventory(Inventory* inventory, std::vector<std::string>* outKeys);
bool TryExtractQuantityNameKeysFromInventory(
    Inventory* inventory,
    std::vector<QuantityNameKey>* outKeys);
std::string ResolveUniqueQuantityNameHint(const std::vector<QuantityNameKey>& keys, int quantity);
std::string ResolveTopQuantityNameHints(
    const std::vector<QuantityNameKey>& keys,
    int quantity,
    std::size_t maxHints);
bool BuildAlignedInventoryNameHintsByQuantity(
    const std::vector<int>& uiQuantities,
    const std::vector<QuantityNameKey>& inventoryQuantityNameKeys,
    std::vector<std::string>* outAlignedNames);
void LogPanelBindingProbeOnce(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    const std::string& status,
    const TraderPanelInventoryBinding* binding);
void RegisterTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    Inventory* inventory,
    const char* stage,
    const std::string& source,
    std::size_t expectedEntryCount,
    std::size_t nonEmptyKeyCount);
void RegisterSectionWidgetInventoryLink(
    MyGUI::Widget* sectionWidget,
    Inventory* inventory,
    const std::string& sectionName);
void PruneSectionWidgetInventoryLinks();
void RegisterInventoryGuiInventoryLink(InventoryGUI* inventoryGui, Inventory* inventory);
void PruneInventoryGuiInventoryLinks();
void ClearInventoryGuiInventoryLinks();
const char* InventoryGuiBackPointerKindLabel(InventoryGuiBackPointerKind kind);
bool TryResolveInventoryFromInventoryGuiBackPointerOffsets(
    InventoryGUI* inventoryGui,
    Inventory** outInventory,
    std::size_t* outOffset,
    InventoryGuiBackPointerKind* outKind,
    RootObject** outResolvedOwner);
bool TryResolveInventoryNameKeysFromCandidates(
    const std::vector<InventoryCandidateInfo>& candidates,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory = 0);
