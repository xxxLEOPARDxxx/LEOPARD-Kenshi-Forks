#pragma once

#include <mygui/MyGUI_Colour.h>

#include <string>
#include <vector>

namespace MyGUI
{
class Widget;
}

namespace vr_marker_ui
{
enum OverlayAnchor
{
    OVERLAY_ANCHOR_BOTTOM_LEFT = 0,
    OVERLAY_ANCHOR_BOTTOM_RIGHT,
    OVERLAY_ANCHOR_TOP_LEFT,
    OVERLAY_ANCHOR_TOP_RIGHT
};

struct Rect
{
    Rect();
    Rect(int leftValue, int topValue, int widthValue, int heightValue);

    int left;
    int top;
    int width;
    int height;
};

struct ViewSize
{
    ViewSize();
    ViewSize(int widthValue, int heightValue);

    int width;
    int height;
};

struct OverlayStyle
{
    OverlayStyle();

    const char* widgetNamePrefix;
    const char* fallbackSkin;
    std::string iconTexture;
    int iconTextureSizePx;
    bool hasIconImageCoord;
    Rect iconImageCoord;
    std::string text;
    int textFontHeightPx;
    int fixedSizePx;
    OverlayAnchor anchor;
    MyGUI::Colour colour;
    float alpha;
    int insetPx;
    int edgeMarginXPx;
    int edgeMarginYPx;
    int minSizePx;
    int maxSizePx;
};

bool TryPlaceMarkerWidget(
    MyGUI::Widget* widget,
    const Rect& targetBounds,
    const ViewSize* viewSize,
    int markerInsetPx,
    int markerMinSizePx,
    int markerMaxSizePx,
    Rect* outMarkerBounds);

bool ShowOverlayMarker(
    std::vector<MyGUI::Widget*>* widgets,
    size_t index,
    const char* pluginName,
    const OverlayStyle& style,
    const Rect& targetBounds,
    const ViewSize* viewSize);

void HideWidgets(std::vector<MyGUI::Widget*>* widgets);
void HideWidgetsFrom(std::vector<MyGUI::Widget*>* widgets, size_t startIndex);
}
