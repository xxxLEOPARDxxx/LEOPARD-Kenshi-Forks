#include "InventoryDiagnostics.h"

#include "InventoryBinding.h"
#include "InventoryCore.h"
#include "InventoryPerformanceTelemetry.h"
#include "InventorySearchText.h"
#include "InventoryWindowDetection.h"

#include <kenshi/Item.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

namespace
{
const DWORD kBackpackDiagnosticsScanIntervalMs = 1000UL;

std::string g_lastBackpackDiagnosticsSignature;
MyGUI::Widget* g_lastBackpackDiagnosticsParent = 0;
DWORD g_lastBackpackDiagnosticsScanTick = 0;

struct CreatureWindowProbeCandidate
{
    MyGUI::Widget* root;
    MyGUI::Widget* parent;
    std::string caption;
    bool isCurrentTarget;
    bool isMainWindowCandidate;
    bool isBackpackPopupCandidate;
    bool hasInventoryToken;
    bool hasEquipmentToken;
    bool hasContainerToken;
    bool hasBackpackContentToken;
    bool likelyInventory;
    int area;
    MyGUI::IntCoord absoluteCoord;
};

bool ContainsAsciiCaseInsensitiveLocal(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return true;
    }

    const std::size_t haystackLength = haystack.size();
    const std::size_t needleLength = std::strlen(needle);
    if (needleLength == 0)
    {
        return true;
    }
    if (needleLength > haystackLength)
    {
        return false;
    }

    for (std::size_t index = 0; index + needleLength <= haystackLength; ++index)
    {
        bool match = true;
        for (std::size_t needleIndex = 0; needleIndex < needleLength; ++needleIndex)
        {
            const unsigned char haystackChar =
                static_cast<unsigned char>(haystack[index + needleIndex]);
            const unsigned char needleChar =
                static_cast<unsigned char>(needle[needleIndex]);
            if (std::tolower(haystackChar) != std::tolower(needleChar))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            return true;
        }
    }

    return false;
}

void LogWidgetSummary(const char* label, MyGUI::Widget* widget)
{
    std::stringstream line;
    line << label
         << " name=" << SafeWidgetName(widget);

    if (widget == 0)
    {
        LogInfoLine(line.str());
        return;
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    line << " visible=" << (widget->getInheritedVisible() ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height
         << ")";
    LogInfoLine(line.str());
}

void LogResolvedTargetSummary(
    const char* label,
    MyGUI::Widget* anchor,
    MyGUI::Widget* parent)
{
    std::stringstream line;
    line << label
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);

    if (parent != 0)
    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        line << " parent_coord=("
             << parentCoord.left << "," << parentCoord.top << "," << parentCoord.width << ","
             << parentCoord.height << ")";
    }

    MyGUI::Window* window = FindOwningWindow(anchor);
    if (window != 0)
    {
        line << " caption=\"" << window->getCaption().asUTF8() << "\"";
    }

    LogInfoLine(line.str());
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

void AppendTokenMatches(
    MyGUI::Widget* root,
    const char* token,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (root == 0 || token == 0 || outWidgets == 0)
    {
        return;
    }

    std::vector<MyGUI::Widget*> matches;
    CollectNamedDescendantsByToken(root, token, true, &matches);
    for (std::size_t index = 0; index < matches.size(); ++index)
    {
        AddWidgetUnique(outWidgets, matches[index]);
    }
}

void LogBackpackCandidateDetails(const char* label, MyGUI::Widget* candidate)
{
    std::vector<MyGUI::Widget*> likelyEntries;
    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(candidate);
    if (entriesRoot == 0)
    {
        entriesRoot = candidate;
    }
    CollectLikelyInventoryEntryWidgets(entriesRoot, &likelyEntries);

    const MyGUI::IntCoord coord = candidate->getAbsoluteCoord();
    const std::size_t entriesRootChildCount =
        entriesRoot == 0 ? 0 : entriesRoot->getChildCount();

    std::stringstream line;
    line << label
         << " name=" << SafeWidgetName(candidate)
         << " visible=" << (candidate->getInheritedVisible() ? "true" : "false")
         << " child_count=" << candidate->getChildCount()
         << " entries_root=" << SafeWidgetName(entriesRoot)
         << " entries_root_child_count=" << entriesRootChildCount
         << " likely_entry_widgets=" << likelyEntries.size()
         << " abs_coord=(" << coord.left << "," << coord.top << "," << coord.width << ","
         << coord.height << ")";
    LogInfoLine(line.str());
}

Item* ResolveDiagnosticItemPointerRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth)
{
    if (widget == 0 || depth > maxDepth)
    {
        return 0;
    }

    Item* item = ResolveInventoryWidgetItemPointer(widget);
    if (item != 0)
    {
        return item;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        item = ResolveDiagnosticItemPointerRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth);
        if (item != 0)
        {
            return item;
        }
    }

    return 0;
}

