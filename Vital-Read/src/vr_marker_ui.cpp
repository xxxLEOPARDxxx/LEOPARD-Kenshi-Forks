#include "vr_marker_ui.h"

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ILayer.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_LayerManager.h>
#include <mygui/MyGUI_IUnlinkWidget.h>
#include <mygui/MyGUI_WidgetManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <cstring>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace vr_marker_ui
{
namespace
{
const int kTopTextAnchorBiasPx = 6;
const int kHorizontalTextAnchorBiasPx = 2;

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

struct ImageTextureCacheEntry
{
    ImageTextureCacheEntry()
        : widgetName()
        , textureKey()
    {
    }

    std::string widgetName;
    std::string textureKey;
};

OverlayPerfStats g_overlayPerfStats;

// Слой значков. 04.10 их перенесли с Top на Back, чтобы не просвечивали
// сквозь окна, - но панель портретов выше Back, и значки оказались под
// портретами (05.10: «ни на одном аватаре нет значков»). Теперь слой берётся
// у самой панели портретов: значки над портретом, а окна - над ними.
std::string g_overlayLayerName = "Main";

const std::string& WidgetLayerName(MyGUI::Widget* widget)
{
    static const std::string kNone;
    MyGUI::Widget* root = widget;
    while (root != 0 && root->getParent() != 0)
    {
        root = root->getParent();
    }
    MyGUI::ILayer* const layer = root == 0 ? 0 : root->getLayer();
    return layer == 0 ? kNone : layer->getName();
}
std::map<MyGUI::Widget*, ImageTextureCacheEntry> g_imageTextureCache;

// 08.10.2026: значки - ДОЧЕРНИЕ виджеты портрета (сначала были корня панели),
// а не корневые на
// её слое. Корневые на перекрывающемся слое панель закрывала, как только
// поднималась наверх (значки появлялись и через время пропадали). Дочерние
// с глубиной kOverlayDepth рисуются поверх всех соседей по панели, а окна
// выше панели закрывают и их. Панель удалили - MyGUI сообщает о каждом
// удаляемом виджете (IUnlinkWidget), и ссылки на наши значки обнуляются.
MyGUI::Widget* g_overlayParent = 0;
const int kOverlayDepth = -1000;
std::vector<std::vector<MyGUI::Widget*>*> g_trackedWidgetLists;
// Свои виджеты - быстрая проверка: MyGUI сообщает о КАЖДОМ удаляемом
// виджете интерфейса, и перебирать на каждый все списки значков незачем
// (ревью 08.10.2026: закрытие большого окна - тысячи виджетов).
std::set<MyGUI::Widget*> g_ownWidgets;
WidgetGoneFn g_widgetGone = 0;

class OverlayUnlinker : public MyGUI::IUnlinkWidget
{
public:
    virtual void _unlinkWidget(MyGUI::Widget* widget)
    {
        if (widget == 0)
        {
            return;
        }
        if (g_widgetGone != 0)
        {
            g_widgetGone(widget);
        }
        if (widget == g_overlayParent)
        {
            g_overlayParent = 0;
        }
        if (g_ownWidgets.erase(widget) == 0)
        {
            return;
        }
        for (size_t list = 0u; list < g_trackedWidgetLists.size(); ++list)
        {
            std::vector<MyGUI::Widget*>& widgets = *g_trackedWidgetLists[list];
            for (size_t index = 0u; index < widgets.size(); ++index)
            {
                if (widgets[index] == widget)
                {
                    widgets[index] = 0;
                }
            }
        }
        g_imageTextureCache.erase(widget);
    }
};

OverlayUnlinker* g_overlayUnlinker = 0;   // не удаляется: живёт, пока жива DLL

void EnsureUnlinkerImpl()
{
    if (g_overlayUnlinker == 0)
    {
        MyGUI::WidgetManager* const manager = MyGUI::WidgetManager::getInstancePtr();
        if (manager == 0)
        {
            return;
        }
        g_overlayUnlinker = new OverlayUnlinker();
        manager->registerUnlinker(g_overlayUnlinker);
    }
}

void TrackWidgetList(std::vector<MyGUI::Widget*>* widgets)
{
    EnsureUnlinkerImpl();
    if (g_overlayUnlinker == 0)
    {
        return;
    }
    if (std::find(g_trackedWidgetLists.begin(), g_trackedWidgetLists.end(), widgets) == g_trackedWidgetLists.end())
    {
        g_trackedWidgetLists.push_back(widgets);
    }
}

template <typename T>
T* CreateOverlayWidget(MyGUI::Gui* gui, const std::string& skin, const MyGUI::IntCoord& coord,
                       const std::string& layer, const std::string& name)
{
    if (g_overlayParent != 0 && g_overlayUnlinker != 0)
    {
        T* const widget = g_overlayParent->createWidget<T>(skin, coord, MyGUI::Align::Left | MyGUI::Align::Top, name);
        if (widget != 0)
        {
            widget->setDepth(kOverlayDepth);
            // Игра делает портрет оглушённого полупрозрачным - значок не
            // должен меркнуть вместе с ним (08.10.2026).
            widget->setInheritsAlpha(false);
            g_ownWidgets.insert(widget);
        }
        return widget;
    }
    T* const widget = gui->createWidget<T>(skin, coord, MyGUI::Align::Left | MyGUI::Align::Top, layer, name);
    if (widget != 0)
    {
        g_ownWidgets.insert(widget);
    }
    return widget;
}

std::string BuildImageTextureCacheKey(const char* pluginName, const std::string& textureName, int textureSizePx)
{
    std::stringstream key;
    key << (pluginName != 0 ? pluginName : "")
        << "|"
        << textureName
        << "|"
        << textureSizePx;
    return key.str();
}

bool IsImageTextureCacheHit(MyGUI::Widget* widget, const std::string& textureKey)
{
    if (widget == 0)
    {
        return false;
    }

    std::map<MyGUI::Widget*, ImageTextureCacheEntry>::const_iterator it = g_imageTextureCache.find(widget);
    if (it == g_imageTextureCache.end())
    {
        return false;
    }

    return it->second.widgetName == widget->getName()
        && it->second.textureKey == textureKey;
}

void SetImageTextureCache(MyGUI::Widget* widget, const std::string& textureKey)
{
    if (widget == 0)
    {
        return;
    }

    ImageTextureCacheEntry entry;
    entry.widgetName = widget->getName();
    entry.textureKey = textureKey;
    g_imageTextureCache[widget] = entry;
}

void ClearImageTextureCache(MyGUI::Widget* widget)
{
    if (widget != 0)
    {
        g_imageTextureCache.erase(widget);
    }
}

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

MyGUI::Align GetTextAlignForAnchor(const OverlayAnchor anchor)
{
    if (anchor == OVERLAY_ANCHOR_BOTTOM_RIGHT || anchor == OVERLAY_ANCHOR_TOP_RIGHT)
    {
        return MyGUI::Align::Right;
    }

    if (anchor == OVERLAY_ANCHOR_BOTTOM_LEFT || anchor == OVERLAY_ANCHOR_TOP_LEFT)
    {
        return MyGUI::Align::Left;
    }

    return MyGUI::Align::Center;
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
            ++g_overlayPerfStats.imageTextureCandidateAttempts;
            imageBox->setImageTexture(candidates[index]);

            const MyGUI::IntSize imageSize = imageBox->getImageSize();
            if (imageSize.width <= 0 || imageSize.height <= 0)
            {
                continue;
            }

            const int resolvedSize = textureSizePx > 0 ? textureSizePx : imageSize.width;
            imageBox->setImageCoord(MyGUI::IntCoord(0, 0, resolvedSize, resolvedSize));
            imageBox->setImageTile(MyGUI::IntSize(resolvedSize, resolvedSize));
            ++g_overlayPerfStats.imageTextureApplySuccesses;
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

    ++g_overlayPerfStats.imageTextureApplyFailures;
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
    TrackWidgetList(widgets);

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
        ++g_overlayPerfStats.widgetModeChanges;
        ClearImageTextureCache(widget);
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

    // Значок не на той панели (панель пересоздали, или он корневой, а панель
    // уже известна) - пересоздать на нужной.
    if (widget != 0 && g_overlayParent != 0 && g_overlayUnlinker != 0 && widget->getParent() != g_overlayParent)
    {
        ClearImageTextureCache(widget);
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

    // Корневой значок (панели нет): слой сменился - переложить его туда.
    if (widget != 0 && widget->getParent() == 0 && WidgetLayerName(widget) != g_overlayLayerName)
    {
        MyGUI::LayerManager* const layers = MyGUI::LayerManager::getInstancePtr();
        if (layers != 0 && layers->isExist(g_overlayLayerName))
        {
            layers->attachToLayerNode(g_overlayLayerName, widget);
        }
    }

    // Слой - g_overlayLayerName (у панели портретов): над портретом, под окнами.
    if (widget == 0)
    {
        std::stringstream name;
        name << (style.widgetNamePrefix != 0 ? style.widgetNamePrefix : "VitalRead_Overlay_") << index;

        try
        {
            if (desiredMode == WIDGET_MODE_IMAGE)
            {
                widget = CreateOverlayWidget<MyGUI::ImageBox>(gui,
                    "ImageBox",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    g_overlayLayerName,
                    name.str());
            }
            else if (desiredMode == WIDGET_MODE_TEXT)
            {
                widget = CreateOverlayWidget<MyGUI::TextBox>(gui,
                    "Kenshi_TextboxStandardText",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    g_overlayLayerName,
                    name.str());
                if (widget == 0)
                {
                    widget = CreateOverlayWidget<MyGUI::TextBox>(gui,
                        "TextBox",
                        MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                        g_overlayLayerName,
                        name.str() + "_fallback");
                }
            }
            else
            {
                widget = CreateOverlayWidget<MyGUI::Button>(gui,
                    style.fallbackSkin != 0 ? style.fallbackSkin : "Kenshi_Button1",
                    MyGUI::IntCoord(0, 0, style.minSizePx, style.minSizePx),
                    g_overlayLayerName,
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

        ++g_overlayPerfStats.widgetsCreated;
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
        const std::string textureKey = BuildImageTextureCacheKey(
            pluginName != 0 ? pluginName : "",
            style.iconTexture,
            style.iconTextureSizePx);
        if (imageBox == 0)
        {
            widget->setVisible(false);
            return false;
        }
        if (IsImageTextureCacheHit(widget, textureKey))
        {
            ++g_overlayPerfStats.imageTextureApplySkips;
        }
        else
        {
            ++g_overlayPerfStats.imageTextureApplyRequests;
            if (!EnsureTextureApplied(imageBox, pluginName != 0 ? pluginName : "", style.iconTexture, style.iconTextureSizePx))
            {
                ClearImageTextureCache(widget);
                widget->setVisible(false);
                return false;
            }
            SetImageTextureCache(widget, textureKey);
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
        textBox->setTextAlign(GetTextAlignForAnchor(style.anchor));
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
    const int edgeMarginXPx,
    const int edgeMarginYPx,
    const int markerMinSizePx,
    const int markerMaxSizePx,
    const int fixedSizePx,
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
    int markerSize = 0;
    if (fixedSizePx > 0)
    {
        markerSize = fixedSizePx;
    }
    else
    {
        markerSize = ClampInt(
            smallerSide / 4,
            markerMinSizePx,
            markerMaxSizePx);
    }
    int markerWidth = markerSize;
    int markerHeight = markerSize;
    if (GetWidgetMode(widget) == WIDGET_MODE_TEXT)
    {
        size_t textLength = 0u;
        if (MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false))
        {
            textLength = textBox->getCaption().size();
        }

        if (textLength >= 3u)
        {
            markerWidth = markerSize + ((markerSize * 2) / 3);
        }
        else if (textLength == 2u)
        {
            markerWidth = markerSize + (markerSize / 3);
        }
    }

    int markerLeft = targetBounds.left + markerInsetPx;
    int markerTop = targetBounds.top + targetBounds.height - markerHeight - markerInsetPx;
    if (anchor == OVERLAY_ANCHOR_BOTTOM_RIGHT || anchor == OVERLAY_ANCHOR_TOP_RIGHT)
    {
        markerLeft = targetBounds.left + targetBounds.width - markerWidth - markerInsetPx;
    }
    if (anchor == OVERLAY_ANCHOR_TOP_LEFT || anchor == OVERLAY_ANCHOR_TOP_RIGHT)
    {
        markerTop = targetBounds.top + markerInsetPx;
    }

    if (anchor == OVERLAY_ANCHOR_BOTTOM_LEFT || anchor == OVERLAY_ANCHOR_TOP_LEFT)
    {
        markerLeft += edgeMarginXPx;
    }
    else
    {
        markerLeft -= edgeMarginXPx;
    }

    if (anchor == OVERLAY_ANCHOR_TOP_LEFT || anchor == OVERLAY_ANCHOR_TOP_RIGHT)
    {
        markerTop += edgeMarginYPx;
    }
    else
    {
        markerTop -= edgeMarginYPx;
    }

    if (GetWidgetMode(widget) == WIDGET_MODE_TEXT
        && (anchor == OVERLAY_ANCHOR_TOP_LEFT || anchor == OVERLAY_ANCHOR_TOP_RIGHT))
    {
        markerTop -= kTopTextAnchorBiasPx;
    }
    if (GetWidgetMode(widget) == WIDGET_MODE_TEXT)
    {
        if (anchor == OVERLAY_ANCHOR_BOTTOM_LEFT || anchor == OVERLAY_ANCHOR_TOP_LEFT)
        {
            markerLeft -= kHorizontalTextAnchorBiasPx;
        }
        else
        {
            markerLeft += kHorizontalTextAnchorBiasPx;
        }
    }

    if (viewSize != 0)
    {
        const int maxLeft = viewSize->width - markerWidth > 0 ? viewSize->width - markerWidth : 0;
        const int maxTop = viewSize->height - markerHeight > 0 ? viewSize->height - markerHeight : 0;
        markerLeft = ClampInt(markerLeft, 0, maxLeft);
        markerTop = ClampInt(markerTop, 0, maxTop);
    }

    const Rect markerBounds(markerLeft, markerTop, markerWidth, markerHeight);
    MyGUI::IntCoord coord = ToMyGuiCoord(markerBounds);
    if (MyGUI::Widget* const parent = widget->getParent())
    {
        const MyGUI::IntPoint origin = parent->getAbsolutePosition();
        coord.left -= origin.left;
        coord.top -= origin.top;
        // Дочерний виджет обрезается по родителю - держим значок внутри
        // портрета целиком.
        const int maxLeft = parent->getWidth() - coord.width;
        const int maxTop = parent->getHeight() - coord.height;
        coord.left = ClampInt(coord.left, 0, maxLeft > 0 ? maxLeft : 0);
        coord.top = ClampInt(coord.top, 0, maxTop > 0 ? maxTop : 0);
    }
    if (widget->getCoord() != coord)
    {
        widget->setCoord(coord);
    }
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
    , fixedSizePx(0)
    , anchor(OVERLAY_ANCHOR_BOTTOM_LEFT)
    , colour(1.0f, 1.0f, 1.0f, 1.0f)
    , alpha(1.0f)
    , insetPx(0)
    , edgeMarginXPx(0)
    , edgeMarginYPx(0)
    , minSizePx(8)
    , maxSizePx(64)
{
}

OverlayPerfStats::OverlayPerfStats()
    : showOverlayCalls(0u)
    , imageTextureApplyRequests(0u)
    , imageTextureApplySkips(0u)
    , imageTextureCandidateAttempts(0u)
    , imageTextureApplySuccesses(0u)
    , imageTextureApplyFailures(0u)
    , widgetsCreated(0u)
    , widgetModeChanges(0u)
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
        0,
        0,
        markerMinSizePx,
        markerMaxSizePx,
        0,
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
    ++g_overlayPerfStats.showOverlayCalls;

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
        style.edgeMarginXPx,
        style.edgeMarginYPx,
        style.minSizePx,
        style.maxSizePx,
        style.fixedSizePx,
        style.anchor,
        0);
}

void EnsureUnlinker()
{
    EnsureUnlinkerImpl();
}

void SetWidgetGoneCallback(WidgetGoneFn fn)
{
    g_widgetGone = fn;
}

void SetOverlayLayerFromWidget(MyGUI::Widget* target)
{
    const std::string& name = WidgetLayerName(target);
    if (!name.empty() && name != g_overlayLayerName)
    {
        g_overlayLayerName = name;
    }
    // Родитель значка - сам портрет (см. g_overlayParent): дочерний виджет
    // рисуется поверх родителя и поднимается вместе с ним. На корне панели
    // значки пропадали при наведении мыши на портрет - игра поднимала
    // портрет над соседями (08.10.2026).
    if (target != 0)
    {
        g_overlayParent = target;
    }
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

OverlayPerfStats GetOverlayPerfStats()
{
    return g_overlayPerfStats;
}

void ResetOverlayPerfStats()
{
    g_overlayPerfStats = OverlayPerfStats();
}
}
