#include "InventorySearchFilterScheduler.h"

#include "InventoryCore.h"
#include "InventorySearchPipeline.h"

#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cctype>
#include <string>

namespace
{
const DWORD kActiveFilterRefreshIntervalMs = 250UL;
const DWORD kCountRefreshIntervalMs = 1000UL;
const DWORD kIdleRefreshIntervalMs = 5000UL;

struct InventorySearchFilterRefreshState
{
    InventorySearchFilterRefreshState()
        : inventoryParent(0)
        , forceShowAll(false)
        , showEntryCount(false)
        , showQuantityCount(false)
        , lastAppliedTick(0)
        , hasApplied(false)
    {
    }

    MyGUI::Widget* inventoryParent;
    std::string rawQuery;
    bool forceShowAll;
    bool showEntryCount;
    bool showQuantityCount;
    DWORD lastAppliedTick;
    bool hasApplied;
};

InventorySearchFilterRefreshState g_filterRefreshState;

bool HasElapsed(DWORD now, DWORD previous, DWORD intervalMs)
{
    return previous == 0 || now - previous >= intervalMs;
}

bool HasActiveRawQuery(const std::string& rawQuery)
{
    std::size_t index = 0;
    while (index < rawQuery.size()
        && std::isspace(static_cast<unsigned char>(rawQuery[index])) != 0)
    {
        ++index;
    }

    return index < rawQuery.size();
}

DWORD FilterRefreshIntervalMs(bool hasActiveFilter, bool hasCountMetric)
{
    if (hasActiveFilter)
    {
        return kActiveFilterRefreshIntervalMs;
    }
    if (hasCountMetric)
    {
        return kCountRefreshIntervalMs;
    }

    return kIdleRefreshIntervalMs;
}

bool FilterRefreshStateMatches(
    MyGUI::Widget* inventoryParent,
    const std::string& rawQuery,
    bool forceShowAll,
    bool showEntryCount,
    bool showQuantityCount)
{
    return g_filterRefreshState.hasApplied
        && g_filterRefreshState.inventoryParent == inventoryParent
        && g_filterRefreshState.rawQuery == rawQuery
        && g_filterRefreshState.forceShowAll == forceShowAll
        && g_filterRefreshState.showEntryCount == showEntryCount
        && g_filterRefreshState.showQuantityCount == showQuantityCount;
}

void RememberFilterRefreshState(
    MyGUI::Widget* inventoryParent,
    const std::string& rawQuery,
    bool forceShowAll,
    bool showEntryCount,
    bool showQuantityCount,
    DWORD now)
{
    g_filterRefreshState.inventoryParent = inventoryParent;
    g_filterRefreshState.rawQuery = rawQuery;
    g_filterRefreshState.forceShowAll = forceShowAll;
    g_filterRefreshState.showEntryCount = showEntryCount;
    g_filterRefreshState.showQuantityCount = showQuantityCount;
    g_filterRefreshState.lastAppliedTick = now;
    g_filterRefreshState.hasApplied = true;
}
}

InventorySearchFilterRefreshResult::InventorySearchFilterRefreshResult()
    : attempted(false)
    , applied(false)
    , skipped(false)
{
}

InventorySearchFilterRefreshResult ApplyInventorySearchFilterIfNeeded(
    MyGUI::Widget* inventoryParent,
    bool forceShowAll)
{
    InventorySearchFilterRefreshResult result;
    if (inventoryParent == 0)
    {
        ClearInventorySearchFilterRefreshState();
        ClearInventorySearchFilterState();
        return result;
    }

    const std::string rawQuery = forceShowAll ? std::string() : InventoryState().g_searchQueryRaw;
    const bool showEntryCount = InventoryState().g_showSearchEntryCount;
    const bool showQuantityCount = InventoryState().g_showSearchQuantityCount;
    const bool hasActiveFilter = !forceShowAll && HasActiveRawQuery(rawQuery);
    const bool hasCountMetric = showEntryCount || showQuantityCount;
    const DWORD now = GetTickCount();
    const bool stateMatches =
        FilterRefreshStateMatches(
            inventoryParent,
            rawQuery,
            forceShowAll,
            showEntryCount,
            showQuantityCount);
    const DWORD intervalMs = FilterRefreshIntervalMs(hasActiveFilter, hasCountMetric);

    if (stateMatches && !HasElapsed(now, g_filterRefreshState.lastAppliedTick, intervalMs))
    {
        result.skipped = true;
        return result;
    }

    result.attempted = true;
    result.applied = ApplyInventorySearchFilterToParent(inventoryParent, forceShowAll);
    RememberFilterRefreshState(
        inventoryParent,
        rawQuery,
        forceShowAll,
        showEntryCount,
        showQuantityCount,
        now);
    return result;
}

void ClearInventorySearchFilterRefreshState()
{
    g_filterRefreshState = InventorySearchFilterRefreshState();
}
