#include "InventoryHooks.h"

#include "InventoryBackpackBinding.h"
#include "InventoryBinding.h"
#include "InventoryCore.h"
#include "InventorySearchUi.h"

#include <core/Functions.h>
#include <kenshi/Character.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

class Inventory;
class InventoryGUI;
class InventoryLayout;
class InventorySectionGUI
{
public:
    MyGUI::Widget* _widget;
};

namespace
{
typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);
typedef InventoryLayout* (*CharacterCreateInventoryLayoutFn)(Character*);
typedef InventoryLayout* (*RootObjectCreateInventoryLayoutFn)(RootObject*);
typedef void (*InventoryLayoutCreateGUIFn)(
    void*,
    InventoryGUI*,
    Ogre::map<std::string, InventorySectionGUI*>::type&,
    Inventory*);

PlayerInterfaceUpdateUTFn g_updateUTOrig = 0;
CharacterCreateInventoryLayoutFn g_characterCreateInventoryLayoutOrig = 0;
RootObjectCreateInventoryLayoutFn g_rootObjectCreateInventoryLayoutOrig = 0;
InventoryLayoutCreateGUIFn g_inventoryLayoutCreateGUIOrig = 0;

bool g_inventoryLayoutCreateGUIHookInstalled = false;
bool g_inventoryLayoutCreateGUIHookAttempted = false;
bool g_inventoryLayoutCreateGUIEarlyAttempted = false;
std::uintptr_t g_expectedInventoryLayoutCreateGUIAddress = 0;
std::string g_lastCreateGuiBindingSignature;

std::uintptr_t ResolveInventoryLayoutCreateGUIHookAddress(KenshiLib::BinaryVersion versionInfo)
{
    if (versionInfo.GetVersion() != "1.0.65")
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
        return baseAddress + 0x0014EEA0;
    }

    return baseAddress + 0x0014F450;
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

void InventoryLayoutCreateGUI_hook(
    void* self,
    InventoryGUI* invGui,
    Ogre::map<std::string, InventorySectionGUI*>::type& sectionGuis,
    Inventory* inventory)
{
    if (g_inventoryLayoutCreateGUIOrig != 0)
    {
        g_inventoryLayoutCreateGUIOrig(self, invGui, sectionGuis, inventory);
    }

    if (inventory == 0)
    {
        return;
    }

    RegisterInventoryGuiInventoryLink(invGui, inventory);

    std::size_t boundSections = 0;
    std::stringstream preview;
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

        RegisterInventorySectionWidgetLink(sectionWidget, inventory, it->first);
        if (boundSections < 6)
        {
            if (boundSections > 0)
            {
                preview << " | ";
            }
            preview << it->first << ":" << sectionWidget->getName();
        }
        ++boundSections;
    }

    if (!ShouldLogBindingDebug())
    {
        return;
    }

    std::stringstream signature;
    signature << self
              << "|" << invGui
              << "|" << inventory
              << "|" << boundSections
              << "|" << preview.str();
    if (signature.str() == g_lastCreateGuiBindingSignature)
    {
        return;
    }
    g_lastCreateGuiBindingSignature = signature.str();

    std::stringstream line;
    line << "inventory createGUI binding captured"
         << " inv_gui=" << invGui
         << " inventory=" << inventory
         << " sections=" << boundSections
         << " preview=\"" << preview.str() << "\"";
    LogBindingDebugLine(line.str());
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
        return;
    }

    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            g_expectedInventoryLayoutCreateGUIAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;

    std::stringstream line;
    line << "inventory layout createGUI hook installed at "
         << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
    LogDebugLine(line.str());
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
                candidate.entryAddress >= g_expectedInventoryLayoutCreateGUIAddress
                    ? candidate.entryAddress - g_expectedInventoryLayoutCreateGUIAddress
                    : g_expectedInventoryLayoutCreateGUIAddress - candidate.entryAddress;
            const std::uintptr_t resolvedDelta =
                candidate.resolvedAddress >= g_expectedInventoryLayoutCreateGUIAddress
                    ? candidate.resolvedAddress - g_expectedInventoryLayoutCreateGUIAddress
                    : g_expectedInventoryLayoutCreateGUIAddress - candidate.resolvedAddress;
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

    std::size_t selectedIndex = 0;
    bool selected = false;
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
                bestDelta = candidates[index].deltaToExpected;
                bestIndex = index;
            }
        }

        if (bestDelta <= kExpectedAddressTolerance)
        {
            selectedIndex = bestIndex;
            selected = true;
        }
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress == 0)
    {
        selectedIndex = 0;
        selected = true;
    }

    if (!selected)
    {
        return;
    }

    g_inventoryLayoutCreateGUIHookAttempted = true;
    const CreateGUIHookCandidate& selectedCandidate = candidates[selectedIndex];
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            selectedCandidate.entryAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;

    std::stringstream line;
    line << "inventory layout createGUI hook installed from layout"
         << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
         << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
         << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
         << " slot=" << selectedCandidate.slot;
    LogDebugLine(line.str());
}

InventoryLayout* Character_createInventoryLayout_hook(Character* self)
{
    if (g_characterCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_characterCreateInventoryLayoutOrig(self);
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
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TickInventorySearchUi();
}
}

bool InstallInventoryHooks()
{
    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    g_expectedInventoryLayoutCreateGUIAddress =
        ResolveInventoryLayoutCreateGUIHookAddress(versionInfo);
    if (g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::stringstream line;
        line << "inventory layout createGUI expected target="
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        LogDebugLine(line.str());
    }
    else
    {
        LogWarnLine("inventory layout createGUI expected target unresolved; search binding will rely on deferred layout hooks only");
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();

    const std::uintptr_t characterCreateLayoutAddress =
        KenshiLib::GetRealAddress(&Character::_NV_createInventoryLayout);
    if (characterCreateLayoutAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            characterCreateLayoutAddress,
            Character_createInventoryLayout_hook,
            &g_characterCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook Character::_NV_createInventoryLayout; deferred inventory binding may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked Character::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(characterCreateLayoutAddress);
        LogDebugLine(line.str());
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
        LogWarnLine("could not hook RootObject::_NV_createInventoryLayout; deferred inventory binding for non-character layouts may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked RootObject::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(rootObjectCreateLayoutAddress);
        LogDebugLine(line.str());
    }

    const std::uintptr_t updateAddress = KenshiLib::GetRealAddress(&PlayerInterface::updateUT);
    if (updateAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            updateAddress,
            PlayerInterface_updateUT_hook,
            &g_updateUTOrig))
    {
        return false;
    }

    std::stringstream line;
    line << "hooked PlayerInterface::updateUT";
    LogDebugLine(line.str());
    return true;
}
