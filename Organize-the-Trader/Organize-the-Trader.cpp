#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/Building.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Minimal ownership interface stub to avoid including Platoon.h
// (it conflicts with Building.h enum declarations in this SDK drop).
class Ownerships
{
public:
    void getHomeFurnitureOfType(lektor<Building*>& out, BuildingFunction type);
    void getBuildingsWithFunction(lektor<Building*>& out, BuildingFunction bf);
};

class InventoryGUI;
class InventoryIcon;
class InventoryLayout;
class InventorySectionGUI
{
public:
    MyGUI::Widget* _widget;
    Ogre::vector<InventoryIcon*>::type _icons;
};

#include "TraderCore.h"
#include "TraderDiagnostics.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchPipeline.h"
#include "TraderSearchUi.h"
#include "TraderSearchText.h"
#include "TraderModHub.h"
#include "TraderWindowDetection.h"

namespace
{
const char* kToggleHotkeyHint = "Ctrl+Shift+F8";
const char* kDiagnosticsHotkeyHint = "Ctrl+Shift+F9";

#define g_updateUTOrig (TraderState().hook.g_updateUTOrig)
#define g_characterCreateInventoryLayoutOrig (TraderState().hook.g_characterCreateInventoryLayoutOrig)
#define g_rootObjectCreateInventoryLayoutOrig (TraderState().hook.g_rootObjectCreateInventoryLayoutOrig)
#define g_inventoryLayoutCreateGUIOrig (TraderState().hook.g_inventoryLayoutCreateGUIOrig)
#define g_inventoryLayoutCreateGUIHookInstalled (TraderState().hook.g_inventoryLayoutCreateGUIHookInstalled)
#define g_inventoryLayoutCreateGUIHookAttempted (TraderState().hook.g_inventoryLayoutCreateGUIHookAttempted)
#define g_expectedInventoryLayoutCreateGUIAddress (TraderState().hook.g_expectedInventoryLayoutCreateGUIAddress)
#define g_inventoryLayoutCreateGUIEarlyAttempted (TraderState().hook.g_inventoryLayoutCreateGUIEarlyAttempted)
#define g_inventoryLayoutCreateGUIHookCallCount (TraderState().hook.g_inventoryLayoutCreateGUIHookCallCount)
#define g_inventoryLayoutCreateInventoryLayoutLogCount (TraderState().hook.g_inventoryLayoutCreateInventoryLayoutLogCount)
#define g_lastInventoryLayoutReturnSignature (TraderState().hook.g_lastInventoryLayoutReturnSignature)

#define g_updateTickCounter (TraderState().core.g_updateTickCounter)
#define g_controlsEnabled (TraderState().core.g_controlsEnabled)
#define g_showSearchEntryCount (TraderState().core.g_showSearchEntryCount)
#define g_showSearchQuantityCount (TraderState().core.g_showSearchQuantityCount)
#define g_debugLogging (TraderState().core.g_debugLogging)
#define g_debugSearchLogging (TraderState().core.g_debugSearchLogging)
#define g_debugBindingLogging (TraderState().core.g_debugBindingLogging)
#define g_searchInputConfiguredWidth (TraderState().core.g_searchInputConfiguredWidth)
#define g_searchInputConfiguredHeight (TraderState().core.g_searchInputConfiguredHeight)

#define g_loggedNoVisibleTraderTarget (TraderState().windowDetection.g_loggedNoVisibleTraderTarget)

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)
#define g_loggedMissingBackpackForSearch (TraderState().search.g_loggedMissingBackpackForSearch)
#define g_loggedMissingSearchableItemText (TraderState().search.g_loggedMissingSearchableItemText)
#define g_loggedNumericOnlyQueryIgnored (TraderState().search.g_loggedNumericOnlyQueryIgnored)
#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_pendingSlashFocusBaseQuery (TraderState().search.g_pendingSlashFocusBaseQuery)
#define g_activeTraderTargetId (TraderState().search.g_activeTraderTargetId)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastInventoryKeysetSelectionSignature (TraderState().search.g_lastInventoryKeysetSelectionSignature)
#define g_lastInventoryKeysetLowCoverageSignature (TraderState().search.g_lastInventoryKeysetLowCoverageSignature)
#define g_lastObservedTraderEntriesStateSignature (TraderState().search.g_lastObservedTraderEntriesStateSignature)
#define g_lastZeroMatchGuardSignature (TraderState().search.g_lastZeroMatchGuardSignature)
#define g_lastCoverageFallbackDecisionSignature (TraderState().search.g_lastCoverageFallbackDecisionSignature)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)
#define g_lockedKeysetTraderParent (TraderState().search.g_lockedKeysetTraderParent)
#define g_lockedKeysetStage (TraderState().search.g_lockedKeysetStage)
#define g_lockedKeysetSourceId (TraderState().search.g_lockedKeysetSourceId)
#define g_lockedKeysetSourcePreview (TraderState().search.g_lockedKeysetSourcePreview)
#define g_lockedKeysetExpectedCount (TraderState().search.g_lockedKeysetExpectedCount)
#define g_lastKeysetLockSignature (TraderState().search.g_lastKeysetLockSignature)
#define g_lastSearchVisibleEntryCount (TraderState().search.g_lastSearchVisibleEntryCount)
#define g_lastSearchTotalEntryCount (TraderState().search.g_lastSearchTotalEntryCount)