std::string TruncateForLog(const std::string& value, std::size_t maxLen)
{
    if (value.size() <= maxLen)
    {
        return value;
    }

    if (maxLen <= 3)
    {
        return value.substr(0, maxLen);
    }

    return value.substr(0, maxLen - 3) + "...";
}

void LogBackpackEntriesRootSample(const char* label, MyGUI::Widget* entriesRoot)
{
    std::stringstream summary;
    summary << label
            << " entries_root=" << SafeWidgetName(entriesRoot)
            << " child_count=" << (entriesRoot == 0 ? 0 : entriesRoot->getChildCount());
    LogInfoLine(summary.str());

    if (entriesRoot == 0)
    {
        return;
    }

    const std::size_t childCount = entriesRoot->getChildCount();
    const std::size_t maxLoggedChildren = childCount < 12 ? childCount : 12;
    for (std::size_t childIndex = 0; childIndex < maxLoggedChildren; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        Item* directItem = ResolveInventoryWidgetItemPointer(child);
        Item* recursiveItem = ResolveDiagnosticItemPointerRecursive(child, 0, 5);
        int quantity = 0;
        const bool hasQuantity = TryResolveInventoryItemQuantityFromWidget(child, &quantity);
        const std::string normalizedSearchText = NormalizeInventorySearchText(
            BuildInventoryItemSearchTextFromResolvedItem(child, recursiveItem));
        const MyGUI::IntCoord coord = child->getCoord();

        std::stringstream line;
        line << label
             << " child[" << childIndex << "]"
             << " name=" << SafeWidgetName(child)
             << " visible=" << (child->getInheritedVisible() ? "true" : "false")
             << " direct_item=" << (directItem != 0 ? "true" : "false")
             << " recursive_item=" << (recursiveItem != 0 ? "true" : "false")
             << " quantity=" << (hasQuantity ? quantity : 0)
             << " child_count=" << child->getChildCount()
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")"
             << " text_len=" << normalizedSearchText.size()
             << " text=\"" << TruncateForLog(normalizedSearchText, 96) << "\"";
        LogInfoLine(line.str());
    }
}

