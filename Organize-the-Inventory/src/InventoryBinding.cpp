#include "InventoryBinding.h"

#include "InventoryCore.h"
#include "InventoryWindowDetection.h"

#include <kenshi/Inventory.h>
#include <kenshi/Item.h>

#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace
{
const std::size_t kMaxTrackedSectionWidgetRoots = 2;

struct SectionWidgetInventoryLink
{
    SectionWidgetInventoryLink()
        : sectionWidget(0)
        , inventory(0)
        , itemCount(0)
        , lastSeenTick(0)
    {
    }

    MyGUI::Widget* sectionWidget;
    Inventory* inventory;
    std::string sectionName;
    std::string widgetName;
    std::string widgetRootKey;
    std::size_t itemCount;
    DWORD lastSeenTick;
};

std::vector<SectionWidgetInventoryLink> g_sectionWidgetInventoryLinks;
std::string g_lastCollectedBindingSignature;
std::string g_lastSectionCollectionMismatchSignature;

struct RootBindingSummary
{
    RootBindingSummary()
        : lastSeenTick(0)
    {
    }

    std::string widgetRootKey;
    DWORD lastSeenTick;
};

DWORD CurrentBindingTick()
{
    return GetTickCount();
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

bool IsDescendantOf(MyGUI::Widget* child, MyGUI::Widget* ancestor)
{
    if (child == 0 || ancestor == 0)
    {
        return false;
    }

    MyGUI::Widget* current = child;
    int depth = 0;
    while (current != 0 && depth < 96)
    {
        if (current == ancestor)
        {
            return true;
        }

        __try
        {
            current = current->getParent();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        ++depth;
    }

    return false;
}

std::string ExtractWidgetRootKey(const std::string& widgetName)
{
    if (widgetName.empty())
    {
        return std::string();
    }

    const std::string::size_type separator = widgetName.rfind('_');
    if (separator == std::string::npos || separator == 0)
    {
        return widgetName;
    }

    return widgetName.substr(0, separator);
}

void PruneSectionWidgetInventoryLinks()
{
    if (g_sectionWidgetInventoryLinks.empty())
    {
        return;
    }

    const DWORD now = CurrentBindingTick();
    std::vector<SectionWidgetInventoryLink> kept;
    kept.reserve(g_sectionWidgetInventoryLinks.size());
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        const SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (now >= link.lastSeenTick && now - link.lastSeenTick > 60000UL)
        {
            continue;
        }

        kept.push_back(link);
    }

    std::vector<RootBindingSummary> rootSummaries;
    for (std::size_t index = 0; index < kept.size(); ++index)
    {
        const SectionWidgetInventoryLink& link = kept[index];
        if (link.widgetRootKey.empty())
        {
            continue;
        }

        RootBindingSummary* summary = 0;
        for (std::size_t rootIndex = 0; rootIndex < rootSummaries.size(); ++rootIndex)
        {
            if (rootSummaries[rootIndex].widgetRootKey == link.widgetRootKey)
            {
                summary = &rootSummaries[rootIndex];
                break;
            }
        }

        if (summary == 0)
        {
            RootBindingSummary newSummary;
            newSummary.widgetRootKey = link.widgetRootKey;
            newSummary.lastSeenTick = link.lastSeenTick;
            rootSummaries.push_back(newSummary);
            continue;
        }

        if (link.lastSeenTick > summary->lastSeenTick)
        {
            summary->lastSeenTick = link.lastSeenTick;
        }
    }

    if (rootSummaries.size() > kMaxTrackedSectionWidgetRoots)
    {
        std::sort(
            rootSummaries.begin(),
            rootSummaries.end(),
            [](const RootBindingSummary& left, const RootBindingSummary& right)
            {
                return left.lastSeenTick > right.lastSeenTick;
            });

        std::vector<SectionWidgetInventoryLink> limited;
        limited.reserve(kept.size());
        for (std::size_t index = 0; index < kept.size(); ++index)
        {
            const SectionWidgetInventoryLink& link = kept[index];
            if (link.widgetRootKey.empty())
            {
                limited.push_back(link);
                continue;
            }

            bool keepRoot = false;
            for (std::size_t rootIndex = 0;
                 rootIndex < kMaxTrackedSectionWidgetRoots && rootIndex < rootSummaries.size();
                 ++rootIndex)
            {
                if (rootSummaries[rootIndex].widgetRootKey == link.widgetRootKey)
                {
                    keepRoot = true;
                    break;
                }
            }

            if (keepRoot)
            {
                limited.push_back(link);
            }
        }

        kept.swap(limited);
    }

    g_sectionWidgetInventoryLinks.swap(kept);
}

bool StringContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return true;
    }

    std::string needleUpper;
    for (const char* current = needle; *current != '\0'; ++current)
    {
        needleUpper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*current))));
    }

    std::string haystackUpper;
    haystackUpper.reserve(haystack.size());
    for (std::size_t index = 0; index < haystack.size(); ++index)
    {
        haystackUpper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(haystack[index]))));
    }

    return haystackUpper.find(needleUpper) != std::string::npos;
}