#define g_prevToggleHotkeyDown (TraderState().searchUi.g_prevToggleHotkeyDown)
#define g_prevDiagnosticsHotkeyDown (TraderState().searchUi.g_prevDiagnosticsHotkeyDown)
#define g_prevSearchSlashHotkeyDown (TraderState().searchUi.g_prevSearchSlashHotkeyDown)
#define g_prevSearchCtrlFHotkeyDown (TraderState().searchUi.g_prevSearchCtrlFHotkeyDown)
#define g_controlsWereInjected (TraderState().searchUi.g_controlsWereInjected)
#define g_suppressNextSearchEditChangeEvent (TraderState().searchUi.g_suppressNextSearchEditChangeEvent)
#define g_pendingSlashFocusTextSuppression (TraderState().searchUi.g_pendingSlashFocusTextSuppression)
#define g_focusSearchEditOnNextInjection (TraderState().searchUi.g_focusSearchEditOnNextInjection)
#define g_searchContainerDragging (TraderState().searchUi.g_searchContainerDragging)
#define g_searchContainerPositionCustomized (TraderState().searchUi.g_searchContainerPositionCustomized)
#define g_searchContainerDragLastMouseX (TraderState().searchUi.g_searchContainerDragLastMouseX)
#define g_searchContainerDragLastMouseY (TraderState().searchUi.g_searchContainerDragLastMouseY)
#define g_searchContainerStoredLeft (TraderState().searchUi.g_searchContainerStoredLeft)
#define g_searchContainerStoredTop (TraderState().searchUi.g_searchContainerStoredTop)

#define g_loggedInventoryBindingFailure (TraderState().binding.g_loggedInventoryBindingFailure)
#define g_loggedInventoryBindingDiagnostics (TraderState().binding.g_loggedInventoryBindingDiagnostics)
#define g_cachedHoveredWidgetInventory (TraderState().binding.g_cachedHoveredWidgetInventory)
#define g_cachedHoveredWidgetInventorySignature (TraderState().binding.g_cachedHoveredWidgetInventorySignature)
#define g_lastSectionWidgetBindingSignature (TraderState().binding.g_lastSectionWidgetBindingSignature)
#define g_sectionWidgetInventoryLinks (TraderState().binding.g_sectionWidgetInventoryLinks)
#define g_lastInventoryGuiBindingSignature (TraderState().binding.g_lastInventoryGuiBindingSignature)
#define g_inventoryGuiInventoryLinks (TraderState().binding.g_inventoryGuiInventoryLinks)
#define g_inventoryGuiBackPointerOffsets (TraderState().binding.g_inventoryGuiBackPointerOffsets)
#define g_lastInventoryGuiBackPointerLearningSignature (TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature)
#define g_lastInventoryGuiBackPointerResolutionSignature (TraderState().binding.g_lastInventoryGuiBackPointerResolutionSignature)
#define g_lastInventoryGuiBackPointerResolutionFailureSignature (TraderState().binding.g_lastInventoryGuiBackPointerResolutionFailureSignature)
#define g_traderPanelInventoryBindings (TraderState().binding.g_traderPanelInventoryBindings)
#define g_lastPanelBindingSignature (TraderState().binding.g_lastPanelBindingSignature)
#define g_lastPanelBindingRefusedSignature (TraderState().binding.g_lastPanelBindingRefusedSignature)
#define g_lastPanelBindingProbeSignature (TraderState().binding.g_lastPanelBindingProbeSignature)
#define g_recentRefreshedInventories (TraderState().binding.g_recentRefreshedInventories)
bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