void DumpCreatureWindowAttachmentProbe(MyGUI::Widget* targetAnchor, MyGUI::Widget* targetParent)
{
    MyGUI::Widget* effectiveTarget = targetParent != 0 ? targetParent : targetAnchor;
    if (effectiveTarget == 0)
    {
        return;
    }

    const std::string targetCaption =
        FindOwningWindow(targetAnchor != 0 ? targetAnchor : targetParent) == 0
            ? ""
            : FindOwningWindow(targetAnchor != 0 ? targetAnchor : targetParent)->getCaption().asUTF8();
    const bool targetLooksLikeBackpack =
        FindWidgetInParentByToken(effectiveTarget, "backpack_content") != 0
        || ContainsAsciiCaseInsensitiveLocal(targetCaption, "backpack");
    if (!targetLooksLikeBackpack)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    std::vector<CreatureWindowProbeCandidate> candidates;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        MyGUI::Window* window = FindOwningWindow(root);
        const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
        const bool hasInventoryToken = FindWidgetInParentByToken(parent, "Inventory") != 0;
        const bool hasEquipmentToken = FindWidgetInParentByToken(parent, "Equipment") != 0;
        const bool hasContainerToken = FindWidgetInParentByToken(parent, "Container") != 0;
        const bool hasBackpackContentToken =
            FindWidgetInParentByToken(parent, "backpack_content") != 0;
        const bool captionHasBackpack = ContainsAsciiCaseInsensitiveLocal(caption, "backpack");
        const bool captionHasLoot = ContainsAsciiCaseInsensitiveLocal(caption, "loot");
        const bool captionHasContainer = ContainsAsciiCaseInsensitiveLocal(caption, "container");
        const bool isCurrentTarget = root == targetAnchor || parent == targetParent;
        const bool isBackpackPopupCandidate = captionHasBackpack || hasBackpackContentToken;
        const bool isMainWindowCandidate =
            hasContainerToken
            && !hasBackpackContentToken
            && !captionHasBackpack
            && !captionHasLoot
            && !captionHasContainer;
        const bool likelyInventory = IsLikelyInventoryWindow(parent);

        if (!isCurrentTarget
            && !isBackpackPopupCandidate
            && !isMainWindowCandidate
            && !hasInventoryToken
            && !hasEquipmentToken
            && !hasContainerToken)
        {
            continue;
        }

        CreatureWindowProbeCandidate candidate;
        candidate.root = root;
        candidate.parent = parent;
        candidate.caption = caption;
        candidate.isCurrentTarget = isCurrentTarget;
        candidate.isMainWindowCandidate = isMainWindowCandidate;
        candidate.isBackpackPopupCandidate = isBackpackPopupCandidate;
        candidate.hasInventoryToken = hasInventoryToken;
        candidate.hasEquipmentToken = hasEquipmentToken;
        candidate.hasContainerToken = hasContainerToken;
        candidate.hasBackpackContentToken = hasBackpackContentToken;
        candidate.likelyInventory = likelyInventory;
        candidate.absoluteCoord = root->getAbsoluteCoord();
        candidate.area = candidate.absoluteCoord.width * candidate.absoluteCoord.height;
        candidates.push_back(candidate);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const CreatureWindowProbeCandidate& left, const CreatureWindowProbeCandidate& right) -> bool
        {
            if (left.isCurrentTarget != right.isCurrentTarget)
            {
                return left.isCurrentTarget;
            }
            if (left.isMainWindowCandidate != right.isMainWindowCandidate)
            {
                return left.isMainWindowCandidate;
            }
            if (left.isBackpackPopupCandidate != right.isBackpackPopupCandidate)
            {
                return left.isBackpackPopupCandidate;
            }
            if (left.likelyInventory != right.likelyInventory)
            {
                return left.likelyInventory;
            }
            return left.area > right.area;
        });

    std::stringstream start;
    start << "inventory creature window probe"
          << " target_anchor=" << SafeWidgetName(targetAnchor)
          << " target_parent=" << SafeWidgetName(targetParent)
          << " target_caption=\"" << TruncateForLog(targetCaption, 80) << "\""
          << " visible_candidates=" << candidates.size();
    LogInfoLine(start.str());

    const std::size_t maxLoggedCandidates = candidates.size() < 16 ? candidates.size() : 16;
    for (std::size_t index = 0; index < maxLoggedCandidates; ++index)
    {
        const CreatureWindowProbeCandidate& candidate = candidates[index];
        std::stringstream line;
        line << "inventory creature window probe candidate[" << index << "]"
             << " root=" << SafeWidgetName(candidate.root)
             << " parent=" << SafeWidgetName(candidate.parent)
             << " caption=\"" << TruncateForLog(candidate.caption, 80) << "\""
             << " current_target=" << (candidate.isCurrentTarget ? "true" : "false")
             << " main_window_candidate=" << (candidate.isMainWindowCandidate ? "true" : "false")
             << " backpack_popup_candidate=" << (candidate.isBackpackPopupCandidate ? "true" : "false")
             << " likely_inventory=" << (candidate.likelyInventory ? "true" : "false")
             << " inventory_token=" << (candidate.hasInventoryToken ? "true" : "false")
             << " equipment_token=" << (candidate.hasEquipmentToken ? "true" : "false")
             << " container_token=" << (candidate.hasContainerToken ? "true" : "false")
             << " backpack_content_token=" << (candidate.hasBackpackContentToken ? "true" : "false")
             << " abs_coord=(" << candidate.absoluteCoord.left << "," << candidate.absoluteCoord.top
             << "," << candidate.absoluteCoord.width << "," << candidate.absoluteCoord.height << ")"
             << " area=" << candidate.area;
        LogInfoLine(line.str());
    }
}
}

