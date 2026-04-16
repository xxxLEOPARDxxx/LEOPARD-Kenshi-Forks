#include "InventoryDiagnostics.h"

#include "InventoryCore.h"
#include "InventoryWindowDetection.h"

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <sstream>

namespace
{
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