void InventoryLayoutCreateGUI_hook(
    void* self,
    InventoryGUI* invGui,
    Ogre::map<std::string, InventorySectionGUI*>::type& sectionGuis,
    Inventory* inv)
{
    if (g_inventoryLayoutCreateGUIOrig != 0)
    {
        g_inventoryLayoutCreateGUIOrig(self, invGui, sectionGuis, inv);
    }

    if (inv == 0)
    {
        return;
    }

    if (FindControlsContainer() != 0)
    {
        MarkSearchFilterDirty("inventory_layout_create_gui");
    }

    ++g_inventoryLayoutCreateGUIHookCallCount;
    if (ShouldLogBindingDebug() && g_inventoryLayoutCreateGUIHookCallCount <= 8)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::vector<std::string> keys;
        std::string keyPreview;
        if (TryExtractSearchKeysFromInventory(inv, &keys) && !keys.empty())
        {
            keyPreview = BuildKeyPreviewForLog(keys, 8);
        }

        MyGUI::Widget* firstSectionWidget = 0;
        std::string firstSectionName;
        for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
             it != sectionGuis.end();
             ++it)
        {
            InventorySectionGUI* sectionGui = it->second;
            if (sectionGui == 0 || sectionGui->_widget == 0)
            {
                continue;
            }

            firstSectionWidget = sectionGui->_widget;
            firstSectionName = it->first;
            break;
        }

        std::stringstream line;
        line << "createGUI hook call"
             << " index=" << g_inventoryLayoutCreateGUIHookCallCount
             << " self=" << self
             << " inv_gui=" << invGui
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections_total=" << sectionGuis.size()
             << " first_section=" << firstSectionName
             << " first_section_widget=" << firstSectionWidget;
        if (!keyPreview.empty())
        {
            line << " key_preview=\"" << TruncateForLog(keyPreview, 180) << "\"";
        }
        LogInfoLine(line.str());
    }

    RegisterInventoryGuiInventoryLink(invGui, inv);

    std::size_t boundSections = 0;
    std::stringstream sectionPreview;
    for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
         it != sectionGuis.end();
         ++it)
    {
        InventorySectionGUI* sectionGui = it->second;
        MyGUI::Widget* sectionWidget = sectionGui == 0 ? 0 : sectionGui->_widget;
        if (sectionWidget == 0)
        {
            continue;
        }

        RegisterSectionWidgetInventoryLink(sectionWidget, inv, it->first);
        if (boundSections < 6)
        {
            if (boundSections > 0)
            {
                sectionPreview << " | ";
            }
            sectionPreview << it->first << ":" << SafeWidgetName(sectionWidget);
        }

        ++boundSections;
    }

    if (boundSections == 0)
    {
        return;
    }

    std::stringstream signature;
    signature << inv
              << "|sections=" << boundSections
              << "|" << sectionPreview.str();
    if (signature.str() != g_lastSectionWidgetBindingSignature)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::stringstream line;
        line << "inventory layout section binding"
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections=" << boundSections
             << " preview=\"" << TruncateForLog(sectionPreview.str(), 220) << "\"";
        LogBindingDebugLine(line.str());
        g_lastSectionWidgetBindingSignature = signature.str();
    }
}