void DumpOnDemandInventoryDiagnosticsSnapshot(MyGUI::Widget* controlsContainer)
{
    std::stringstream start;
    start << "manual inventory diagnostics start"
          << " controls_enabled=" << (InventoryState().g_controlsEnabled ? "true" : "false")
          << " debug_logging=" << (InventoryState().g_debugLogging ? "true" : "false")
          << " debug_binding_logging=" << (InventoryState().g_debugBindingLogging ? "true" : "false")
          << " enable_debug_probes=" << (InventoryState().g_enableDebugProbes ? "true" : "false")
          << " query_len=" << InventoryState().g_searchQueryRaw.size()
          << " controls_present=" << (controlsContainer != 0 ? "true" : "false");
    LogWarnLine(start.str());

    LogWidgetSummary("manual diagnostics: controls_container", controlsContainer);
    LogWidgetSummary(
        "manual diagnostics: controls_parent",
        controlsContainer == 0 ? 0 : controlsContainer->getParent());

    DumpInventoryTargetProbe();
    DumpVisibleInventoryWindowCandidateDiagnostics();

    MyGUI::Widget* visibleAnchor = 0;
    MyGUI::Widget* visibleParent = 0;
    if (TryResolveVisibleInventoryTarget(&visibleAnchor, &visibleParent))
    {
        LogResolvedTargetSummary("manual diagnostics: visible_target", visibleAnchor, visibleParent);
        DumpInventoryBackpackCandidateDiagnosticsIfChanged(visibleParent, true);
        DumpCreatureWindowAttachmentProbe(visibleAnchor, visibleParent);
    }
    else
    {
        LogWarnLine("manual diagnostics: visible_target unresolved");
    }

    MyGUI::Widget* hoveredAnchor = 0;
    MyGUI::Widget* hoveredParent = 0;
    if (TryResolveHoveredInventoryTarget(&hoveredAnchor, &hoveredParent, true))
    {
        LogResolvedTargetSummary("manual diagnostics: hovered_target", hoveredAnchor, hoveredParent);
    }
    else
    {
        LogWarnLine("manual diagnostics: hovered_target unresolved (hover player inventory and retry)");
    }

    LogWarnLine("manual inventory diagnostics end");
}

