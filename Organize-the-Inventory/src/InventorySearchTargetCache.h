#pragma once

namespace MyGUI
{
class Widget;
}

struct InventorySearchTargetResolution
{
    InventorySearchTargetResolution();

    MyGUI::Widget* anchor;
    MyGUI::Widget* parent;
    bool hasTarget;
    bool visibleTarget;
    bool hoverTarget;
    bool cacheHit;
    bool visibleScanAttempted;
    bool visibleScanSkipped;
    bool hoverScanAttempted;
    unsigned long visibleScanMicros;
    unsigned long hoverScanMicros;
};

InventorySearchTargetResolution ResolveInventorySearchTarget(
    bool forceVisibleScan,
    bool allowHoverFallback);
void ClearInventorySearchTargetCache();
