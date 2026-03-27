#include "vr_marker_ui.h"

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <cstring>
#include <sstream>

namespace vr_marker_ui
{
namespace
{
bool StringListContains(const std::vector<std::string>& values, const std::string& needle)
{
    for (size_t index = 0u; index < values.size(); ++index)
    {
        if (values[index] == needle)
        {
            return true;
        }
    }
    return false;
}

int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

MyGUI::IntCoord ToMyGuiCoord(const Rect& rect)
{
    return MyGUI::IntCoord(rect.left, rect.top, rect.width, rect.height);
}

Rect FromMyGuiCoord(const MyGUI::IntCoord& coord)
{
    return Rect(coord.left, coord.top, coord.width, coord.height);
}

bool IsImageModeWidget(MyGUI::Widget* widget)
{
    return widget != 0 && widget->castType<MyGUI::ImageBox>(false) != 0;
}

enum WidgetMode
{
    WIDGET_MODE_NONE = 0,
    WIDGET_MODE_BUTTON,
    WIDGET_MODE_IMAGE,
    WIDGET_MODE_TEXT
};

WidgetMode GetWidgetMode(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return WIDGET_MODE_NONE;
    }

    if (widget->castType<MyGUI::ImageBox>(false) != 0)
    {
        return WIDGET_MODE_IMAGE;
    }

    if (widget->castType<MyGUI::Button>(false) != 0)
    {
        return WIDGET_MODE_BUTTON;
    }

    if (widget->castType<MyGUI::TextBox>(false) != 0)
    {
        return WIDGET_MODE_TEXT;
    }

    return WIDGET_MODE_NONE;
}

bool EnsureTextureApplied(MyGUI::ImageBox* imageBox, const char* pluginName, const std::string& textureName, int textureSizePx)
{
    if (imageBox == 0 || textureName.empty())
    {
        return false;
    }

    const char* fallbackTexture = "default_icon.png";
    std::vector<std::string> candidates;

    const auto addCandidate = [&candidates](const std::string& candidate)
    {
        if (!candidate.empty() && !StringListContains(candidates, candidate))
        {
            candidates.push_back(candidate);
        }
    };

    const auto addCandidateWithSeparatorVariants = [&addCandidate](const std::string& candidate)
    {
        if (candidate.empty())
        {
            return;
        }

        addCandidate(candidate);

        std::string withForwardSlashes = candidate;
        bool changedToForward = false;
        for (size_t index = 0u; index < withForwardSlashes.size(); ++index)
        {
            if (withForwardSlashes[index] == '\\')
            {
                withForwardSlashes[index] = '/';
                changedToForward = true;
            }
        }
        if (changedToForward)
        {
            addCandidate(withForwardSlashes);
        }

        std::string withBackSlashes = candidate;
        bool changedToBack = false;
        for (size_t index = 0u; index < withBackSlashes.size(); ++index)
        {
            if (withBackSlashes[index] == '/')
            {
                withBackSlashes[index] = '\\';
                changedToBack = true;
            }
        }
        if (changedToBack)
        {
            addCandidate(withBackSlashes);
        }
    };

    addCandidateWithSeparatorVariants(textureName);

    const bool hasPathSeparator = (textureName.find('/') != std::string::npos || textureName.find('\\') != std::string::npos);
    const bool isAbsolutePath = (textureName.size() >= 2u && textureName[1] == ':')
        || (!textureName.empty() && (textureName[0] == '/' || textureName[0] == '\\'));
    const bool isModQualified = (textureName.rfind("mods/", 0) == 0 || textureName.rfind("mods\\", 0) == 0);

    if (!hasPathSeparator)
    {
        addCandidateWithSeparatorVariants(std::string("icons/") + textureName);
        addCandidateWithSeparatorVariants(std::string("gui/gfx/") + textureName);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/icons/" + textureName);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/gui/gfx/" + textureName);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + textureName);
    }
    else if (!isAbsolutePath && !isModQualified)
    {
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + textureName);

        const size_t fileNameStart = textureName.find_last_of("/\\");
        if (fileNameStart != std::string::npos && (fileNameStart + 1u) < textureName.size())
        {
            const std::string fileName = textureName.substr(fileNameStart + 1u);
            addCandidateWithSeparatorVariants(fileName);
            addCandidateWithSeparatorVariants(std::string("icons/") + fileName);
            addCandidateWithSeparatorVariants(std::string("gui/gfx/") + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/icons/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/gui/gfx/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + fileName);
        }
    }

    for (size_t index = 0u; index < candidates.size(); ++index)
    {
        try
        {
            imageBox->setImageTexture(candidates[index]);

            const MyGUI::IntSize imageSize = imageBox->getImageSize();
            if (imageSize.width <= 0 || imageSize.height <= 0)
            {
                continue;
            }

            const int resolvedSize = textureSizePx > 0 ? textureSizePx : imageSize.width;
            imageBox->setImageCoord(MyGUI::IntCoord(0, 0, resolvedSize, resolvedSize));
            imageBox->setImageTile(MyGUI::IntSize(resolvedSize, resolvedSize));
            return true;
        }
        catch (...)
        {
        }
    }

    if (textureName != fallbackTexture)
    {
        return EnsureTextureApplied(imageBox, pluginName, fallbackTexture, textureSizePx);
    }

    return false;
}