void DumpInventoryBackpackCandidateDiagnosticsIfChanged(
    MyGUI::Widget* inventoryParent,
    bool forceScan)
{
    if (!ShouldEnableDebugProbes() || inventoryParent == 0 || !inventoryParent->getInheritedVisible())
    {
        g_lastBackpackDiagnosticsSignature.clear();
        g_lastBackpackDiagnosticsParent = 0;
        g_lastBackpackDiagnosticsScanTick = 0;
        return;
    }

    const DWORD now = GetTickCount();
    if (!forceScan
        && g_lastBackpackDiagnosticsParent == inventoryParent
        && g_lastBackpackDiagnosticsScanTick != 0
        && now - g_lastBackpackDiagnosticsScanTick < kBackpackDiagnosticsScanIntervalMs)
    {
        return;
    }
    g_lastBackpackDiagnosticsParent = inventoryParent;
    g_lastBackpackDiagnosticsScanTick = now;

    InventoryDebugProbePerfScope perfScope;
    InventoryDebugProbePerfSample& perfSample = perfScope.Sample();

    std::vector<MyGUI::Widget*> candidates;
    AppendTokenMatches(inventoryParent, "backpack_content", &candidates);
    AppendTokenMatches(inventoryParent, "scrollview_backpack_content", &candidates);
    AppendTokenMatches(inventoryParent, "backpack", &candidates);
    AppendTokenMatches(inventoryParent, "pack", &candidates);

    std::vector<MyGUI::Widget*> globalBackpackContents;
    CollectVisibleWidgetsByToken("backpack_content", &globalBackpackContents);
    perfSample.localCandidates = candidates.size();
    perfSample.globalBackpackContents = globalBackpackContents.size();

    std::stringstream signature;
    signature << inventoryParent
              << "|local=" << candidates.size()
              << "|global_backpack_content=" << globalBackpackContents.size();
    for (std::size_t index = 0; index < candidates.size() && index < 8; ++index)
    {
        MyGUI::Widget* candidate = candidates[index];
        const MyGUI::IntCoord coord = candidate->getAbsoluteCoord();
        signature << "|" << SafeWidgetName(candidate)
                  << "@" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height;
    }
    for (std::size_t index = 0; index < globalBackpackContents.size() && index < 4; ++index)
    {
        MyGUI::Widget* candidate = globalBackpackContents[index];
        const MyGUI::IntCoord coord = candidate->getAbsoluteCoord();
        signature << "|global:" << SafeWidgetName(candidate)
                  << "@" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height;
    }

    if (signature.str() == g_lastBackpackDiagnosticsSignature)
    {
        return;
    }
    g_lastBackpackDiagnosticsSignature = signature.str();
    perfSample.signatureChanged = true;

    MyGUI::Window* owningWindow = FindOwningWindow(inventoryParent);
    std::stringstream summary;
    summary << "inventory backpack probe parent=" << SafeWidgetName(inventoryParent)
            << " caption=\"" << (owningWindow == 0 ? "" : owningWindow->getCaption().asUTF8()) << "\""
            << " local_candidates=" << candidates.size()
            << " global_backpack_content=" << globalBackpackContents.size();
    LogInfoLine(summary.str());

    if (candidates.empty())
    {
        LogInfoLine("inventory backpack probe found no visible backpack-like widgets under active inventory target");
        return;
    }

    const std::size_t maxLoggedCandidates = candidates.size() < 12 ? candidates.size() : 12;
    for (std::size_t index = 0; index < maxLoggedCandidates; ++index)
    {
        std::stringstream label;
        label << "inventory backpack probe candidate[" << index << "]";
        LogBackpackCandidateDetails(label.str().c_str(), candidates[index]);
    }

    const std::size_t maxLoggedGlobalCandidates =
        globalBackpackContents.size() < 4 ? globalBackpackContents.size() : 4;
    for (std::size_t index = 0; index < maxLoggedGlobalCandidates; ++index)
    {
        std::stringstream label;
        label << "inventory backpack probe global_content[" << index << "]";
        MyGUI::Widget* globalContent = globalBackpackContents[index];
        LogBackpackCandidateDetails(label.str().c_str(), globalContent);

        MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(globalContent);
        if (entriesRoot == 0)
        {
            entriesRoot = globalContent;
        }

        std::stringstream childLabel;
        childLabel << "inventory backpack probe global_entries[" << index << "]";
        LogBackpackEntriesRootSample(childLabel.str().c_str(), entriesRoot);
    }
}