bool HasEntryMarkerWithinDepth(MyGUI::Widget* widget, const char* token, int depthRemaining)
{
    if (widget == 0 || token == 0 || depthRemaining < 0)
    {
        return false;
    }

    if (StringContainsAsciiCaseInsensitive(widget->getName(), token))
    {
        return true;
    }

    if (depthRemaining == 0)
    {
        return false;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (HasEntryMarkerWithinDepth(widget->getChildAt(childIndex), token, depthRemaining - 1))
        {
            return true;
        }
    }

    return false;
}

bool LooksLikeInventoryEntryWidget(MyGUI::Widget* widget)
{
    if (widget == 0 || !widget->getInheritedVisible())
    {
        return false;
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    if (coord.width <= 0 || coord.height <= 0)
    {
        return false;
    }

    const std::string widgetName = SafeWidgetName(widget);
    if (StringContainsAsciiCaseInsensitive(widgetName, "background")
        || StringContainsAsciiCaseInsensitive(widgetName, "client")
        || StringContainsAsciiCaseInsensitive(widgetName, "slot"))
    {
        return false;
    }

    return HasEntryMarkerWithinDepth(widget, "ItemImage", 1)
        || HasEntryMarkerWithinDepth(widget, "QuantityText", 1)
        || HasEntryMarkerWithinDepth(widget, "ChargeBar", 1);
}

void AddWidgetUnique(std::vector<MyGUI::Widget*>* widgets, MyGUI::Widget* candidate)
{
    if (widgets == 0 || candidate == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < widgets->size(); ++index)
    {
        if ((*widgets)[index] == candidate)
        {
            return;
        }
    }

    widgets->push_back(candidate);
}

void CollectLikelySectionEntryWidgets(
    MyGUI::Widget* sectionWidget,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (sectionWidget == 0 || outWidgets == 0)
    {
        return;
    }

    const std::size_t childCount = sectionWidget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = sectionWidget->getChildAt(childIndex);
        if (LooksLikeInventoryEntryWidget(child))
        {
            AddWidgetUnique(outWidgets, child);
        }
    }

    if (!outWidgets->empty())
    {
        return;
    }

    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* wrapper = sectionWidget->getChildAt(childIndex);
        if (wrapper == 0 || !wrapper->getInheritedVisible())
        {
            continue;
        }

        const std::size_t nestedChildCount = wrapper->getChildCount();
        for (std::size_t nestedIndex = 0; nestedIndex < nestedChildCount; ++nestedIndex)
        {
            MyGUI::Widget* nested = wrapper->getChildAt(nestedIndex);
            if (LooksLikeInventoryEntryWidget(nested))
            {
                AddWidgetUnique(outWidgets, nested);
            }
        }
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

void AddBoundEntryUnique(
    std::vector<InventoryBoundEntry>* entries,
    MyGUI::Widget* widget,
    Item* item,
    const std::string& sectionName)
{
    if (entries == 0 || widget == 0 || item == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < entries->size(); ++index)
    {
        if ((*entries)[index].widget == widget)
        {
            return;
        }
    }

    InventoryBoundEntry entry;
    entry.widget = widget;
    entry.item = item;
    entry.quantity = item->quantity > 0 ? item->quantity : 1;
    entry.sectionName = sectionName;
    entries->push_back(entry);
}

void LogSectionCollectionMismatchOnce(
    const SectionWidgetInventoryLink& link,
    std::size_t widgetCount,
    std::size_t itemCount)
{
    if (!ShouldLogBindingDebug())
    {
        return;
    }

    std::stringstream signature;
    signature << link.sectionWidget
              << "|" << link.inventory
              << "|" << link.sectionName
              << "|" << widgetCount
              << "|" << itemCount;
    if (signature.str() == g_lastSectionCollectionMismatchSignature)
    {
        return;
    }
    g_lastSectionCollectionMismatchSignature = signature.str();

    std::stringstream line;
    line << "inventory binding section candidate mismatch"
         << " section=" << link.sectionName
         << " widget=" << link.widgetName
         << " ui_entries=" << widgetCount
         << " section_items=" << itemCount;
    LogBindingDebugLine(line.str());
}

void AppendBoundEntriesForSectionLink(
    const SectionWidgetInventoryLink& link,
    std::vector<InventoryBoundEntry>* outEntries)
{
    if (outEntries == 0
        || link.sectionWidget == 0
        || link.inventory == 0
        || !IsInventoryPointerValidSafe(link.inventory))
    {
        return;
    }

    InventorySection* section = link.inventory->getSection(link.sectionName);
    if (section == 0)
    {
        return;
    }

    std::vector<MyGUI::Widget*> sectionEntryWidgets;
    CollectLikelySectionEntryWidgets(link.sectionWidget, &sectionEntryWidgets);
    if (sectionEntryWidgets.empty())
    {
        return;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& rawItems = section->getItems();
    if (rawItems.empty())
    {
        return;
    }

    std::vector<InventorySection::SectionItem> sortedItems(rawItems.begin(), rawItems.end());
    std::sort(sortedItems.begin(), sortedItems.end(), SectionItemTopLeftLess);
    std::sort(sectionEntryWidgets.begin(), sectionEntryWidgets.end(), WidgetCoordTopLeftLess);

    if (sectionEntryWidgets.size() != sortedItems.size())
    {
        LogSectionCollectionMismatchOnce(link, sectionEntryWidgets.size(), sortedItems.size());
    }

    const std::size_t pairCount =
        sectionEntryWidgets.size() < sortedItems.size()
            ? sectionEntryWidgets.size()
            : sortedItems.size();
    for (std::size_t index = 0; index < pairCount; ++index)
    {
        AddBoundEntryUnique(
            outEntries,
            sectionEntryWidgets[index],
            sortedItems[index].item,
            link.sectionName);
    }
}
}

void RegisterInventorySectionWidgetLink(
    MyGUI::Widget* sectionWidget,
    Inventory* inventory,
    const std::string& sectionName)
{
    if (sectionWidget == 0
        || inventory == 0
        || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    const DWORD now = CurrentBindingTick();
    const std::string widgetName = SafeWidgetName(sectionWidget);
    const std::string widgetRootKey = ExtractWidgetRootKey(widgetName);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        const bool sameWidget = link.sectionWidget == sectionWidget;
        const bool sameRootSection =
            !widgetRootKey.empty()
            && link.widgetRootKey == widgetRootKey
            && link.sectionName == sectionName;
        if (!sameWidget && !sameRootSection)
        {
            continue;
        }

        link.inventory = inventory;
        link.sectionName = sectionName;
        link.widgetName = widgetName;
        link.widgetRootKey = widgetRootKey;
        link.itemCount = itemCount;
        link.lastSeenTick = now;
        link.sectionWidget = sectionWidget;
        return;
    }

    SectionWidgetInventoryLink link;
    link.sectionWidget = sectionWidget;
    link.inventory = inventory;
    link.sectionName = sectionName;
    link.widgetName = widgetName;
    link.widgetRootKey = widgetRootKey;
    link.itemCount = itemCount;
    link.lastSeenTick = now;
    g_sectionWidgetInventoryLinks.push_back(link);
}

bool CollectBoundInventoryEntriesForRoot(
    MyGUI::Widget* inventoryRoot,
    std::vector<InventoryBoundEntry>* outEntries,
    std::string* outReason)
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

    if (inventoryRoot == 0)
    {
        return false;
    }

    PruneSectionWidgetInventoryLinks();
    if (g_sectionWidgetInventoryLinks.empty())
    {
        if (outReason != 0)
        {
            *outReason = "no_section_widget_links";
        }
        return false;
    }

    const std::string inventoryRootKey = ExtractWidgetRootKey(SafeWidgetName(inventoryRoot));
    std::size_t matchedSections = 0;
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        const SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        if (!inventoryRootKey.empty()
            && !link.widgetRootKey.empty()
            && link.widgetRootKey != inventoryRootKey)
        {
            continue;
        }

        if (!IsDescendantOf(link.sectionWidget, inventoryRoot))
        {
            continue;
        }

        ++matchedSections;
        AppendBoundEntriesForSectionLink(link, outEntries);
    }

    if (outReason != 0)
    {
        std::stringstream reason;
        reason << "matched_sections=" << matchedSections
               << " tracked_links=" << g_sectionWidgetInventoryLinks.size()
               << " bound_entries=" << outEntries->size();
        *outReason = reason.str();
    }

    if (ShouldLogBindingDebug())
    {
        std::stringstream signature;
        signature << inventoryRoot
                  << "|" << matchedSections
                  << "|" << outEntries->size();
        if (signature.str() != g_lastCollectedBindingSignature)
        {
            g_lastCollectedBindingSignature = signature.str();

            std::stringstream line;
            line << "inventory binding collected section entries"
                 << " root=" << SafeWidgetName(inventoryRoot)
                 << " matched_sections=" << matchedSections
                 << " bound_entries=" << outEntries->size()
                 << " tracked_links=" << g_sectionWidgetInventoryLinks.size();
            LogBindingDebugLine(line.str());
        }
    }

    return !outEntries->empty();
}