std::uintptr_t ResolveInventoryLayoutCreateGUIHookAddress(KenshiLib::BinaryVersion versionInfo)
{
    const std::string version = versionInfo.GetVersion();
    if (version != "1.0.65" && version != "1.0.68")
    {
        return 0;
    }

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0)
    {
        return 0;
    }

    const bool hasNonZeroPlatform = versionInfo.GetPlatform() != 0;
    if (hasNonZeroPlatform)
    {
        if (version == "1.0.65")
        {
            // 1.0.65 Steam: slot 37 (0x14F530) resolves into the backpack attach branch,
            // but the inventory-item population path lives at slot 1 / RVA 0x14EEA0.
            return baseAddress + 0x0014EEA0;
        }
        return baseAddress + 0x0014F570;
    }

    if (version == "1.0.65")
    {
        return baseAddress + 0x0014F450;
    }
    return baseAddress + 0x0014F470;
}

bool IsAddressInMainModuleTextSection(std::uintptr_t address)
{
    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0 || address == 0)
    {
        return false;
    }

    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(baseAddress);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    const IMAGE_NT_HEADERS64* ntHeaders =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(baseAddress + static_cast<std::uintptr_t>(dosHeader->e_lfanew));
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (unsigned int index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index)
    {
        const IMAGE_SECTION_HEADER& section = sectionHeader[index];
        if (std::memcmp(section.Name, ".text", 5) != 0)
        {
            continue;
        }

        const std::uintptr_t sectionBegin = baseAddress + static_cast<std::uintptr_t>(section.VirtualAddress);
        const std::uintptr_t sectionSpan =
            section.Misc.VirtualSize > 0
                ? static_cast<std::uintptr_t>(section.Misc.VirtualSize)
                : static_cast<std::uintptr_t>(section.SizeOfRawData);
        const std::uintptr_t sectionEnd = sectionBegin + sectionSpan;
        return address >= sectionBegin && address < sectionEnd;
    }

    return false;
}

std::string FormatAbsoluteAddressForLog(std::uintptr_t address)
{
    if (address == 0)
    {
        return "0x0";
    }

    std::stringstream out;
    out << "0x" << std::hex << std::uppercase << address;

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress != 0 && address >= baseAddress)
    {
        out << " (rva=0x" << std::hex << std::uppercase << (address - baseAddress) << ")";
    }
    return out.str();
}

