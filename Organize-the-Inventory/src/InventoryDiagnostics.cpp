#include "InventoryDiagnostics.h"

#include "InventoryBinding.h"
#include "InventoryCore.h"
#include "InventorySearchText.h"
#include "InventoryWindowDetection.h"

#include <kenshi/Item.h>

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <sstream>
#include <vector>

namespace
{
std::string g_lastBackpackDiagnosticsSignature;

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
        DumpInventoryBackpackCandidateDiagnosticsIfChanged(visibleParent);
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

void DumpInventoryBackpackCandidateDiagnosticsIfChanged(MyGUI::Widget* inventoryParent)
{
    if (!ShouldEnableDebugProbes() || inventoryParent == 0 || !inventoryParent->getInheritedVisible())
    {
        g_lastBackpackDiagnosticsSignature.clear();
        return;
    }

    std::vector<MyGUI::Widget*> candidates;
    AppendTokenMatches(inventoryParent, "backpack_content", &candidates);
    AppendTokenMatches(inventoryParent, "scrollview_backpack_content", &candidates);
    AppendTokenMatches(inventoryParent, "backpack", &candidates);
    AppendTokenMatches(inventoryParent, "pack", &candidates);

    std::vector<MyGUI::Widget*> globalBackpackContents;
    CollectVisibleWidgetsByToken("backpack_content", &globalBackpackContents);

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
