#include "InventorySearchTargetCache.h"

#include "InventoryPerformanceTelemetry.h"
#include "InventoryWindowDetection.h"

#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

namespace
{
const DWORD kNoTargetVisibleScanIntervalMs = 500UL;
const DWORD kCachedTargetRefreshIntervalMs = 1000UL;

MyGUI::Widget* g_cachedTargetAnchor = 0;
MyGUI::Widget* g_cachedTargetParent = 0;
DWORD g_lastVisibleScanTick = 0;

bool HasElapsed(DWORD now, DWORD previous, DWORD intervalMs)
{
    return previous == 0 || now - previous >= intervalMs;
}

bool HasCachedTarget()
{
    return g_cachedTargetAnchor != 0 || g_cachedTargetParent != 0;
}

bool TryIsWidgetInheritedVisible(MyGUI::Widget* widget, bool* outVisible)
{
    if (outVisible != 0)
    {
        *outVisible = false;
    }
    if (widget == 0)
    {
        return false;
    }

    __try
    {
        if (outVisible != 0)
        {
            *outVisible = widget->getInheritedVisible();
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsCachedTargetVisible(InventorySearchTargetResolution* resolution)
{
    if (!HasCachedTarget())
    {
        return false;
    }

    InventoryPerfTimer validationTimer;
    if (resolution != 0)
    {
        resolution->cacheValidated = true;
    }

    bool anchorVisible = false;
    bool parentVisible = false;
    const bool visible =
        TryIsWidgetInheritedVisible(g_cachedTargetAnchor, &anchorVisible)
        && TryIsWidgetInheritedVisible(g_cachedTargetParent, &parentVisible)
        && anchorVisible
        && parentVisible;

    if (resolution != 0)
    {
        resolution->cacheValidationMicros = validationTimer.ElapsedMicros();
    }

    if (!visible)
    {
        if (resolution != 0)
        {
            resolution->cacheInvalidated = true;
        }
        ClearInventorySearchTargetCache();
        return false;
    }

    return true;
}

void StoreCachedTarget(MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    g_cachedTargetAnchor = anchor;
    g_cachedTargetParent = parent;
}

void FillFromCachedTarget(InventorySearchTargetResolution* resolution)
{
    if (resolution == 0)
    {
        return;
    }

    resolution->anchor = g_cachedTargetAnchor;
    resolution->parent = g_cachedTargetParent;
    resolution->hasTarget = true;
    resolution->visibleTarget = true;
    resolution->cacheHit = true;
}
}

InventorySearchTargetResolution::InventorySearchTargetResolution()
    : anchor(0)
    , parent(0)
    , hasTarget(false)
    , visibleTarget(false)
    , hoverTarget(false)
    , cacheHit(false)
    , cacheValidated(false)
    , cacheInvalidated(false)
    , visibleScanAttempted(false)
    , visibleScanSkipped(false)
    , hoverScanAttempted(false)
    , cacheValidationMicros(0)
    , visibleScanMicros(0)
    , hoverScanMicros(0)
{
}

InventorySearchTargetResolution ResolveInventorySearchTarget(
    bool forceVisibleScan,
    bool allowHoverFallback)
{
    InventorySearchTargetResolution resolution;
    const DWORD now = GetTickCount();
    const bool cachedTargetVisible = IsCachedTargetVisible(&resolution);
    const bool refreshCachedTarget =
        cachedTargetVisible
        && HasElapsed(now, g_lastVisibleScanTick, kCachedTargetRefreshIntervalMs);
    const bool scanWithoutCachedTarget =
        !cachedTargetVisible
        && HasElapsed(now, g_lastVisibleScanTick, kNoTargetVisibleScanIntervalMs);
    const bool shouldScanVisible =
        (forceVisibleScan && !cachedTargetVisible)
        || refreshCachedTarget
        || scanWithoutCachedTarget;

    if (shouldScanVisible)
    {
        MyGUI::Widget* targetAnchor = 0;
        MyGUI::Widget* targetParent = 0;
        InventoryPerfTimer visibleTargetTimer;
        resolution.visibleScanAttempted = true;
        const bool hasVisibleTarget =
            TryResolveVisibleInventoryTarget(&targetAnchor, &targetParent);
        resolution.visibleScanMicros = visibleTargetTimer.ElapsedMicros();
        g_lastVisibleScanTick = now;

        if (hasVisibleTarget)
        {
            StoreCachedTarget(targetAnchor, targetParent);
            resolution.anchor = targetAnchor;
            resolution.parent = targetParent;
            resolution.hasTarget = true;
            resolution.visibleTarget = true;
            return resolution;
        }

        if (cachedTargetVisible)
        {
            ClearInventorySearchTargetCache();
        }
    }
    else
    {
        resolution.visibleScanSkipped = true;
        if (cachedTargetVisible)
        {
            FillFromCachedTarget(&resolution);
            return resolution;
        }
    }

    if (allowHoverFallback)
    {
        MyGUI::Widget* targetAnchor = 0;
        MyGUI::Widget* targetParent = 0;
        InventoryPerfTimer hoverTargetTimer;
        resolution.hoverScanAttempted = true;
        const bool hasHoverTarget =
            TryResolveHoveredInventoryTarget(&targetAnchor, &targetParent, false);
        resolution.hoverScanMicros = hoverTargetTimer.ElapsedMicros();

        if (hasHoverTarget)
        {
            StoreCachedTarget(targetAnchor, targetParent);
            resolution.anchor = targetAnchor;
            resolution.parent = targetParent;
            resolution.hasTarget = true;
            resolution.hoverTarget = true;
            return resolution;
        }
    }

    return resolution;
}

void ClearInventorySearchTargetCache()
{
    g_cachedTargetAnchor = 0;
    g_cachedTargetParent = 0;
}