void TryInstallInventoryLayoutCreateGUIHookEarly()
{
    if (g_inventoryLayoutCreateGUIHookInstalled || g_inventoryLayoutCreateGUIOrig != 0)
    {
        return;
    }

    if (g_inventoryLayoutCreateGUIEarlyAttempted)
    {
        return;
    }
    g_inventoryLayoutCreateGUIEarlyAttempted = true;

    if (g_expectedInventoryLayoutCreateGUIAddress == 0
        || !IsAddressInMainModuleTextSection(g_expectedInventoryLayoutCreateGUIAddress))
    {
        std::stringstream line;
        line << "early createGUI hook skipped: expected target invalid "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            g_expectedInventoryLayoutCreateGUIAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "early createGUI hook install failed at "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (will rely on deferred install)";
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    std::stringstream line;
    line << "early createGUI hook installed at "
         << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
    LogInfoLine(line.str());
}

std::uintptr_t PointerDistance(std::uintptr_t left, std::uintptr_t right)
{
    return left >= right ? (left - right) : (right - left);
}

bool TryResolveRelativeJumpTarget(std::uintptr_t address, std::uintptr_t* outTarget)
{
    if (outTarget == 0 || address == 0 || !IsAddressInMainModuleTextSection(address))
    {
        return false;
    }

    const unsigned char* code = reinterpret_cast<const unsigned char*>(address);
    if (code == 0 || code[0] != 0xE9)
    {
        return false;
    }

    const std::int32_t rel = *reinterpret_cast<const std::int32_t*>(code + 1);
    const std::uintptr_t target =
        address + static_cast<std::uintptr_t>(5)
        + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
    if (!IsAddressInMainModuleTextSection(target))
    {
        return false;
    }

    *outTarget = target;
    return true;
}

std::uintptr_t ResolveExecutableThunkTarget(std::uintptr_t address, int maxHops)
{
    std::uintptr_t resolved = address;
    for (int hop = 0; hop < maxHops; ++hop)
    {
        std::uintptr_t nextTarget = 0;
        if (!TryResolveRelativeJumpTarget(resolved, &nextTarget) || nextTarget == resolved)
        {
            break;
        }
        resolved = nextTarget;
    }
    return resolved;
}

void LogInventoryLayoutReturnDiagnostics(
    const char* sourceTag,
    RootObject* owner,
    InventoryLayout* layout)
{
    if (!ShouldLogVerboseBindingDiagnostics())
    {
        return;
    }

    if (layout == 0 || sourceTag == 0)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct LayoutSlotCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    std::vector<LayoutSlotCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t entryAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (entryAddress == 0 || !IsAddressInMainModuleTextSection(entryAddress))
        {
            continue;
        }

        LayoutSlotCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = entryAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(entryAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return;
    }

    std::size_t nearestIndex = 0;
    bool exactMatchFound = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (candidates[index].exactExpectedMatch)
        {
            nearestIndex = index;
            exactMatchFound = true;
            break;
        }

        if (index == 0 || candidates[index].deltaToExpected < candidates[nearestIndex].deltaToExpected)
        {
            nearestIndex = index;
        }
    }

    std::stringstream preview;
    const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
    for (std::size_t index = 0; index < maxPreview; ++index)
    {
        if (index > 0)
        {
            preview << " | ";
        }
        preview << "slot" << candidates[index].slot
                << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
    }

    const LayoutSlotCandidate& nearest = candidates[nearestIndex];
    std::stringstream signature;
    signature << sourceTag
              << "|" << owner
              << "|" << layout
              << "|" << reinterpret_cast<void*>(*vtableHolder)
              << "|" << exactMatchFound
              << "|" << nearest.slot
              << "|" << nearest.entryAddress
              << "|" << g_inventoryLayoutCreateGUIHookInstalled
              << "|" << g_inventoryLayoutCreateGUIHookCallCount;
    if (signature.str() == g_lastInventoryLayoutReturnSignature
        && g_inventoryLayoutCreateInventoryLayoutLogCount >= 16)
    {
        return;
    }

    ++g_inventoryLayoutCreateInventoryLayoutLogCount;
    g_lastInventoryLayoutReturnSignature = signature.str();

    std::stringstream line;
    line << "createInventoryLayout result"
         << " source=" << sourceTag
         << " owner=" << RootObjectDisplayNameForLog(owner)
         << " layout=" << layout
         << " vtable=" << reinterpret_cast<void*>(*vtableHolder)
         << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
         << " exact_expected_slot=" << (exactMatchFound ? "true" : "false")
         << " nearest_slot=" << nearest.slot
         << " nearest_entry=" << FormatAbsoluteAddressForLog(nearest.entryAddress)
         << " nearest_resolved=" << FormatAbsoluteAddressForLog(nearest.resolvedAddress);
    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        line << " nearest_delta=0x" << std::hex << std::uppercase << nearest.deltaToExpected << std::dec;
    }
    line << " hook_installed=" << (g_inventoryLayoutCreateGUIHookInstalled ? "true" : "false")
         << " hook_calls=" << g_inventoryLayoutCreateGUIHookCallCount
         << " preview=\"" << TruncateForLog(preview.str(), 320) << "\"";

    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        LogWarnLine(line.str());
        return;
    }

    LogInfoLine(line.str());
}