void ApplyImageCoord(MyGUI::ImageBox* imageBox, const OverlayStyle& style)
{
    if (imageBox == 0)
    {
        return;
    }

    try
    {
        if (style.hasIconImageCoord)
        {
            const MyGUI::IntCoord coord(
                style.iconImageCoord.left,
                style.iconImageCoord.top,
                style.iconImageCoord.width,
                style.iconImageCoord.height);
            imageBox->setImageCoord(coord);
            imageBox->setImageTile(MyGUI::IntSize(coord.width, coord.height));
            return;
        }
    }
    catch (...)
    {
    }
}

bool EnsureWidgetMode(
    std::vector<MyGUI::Widget*>* widgets,
    size_t index,
    const char* pluginName,
    const OverlayStyle& style,
    MyGUI::Widget** outWidget)
{
    if (widgets == 0 || outWidget == 0)
    {
        return false;
    }

    if (widgets->size() <= index)
    {
        widgets->resize(index + 1u, 0);
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    const WidgetMode desiredMode = !style.text.empty()
        ? WIDGET_MODE_TEXT
        : (!style.iconTexture.empty() ? WIDGET_MODE_IMAGE : WIDGET_MODE_BUTTON);
    MyGUI::Widget* widget = (*widgets)[index];
    if (widget != 0 && GetWidgetMode(widget) != desiredMode)
    {
        try
        {
            gui->destroyWidget(widget);
        }
        catch (...)
        {
        }
        widget = 0;
        (*widgets)[index] = 0;
    }

    if (widget == 0)
    {
        std::stringstream name;
        name << (style.widgetNamePrefix != 0 ? style.widgetNamePrefix : "VitalRead_Overlay_") << index;

        try
        {
            if (desiredMode == WIDGET_MODE_IMAGE)
            {
                widget = gui->createWidget<MyGUI::ImageBox>(
                    "ImageBox",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    MyGUI::Align::Left | MyGUI::Align::Top,
                    "Top",
                    name.str());
            }
            else if (desiredMode == WIDGET_MODE_TEXT)
            {
                widget = gui->createWidget<MyGUI::TextBox>(
                    "Kenshi_TextboxStandardText",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    MyGUI::Align::Left | MyGUI::Align::Top,
                    "Top",
                    name.str());
                if (widget == 0)
                {
                    widget = gui->createWidget<MyGUI::TextBox>(
                        "TextBox",
                        MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                        MyGUI::Align::Left | MyGUI::Align::Top,
                        "Top",
                        name.str() + "_fallback");
                }
            }
            else
            {
                widget = gui->createWidget<MyGUI::Button>(
                    style.fallbackSkin != 0 ? style.fallbackSkin : "Kenshi_Button1",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    MyGUI::Align::Left | MyGUI::Align::Top,
                    "Top",
                    name.str());
            }
        }
        catch (...)
        {
            widget = 0;
        }

        if (widget == 0)
        {
            return false;
        }

        widget->setNeedMouseFocus(false);
        widget->setVisible(false);
        (*widgets)[index] = widget;
    }

    widget->setAlpha(style.alpha);
    widget->setColour(style.colour);

    if (desiredMode == WIDGET_MODE_BUTTON)
    {
        if (MyGUI::Button* button = widget->castType<MyGUI::Button>(false))
        {
            button->setCaption("");
        }
    }

    if (desiredMode == WIDGET_MODE_IMAGE)
    {
        MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
        if (imageBox == 0 || !EnsureTextureApplied(imageBox, pluginName != 0 ? pluginName : "", style.iconTexture, style.iconTextureSizePx))
        {
            widget->setVisible(false);
            return false;
        }
        ApplyImageCoord(imageBox, style);
    }
    else if (desiredMode == WIDGET_MODE_TEXT)
    {
        MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false);
        if (textBox == 0)
        {
            widget->setVisible(false);
            return false;
        }

        textBox->setCaption(style.text.c_str());
        textBox->setTextAlign(MyGUI::Align::Center);
        textBox->setTextColour(style.colour);
        textBox->setTextShadow(true);
        textBox->setTextShadowColour(MyGUI::Colour(0.0f, 0.0f, 0.0f, 0.95f));
        if (style.textFontHeightPx > 0)
        {
            textBox->setFontHeight(style.textFontHeightPx);
        }
    }

    *outWidget = widget;
    return true;
}

