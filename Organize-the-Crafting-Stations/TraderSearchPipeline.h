#pragma once

#include <cstddef>
#include <string>

namespace MyGUI
{
class Widget;
}

void MarkSearchFilterDirty(const char* reason);
void LogSearchSampleForQuery(MyGUI::Widget* entriesRoot, const std::string& normalizedQuery, std::size_t maxItems);
void ResetObservedTraderEntriesState();
void ObserveTraderEntriesStateForRefresh();
bool ApplySearchFilterToTraderParent(MyGUI::Widget* traderParent, bool forceShowAll, bool logSummary);
void ApplySearchFilterFromControls(bool forceShowAll, bool logSummary);