void TryInstallInventoryLayoutCreateGUIHookFromLayout(InventoryLayout* layout)
{
    if (layout == 0 || g_inventoryLayoutCreateGUIHookInstalled)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct CreateGUIHookCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    const std::uintptr_t kExpectedAddressTolerance = 0x3000;
    std::vector<CreateGUIHookCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t candidateAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (candidateAddress == 0 || !IsAddressInMainModuleTextSection(candidateAddress))
        {
            continue;
        }

        CreateGUIHookCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = candidateAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(candidateAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        if (!g_inventoryLayoutCreateGUIHookAttempted)
        {
            LogWarnLine("deferred inventory layout createGUI hook skipped: no valid vtable slot candidate");
            g_inventoryLayoutCreateGUIHookAttempted = true;
        }
        return;
    }

    std::size_t selectedIndex = 0;
    bool selected = false;
    bool selectedNearExpected = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (!candidates[index].exactExpectedMatch)
        {
            continue;
        }
        selectedIndex = index;
        selected = true;
        break;
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::size_t bestIndex = 0;
        std::uintptr_t bestDelta = static_cast<std::uintptr_t>(-1);
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            if (candidates[index].deltaToExpected < bestDelta)
            {
                bestIndex = index;
                bestDelta = candidates[index].deltaToExpected;
            }
        }

        if (bestDelta <= kExpectedAddressTolerance)
        {
            selectedIndex = bestIndex;
            selected = true;
            selectedNearExpected = true;
        }
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress == 0)
    {
        selectedIndex = 0;
        selected = true;
    }

    if (!selected)
    {
        std::stringstream candidatesLine;
        candidatesLine << "deferred inventory layout createGUI hook skipped: no expected/nearby candidate"
                       << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
        for (std::size_t index = 0; index < maxPreview; ++index)
        {
            candidatesLine << " slot" << candidates[index].slot
                           << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                           << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
        }
        LogWarnLine(candidatesLine.str());
        g_inventoryLayoutCreateGUIHookAttempted = true;
        return;
    }

    const CreateGUIHookCandidate& selectedCandidate = candidates[selectedIndex];
    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            selectedCandidate.entryAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "could not install deferred inventory layout createGUI hook"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    if (selectedNearExpected)
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed from nearby candidate"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " delta=0x" << std::hex << std::uppercase << selectedCandidate.deltaToExpected
             << std::dec
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
    }
    else
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogInfoLine(line.str());
    }
}

InventoryLayout* Character_createInventoryLayout_hook(Character* self)
{
    if (g_characterCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_characterCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("character", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

InventoryLayout* RootObject_createInventoryLayout_hook(RootObject* self)
{
    if (g_rootObjectCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_rootObjectCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("root_object", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    ++g_updateTickCounter;
    if ((g_updateTickCounter % 240ULL) == 0ULL)
    {
        PruneRecentlyRefreshedInventories();
        PruneSectionWidgetInventoryLinks();
        PruneInventoryGuiInventoryLinks();
        PruneTraderPanelInventoryBindings();
    }
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TraderModHub_TickAttachRetry();
    TickPhase2ControlsScaffold();
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfoLine("startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogErrorLine("unsupported Kenshi version/platform");
        return;
    }

    {
        std::stringstream line;
        line << "binary version detected"
             << " platform=" << versionInfo.GetPlatform()
             << " version=" << versionInfo.GetVersion();
        LogInfoLine(line.str());
    }

    LoadModConfig();

    g_expectedInventoryLayoutCreateGUIAddress =
        ResolveInventoryLayoutCreateGUIHookAddress(versionInfo);
    if (g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::stringstream line;
        line << "inventory layout createGUI expected target="
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (deferred install)";
        LogInfoLine(line.str());
    }
    else
    {
        LogWarnLine("inventory layout createGUI expected target unresolved; deferred hook disabled");
    }

    LogInfoLine("Inventory::refreshGui hook disabled (unsafe in current runtime)");

    const std::uintptr_t characterCreateLayoutAddress =
        KenshiLib::GetRealAddress(&Character::_NV_createInventoryLayout);
    if (characterCreateLayoutAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            characterCreateLayoutAddress,
            Character_createInventoryLayout_hook,
            &g_characterCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook Character::_NV_createInventoryLayout; deferred createGUI binding may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked Character::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(characterCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    const std::uintptr_t rootObjectCreateLayoutAddress =
        KenshiLib::GetRealAddress(&RootObject::_NV_createInventoryLayout);
    if (rootObjectCreateLayoutAddress == 0
        || rootObjectCreateLayoutAddress == characterCreateLayoutAddress
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            rootObjectCreateLayoutAddress,
            RootObject_createInventoryLayout_hook,
            &g_rootObjectCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook RootObject::_NV_createInventoryLayout; deferred createGUI binding for building/shop layouts may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked RootObject::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(rootObjectCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &g_updateUTOrig))
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    TraderModHub_OnStartup();

    std::stringstream info;
    info << "phase 2 controls scaffold active."
         << " Auto-attach is enabled for detected trader windows."
         << " Press " << kToggleHotkeyHint << " to hide/show."
         << " Press " << kDiagnosticsHotkeyHint << " for a diagnostics snapshot.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