bool TryPlaceOverlayWidget(
    MyGUI::Widget* widget,
    const Rect& targetBounds,
    const ViewSize* viewSize,
    const int markerInsetPx,
    const int markerMinSizePx,
    const int markerMaxSizePx,
    const OverlayAnchor anchor,
    Rect* outMarkerBounds)
{
    if (widget == 0)
    {
        return false;
    }

    const int smallerSide = targetBounds.width < targetBounds.height
        ? targetBounds.width
        : targetBounds.height;
    const int markerSize = ClampInt(
        smallerSide / 4,
        markerMinSizePx,
        markerMaxSizePx);
    int markerWidth = markerSize;
    int markerHeight = markerSize;
    if (GetWidgetMode(widget) == WIDGET_MODE_TEXT)
    {
        markerWidth = markerSize + (markerSize / 2);
    }

    int markerLeft = targetBounds.left + markerInsetPx;
    int markerTop = targetBounds.top + targetBounds.height - markerHeight - markerInsetPx;
    if (anchor == OVERLAY_ANCHOR_TOP_RIGHT)
    {
        markerLeft = targetBounds.left + targetBounds.width - markerWidth - markerInsetPx;
        markerTop = targetBounds.top + markerInsetPx;
    }

    if (viewSize != 0)
    {
        const int maxLeft = viewSize->width - markerWidth > 0 ? viewSize->width - markerWidth : 0;
        const int maxTop = viewSize->height - markerHeight > 0 ? viewSize->height - markerHeight : 0;
        markerLeft = ClampInt(markerLeft, 0, maxLeft);
        markerTop = ClampInt(markerTop, 0, maxTop);
    }

    const Rect markerBounds(markerLeft, markerTop, markerWidth, markerHeight);
    widget->setCoord(ToMyGuiCoord(markerBounds));
    widget->setVisible(true);

    if (outMarkerBounds != 0)
    {
        *outMarkerBounds = markerBounds;
    }

    return true;
}
}

Rect::Rect()
    : left(0)
    , top(0)
    , width(0)
    , height(0)
{
}

Rect::Rect(const int leftValue, const int topValue, const int widthValue, const int heightValue)
    : left(leftValue)
    , top(topValue)
    , width(widthValue)
    , height(heightValue)
{
}

ViewSize::ViewSize()
    : width(0)
    , height(0)
{
}

ViewSize::ViewSize(const int widthValue, const int heightValue)
    : width(widthValue)
    , height(heightValue)
{
}

OverlayStyle::OverlayStyle()
    : widgetNamePrefix(0)
    , fallbackSkin(0)
    , iconTexture()
    , iconTextureSizePx(64)
    , hasIconImageCoord(false)
    , iconImageCoord()
    , text()
    , textFontHeightPx(0)
    , anchor(OVERLAY_ANCHOR_BOTTOM_LEFT)
    , colour(1.0f, 1.0f, 1.0f, 1.0f)
    , alpha(1.0f)
    , insetPx(0)
    , minSizePx(8)
    , maxSizePx(64)
{
}

bool TryPlaceMarkerWidget(
    MyGUI::Widget* widget,
    const Rect& targetBounds,
    const ViewSize* viewSize,
    const int markerInsetPx,
    const int markerMinSizePx,
    const int markerMaxSizePx,
    Rect* outMarkerBounds)
{
    return TryPlaceOverlayWidget(
        widget,
        targetBounds,
        viewSize,
        markerInsetPx,
        markerMinSizePx,
        markerMaxSizePx,
        OVERLAY_ANCHOR_BOTTOM_LEFT,
        outMarkerBounds);
}

bool ShowOverlayMarker(
    std::vector<MyGUI::Widget*>* widgets,
    const size_t index,
    const char* pluginName,
    const OverlayStyle& style,
    const Rect& targetBounds,
    const ViewSize* viewSize)
{
    MyGUI::Widget* widget = 0;
    if (!EnsureWidgetMode(widgets, index, pluginName, style, &widget))
    {
        return false;
    }

    return TryPlaceOverlayWidget(
        widget,
        targetBounds,
        viewSize,
        style.insetPx,
        style.minSizePx,
        style.maxSizePx,
        style.anchor,
        0);
}

void HideWidgets(std::vector<MyGUI::Widget*>* widgets)
{
    if (widgets == 0)
    {
        return;
    }

    for (size_t index = 0u; index < widgets->size(); ++index)
    {
        if ((*widgets)[index] != 0)
        {
            (*widgets)[index]->setVisible(false);
        }
    }
}

void HideWidgetsFrom(std::vector<MyGUI::Widget*>* widgets, const size_t startIndex)
{
    if (widgets == 0)
    {
        return;
    }

    for (size_t index = startIndex; index < widgets->size(); ++index)
    {
        if ((*widgets)[index] != 0)
        {
            (*widgets)[index]->setVisible(false);
        }
    }
}
}
