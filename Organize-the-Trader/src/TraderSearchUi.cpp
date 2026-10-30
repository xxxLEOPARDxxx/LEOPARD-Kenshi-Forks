#include "TraderSearchUi.h"
#include <WidgetRef.h>
#include <GameTheme.h>

#include "TraderCore.h"
#include "TraderDiagnostics.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchInputBehavior.h"
#include "TraderSearchPipeline.h"
#include "TraderSearchText.h"
#include "TraderWindowDetection.h"
#include "TraderInlinePlacement.h"
#include "TraderAdaptiveLayout.h"
#include "TraderRenderedText.h"


#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/GUI/ForgottenGUI.h>
#include <kenshi/GUI/ToolTip.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_FontManager.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_ListBox.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <sstream>

// Подсказка в пустом поле - через перевод мода (08.10.2026; раньше была
// по-русски и в английской игре). После Ogre/MyGUI: Localization.h тянет
// Windows.h с макросами min/max.
#define KLOC_DOMAIN "organize_the_trader"
#include <Localization.h>

namespace
{
const char* kDockWindowName = "OTT_InlineTraderSearch";
const int kHeaderCount = 2;

MyGUI::ISubWidgetText* RenderedText(MyGUI::Widget* widget)
{
    MyGUI::Widget* rendered = TraderRenderedText::Resolve(widget);
    return rendered == 0 ? 0 : rendered->getSubWidgetText();
}
int FontHeight(MyGUI::Widget* widget)
{
    MyGUI::ISubWidgetText* text = RenderedText(widget);
    return text == 0 ? 0 : text->getFontHeight();
}
const std::string& FontName(MyGUI::Widget* widget)
{
    static const std::string empty;
    MyGUI::ISubWidgetText* text = RenderedText(widget);
    return text == 0 ? empty : text->getFontName();
}

// Пристыкованная панель поиска - по ссылке (shared/WidgetRef.h), без
// поиска по имени через весь интерфейс каждый кадр.
MyGUI::Widget* FindSortControlsContainer();

static WidgetRef& DockPanelRef()
{
    static WidgetRef* ref = new WidgetRef();   // не разрушается: MyGUI держит его в списке
    return *ref;
}
// Non-owning identity only: validate against GUI roots before dereferencing.
MyGUI::Widget* g_dockOwner = 0;
MyGUI::Widget* FindLiveDockOwner(bool requireVisible)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0 || g_dockOwner == 0) return 0;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == g_dockOwner)
            return !requireVisible || root->getInheritedVisible() ? root : 0;
    }
    return 0;
}

MyGUI::IntCoord InlineSearchCoord(MyGUI::Widget* parent)
{
    MyGUI::Widget* data = FindWidgetInParentByToken(parent, "datapanel");
    MyGUI::Widget* money = data == 0 ? 0 : data->getParent();
    if (money == 0) return MyGUI::IntCoord(220, 8, 240, 30);
    const MyGUI::IntCoord origin = parent->getAbsoluteCoord();
    const MyGUI::IntCoord box = money->getAbsoluteCoord();
    const TraderInlinePlacement::Rect coord = TraderInlinePlacement::Place(parent->getWidth(),
        TraderInlinePlacement::Rect(box.left - origin.left, box.top - origin.top, box.width, box.height));
    // LEOPARD (15.10.2026): плашка денег во всю ширину окна (Dark UI) -
    // справа от неё места нет, и окно оставалось без поиска. Тогда - правая
    // половина её строки (с ARRANGE поиск всё равно сразу уходит вниз).
    if (coord.width < 160)
    {
        const int half = parent->getWidth() / 2;
        return MyGUI::IntCoord(half, box.top - origin.top, parent->getWidth() - half - 16, box.height);
    }
    return MyGUI::IntCoord(coord.left, coord.top, coord.width, coord.height);
}

// LEOPARD: the game's combo box keeps its text line at a fixed 20 px from
// the top (Kenshi_ComboBox: Client 10 5 130 20), so in the shorter sort row
// of the vanilla interface the text sat low and was cut. Stretch the text
// line over the whole box and centre it.
void FitSortComboText(MyGUI::ComboBox* combo)
{
    if (combo == 0) return;
    MyGUI::Widget* client = combo->getClientWidget();
    if (client != 0)
    {
        const int right = combo->getWidth() >= 60 ? 26 : 18;
        const MyGUI::IntCoord want(8, 0, combo->getWidth() - 8 - right, combo->getHeight());
        if (want.width > 10 && client->getCoord() != want) client->setCoord(want);
    }
    combo->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
}

// Width of the longest item in the combo's font.
int SortComboItemsWidth(MyGUI::ComboBox* combo, MyGUI::ISubWidgetText* rendered = 0, bool popup = false);

// ---------------------------------------------------------------------------
// LEOPARD (14.10.2026, просьба пользователя): итог торговли. Наверху, где
// раньше стоял поиск, рядом с деньгами торговца - сколько наторговали за
// это окно: деньги торговца при открытии минус деньги сейчас (продали нам
// заплатили - у него меньше, у нас плюс). Деньги читаем из подписи самого
// окна («Money: c.6,301»): мимо GameData и без лишних хуков.
// ---------------------------------------------------------------------------
static WidgetRef& TradeBalanceRef()
{
    static WidgetRef* ref = new WidgetRef();   // не разрушается: MyGUI держит его в списке
    return *ref;
}
// Блок денег и ARRANGE нужны каждый кадр, а поиск по имени обходит всё окно
// с сотнями иконок вещей. Ищем один раз, дальше - из запомненного, пока
// виджет жив и лежит в этом же окне (ревью 15.10.2026).
static WidgetRef& DataPanelRef()
{
    static WidgetRef* ref = new WidgetRef();
    return *ref;
}
static WidgetRef& ArrangeRef()
{
    static WidgetRef* ref = new WidgetRef();
    return *ref;
}

MyGUI::Widget* CachedInParent(WidgetRef& ref, MyGUI::Widget* parent, const char* token)
{
    MyGUI::Widget* widget = ref.get();
    for (MyGUI::Widget* up = widget != 0 ? widget->getParent() : 0; up != 0; up = up->getParent())
        if (up == parent)
            return widget;
    widget = FindWidgetInParentByToken(parent, token);
    ref.reset();
    if (widget != 0)
        ref.set(widget);
    return widget;
}

MyGUI::Widget* g_balanceOwner = 0;      // окно торговца, для которого считаем
int g_balanceStartMoney = -1;
std::string g_balanceMoneyCaption;      // последняя разобранная подпись
int g_balanceMoney = -1;

int ParseMoneyCaption(const std::string& caption)
{
    return TraderAdaptiveLayout::ParseMoneyCaption(caption);
}

// подписи блока денег: метка («Нал.:») - первая без цифр, значение -
// последняя с цифрами
void FindMoneyTexts(MyGUI::Widget* widget, int depth, MyGUI::TextBox** labelOut, MyGUI::TextBox** valueOut)
{
    if (widget == 0 || depth > 6)
        return;
    MyGUI::TextBox* const text = widget->castType<MyGUI::TextBox>(false);
    if (text != 0 && !text->getCaption().empty())
    {
        bool digits = false;
        const MyGUI::UString& caption = text->getCaption();
        for (size_t i = 0; i < caption.size() && !digits; ++i)
            digits = caption[i] >= '0' && caption[i] <= '9';
        if (digits)
            *valueOut = text;
        else if (*labelOut == 0)
            *labelOut = text;
    }
    for (size_t i = 0; i < widget->getChildCount(); ++i)
        FindMoneyTexts(widget->getChildAt(i), depth + 1, labelOut, valueOut);
}

// Знак валюты и разделитель тысяч - как в подписи денег самой игры
// («к.8 146» в русской, «c.8,146» в английской): язык игры, не свой.
std::string g_moneyPrefix = "c.";
std::string g_moneySeparator = ",";

void LearnMoneyFormat(const std::string& caption)
{
    size_t first = 0;
    while (first < caption.size() && !(caption[first] >= '0' && caption[first] <= '9'))
        ++first;
    if (first >= caption.size())
        return;
    // приставка - последнее «слово» перед числом («Нал.: к.» -> «к.»)
    size_t start = first;
    while (start > 0 && caption[start - 1] != ' ' && caption[start - 1] != ':')
        --start;
    g_moneyPrefix = caption.substr(start, first - start);
    // разделитель - то, что стоит между группами цифр
    size_t i = first;
    while (i < caption.size() && caption[i] >= '0' && caption[i] <= '9')
        ++i;
    size_t j = i;
    while (j < caption.size() && !(caption[j] >= '0' && caption[j] <= '9'))
        ++j;
    if (j < caption.size() && j > i && j - i <= 3)
        g_moneySeparator = caption.substr(i, j - i);
    else
        g_moneySeparator = (i - first) > 3 ? "" : g_moneySeparator;
}

std::string FormatMoney(int value)
{
    char digits[32];
    sprintf_s(digits, "%d", value < 0 ? -value : value);
    std::string out;
    const int n = static_cast<int>(strlen(digits));
    for (int i = 0; i < n; ++i)
    {
        if (i > 0 && (n - i) % 3 == 0)
            out += g_moneySeparator;
        out += digits[i];
    }
    return out;
}

// All non-owning widget references unlink synchronously on MyGUI destruction.
// No item-tree walks or font measurements occur on an unchanged frame.
struct TextSnapshot
{
    MyGUI::UString caption;
    std::string font;
    int height;
    bool visible;
    TextSnapshot() : height(-1), visible(false) {}
    bool Observe(MyGUI::Widget* widget)
    {
        MyGUI::TextBox* text = widget == 0 ? 0 : widget->castType<MyGUI::TextBox>(false);
        if (text == 0) return false;
        if (caption == text->getCaption() && font == FontName(text)
            && height == FontHeight(text) && visible == text->getVisible()) return false;
        caption = text->getCaption(); font = FontName(text);
        height = FontHeight(text); visible = text->getVisible();
        return true;
    }
};
struct AdaptiveUiCache
{
    WidgetRef controls[7]; // label, combo, direction, edit, hint, clear, count
    WidgetRef viewports[2];
    WidgetRef nativeMoney, plate, trader;
    WidgetRef measure, popup;
    TraderRenderedText::MeasureCache<MyGUI::UString> metrics[2];
    int listTextWidth;
    int closedChrome, popupChrome, popupNaturalWidth, popupRenderedWidth;
    size_t measureCreates, measureFontChanges, headerCaptionSets;
    TextSnapshot footerText[7], headerText[kHeaderCount];
    std::vector<MyGUI::UString> comboItems;
    MyGUI::UString nativeCaption;
    MyGUI::IntCoord footerCoord, headerCoord;
    int sortSetting, searchSetting, balance, listWidth, moneyFontHeight;
    std::string moneyFontName;
    MyGUI::Colour moneyColour;
    std::string popupFont;
    MyGUI::IntSize popupSize;
    int popupFontHeight, popupTextWidth;
    bool popupValid;
    int baseFont[7], fittedFont[7];
    bool footerValid, headerValid, showDeal, viewportsBound, uiStateValid;
    int uiState;
    int headerExtra, footerExtra;
    TraderAdaptiveLayout::Viewport viewportLayout[2];
    bool viewportHidden[2], viewportVisible[2];
    float dataAlpha;
    WidgetRef nativePanel, nativePlateParent, failedNativePanel;
    TraderAdaptiveLayout::Viewport plateLayout;
    MyGUI::IntCoord headerContent;
    WidgetRef viewportParents[2], viewportHost;
    int viewportRetry;
    AdaptiveUiCache() : sortSetting(-1), searchSetting(-1), balance(0), listWidth(0), moneyFontHeight(-1),
        listTextWidth(0), closedChrome(0), popupChrome(0), popupNaturalWidth(0), popupRenderedWidth(0),
        measureCreates(0), measureFontChanges(0), headerCaptionSets(0),
        popupFontHeight(0), popupTextWidth(0), popupValid(false),
        footerValid(false), headerValid(false), showDeal(false), viewportsBound(false), uiStateValid(false), uiState(0),
        headerExtra(0), footerExtra(0), dataAlpha(1.0f), viewportRetry(0)
    {
        for (int i = 0; i < 7; ++i) baseFont[i] = fittedFont[i] = -1;
        for (int i = 0; i < 2; ++i)
        { viewportHidden[i] = false; viewportVisible[i] = false; }
    }
};
AdaptiveUiCache& AdaptiveUi()
{
    static AdaptiveUiCache* cache = new AdaptiveUiCache();
    return *cache;
}
struct NativeCaptionRenderer
{
    MyGUI::TextBox* measure;
    NativeCaptionRenderer() : measure(0) {}
    bool Begin(const std::string& font, int height)
    {
        AdaptiveUiCache& cache = AdaptiveUi();
        measure = cache.measure.get() == 0 ? 0 : cache.measure.get()->castType<MyGUI::TextBox>(false);
        if (measure == 0)
        {
            MyGUI::Widget* parent = DockPanelRef().get();
            if (parent == 0) return false;
            measure = parent->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText",
                MyGUI::IntCoord(0, 0, 8192, 256), MyGUI::Align::Default, "OTT_TextMeasure");
            if (measure == 0) return false;
            measure->setVisible(false);
            measure->setNeedMouseFocus(false);
            cache.measure.set(measure);
            ++cache.measureCreates;
        }
        if (FontName(measure) != font) { measure->setFontName(font); ++cache.measureFontChanges; }
        if (FontHeight(measure) != height) { measure->setFontHeight(height); ++cache.measureFontChanges; }
        MyGUI::ISubWidgetText* text = measure->getSubWidgetText();
        if (text == 0) return false;
        text->setWordWrap(false);
        return true;
    }
    int CaptionWidth(const MyGUI::UString& caption)
    {
        measure->setCaption(caption);
        return measure->getSubWidgetText()->getTextSize().width;
    }
};
int SortComboItemsWidth(MyGUI::ComboBox* combo, MyGUI::ISubWidgetText* rendered, bool popup)
{
    if (rendered == 0) rendered = RenderedText(combo);
    if (combo == 0 || rendered == 0 || rendered->getFontHeight() <= 0 || rendered->getFontName().empty()) return -1;
    AdaptiveUiCache& cache = AdaptiveUi();
    NativeCaptionRenderer renderer;
    return cache.metrics[popup ? 1 : 0].Get(rendered->getFontName(), rendered->getFontHeight(), cache.comboItems, renderer);
}

void CoordIfChanged(MyGUI::Widget* widget, const MyGUI::IntCoord& coord)
{
    if (widget != 0 && widget->getCoord() != coord) widget->setCoord(coord);
}
void AlignIfChanged(MyGUI::TextBox* text, MyGUI::Align align)
{
    if (text != 0 && text->getTextAlign() != align) text->setTextAlign(align);
}
MyGUI::TextBox* AsText(MyGUI::Widget* widget)
{
    return widget == 0 ? 0 : widget->castType<MyGUI::TextBox>(false);
}
void HeaderCaptionIfChanged(MyGUI::TextBox* text, const MyGUI::UString& caption)
{
    if (text != 0 && text->getCaption() != caption)
    {
        text->setCaption(caption);
        ++AdaptiveUi().headerCaptionSets;
    }
}
int TextWidth(MyGUI::Widget* widget)
{
    MyGUI::TextBox* text = AsText(widget);
    return text == 0 ? 0 : text->getTextSize().width;
}
void FullCaptionTip(MyGUI::Widget* widget, const MyGUI::UString& caption, bool needed)
{
    // Resolve the game's tooltip afresh; do not retain a world-owned pointer.
    ToolTip* tooltip = ::gui == 0 ? 0 : ::gui->getToolTip();
    if (tooltip == 0 || widget == 0) return;
    if (needed) tooltip->setup(widget, caption.asUTF8());
    else
    {
        if (tooltip->caller == widget) tooltip->hide();
        tooltip->clear(widget);
    }
}
// Native inventory viewport ownership (extracted by production tests).
bool NativeViewportParentInHost(MyGUI::Widget* widget, MyGUI::Widget* host)
{
    for (int depth = 0; widget != 0 && depth < 6; ++depth, widget = widget->getParent())
    {
        if (widget == host) return true;
        if (widget->castType<MyGUI::Window>(false) != 0) return false;
    }
    return false;
}
bool NativeViewportName(const std::string& name, const char* suffix)
{
    const size_t length = std::strlen(suffix);
    return name.size() >= length && name.compare(name.size()-length,length,suffix) == 0
        && (name.size() == length || name[name.size()-length-1] == '_');
}
void FindAdaptiveViewports(MyGUI::Widget* host, MyGUI::Widget** found)
{
    // Breadth first: inventory icons must not consume the lookup budget before
    // the sibling ScrollView is visited. No heap allocation or icon-tree walk.
    struct Entry { MyGUI::Widget* widget; int depth; } queue[128];
    int begin = 0, end = 1; queue[0].widget = host; queue[0].depth = 0;
    bool ambiguous[2] = { false, false }; found[0] = found[1] = 0;
    while (begin < end)
    {
        const Entry entry = queue[begin++]; MyGUI::Widget* widget = entry.widget;
        if (widget == 0 || (widget != host && widget->castType<MyGUI::Window>(false) != 0)) continue;
        const std::string& name = widget->getName();
        // Check the longer role first: a ScrollView also ends in backpack_content.
        const int slot = NativeViewportName(name,"scrollview_backpack_content") ? 1
            : NativeViewportName(name,"backpack_content") ? 0 : -1;
        if (slot >= 0)
        {
            if (NativeViewportParentInHost(widget->getParent(),host))
            {
                if (found[slot] != 0) ambiguous[slot] = true;
                found[slot] = widget;
            }
            continue;
        }
        if (entry.depth >= 6) continue;
        const size_t children = widget->getChildCount();
        for (size_t i = 0; i < children && end < 128; ++i)
        { queue[end].widget = widget->getChildAt(i); queue[end++].depth = entry.depth+1; }
    }
    for (int i = 0; i < 2; ++i) if (ambiguous[i]) found[i] = 0;
}
void ClearAdaptiveViewport(int index, MyGUI::Widget* host, bool restore)
{
    AdaptiveUiCache& cache = AdaptiveUi();
    MyGUI::Widget* viewport = cache.viewports[index].get();
    MyGUI::Widget* parent = viewport == 0 ? 0 : viewport->getParent();
    bool ownsGeometry = false;
    if (restore && viewport != 0 && NativeViewportParentInHost(parent,host))
    {
        const MyGUI::IntCoord current = viewport->getCoord();
        TraderAdaptiveLayout::Viewport& baseline = cache.viewportLayout[index];
        const TraderAdaptiveLayout::Rect rect(current.left,current.top,current.width,current.height);
        if (parent == cache.viewportParents[index].get())
        {
            const MyGUI::Align align = viewport->getAlign();
            const TraderAdaptiveLayout::Rect want = baseline.Update(rect,parent->getWidth(),parent->getHeight(),
                align.isHStretch()?3:align.isRight()?2:align.isHCenter()?1:0,
                align.isVStretch()?3:align.isBottom()?2:align.isVCenter()?1:0,0,0);
            CoordIfChanged(viewport,MyGUI::IntCoord(want.left,want.top,want.width,want.height));
            ownsGeometry = true;
        }
        else if (baseline.bound && rect == baseline.written)
        {
            // In-scope reparenting retained our exact local rectangle. Undo only
            // that proven write before adopting the new parent's native baseline.
            const TraderAdaptiveLayout::Rect& want = baseline.baseline;
            CoordIfChanged(viewport,MyGUI::IntCoord(want.left,want.top,want.width,want.height));
            ownsGeometry = true;
        }
        if (ownsGeometry && cache.viewports[index].get() == viewport
            && viewport->getParent() == parent && NativeViewportParentInHost(parent,host)
            && cache.viewportHidden[index] && !viewport->getVisible())
            viewport->setVisible(cache.viewportVisible[index]);
    }
    cache.viewports[index].reset(); cache.viewportParents[index].reset();
    cache.viewportLayout[index] = TraderAdaptiveLayout::Viewport();
    cache.viewportHidden[index] = cache.viewportVisible[index] = false;
}
void ReserveAdaptiveRows(MyGUI::Widget* parent, int header, int footer)
{
    AdaptiveUiCache& cache = AdaptiveUi();
    bool bind = !cache.viewportsBound;
    if (cache.viewportHost.get() != parent)
    {
        for (int i = 0; i < 2; ++i) ClearAdaptiveViewport(i,cache.viewportHost.get(),true);
        cache.viewportHost.set(parent); bind = true; cache.viewportRetry = 0;
    }
    if (parent == 0) { cache.viewportsBound = false; return; }
    bool missing = false;
    for (int i = 0; i < 2; ++i)
    {
        MyGUI::Widget* viewport = cache.viewports[i].get();
        if ((viewport == 0 && cache.viewportLayout[i].bound)
            || (viewport != 0 && (viewport->getParent() != cache.viewportParents[i].get()
                || !NativeViewportParentInHost(viewport->getParent(),parent))))
        { ClearAdaptiveViewport(i,parent,true); bind = true; }
        if (cache.viewports[i].get() == 0) missing = true;
    }
    if (missing && cache.viewportRetry == 0) bind = true;
    if (bind && (header != 0 || footer != 0))
    {
        MyGUI::Widget* found[2]; FindAdaptiveViewports(parent,found);
        for (int i = 0; i < 2; ++i)
            if (cache.viewports[i].get() == 0 && found[i] != 0)
            {
                cache.viewports[i].set(found[i]); cache.viewportParents[i].set(found[i]->getParent());
                cache.viewportVisible[i] = found[i]->getVisible();
            }
        cache.viewportRetry = 60;
    }
    else if (missing && cache.viewportRetry > 0) --cache.viewportRetry;
    for (int i = 0; i < 2; ++i)
    {
        MyGUI::Widget* viewport = cache.viewports[i].get();
        MyGUI::Widget* actualParent = cache.viewportParents[i].get();
        if (viewport == 0 || actualParent == 0) continue;
        const MyGUI::IntCoord current = viewport->getCoord();
        const MyGUI::Align align = viewport->getAlign();
        const int horizontal = align.isHStretch() ? 3 : align.isRight() ? 2 : align.isHCenter() ? 1 : 0;
        const int vertical = align.isVStretch() ? 3 : align.isBottom() ? 2 : align.isVCenter() ? 1 : 0;
        const TraderAdaptiveLayout::Rect want = cache.viewportLayout[i].Update(
            TraderAdaptiveLayout::Rect(current.left, current.top, current.width, current.height),
            actualParent->getWidth(), actualParent->getHeight(), horizontal, vertical, header, footer);
        const MyGUI::IntCoord coord(want.left, want.top, want.width, want.height);
        CoordIfChanged(viewport, coord);
        // A public coordinate callback may synchronously destroy/reparent GUI.
        if (cache.viewports[i].get() != viewport || viewport->getParent() != actualParent
            || cache.viewportParents[i].get() != actualParent)
        { ClearAdaptiveViewport(i,parent,true); cache.viewportRetry = 0; continue; }
        if (coord.height == 0 && !cache.viewportHidden[i])
        {
            cache.viewportVisible[i] = viewport->getVisible();
            if (viewport->getVisible()) viewport->setVisible(false);
            cache.viewportHidden[i] = true;
        }
        else if (coord.height > 0 && cache.viewportHidden[i])
        {
            if (viewport->getVisible() != cache.viewportVisible[i]) viewport->setVisible(cache.viewportVisible[i]);
            cache.viewportHidden[i] = false;
        }
    }
    cache.viewportsBound = true;
}

void LayoutBottomSearchRow(MyGUI::Widget* search, int width, int height)
{
    AdaptiveUiCache& cache = AdaptiveUi();
    MyGUI::Widget* edit = cache.controls[3].get();
    MyGUI::Widget* hint = cache.controls[4].get();
    MyGUI::Widget* clear = cache.controls[5].get();
    MyGUI::Widget* count = cache.controls[6].get();
    const int countWidth = count != 0 && count->getVisible() ? TextWidth(count) + 4 : 0;
    const TraderAdaptiveLayout::SearchRow row(width, countWidth,
        clear != 0 && clear->getVisible() ? height : 0);
    const int editWidth = row.editWidth;
    const int clearSize = row.clearWidth;
    CoordIfChanged(edit, MyGUI::IntCoord(0, 0, editWidth, height));
    MyGUI::EditBox* box = edit == 0 ? 0 : edit->castType<MyGUI::EditBox>(false);
    if (box != 0)
    {
        CoordIfChanged(box->getClientWidget(), MyGUI::IntCoord(editWidth > 8 ? 8 : 0, 0, row.clientWidth, height));
        AlignIfChanged(box, MyGUI::Align::Left | MyGUI::Align::VCenter);
    }
    CoordIfChanged(hint, MyGUI::IntCoord(editWidth > 8 ? 8 : 0, 0, row.clientWidth, height));
    AlignIfChanged(AsText(hint), MyGUI::Align::Left | MyGUI::Align::VCenter);
    CoordIfChanged(clear, MyGUI::IntCoord(row.clearLeft, 0, clearSize, height));
    CoordIfChanged(count, MyGUI::IntCoord(row.countLeft, 0, row.countWidth, height));
    AlignIfChanged(AsText(count), MyGUI::Align::Right | MyGUI::Align::VCenter);
}

void DestroyTradeBalance();
void UpdateTradeBalance(MyGUI::Widget* owner, MyGUI::Widget* parent)
{
    AdaptiveUiCache& cache = AdaptiveUi();
    MyGUI::Widget* data = CachedInParent(DataPanelRef(), parent, "datapanel");
    MyGUI::Widget* moneyBox = data == 0 ? 0 : data->getParent();
    if (moneyBox == 0) return;
    if (cache.failedNativePanel.get() == data) return;
    if (!TraderAdaptiveLayout::SafeMoneyPlate(moneyBox == parent || moneyBox == owner,
        moneyBox->getName() == "Content", moneyBox->getHeight(), parent->getHeight(), cache.plate.get() == moneyBox))
    {
        if (cache.plate.get() != 0) DestroyTradeBalance();
        return;
    }
    // Keep the dedicated native plate in its actual layout parent. Both stock
    // themes insert Content between this plate and the Window skin Client.
    MyGUI::Widget* plateParent = moneyBox->getParent();
    if (!TraderAdaptiveLayout::WithinParent(plateParent, parent) || (cache.plate.get() != 0
        && (cache.plate.get() != moneyBox || cache.nativePanel.get() != data
            || cache.nativePlateParent.get() != plateParent)))
    {
        if (cache.plate.get() != 0) DestroyTradeBalance();
        return;
    }
    if (cache.nativeMoney.get() == 0)
    {
        MyGUI::TextBox* nativeLabel = 0;
        MyGUI::TextBox* nativeValue = 0;
        FindMoneyTexts(data, 0, &nativeLabel, &nativeValue);
        if (nativeValue == 0) return; // Native data panel is not populated yet.
        cache.nativeMoney.set(nativeValue);
    }
    MyGUI::TextBox* nativeValue = AsText(cache.nativeMoney.get());
    if (nativeValue == 0) return;
    MyGUI::ISubWidgetText* moneyRender = RenderedText(nativeValue);
    if (moneyRender == 0 || moneyRender->getFontHeight() <= 0 || moneyRender->getFontName().empty()
        || moneyRender->getTextColour().alpha <= 0.0f)
    {
        // Never fade the live native wallet when its actual skin text is unknown.
        if (cache.plate.get() != 0) DestroyTradeBalance();
        return;
    }
    const int nativeFontHeight = moneyRender->getFontHeight();
    const std::string& nativeFontName = moneyRender->getFontName();
    const MyGUI::Colour nativeColour = moneyRender->getTextColour();
    if (cache.nativeCaption != nativeValue->getCaption())
    {
        cache.nativeCaption = nativeValue->getCaption();
        g_balanceMoneyCaption = cache.nativeCaption.asUTF8();
        g_balanceMoney = ParseMoneyCaption(g_balanceMoneyCaption);
        LearnMoneyFormat(g_balanceMoneyCaption);
        cache.headerValid = false;
    }
    if (owner != g_balanceOwner)
    {
        g_balanceOwner = owner;
        g_balanceStartMoney = g_balanceMoney;
        cache.headerValid = false;
    }
    if (g_balanceStartMoney < 0 && g_balanceMoney >= 0) g_balanceStartMoney = g_balanceMoney;
    if (g_balanceMoney < 0) return;
    if (cache.plate.get() == 0)
    {
        cache.plate.set(moneyBox);
        cache.nativePanel.set(data);
        cache.nativePlateParent.set(plateParent);
        cache.dataAlpha = data->getAlpha();
    }
    WidgetRef* refs[] = { &cache.trader, &TradeBalanceRef() };
    const char* names[] = { "OTT_TraderMoney", "OTT_TradeBalance" };
    try
    {
        for (int i = 0; i < kHeaderCount; ++i)
            if (refs[i]->get() == 0)
            {
                MyGUI::TextBox* text = moneyBox->createWidget<MyGUI::TextBox>(
                    "Kenshi_TextboxStandardText", MyGUI::IntCoord(0, 0, 10, 10), MyGUI::Align::Default, names[i]);
                if (text == 0) throw 0;
                refs[i]->set(text); // Register immediately for partial-build cleanup.
                text->setNeedMouseFocus(false);
                text->setFontName(nativeFontName);
                text->setFontHeight(nativeFontHeight);
                cache.headerValid = false;
            }
    }
    catch (...)
    {
        DestroyTradeBalance(); cache.failedNativePanel.set(data);
        LogErrorLine("money header skin could not be created; native header retained");
        return;
    }
    for (int i = 0; i < kHeaderCount; ++i)
    {
        MyGUI::ISubWidgetText* rendered = RenderedText(refs[i]->get());
        if (rendered == 0 || rendered->getTextColour().alpha <= 0.0f)
        {
            DestroyTradeBalance(); cache.failedNativePanel.set(data);
            LogErrorLine("money header XML text style is unavailable; native header retained");
            return;
        }
    }
    // Preserve the real plate, skin and opacity, including XML UI replacements.
    // Conceal only the old contents, which remain visible/updateable to Native.
    // UITweaks yields this specific panel while this marker is present.
    static const std::string headerMarker("LEOPARD_OTT_MoneyHeader");
    if (data->getUserString(headerMarker) != "1") data->setUserString(headerMarker, "1");
    if (data->getAlpha() != 0.0f)
    { cache.dataAlpha = data->getAlpha(); data->setAlpha(0.0f); }
    for (int i = 0; i < kHeaderCount; ++i)
        if (refs[i]->get()->getAlpha() != cache.dataAlpha) refs[i]->get()->setAlpha(cache.dataAlpha);
    const int balance = g_balanceStartMoney < 0 ? 0 : g_balanceStartMoney - g_balanceMoney;
    const MyGUI::IntCoord origin = plateParent->getAbsoluteCoord();
    const MyGUI::IntCoord currentPlate = moneyBox->getCoord();
    const MyGUI::Align plateAlign = moneyBox->getAlign();
    const int horizontal = plateAlign.isHStretch() ? 3 : plateAlign.isRight() ? 2 : plateAlign.isHCenter() ? 1 : 0;
    const int vertical = plateAlign.isVStretch() ? 3 : plateAlign.isBottom() ? 2 : plateAlign.isVCenter() ? 1 : 0;
    const TraderAdaptiveLayout::Rect currentGeometry(currentPlate.left,currentPlate.top,currentPlate.width,currentPlate.height);
    // A native layout can reset to the same baseline without changing captions
    // or any cached semantic key. Detect it before Update adopts that baseline.
    const bool nativePlateReset = cache.plateLayout.bound && !(currentGeometry == TraderAdaptiveLayout::ProjectAlign(
        cache.plateLayout.written,plateParent->getWidth()-cache.plateLayout.parentWidth,
        plateParent->getHeight()-cache.plateLayout.parentHeight,horizontal,vertical));
    const TraderAdaptiveLayout::Rect baseline = cache.plateLayout.Update(
        TraderAdaptiveLayout::Rect(currentPlate.left, currentPlate.top, currentPlate.width, currentPlate.height),
        plateParent->getWidth(), plateParent->getHeight(), horizontal, vertical, 0, 0);
    // Capture the latest native baseline without forgetting our still-live write
    // when the cached layout below needs no further changes this frame.
    cache.plateLayout.written = TraderAdaptiveLayout::Rect(currentPlate.left, currentPlate.top, currentPlate.width, currentPlate.height);
    const MyGUI::IntCoord box(origin.left + baseline.left, origin.top + baseline.top, baseline.width, baseline.height);
    const int width = plateParent->getWidth() - 32;
    const MyGUI::IntCoord dataCoord = data->getCoord();
    const MyGUI::Align dataAlign = data->getAlign();
    const int dataHorizontal = dataAlign.isHStretch() ? 3 : dataAlign.isRight() ? 2 : dataAlign.isHCenter() ? 1 : 0;
    const TraderAdaptiveLayout::Rect originalData = TraderAdaptiveLayout::ProjectAlign(
        TraderAdaptiveLayout::Rect(dataCoord.left,dataCoord.top,dataCoord.width,dataCoord.height),
        baseline.width-currentPlate.width,0,dataHorizontal,0);
    int leftPadding = originalData.left;
    int rightPadding = baseline.width-originalData.left-originalData.width;
    if (leftPadding < 0) leftPadding = 0;
    if (rightPadding < 0) rightPadding = 0;
    const int contentWidth = width-leftPadding-rightPadding;
    const int dealPadding = leftPadding > rightPadding ? leftPadding : rightPadding;
    const int dealContentWidth = width-2*dealPadding;
    if (contentWidth <= 4 || dealContentWidth <= 4) { DestroyTradeBalance(); return; }
    const MyGUI::IntCoord headerContent(leftPadding,0,contentWidth,0);
    const MyGUI::IntCoord headerCoord(width, box.top - origin.top, box.width, box.height);
    bool changed = !cache.headerValid || nativePlateReset || cache.headerCoord != headerCoord
        || cache.headerContent != headerContent
        || cache.balance != balance || cache.showDeal != TraderState().core.g_showDealBalance
        || cache.moneyFontHeight != nativeFontHeight || cache.moneyFontName != nativeFontName
        || cache.moneyColour != nativeColour;
    for (int i = 0; i < kHeaderCount; ++i)
        if (cache.headerText[i].Observe(refs[i]->get())) changed = true;
    if (!changed) return;
    cache.headerValid = true;
    cache.headerCoord = headerCoord; cache.balance = balance;
    cache.headerContent = headerContent;
    cache.showDeal = TraderState().core.g_showDealBalance;
    cache.moneyFontHeight = nativeFontHeight;
    cache.moneyFontName = nativeFontName;
    cache.moneyColour = nativeColour;
    MyGUI::TextBox* trader = AsText(cache.trader.get());
    MyGUI::TextBox* deal = AsText(TradeBalanceRef().get());
    HeaderCaptionIfChanged(trader, MyGUI::UString(std::string(Tr("Trader")) + ": " + g_balanceMoneyCaption));
    std::string line = std::string(Tr("Deal")) + ": ";
    // Colour only the amount with the active game's semantic XML palette.
    // A missing/malformed key keeps this widget's own native text style.
    // This branch runs only when the cached header has actually changed.
    const char* amountColour = balance > 0 ? GameTheme::Hex("Good", NULL)
        : balance < 0 ? GameTheme::Hex("Bad", NULL) : NULL;
    if (amountColour != NULL && amountColour[0] == '#' && std::strlen(amountColour) == 7
        && std::strspn(amountColour + 1, "0123456789abcdefABCDEF") == 6)
        line += amountColour;
    if (balance > 0) line += "+";
    else if (balance < 0) line += "-";
    line += g_moneyPrefix + FormatMoney(balance);
    HeaderCaptionIfChanged(deal, MyGUI::UString(line));
    deal->setVisible(TraderState().core.g_showDealBalance);
    int height = box.height > nativeFontHeight + 8 ? box.height : nativeFontHeight + 8;
    for (int i = 0; i < kHeaderCount; ++i)
    {
        MyGUI::TextBox* text = AsText(refs[i]->get());
        if (FontHeight(text) + 8 > height) height = FontHeight(text) + 8;
        // Keep this widget's active XML text colour/shadow paired with its
        // own skin. The source EditBox may use a different input background.
    }
    for (int i = 0; i < kHeaderCount; ++i)
    {
        MyGUI::TextBox* text = AsText(refs[i]->get());
        const int nativeHeight = nativeFontHeight;
        if (FontName(text) != nativeFontName) text->setFontName(nativeFontName);
        if (FontHeight(text) != nativeHeight) text->setFontHeight(nativeHeight);
        const int natural = text->getTextSize().width;
        const int font = TraderAdaptiveLayout::FitFont(nativeHeight, natural, (i == 0 ? contentWidth : dealContentWidth) - 4);
        if (font != FontHeight(text)) text->setFontHeight(font);
    }
    for (int i = 0; i < kHeaderCount; ++i)
    {
        MyGUI::TextBox* text = AsText(refs[i]->get());
        const bool overflow = text->getTextSize().width > (i == 0 ? contentWidth : dealContentWidth) - 4;
        if (text->getNeedMouseFocus() != overflow) text->setNeedMouseFocus(overflow);
        FullCaptionTip(text, text->getCaption(), overflow);
    }
    const int traderWidth = trader->getTextSize().width + 4;
    const int dealWidth = deal->getVisible() ? deal->getTextSize().width + 4 : 0;
    const TraderAdaptiveLayout::Header layout(width, leftPadding + traderWidth, 0, dealWidth, height);
    cache.headerExtra = layout.extraHeight + (height > box.height ? height - box.height : 0);
    const MyGUI::IntCoord plateCoord(16, baseline.top, width, height + layout.extraHeight);
    CoordIfChanged(moneyBox, plateCoord);
    cache.plateLayout.written = TraderAdaptiveLayout::Rect(plateCoord.left, plateCoord.top, plateCoord.width, plateCoord.height);
    CoordIfChanged(trader, MyGUI::IntCoord(leftPadding, layout.traderTop, contentWidth, height));
    CoordIfChanged(deal, MyGUI::IntCoord(dealPadding, layout.dealTop, dealContentWidth, height));
    AlignIfChanged(trader, MyGUI::Align::Left | MyGUI::Align::VCenter);
    AlignIfChanged(deal, MyGUI::Align::HCenter | MyGUI::Align::VCenter);

}

void DestroyTradeBalance()
{
    AdaptiveUiCache& cache = AdaptiveUi();
    MyGUI::Widget* data = cache.nativePanel.get();
    if (data != 0) data->clearUserString("LEOPARD_OTT_MoneyHeader");
    if (data != 0 && data->getAlpha() == 0.0f) data->setAlpha(cache.dataAlpha);
    MyGUI::Widget* plate = cache.plate.get();
    MyGUI::Widget* plateParent = cache.nativePlateParent.get();
    if (plate != 0 && plateParent != 0 && plate->getParent() == plateParent && cache.plateLayout.bound)
    {
        const MyGUI::IntCoord current = plate->getCoord();
        const MyGUI::Align align = plate->getAlign();
        const int horizontal = align.isHStretch() ? 3 : align.isRight() ? 2 : align.isHCenter() ? 1 : 0;
        const int vertical = align.isVStretch() ? 3 : align.isBottom() ? 2 : align.isVCenter() ? 1 : 0;
        const TraderAdaptiveLayout::Rect restore = cache.plateLayout.Update(
            TraderAdaptiveLayout::Rect(current.left,current.top,current.width,current.height),
            plateParent->getWidth(),plateParent->getHeight(),horizontal,vertical,0,0);
        CoordIfChanged(plate,MyGUI::IntCoord(restore.left,restore.top,restore.width,restore.height));
    }
    // Restore the latest native baseline, including resize/reset while enabled.
    MyGUI::Widget* owner = FindLiveDockOwner(false);
    MyGUI::Widget* parent = owner == 0 ? 0 : ResolveInjectionParent(owner);
    if (parent != 0) ReserveAdaptiveRows(parent, 0, 0);
    WidgetRef* refs[] = { &cache.trader, &TradeBalanceRef() };
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    for (int i = 0; i < kHeaderCount; ++i)
    {
        FullCaptionTip(refs[i]->get(), MyGUI::UString(), false);
        if (refs[i]->get() != 0 && gui != 0) gui->destroyWidget(refs[i]->get());
        refs[i]->reset();
    }
    if (cache.measure.get() != 0 && gui != 0) gui->destroyWidget(cache.measure.get());
    cache.measure.reset(); cache.popup.reset(); cache.popupValid = false;
    cache.metrics[0].Reset(); cache.metrics[1].Reset();
    cache.nativeCaption.clear();
    cache.viewportsBound = false;
    cache.uiStateValid = false;
    cache.nativeMoney.reset();
    cache.plate.reset();
    cache.nativePanel.reset(); cache.nativePlateParent.reset();
    cache.failedNativePanel.reset();
    cache.plateLayout = TraderAdaptiveLayout::Viewport();
    cache.viewportHost.reset(); cache.viewportRetry = 0;
    for (int i = 0; i < 7; ++i)
    { cache.controls[i].reset(); cache.baseFont[i] = cache.fittedFont[i] = -1; }
    for (int i = 0; i < 2; ++i)
    {
        cache.viewports[i].reset(); cache.viewportParents[i].reset(); cache.viewportLayout[i] = TraderAdaptiveLayout::Viewport();
        cache.viewportHidden[i] = false; cache.viewportVisible[i] = false;
    }
    cache.headerValid = false;
    cache.footerValid = false;
    cache.headerExtra = cache.footerExtra = 0;
    DataPanelRef().reset();
    ArrangeRef().reset();
    g_balanceOwner = 0;
    g_balanceStartMoney = -1;
    g_balanceMoneyCaption.clear();
    g_balanceMoney = -1;
}

// The drop-down list is as wide as the combo, and on the vanilla interface
// the combo is narrow - item names were cut. An open list takes the key
// focus (ComboBox::showList) and is the combo's own child: widen it to the
// longest item.
void WidenOpenSortList()
{
    AdaptiveUiCache& cache = AdaptiveUi();
    MyGUI::Widget* sort = FindSortControlsContainer();
    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    MyGUI::Widget* focus = input == 0 ? 0 : input->getKeyFocusWidget();
    if (sort == 0 || focus == 0 || !focus->getVisible()) { cache.popupValid = false; return; }
    MyGUI::Widget* owner = focus->getParent();
    if (owner == 0 || owner->getName() != "OTT_SortModeCombo") { cache.popupValid = false; return; }
    MyGUI::ComboBox* combo = owner->castType<MyGUI::ComboBox>(false);
    MyGUI::ListBox* list = focus->castType<MyGUI::ListBox>(false);
    if (combo == 0 || list == 0) return;
    // The public list API returns a row valid only for this call. Never cache it.
    MyGUI::ISubWidgetText* rowText = 0;
    for (size_t i = 0; i < list->getItemCount() && i < 32 && rowText == 0; ++i)
        rowText = RenderedText(list->getWidgetByIndex(i));
    if (rowText == 0 || rowText->getFontHeight() <= 0 || rowText->getFontName().empty()) return;
    if (cache.popup.get() != focus)
    {
        cache.popup.set(focus);
        cache.popupValid = false;
    }
    const bool changed = !cache.popupValid || cache.popupSize != focus->getSize()
        || cache.popupFontHeight != rowText->getFontHeight() || cache.popupFont != rowText->getFontName()
        || cache.popupTextWidth != cache.listTextWidth;
    if (!changed) return;
    const int natural = SortComboItemsWidth(combo, rowText, true);
    if (natural < 0) return;
    const int want = TraderRenderedText::OuterWidth(natural, focus->getWidth(), rowText->getWidth());
    cache.popupChrome = focus->getWidth() - rowText->getWidth();
    cache.popupNaturalWidth = natural;
    // Copy transient row values before resizing: the list may recreate its rows.
    cache.popupFontHeight = rowText->getFontHeight();
    cache.popupFont = rowText->getFontName();
    cache.popupTextWidth = cache.listTextWidth;
    if (focus->getWidth() < want) focus->setSize(want, focus->getHeight());
    cache.popupRenderedWidth = 0;
    for (size_t i = 0; i < list->getItemCount() && i < 32 && cache.popupRenderedWidth == 0; ++i)
    {
        MyGUI::ISubWidgetText* after = RenderedText(list->getWidgetByIndex(i));
        if (after != 0) cache.popupRenderedWidth = after->getWidth();
    }
    cache.popupSize = focus->getSize();
    cache.popupValid = true;
}

void PositionInlineControls()
{
    MyGUI::Widget* owner = FindLiveDockOwner(true);
    MyGUI::Widget* panel = DockPanelRef().get();
    if (owner == 0 || panel == 0) return;
    MyGUI::Widget* parent = ResolveInjectionParent(owner);
    if (parent == 0) return;
    UpdateTradeBalance(owner, parent);
    MyGUI::Widget* search = FindControlsContainer();
    MyGUI::Widget* sort = FindSortControlsContainer();
    MyGUI::Widget* arrange = CachedInParent(ArrangeRef(), parent, "ArrangeButton");
    if (arrange == 0 || search == 0 || sort == 0) return;
    if (arrange->getVisible()) arrange->setVisible(false);
    AdaptiveUiCache& cache = AdaptiveUi();
    const char* names[] = { "OTT_SortLabel", "OTT_SortModeCombo", "OTT_SortDirectionButton",
        "OTT_SearchEdit", "OTT_SearchPlaceholder", "OTT_SearchClearButton", "OTT_SearchCountText" };
    for (int i = 0; i < 7; ++i)
        if (cache.controls[i].get() == 0)
        {
            MyGUI::Widget* widget = FindNamedDescendantRecursive(i < 3 ? sort : search, names[i], false);
            if (widget != 0) cache.controls[i].set(widget);
        }
    MyGUI::ComboBox* combo = cache.controls[1].get() == 0 ? 0
        : cache.controls[1].get()->castType<MyGUI::ComboBox>(false);
    if (combo == 0) return;
    const MyGUI::IntCoord origin = parent->getAbsoluteCoord();
    const MyGUI::IntCoord button = arrange->getAbsoluteCoord();
    const int width = parent->getWidth() - 24;
    const MyGUI::IntCoord footerCoord(width, button.top - origin.top, button.width, button.height);
    bool changed = !cache.footerValid || cache.footerCoord != footerCoord
        || cache.sortSetting != TraderState().core.g_sortComboWidth
        || cache.searchSetting != TraderState().core.g_searchFieldWidth;
    for (int i = 0; i < 7; ++i)
    {
        MyGUI::TextBox* text = AsText(cache.controls[i].get());
        if (text != 0 && FontHeight(text) != cache.fittedFont[i])
            cache.baseFont[i] = FontHeight(text);
        if (cache.footerText[i].Observe(cache.controls[i].get())) changed = true;
    }
    if (cache.comboItems.size() != combo->getItemCount())
    {
        cache.comboItems.resize(combo->getItemCount()); changed = true;
    }
    for (size_t i = 0; i < combo->getItemCount(); ++i)
        if (cache.comboItems[i] != combo->getItemNameAt(i))
        {
            cache.comboItems[i] = combo->getItemNameAt(i); changed = true;
        }
    if (changed)
    {
        cache.footerValid = true; cache.footerCoord = footerCoord;
        cache.sortSetting = TraderState().core.g_sortComboWidth;
        cache.searchSetting = TraderState().core.g_searchFieldWidth;
        for (int i = 0; i < 7; ++i)
        {
            MyGUI::TextBox* text = AsText(cache.controls[i].get());
            if (text != 0 && cache.baseFont[i] > 0 && FontHeight(text) != cache.baseFont[i])
                text->setFontHeight(cache.baseFont[i]);
        }
        int height = button.height > 4 ? button.height - 4 : 22;
        for (int i = 0; i < 7; ++i)
        {
            MyGUI::TextBox* text = AsText(cache.controls[i].get());
            if (text != 0 && FontHeight(text) + 8 > height) height = FontHeight(text) + 8;
        }
        MyGUI::TextBox* sortLabel = AsText(cache.controls[0].get());
        if (sortLabel != 0)
        {
            const int font = TraderAdaptiveLayout::FitFont(FontHeight(sortLabel), sortLabel->getTextSize().width, width / 3);
            if (font != FontHeight(sortLabel)) sortLabel->setFontHeight(font);
        }
        int labelWidth = TextWidth(cache.controls[0].get()) + 8;
        FitSortComboText(combo);
        MyGUI::ISubWidgetText* comboRender = RenderedText(combo);
        const int naturalComboWidth = SortComboItemsWidth(combo);
        if (comboRender == 0 || naturalComboWidth < 0)
        {
            cache.footerValid = false;
            return;
        }
        const int comboNeed = TraderRenderedText::OuterWidth(naturalComboWidth, combo->getWidth(), comboRender->getWidth());
        const int comboChrome = comboNeed - naturalComboWidth;
        cache.closedChrome = comboChrome - 4;
        cache.listWidth = comboNeed;
        cache.listTextWidth = naturalComboWidth;
        int sortExtra = labelWidth + 8 + height + 8;
        int sortNeed = sortExtra + comboNeed;
        const bool showLabel = sortNeed <= width;
        if (!showLabel) { labelWidth = 0; sortExtra = height + 12; sortNeed = sortExtra + comboNeed; }
        if (sortLabel != 0 && sortLabel->getVisible() != showLabel) sortLabel->setVisible(showLabel);
        const int countWidth = cache.controls[6].get() != 0 && cache.controls[6].get()->getVisible()
            ? TextWidth(cache.controls[6].get()) + 12 : 0;
        const int clearWidth = cache.controls[5].get() != 0 && cache.controls[5].get()->getVisible() ? height + 6 : 0;
        int searchNeed = TextWidth(cache.controls[4].get()) + 24 + clearWidth + countWidth;
        if (searchNeed < 120 + clearWidth + countWidth) searchNeed = 120 + clearWidth + countWidth;
        // Manual widths remain preferences; readable localized text sets the floor.
        const int requested = TraderState().core.g_sortComboWidth + sortExtra;
        if (requested > sortNeed && requested + searchNeed + 8 <= width) sortNeed = requested;
        const int requestedSearch = TraderState().core.g_searchFieldWidth + clearWidth + countWidth;
        if (requestedSearch > searchNeed && requestedSearch + sortNeed + 8 <= width) searchNeed = requestedSearch;
        const TraderAdaptiveLayout::Footer layout(width, sortNeed, searchNeed, height);
        cache.footerExtra = layout.extraHeight + (height + 4 > button.height ? height + 4 - button.height : 0);
        const int top = button.top - origin.top + button.height - height - 2;
        CoordIfChanged(panel, MyGUI::IntCoord(layout.extraHeight ? 8 : 8 + layout.sortWidth + 8,
            top + layout.searchTop, layout.searchWidth, height));
        CoordIfChanged(search, MyGUI::IntCoord(0, 0, layout.searchWidth, height));
        CoordIfChanged(sort, MyGUI::IntCoord(8, top + layout.sortTop, layout.sortWidth, height));
        if (!sort->getVisible()) sort->setVisible(true);
        CoordIfChanged(cache.controls[0].get(), MyGUI::IntCoord(0, 0, labelWidth, height));
        CoordIfChanged(combo, MyGUI::IntCoord(labelWidth + (showLabel ? 8 : 4), 0, layout.sortWidth - sortExtra, height));
        const int fittedCombo = TraderAdaptiveLayout::FitFont(FontHeight(combo), naturalComboWidth,
            layout.sortWidth - sortExtra - comboChrome);
        if (fittedCombo != FontHeight(combo)) combo->setFontHeight(fittedCombo);
        MyGUI::TextBox* hint = AsText(cache.controls[4].get());
        MyGUI::TextBox* count = AsText(cache.controls[6].get());
        const int hintNatural = hint == 0 ? 0 : hint->getTextSize().width;
        const int countNatural = count == 0 || !count->getVisible() ? 0 : count->getTextSize().width;
        const int allTextSpace = layout.searchWidth - clearWidth - 28;
        const int allTextNeed = hintNatural + countNatural;
        if (count != 0 && countNatural > 0 && allTextNeed > allTextSpace)
        {
            const int font = TraderAdaptiveLayout::FitFont(FontHeight(count), allTextNeed, allTextSpace);
            if (font != FontHeight(count)) count->setFontHeight(font);
        }
        const int actualCountWidth = count == 0 || !count->getVisible() ? 0 : count->getTextSize().width + 12;
        if (hint != 0)
        {
            const int font = TraderAdaptiveLayout::FitFont(FontHeight(hint), hintNatural,
                layout.searchWidth - clearWidth - actualCountWidth - 24);
            if (font != FontHeight(hint)) hint->setFontHeight(font);
        }
        FitSortComboText(combo);
        const size_t selectedIndex = combo->getIndexSelected();
        if (selectedIndex < combo->getItemCount())
            FullCaptionTip(combo, combo->getItemNameAt(selectedIndex),
                combo->getTextSize().width > layout.sortWidth - sortExtra - comboChrome);
        const int hintSpace = layout.searchWidth - clearWidth - actualCountWidth - 24;
        FullCaptionTip(hint, hint == 0 ? MyGUI::UString() : hint->getCaption(),
            hint != 0 && hint->getTextSize().width > hintSpace);
        const bool countOverflow = count != 0 && count->getVisible() && count->getTextSize().width > layout.searchWidth - 4;
        if (count != 0 && count->getNeedMouseFocus() != countOverflow) count->setNeedMouseFocus(countOverflow);
        FullCaptionTip(count, count == 0 ? MyGUI::UString() : count->getCaption(), countOverflow);
        CoordIfChanged(cache.controls[2].get(), MyGUI::IntCoord(layout.sortWidth - height, 0, height, height));
        LayoutBottomSearchRow(search, layout.searchWidth, height);
        for (int i = 0; i < 7; ++i)
        {
            MyGUI::TextBox* text = AsText(cache.controls[i].get());
            if (text != 0) cache.fittedFont[i] = FontHeight(text);
        }
    }
    ReserveAdaptiveRows(parent, cache.headerExtra, cache.footerExtra);
}

const char* kControlsContainerName = "OTT_TraderControlsContainer";
const char* kSortControlsContainerName = "OTT_TraderSortControlsContainer";
const char* kSearchEditName = "OTT_SearchEdit";
const char* kSearchPlaceholderName = "OTT_SearchPlaceholder";
const char* kSearchClearButtonName = "OTT_SearchClearButton";
const char* kSearchDragHandleName = "OTT_SearchDragHandle";
const char* kSearchCountTextName = "OTT_SearchCountText";
const char* kSortDragHandleName = "OTT_SortDragHandle";
const char* kSortModeComboName = "OTT_SortModeCombo";
const char* kSortDirectionButtonName = "OTT_SortDirectionButton";
const int kSearchCountGap = 2;
const int kPanelOuterPadding = 8;
const int kPanelGap = 8;
const int kPanelHandleWidth = 28;
const int kPanelHandleGap = 6;

struct PendingSearchEditShortcut
{
    PendingSearchEditShortcut()
        : active(false)
        , keyValue(0)
    {
    }

    bool active;
    int keyValue;
    TraderSearchInputBehavior::EditResult editResult;
};

PendingSearchEditShortcut g_pendingSearchEditShortcut;
bool g_haveSearchEditSnapshot = false;
TraderSearchInputBehavior::Snapshot g_searchEditSnapshot;

#define g_showSearchEntryCount (TraderState().core.g_showSearchEntryCount)
#define g_showSearchQuantityCount (TraderState().core.g_showSearchQuantityCount)
#define g_showSearchClearButton (TraderState().core.g_showSearchClearButton)
#define g_autoFocusSearchInput (TraderState().core.g_autoFocusSearchInput)
#define g_searchInputConfiguredWidth (TraderState().core.g_searchInputConfiguredWidth)
#define g_searchInputConfiguredHeight (TraderState().core.g_searchInputConfiguredHeight)
#define g_sortPanelConfiguredWidth (TraderState().core.g_sortPanelConfiguredWidth)
#define g_sortPanelConfiguredHeight (TraderState().core.g_sortPanelConfiguredHeight)

#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_loggedNumericOnlyQueryIgnored (TraderState().search.g_loggedNumericOnlyQueryIgnored)
#define g_sortMode (TraderState().search.g_sortMode)
#define g_sortDirection (TraderState().search.g_sortDirection)
#define g_activeTraderTargetId (TraderState().search.g_activeTraderTargetId)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)
#define g_lastSearchVisibleEntryCount (TraderState().search.g_lastSearchVisibleEntryCount)
#define g_lastSearchTotalEntryCount (TraderState().search.g_lastSearchTotalEntryCount)
#define g_sortedEntriesRoot (TraderState().search.g_sortedEntriesRoot)
#define g_entryBaseCoords (TraderState().search.g_entryBaseCoords)

#define g_controlsEnabled (TraderState().core.g_controlsEnabled)

#define g_loggedNoVisibleTraderTarget (TraderState().windowDetection.g_loggedNoVisibleTraderTarget)

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)

#define g_prevSearchSlashHotkeyDown (TraderState().searchUi.g_prevSearchSlashHotkeyDown)
#define g_prevToggleHotkeyDown (TraderState().searchUi.g_prevToggleHotkeyDown)
#define g_prevDiagnosticsHotkeyDown (TraderState().searchUi.g_prevDiagnosticsHotkeyDown)
#define g_prevSearchCtrlFHotkeyDown (TraderState().searchUi.g_prevSearchCtrlFHotkeyDown)
#define g_controlsWereInjected (TraderState().searchUi.g_controlsWereInjected)
#define g_suppressNextSearchEditChangeEvent (TraderState().searchUi.g_suppressNextSearchEditChangeEvent)
#define g_pendingSlashFocusBaseQuery (TraderState().search.g_pendingSlashFocusBaseQuery)
#define g_pendingSlashFocusTextSuppression (TraderState().searchUi.g_pendingSlashFocusTextSuppression)
#define g_focusSearchEditOnNextInjection (TraderState().searchUi.g_focusSearchEditOnNextInjection)
#define g_searchContainerDragging (TraderState().searchUi.g_searchContainerDragging)
#define g_searchContainerPositionCustomized (TraderState().searchUi.g_searchContainerPositionCustomized)
#define g_searchContainerDragLastMouseX (TraderState().searchUi.g_searchContainerDragLastMouseX)
#define g_searchContainerDragLastMouseY (TraderState().searchUi.g_searchContainerDragLastMouseY)
#define g_searchContainerDragStartLeft (TraderState().searchUi.g_searchContainerDragStartLeft)
#define g_searchContainerDragStartTop (TraderState().searchUi.g_searchContainerDragStartTop)
#define g_searchContainerStoredLeft (TraderState().searchUi.g_searchContainerStoredLeft)
#define g_searchContainerStoredTop (TraderState().searchUi.g_searchContainerStoredTop)
#define g_sortContainerDragging (TraderState().searchUi.g_sortContainerDragging)
#define g_sortContainerPositionCustomized (TraderState().searchUi.g_sortContainerPositionCustomized)
#define g_sortContainerDragLastMouseX (TraderState().searchUi.g_sortContainerDragLastMouseX)
#define g_sortContainerDragLastMouseY (TraderState().searchUi.g_sortContainerDragLastMouseY)
#define g_sortContainerDragStartLeft (TraderState().searchUi.g_sortContainerDragStartLeft)
#define g_sortContainerDragStartTop (TraderState().searchUi.g_sortContainerDragStartTop)
#define g_sortContainerStoredLeft (TraderState().searchUi.g_sortContainerStoredLeft)
#define g_sortContainerStoredTop (TraderState().searchUi.g_sortContainerStoredTop)

#define g_cachedHoveredWidgetInventory (TraderState().binding.g_cachedHoveredWidgetInventory)
#define g_cachedHoveredWidgetInventorySignature (TraderState().binding.g_cachedHoveredWidgetInventorySignature)

bool IsInterestingSearchEditMyGuiKey(MyGUI::KeyCode keyCode)
{
    const int value = keyCode.getValue();
    return value == MyGUI::KeyCode::LeftControl
        || value == MyGUI::KeyCode::RightControl
        || value == MyGUI::KeyCode::ArrowLeft
        || value == MyGUI::KeyCode::ArrowRight
        || value == MyGUI::KeyCode::Backspace
        || value == MyGUI::KeyCode::Tab;
}

void ResetPendingSearchEditShortcut()
{
    g_pendingSearchEditShortcut = PendingSearchEditShortcut();
}

void ResetSearchEditSnapshot()
{
    g_haveSearchEditSnapshot = false;
    g_searchEditSnapshot = TraderSearchInputBehavior::Snapshot();
}

TraderSearchInputBehavior::Text ToSearchInputText(const MyGUI::UString& text)
{
    TraderSearchInputBehavior::Text result;
    const std::size_t length = text.size();
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        result.push_back(static_cast<TraderSearchInputBehavior::Codepoint>(text[i]));
    }
    return result;
}

MyGUI::UString ToMyGuiText(const TraderSearchInputBehavior::Text& text)
{
    MyGUI::UString result;
    const std::size_t length = text.size();
    for (std::size_t i = 0; i < length; ++i)
    {
        result.push_back(static_cast<MyGUI::UString::unicode_char>(text[i]));
    }
    return result;
}

TraderSearchInputBehavior::Selection CaptureSearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t textLength)
{
    if (searchEdit == 0 || !searchEdit->isTextSelection())
    {
        return TraderSearchInputBehavior::Selection();
    }

    const std::size_t selectionStart = searchEdit->getTextSelectionStart();
    if (selectionStart == MyGUI::ITEM_NONE)
    {
        return TraderSearchInputBehavior::Selection();
    }

    const std::size_t selectionLength = searchEdit->getTextSelectionLength();
    return TraderSearchInputBehavior::NormalizeSelection(
        TraderSearchInputBehavior::Selection(true, selectionStart, selectionLength),
        textLength);
}

TraderSearchInputBehavior::Snapshot BuildSearchInputSnapshot(
    const MyGUI::UString& text,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
{
    return TraderSearchInputBehavior::Snapshot(
        ToSearchInputText(text),
        cursorPosition,
        TraderSearchInputBehavior::NormalizeSelection(selection, text.size()));
}

TraderSearchInputBehavior::Snapshot CaptureSearchEditSnapshot(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return TraderSearchInputBehavior::Snapshot();
    }

    const MyGUI::UString text = searchEdit->getOnlyText();
    const std::size_t textLength = text.size();
    const std::size_t cursorPosition = TraderSearchInputBehavior::ClampCursor(
        searchEdit->getTextCursor(),
        textLength);
    return BuildSearchInputSnapshot(
        text,
        cursorPosition,
        CaptureSearchEditSelection(searchEdit, textLength));
}

TraderSearchInputBehavior::ShortcutKind ClassifySearchEditShortcut(MyGUI::KeyCode keyCode)
{
    const int keyValue = keyCode.getValue();
    if (keyValue == MyGUI::KeyCode::ArrowLeft)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlLeft;
    }

    if (keyValue == MyGUI::KeyCode::ArrowRight)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlRight;
    }

    if (keyValue == MyGUI::KeyCode::Backspace)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlBackspace;
    }

    return TraderSearchInputBehavior::ShortcutKind_None;
}

void ApplySearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
{
    if (searchEdit == 0)
    {
        return;
    }

    const std::size_t textLength = searchEdit->getTextLength();
    const std::size_t clampedCursor = TraderSearchInputBehavior::ClampCursor(cursorPosition, textLength);
    const TraderSearchInputBehavior::Selection normalizedSelection =
        TraderSearchInputBehavior::NormalizeSelection(selection, textLength);
    if (normalizedSelection.active)
    {
        searchEdit->setTextSelection(
            normalizedSelection.start,
            normalizedSelection.start + normalizedSelection.length);
        return;
    }

    searchEdit->setTextCursor(clampedCursor);
    searchEdit->setTextSelection(clampedCursor, clampedCursor);
}

TraderSearchInputBehavior::Snapshot BuildScheduledSearchShortcutSnapshot(
    MyGUI::EditBox* searchEdit,
    TraderSearchInputBehavior::ShortcutKind shortcut)
{
    TraderSearchInputBehavior::Snapshot snapshot = CaptureSearchEditSnapshot(searchEdit);
    if (shortcut == TraderSearchInputBehavior::ShortcutKind_CtrlBackspace && g_haveSearchEditSnapshot)
    {
        snapshot.text = g_searchEditSnapshot.text;
        snapshot.cursor = TraderSearchInputBehavior::ClampCursor(
            g_searchEditSnapshot.cursor,
            snapshot.text.size());
        snapshot.selection = TraderSearchInputBehavior::NormalizeSelection(
            snapshot.selection,
            snapshot.text.size());
    }

    return snapshot;
}

bool ShouldShowAnySearchCountMetric()
{
    return g_showSearchEntryCount || g_showSearchQuantityCount;
}

bool SearchHasNoVisibleResults()
{
    return !g_searchQueryNormalized.empty()
        && g_lastSearchTotalEntryCount > 0
        && g_lastSearchVisibleEntryCount == 0;
}

int ResolvePreferredSearchCountTextWidth()
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    if (g_showSearchEntryCount && g_showSearchQuantityCount)
    {
        return 132;
    }

    return 72;
}

int ResolveSearchCountTextWidth(int availableWidth)
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    int desiredWidth = ResolvePreferredSearchCountTextWidth();

    const int minSearchInputWidth = 120;
    const int minCountWidth = 44;
    const int maxAllowedWidth = availableWidth - minSearchInputWidth;
    if (maxAllowedWidth < minCountWidth)
    {
        return 0;
    }
    if (desiredWidth > maxAllowedWidth)
    {
        desiredWidth = maxAllowedWidth;
    }

    return desiredWidth;
}

std::string BuildSearchCountCaption(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    std::stringstream line;
    bool wroteMetric = false;

    if (g_showSearchEntryCount)
    {
        line << visibleEntryCount << " / " << totalEntryCount;
        wroteMetric = true;
    }

    if (g_showSearchQuantityCount)
    {
        if (wroteMetric)
        {
            line << " | ";
        }
        line << visibleQuantity;
    }

    return line.str();
}

bool TryGetCurrentMousePosition(int* xOut, int* yOut)
{
    if (xOut == 0 || yOut == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    *xOut = mouse.left;
    *yOut = mouse.top;
    return true;
}

MyGUI::IntCoord ClampPanelCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& inputCoord)
{
    if (parent == 0)
    {
        return inputCoord;
    }

    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = inputCoord.width;
    const int height = inputCoord.height;

    if (left < 8)
    {
        left = 8;
    }
    if (top < 8)
    {
        top = 8;
    }

    int maxLeft = parent->getWidth() - width - 8;
    int maxTop = parent->getHeight() - height - 8;
    if (maxLeft < 8)
    {
        maxLeft = 8;
    }
    if (maxTop < 8)
    {
        maxTop = 8;
    }

    if (left > maxLeft)
    {
        left = maxLeft;
    }
    if (top > maxTop)
    {
        top = maxTop;
    }

    return MyGUI::IntCoord(left, top, width, height);
}

// Своя панель - по ссылке, которая сама обнуляется при удалении виджета
// (shared/WidgetRef.h). Раньше её искали по имени через весь интерфейс по
// нескольку раз за кадр, и пока окно закрыто - всегда безуспешно.
static WidgetRef& ControlsContainerRef()
{
    static WidgetRef* ref = new WidgetRef();   // не разрушается: MyGUI держит его в списке
    return *ref;
}

static WidgetRef& SortControlsContainerRef()
{
    static WidgetRef* ref = new WidgetRef();
    return *ref;
}

MyGUI::Widget* FindSortControlsContainer()
{
    return SortControlsContainerRef().get();
}

void RememberSearchContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    g_searchContainerStoredLeft = coord.left;
    g_searchContainerStoredTop = coord.top;
    g_searchContainerPositionCustomized = true;
}

void RememberSortContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    g_sortContainerStoredLeft = coord.left;
    g_sortContainerStoredTop = coord.top;
    g_sortContainerPositionCustomized = true;
}

void RememberSearchEditSnapshotValue(
    const std::string& query,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
{
    g_haveSearchEditSnapshot = true;
    g_searchEditSnapshot = BuildSearchInputSnapshot(MyGUI::UString(query), cursorPosition, selection);
}

void RememberSearchEditSnapshot(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        ResetSearchEditSnapshot();
        return;
    }

    g_haveSearchEditSnapshot = true;
    g_searchEditSnapshot = CaptureSearchEditSnapshot(searchEdit);
}

void PersistSearchContainerPosition()
{
    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    SaveTraderConfigSnapshot(config);
}

void PersistSortContainerPosition()
{
    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    SaveTraderConfigSnapshot(config);
}

void MoveSearchContainerByDelta(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    MyGUI::Widget* parent = container->getParent();
    const MyGUI::IntCoord current = container->getCoord();
    const MyGUI::IntCoord moved = ClampPanelCoord(
        parent,
        MyGUI::IntCoord(current.left + deltaX, current.top + deltaY, current.width, current.height));
    if (moved.left == current.left && moved.top == current.top)
    {
        return;
    }

    container->setCoord(moved);
    RememberSearchContainerPosition(container);
}

void MoveSortContainerByDelta(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MyGUI::Widget* container = FindSortControlsContainer();
    if (container == 0)
    {
        return;
    }

    MyGUI::Widget* parent = container->getParent();
    const MyGUI::IntCoord current = container->getCoord();
    const MyGUI::IntCoord moved = ClampPanelCoord(
        parent,
        MyGUI::IntCoord(current.left + deltaX, current.top + deltaY, current.width, current.height));
    if (moved.left == current.left && moved.top == current.top)
    {
        return;
    }

    container->setCoord(moved);
    RememberSortContainerPosition(container);
}

void FinalizeSearchContainerDrag(const char* source)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    g_searchContainerDragging = false;
    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    RememberSearchContainerPosition(container);

    std::stringstream line;
    const MyGUI::IntCoord coord = container->getCoord();
    const bool positionChanged =
        coord.left != g_searchContainerDragStartLeft || coord.top != g_searchContainerDragStartTop;
    if (positionChanged)
    {
        PersistSearchContainerPosition();
    }

    line << "search container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " moved=" << (positionChanged ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogDebugLine(line.str());
}

void FinalizeSortContainerDrag(const char* source)
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    g_sortContainerDragging = false;
    MyGUI::Widget* container = FindSortControlsContainer();
    if (container == 0)
    {
        return;
    }

    RememberSortContainerPosition(container);

    std::stringstream line;
    const MyGUI::IntCoord coord = container->getCoord();
    const bool positionChanged =
        coord.left != g_sortContainerDragStartLeft || coord.top != g_sortContainerDragStartTop;
    if (positionChanged)
    {
        PersistSortContainerPosition();
    }

    line << "sort container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " moved=" << (positionChanged ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogDebugLine(line.str());
}

void OnSearchDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MyGUI::Widget* container = FindControlsContainer();
    if (container != 0)
    {
        const MyGUI::IntCoord coord = container->getCoord();
        g_searchContainerDragStartLeft = coord.left;
        g_searchContainerDragStartTop = coord.top;
    }
    else
    {
        g_searchContainerDragStartLeft = 0;
        g_searchContainerDragStartTop = 0;
    }

    g_searchContainerDragging = true;
    if (!TryGetCurrentMousePosition(&g_searchContainerDragLastMouseX, &g_searchContainerDragLastMouseY))
    {
        g_searchContainerDragLastMouseX = left;
        g_searchContainerDragLastMouseY = top;
    }
}

void OnSortDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MyGUI::Widget* container = FindSortControlsContainer();
    if (container != 0)
    {
        const MyGUI::IntCoord coord = container->getCoord();
        g_sortContainerDragStartLeft = coord.left;
        g_sortContainerDragStartTop = coord.top;
    }
    else
    {
        g_sortContainerDragStartLeft = 0;
        g_sortContainerDragStartTop = 0;
    }

    g_sortContainerDragging = true;
    if (!TryGetCurrentMousePosition(&g_sortContainerDragLastMouseX, &g_sortContainerDragLastMouseY))
    {
        g_sortContainerDragLastMouseX = left;
        g_sortContainerDragLastMouseY = top;
    }
}

void OnSearchDragHandleMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_searchContainerDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_searchContainerDragLastMouseX;
    const int deltaY = mouseY - g_searchContainerDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveSearchContainerByDelta(deltaX, deltaY);
    g_searchContainerDragLastMouseX = mouseX;
    g_searchContainerDragLastMouseY = mouseY;
}

void OnSortDragHandleMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_sortContainerDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_sortContainerDragLastMouseX;
    const int deltaY = mouseY - g_sortContainerDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveSortContainerByDelta(deltaX, deltaY);
    g_sortContainerDragLastMouseX = mouseX;
    g_sortContainerDragLastMouseY = mouseY;
}

void OnSearchDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    OnSearchDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSortDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    OnSortDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSearchDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSearchContainerDrag("drag_release");
}

void OnSortDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSortContainerDrag("drag_release");
}

int RelativeBottomInParent(MyGUI::Widget* parent, MyGUI::Widget* child)
{
    if (parent == 0 || child == 0)
    {
        return 0;
    }

    const MyGUI::IntCoord parentAbsolute = parent->getAbsoluteCoord();
    const MyGUI::IntCoord childAbsolute = child->getAbsoluteCoord();
    return (childAbsolute.top - parentAbsolute.top) + childAbsolute.height;
}

int ResolveControlsTop(MyGUI::Widget* parent)
{
    const int defaultTop = 16;
    if (parent == 0)
    {
        return defaultTop;
    }

    const char* moneyWidgetPriority[] =
    {
        "MoneyAmountTextBox",
        "MoneyAmountText",
        "TotalMoneyBuyer",
        "lbTotalMoney",
        "MoneyLabelText",
        "lbBuyersMoney",
        "datapanel"
    };

    for (std::size_t index = 0; index < sizeof(moneyWidgetPriority) / sizeof(moneyWidgetPriority[0]); ++index)
    {
        MyGUI::Widget* moneyWidget = FindWidgetInParentByToken(parent, moneyWidgetPriority[index]);
        if (moneyWidget == 0)
        {
            continue;
        }

        int top = RelativeBottomInParent(parent, moneyWidget) + 8;
        if (top < defaultTop)
        {
            top = defaultTop;
        }

        std::stringstream line;
        line << "controls top resolved from widget=" << moneyWidgetPriority[index]
             << " top=" << top;
        LogDebugLine(line.str());
        return top;
    }

    LogDebugLine("controls top fallback to default (no money widget found)");
    return defaultTop;
}

void DestroyWidgetDirect(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(widget);
    }
}

bool IsVirtualKeyDown(int virtualKey)
{
    return virtualKey > 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsSlashCharacterChordDown(bool shiftDown, bool ctrlDown, bool altDown)
{
    const HKL keyboardLayout = GetKeyboardLayout(0);
    const SHORT slashMapping = VkKeyScanExA('/', keyboardLayout);
    if (slashMapping != -1)
    {
        const int virtualKey = LOBYTE(static_cast<WORD>(slashMapping));
        const BYTE modifierMask = HIBYTE(static_cast<WORD>(slashMapping));
        const bool shiftRequired = (modifierMask & 1U) != 0;
        const bool ctrlRequired = (modifierMask & 2U) != 0;
        const bool altRequired = (modifierMask & 4U) != 0;

        if (shiftDown == shiftRequired
            && ctrlDown == ctrlRequired
            && altDown == altRequired
            && IsVirtualKeyDown(virtualKey))
        {
            return true;
        }
    }

    const bool oemSlashDown = IsVirtualKeyDown(VK_OEM_2);
    const bool numpadSlashDown = IsVirtualKeyDown(VK_DIVIDE);
    return !ctrlDown && !altDown && ((!shiftDown && oemSlashDown) || numpadSlashDown);
}
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character);
void OnSearchEditKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode);
void OnSortModeComboAccepted(MyGUI::ComboBox* sender, std::size_t index);
void OnSortDirectionButtonClicked(MyGUI::Widget* sender);
void EnsureControlsInjectedIfEnabled();

MyGUI::Widget* FindControlsContainer()
{
    return ControlsContainerRef().get();
}

MyGUI::EditBox* FindSearchEditBox()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[3].get();
    if (cached != 0) return cached->castType<MyGUI::EditBox>(false);
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchEditName, false);
    return found == 0 ? 0 : found->castType<MyGUI::EditBox>(false);
}

MyGUI::TextBox* FindSearchPlaceholderTextBox()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[4].get();
    if (cached != 0) return cached->castType<MyGUI::TextBox>(false);
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchPlaceholderName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::Button* FindSearchClearButton()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[5].get();
    if (cached != 0) return cached->castType<MyGUI::Button>(false);
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchClearButtonName, false);
    return found == 0 ? 0 : found->castType<MyGUI::Button>(false);
}

MyGUI::TextBox* FindSearchCountTextBox()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[6].get();
    if (cached != 0) return cached->castType<MyGUI::TextBox>(false);
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchCountTextName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::ComboBox* FindSortModeComboBox()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[1].get();
    if (cached != 0) return cached->castType<MyGUI::ComboBox>(false);
    MyGUI::Widget* sortContainer = FindSortControlsContainer();
    if (sortContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(sortContainer, kSortModeComboName, false);
    return found == 0 ? 0 : found->castType<MyGUI::ComboBox>(false);
}

MyGUI::Button* FindSortDirectionButton()
{
    MyGUI::Widget* cached = AdaptiveUi().controls[2].get();
    if (cached != 0) return cached->castType<MyGUI::Button>(false);
    MyGUI::Widget* sortContainer = FindSortControlsContainer();
    if (sortContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(sortContainer, kSortDirectionButtonName, false);
    return found == 0 ? 0 : found->castType<MyGUI::Button>(false);
}

MyGUI::Widget* ResolveTraderParentFromControlsContainer()
{
    return ResolveInjectionParent(FindLiveDockOwner(false));
}

void FocusSearchEdit(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        LogWarnLine("search focus skipped: MyGUI InputManager unavailable");
        return;
    }

    input->setKeyFocusWidget(searchEdit);

    std::stringstream line;
    line << "search edit focused"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
}

void BlurSearchEdit(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        LogWarnLine("search blur skipped: MyGUI InputManager unavailable");
        return;
    }

    if (input->getKeyFocusWidget() != searchEdit)
    {
        return;
    }

    input->setKeyFocusWidget(0);

    std::stringstream line;
    line << "search edit blurred"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
}

void FocusSearchEditIfRequested(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (!g_focusSearchEditOnNextInjection || searchEdit == 0)
    {
        return;
    }

    g_focusSearchEditOnNextInjection = false;
    if (g_autoFocusSearchInput)
    {
        FocusSearchEdit(searchEdit, reason);
    }
}

bool IsSearchEditFocused(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return false;
    }

    return input->getKeyFocusWidget() == searchEdit;
}

std::size_t ResolveSortModeComboIndex(TraderSortMode mode)
{
    switch (mode)
    {
    case TraderSortMode_Name:
        return 1;
    case TraderSortMode_UnitPrice:
        return 2;
    case TraderSortMode_StackValue:
        return 3;
    case TraderSortMode_UnitWeight:
        return 4;
    case TraderSortMode_StackWeight:
        return 5;
    case TraderSortMode_ValuePerWeight:
        return 6;
    default:
        return 0;
    }
}

void UpdateSearchUiState()
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::Button* clearButton = FindSearchClearButton();
    MyGUI::TextBox* placeholder = FindSearchPlaceholderTextBox();
    MyGUI::TextBox* countText = FindSearchCountTextBox();
    MyGUI::ComboBox* sortModeCombo = FindSortModeComboBox();
    MyGUI::Button* sortDirectionButton = FindSortDirectionButton();

    const bool hasQuery = !g_searchQueryRaw.empty();
    const bool focused = IsSearchEditFocused(searchEdit);
    const bool noVisibleResults = SearchHasNoVisibleResults();
    const bool sortActive = g_sortMode != TraderSortMode_None;
    AdaptiveUiCache& cache = AdaptiveUi();
    const int state = (hasQuery ? 1 : 0) | (focused ? 2 : 0) | (noVisibleResults ? 4 : 0)
        | (g_showSearchClearButton ? 8 : 0) | (ShouldShowAnySearchCountMetric() ? 16 : 0)
        | (static_cast<int>(g_sortMode) << 8) | (static_cast<int>(g_sortDirection) << 16);
    if (DockPanelRef().get() != 0 && cache.uiStateValid && cache.uiState == state) return;
    cache.uiStateValid = true; cache.uiState = state;

    if (clearButton != 0)
    {
        if (DockPanelRef().get() == 0)
        {
        MyGUI::Widget* container = searchEdit->getParent();
        const MyGUI::IntCoord clearCoord = clearButton->getCoord();
        const int clearButtonWidth = clearCoord.width;
        const int clearGap = 4;
        const bool showClearButton = g_showSearchClearButton;
        const bool clearButtonHasAction = hasQuery;
        int availableRight = 0;
        if (countText != 0 && ShouldShowAnySearchCountMetric())
        {
            availableRight = countText->getLeft() - kSearchCountGap;
        }
        else if (container != 0)
        {
            availableRight = container->getWidth() - 8;
        }
        else
        {
            availableRight = clearCoord.left + clearCoord.width;
        }

        int clearLeft = availableRight - clearButtonWidth;
        if (clearLeft < searchEdit->getLeft())
        {
            clearLeft = searchEdit->getLeft();
        }

        int desiredEditWidth = availableRight - searchEdit->getLeft();
        if (showClearButton)
        {
            desiredEditWidth = clearLeft - clearGap - searchEdit->getLeft();
        }
        if (desiredEditWidth < 80)
        {
            desiredEditWidth = 80;
        }

        const MyGUI::IntCoord editCoord = searchEdit->getCoord();
        if (editCoord.width != desiredEditWidth)
        {
            searchEdit->setCoord(editCoord.left, editCoord.top, desiredEditWidth, editCoord.height);
        }

        if (clearCoord.left != clearLeft)
        {
            clearButton->setCoord(clearLeft, clearCoord.top, clearCoord.width, clearCoord.height);
        }

        }
        const bool showClearButton = g_showSearchClearButton;
        const bool clearButtonHasAction = hasQuery;
        clearButton->setVisible(showClearButton);
        clearButton->setAlpha(
            clearButtonHasAction
                ? (focused ? 1.0f : 0.86f)
                : (focused ? 0.72f : 0.6f));
        clearButton->setColour(
            clearButtonHasAction
                ? (noVisibleResults
                    ? MyGUI::Colour(1.0f, 0.78f, 0.78f, 1.0f)
                    : MyGUI::Colour::White)
                : MyGUI::Colour(0.7f, 0.7f, 0.7f, 1.0f));
    }

    if (placeholder != 0)
    {
        const bool showPlaceholder = !hasQuery && !focused;
        if (clearButton != 0 && DockPanelRef().get() == 0)
        {
            const MyGUI::IntCoord editCoord = searchEdit->getCoord();
            const int placeholderLeft = editCoord.left + 10;
            const int placeholderTop = editCoord.top + 1;
            int placeholderWidth = editCoord.width - 16;
            if (placeholderWidth < 40)
            {
                placeholderWidth = 40;
            }

            const MyGUI::IntCoord placeholderCoord = placeholder->getCoord();
            if (placeholderCoord.left != placeholderLeft
                || placeholderCoord.top != placeholderTop
                || placeholderCoord.width != placeholderWidth
                || placeholderCoord.height != editCoord.height)
            {
                placeholder->setCoord(
                    placeholderLeft,
                    placeholderTop,
                    placeholderWidth,
                    editCoord.height);
            }
        }
        placeholder->setVisible(showPlaceholder);
        placeholder->setTextColour(
            focused
                ? MyGUI::Colour(0.82f, 0.82f, 0.82f, 1.0f)
                : MyGUI::Colour(0.63f, 0.63f, 0.63f, 1.0f));
    }

    searchEdit->setAlpha(focused ? 1.0f : 0.92f);
    searchEdit->setColour(
        noVisibleResults
            ? MyGUI::Colour(1.0f, 0.9f, 0.9f, 1.0f)
            : (focused
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.93f, 0.93f, 0.93f, 1.0f)));

    if (countText != 0)
    {
        countText->setTextColour(
            noVisibleResults
                ? MyGUI::Colour(1.0f, 0.42f, 0.42f, 1.0f)
                : MyGUI::Colour(0.83f, 0.83f, 0.83f, 1.0f));
        countText->setAlpha(focused ? 1.0f : 0.92f);
    }

    if (sortModeCombo != 0)
    {
        const std::size_t desiredIndex = ResolveSortModeComboIndex(g_sortMode);
        if (sortModeCombo->getIndexSelected() != desiredIndex)
        {
            sortModeCombo->setIndexSelected(desiredIndex);
        }
        sortModeCombo->setAlpha(sortActive ? 1.0f : 0.92f);
        sortModeCombo->setColour(
            sortActive
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.88f, 0.88f, 0.88f, 1.0f));
    }

    if (sortDirectionButton != 0)
    {
        sortDirectionButton->setCaption(
            g_sortDirection == TraderSortDirection_Descending ? "v" : "^");
        sortDirectionButton->setAlpha(sortActive ? 1.0f : 0.84f);
        sortDirectionButton->setColour(
            sortActive
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.82f, 0.82f, 0.82f, 1.0f));
    }
}

void UpdateSearchCountText(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    g_lastSearchVisibleEntryCount = visibleEntryCount;
    g_lastSearchTotalEntryCount = totalEntryCount;
    MyGUI::TextBox* countText = FindSearchCountTextBox();
    if (countText == 0)
    {
        UpdateSearchUiState();
        return;
    }

    countText->setVisible(ShouldShowAnySearchCountMetric());
    countText->setCaption(BuildSearchCountCaption(visibleEntryCount, totalEntryCount, visibleQuantity));
    UpdateSearchUiState();
}

void TickSearchContainerDrag()
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetCurrentMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_searchContainerDragLastMouseX;
        const int deltaY = mouseY - g_searchContainerDragLastMouseY;
        MoveSearchContainerByDelta(deltaX, deltaY);
        g_searchContainerDragLastMouseX = mouseX;
        g_searchContainerDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeSearchContainerDrag("drag_release_poll");
    }
}

void TickSortContainerDrag()
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetCurrentMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_sortContainerDragLastMouseX;
        const int deltaY = mouseY - g_sortContainerDragLastMouseY;
        MoveSortContainerByDelta(deltaX, deltaY);
        g_sortContainerDragLastMouseX = mouseX;
        g_sortContainerDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeSortContainerDrag("drag_release_poll");
    }
}

bool AreControlsScaffoldPresent()
{
    MyGUI::Widget* search = FindControlsContainer();
    MyGUI::Widget* sort = FindSortControlsContainer();
    return search != 0 && sort != 0
        && search->getInheritedVisible();
}

bool BuildControlsScaffold(
    MyGUI::Widget* parent,
    int topOverride,
    const SearchUiCallbacks& callbacks)
{
    if (parent == 0)
    {
        return false;
    }
    if (callbacks.onSearchTextChanged == 0
        || callbacks.onSearchEditFocusChanged == 0
        || callbacks.onSearchPlaceholderClicked == 0
        || callbacks.onSearchClearButtonClicked == 0)
    {
        LogErrorLine("failed to build controls scaffold: missing search UI callbacks");
        return false;
    }

    const int outerPadding = 0;
    const int handleWidth = 0;
    const int handleGap = 0;
    const int searchRowHeight = parent->getHeight();
    const int sortRowHeight = g_sortPanelConfiguredHeight;
    const int searchRowTop = outerPadding;
    const int preferredCountWidth = ResolvePreferredSearchCountTextWidth();
    const int preferredCountGap = preferredCountWidth > 0 ? kSearchCountGap : 0;
    const int searchInputLeft = outerPadding + handleWidth + handleGap;
    const int desiredContainerWidth =
        searchInputLeft
        + g_searchInputConfiguredWidth
        + outerPadding
        + preferredCountWidth
        + preferredCountGap;
    const int maxContainerWidth =
        parent->getWidth();
    int containerWidth = desiredContainerWidth;
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }
    if (containerWidth < 240)
    {
        containerWidth = 240;
    }
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }

    const int searchContainerHeight = searchRowHeight + (outerPadding * 2);
    int sortContainerWidth = containerWidth;
    if (sortContainerWidth < kSortPanelConfiguredWidthMin)
    {
        sortContainerWidth = kSortPanelConfiguredWidthMin;
    }
    if (maxContainerWidth > 0 && sortContainerWidth > maxContainerWidth)
    {
        sortContainerWidth = maxContainerWidth;
    }
    const int sortContainerHeight = sortRowHeight + (outerPadding * 2);

    MyGUI::IntCoord containerCoord(0, 0, containerWidth, searchContainerHeight);
    MyGUI::IntCoord sortContainerCoord(8, 8 + searchContainerHeight + kPanelGap,
        sortContainerWidth, sortContainerHeight);

    if (ShouldLogDebug())
    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        std::stringstream line;
        line << "building controls scaffold"
             << " parent=" << SafeWidgetName(parent)
             << " parent_coord=(" << parentCoord.left << "," << parentCoord.top << ","
             << parentCoord.width << "," << parentCoord.height << ")"
             << " search_coord=(" << containerCoord.left << "," << containerCoord.top << ","
             << containerCoord.width << "," << containerCoord.height << ")"
             << " sort_coord=(" << sortContainerCoord.left << "," << sortContainerCoord.top << ","
             << sortContainerCoord.width << "," << sortContainerCoord.height << ")"
             << " search_customized=" << (g_searchContainerPositionCustomized ? "true" : "false")
             << " sort_customized=" << (g_sortContainerPositionCustomized ? "true" : "false");
        LogDebugLine(line.str());
    }

    MyGUI::Widget* container = parent->createWidget<MyGUI::Widget>(
        "PanelEmpty",
        containerCoord,
        MyGUI::Align::HStretch | MyGUI::Align::Top,
        kControlsContainerName);
    if (container == 0)
    {
        LogErrorLine("failed to create controls container");
        return false;
    }
    ControlsContainerRef().set(container);

    MyGUI::Widget* sortContainer = 0;
    const int searchInputAvailableWidth = containerWidth - searchInputLeft - outerPadding;
    int countWidth = ResolveSearchCountTextWidth(searchInputAvailableWidth);
    int countGap = countWidth > 0 ? kSearchCountGap : 0;
    int searchAreaWidth = searchInputAvailableWidth - countWidth - countGap;
    if (searchAreaWidth < 120)
    {
        searchAreaWidth = 120;
    }
    int clearButtonWidth = searchRowHeight;
    if (clearButtonWidth > 32)
    {
        clearButtonWidth = 32;
    }
    if (clearButtonWidth < 18)
    {
        clearButtonWidth = 18;
    }
    const int clearButtonTop = searchRowTop + ((searchRowHeight - clearButtonWidth) / 2);

    MyGUI::Button* dragHandle = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(outerPadding, outerPadding, handleWidth, searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchDragHandleName);
    if (dragHandle == 0)
    {
        LogErrorLine("failed to create search drag handle");
        DestroyWidgetDirect(container);
        return false;
    }
    dragHandle->setVisible(false);
    dragHandle->setCaption("::");
    dragHandle->setNeedMouseFocus(true);
    dragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSearchDragHandleMousePressed);
    dragHandle->eventMouseMove += MyGUI::newDelegate(&OnSearchDragHandleMouseMove);
    dragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSearchDragHandleMouseDrag);
    dragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSearchDragHandleMouseReleased);

    if (countWidth > 0)
    {
        MyGUI::TextBox* countText = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(
                containerWidth - outerPadding - countWidth,
                searchRowTop,
                countWidth,
                searchRowHeight),
            MyGUI::Align::Right | MyGUI::Align::Top,
            kSearchCountTextName);
        if (countText == 0)
        {
            LogErrorLine("failed to create search count text");
            DestroyWidgetDirect(container);
            return false;
        }
        countText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
        UpdateSearchCountText(0, 0, 0);
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(searchInputLeft, searchRowTop, searchAreaWidth, searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchEditName);
    if (searchEdit == 0)
    {
        LogErrorLine("failed to create search edit box");
        DestroyWidgetDirect(container);
        return false;
    }
    searchEdit->setOnlyText(g_searchQueryRaw);
    searchEdit->eventEditTextChange += MyGUI::newDelegate(callbacks.onSearchTextChanged);
    searchEdit->eventKeySetFocus += MyGUI::newDelegate(callbacks.onSearchEditFocusChanged);
    searchEdit->eventKeyLostFocus += MyGUI::newDelegate(callbacks.onSearchEditFocusChanged);
    searchEdit->eventKeyButtonPressed += MyGUI::newDelegate(&OnSearchEditKeyPressed);
    searchEdit->eventKeyButtonReleased += MyGUI::newDelegate(&OnSearchEditKeyReleased);

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(
            searchInputLeft + 10,
            searchRowTop + 1,
            searchAreaWidth - 16,
            searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchPlaceholderName);
    if (placeholder == 0)
    {
        LogErrorLine("failed to create search placeholder");
        DestroyWidgetDirect(container);
        return false;
    }
    placeholder->setCaption(Tr("Search items..."));
    placeholder->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    placeholder->setNeedMouseFocus(true);
    placeholder->eventMouseButtonClick += MyGUI::newDelegate(callbacks.onSearchPlaceholderClicked);

    MyGUI::Button* clearButton = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            searchInputLeft + searchAreaWidth - clearButtonWidth,
            clearButtonTop,
            clearButtonWidth,
            clearButtonWidth),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchClearButtonName);
    if (clearButton == 0)
    {
        LogErrorLine("failed to create search clear button");
        DestroyWidgetDirect(container);
        return false;
    }
    clearButton->setCaption("x");
    clearButton->eventMouseButtonClick += MyGUI::newDelegate(callbacks.onSearchClearButtonClicked);

    MyGUI::Widget* sortParent = ResolveInjectionParent(FindLiveDockOwner(true));
    if (sortParent == 0) { DestroyWidgetDirect(container); return false; }
    sortContainer = sortParent->createWidget<MyGUI::Widget>(
        "PanelEmpty",
        sortContainerCoord,
        MyGUI::Align::Right | MyGUI::Align::Top,
        kSortControlsContainerName);
    SortControlsContainerRef().set(sortContainer);
    if (sortContainer == 0)
    {
        LogErrorLine("failed to create sort controls container");
        DestroyWidgetDirect(container);
        return false;
    }

    MyGUI::Button* sortDragHandle = sortContainer->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            outerPadding,
            outerPadding,
            handleWidth,
            sortContainerHeight - (outerPadding * 2)),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortDragHandleName);
    if (sortDragHandle == 0)
    {
        LogErrorLine("failed to create sort drag handle");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortDragHandle->setVisible(false);
    sortDragHandle->setCaption("::");
    sortDragHandle->setNeedMouseFocus(true);
    sortDragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSortDragHandleMousePressed);
    sortDragHandle->eventMouseMove += MyGUI::newDelegate(&OnSortDragHandleMouseMove);
    sortDragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSortDragHandleMouseDrag);
    sortDragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSortDragHandleMouseReleased);

    const int sortPanelLeft = outerPadding + handleWidth + handleGap;
    const int sortPanelWidth = sortContainerWidth - sortPanelLeft - outerPadding;
    const int sortPanelInnerPadding = 6;
    const int sortLabelWidth = 52;
    const int sortControlGap = 4;
    const int sortControlsTop = outerPadding;
    int sortDirectionButtonWidth = sortRowHeight;
    if (sortDirectionButtonWidth > 32)
    {
        sortDirectionButtonWidth = 32;
    }
    if (sortDirectionButtonWidth < 22)
    {
        sortDirectionButtonWidth = 22;
    }
    int sortComboWidth =
        sortPanelWidth
        - (sortPanelInnerPadding * 2)
        - sortLabelWidth
        - (sortControlGap * 2)
        - sortDirectionButtonWidth;
    if (sortComboWidth < 80)
    {
        sortComboWidth = 80;
    }
    const int sortComboLeft = sortPanelInnerPadding + sortLabelWidth + sortControlGap;
    const int sortDirectionButtonLeft = sortComboLeft + sortComboWidth + sortControlGap;

    MyGUI::TextBox* sortLabel = sortContainer->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(
            sortPanelLeft + sortPanelInnerPadding,
            sortControlsTop,
            sortLabelWidth,
            sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        "OTT_SortLabel");
    if (sortLabel == 0)
    {
        LogErrorLine("failed to create sort label");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortLabel->setCaption(MyGUI::UString(Tr("Sort")));
    sortLabel->setTextColour(MyGUI::Colour(0.65f, 0.63f, 0.59f, 1.0f));
    sortLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    MyGUI::ComboBox* sortModeCombo = sortContainer->createWidget<MyGUI::ComboBox>(
        "Kenshi_ComboBox",
        MyGUI::IntCoord(
            sortPanelLeft + sortComboLeft,
            sortControlsTop,
            sortComboWidth,
            sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortModeComboName);
    if (sortModeCombo == 0)
    {
        LogErrorLine("failed to create sort mode combo");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortModeCombo->setComboModeDrop(true);
    sortModeCombo->setSmoothShow(false);
    sortModeCombo->addItem(MyGUI::UString(Tr("Default")));
    sortModeCombo->addItem(MyGUI::UString(Tr("By name")));
    sortModeCombo->addItem(MyGUI::UString(Tr("Unit price")));
    sortModeCombo->addItem(MyGUI::UString(Tr("Stack value")));
    sortModeCombo->addItem(MyGUI::UString(Tr("Unit weight")));
    sortModeCombo->addItem(MyGUI::UString(Tr("Stack weight")));
    sortModeCombo->addItem(MyGUI::UString(Tr("Price / weight")));
    sortModeCombo->setIndexSelected(ResolveSortModeComboIndex(g_sortMode));
    FitSortComboText(sortModeCombo);
    sortModeCombo->eventComboAccept += MyGUI::newDelegate(&OnSortModeComboAccepted);

    MyGUI::Button* sortDirectionButton = sortContainer->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            sortPanelLeft + sortDirectionButtonLeft,
            sortControlsTop,
            sortDirectionButtonWidth,
            sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortDirectionButtonName);
    if (sortDirectionButton == 0)
    {
        LogErrorLine("failed to create sort direction button");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortDirectionButton->setCaption(
        g_sortDirection == TraderSortDirection_Descending ? "v" : "^");
    sortDirectionButton->eventMouseButtonClick +=
        MyGUI::newDelegate(&OnSortDirectionButtonClicked);

    FocusSearchEditIfRequested(searchEdit, "controls_built");
    UpdateSearchUiState();
    RememberSearchEditSnapshot(searchEdit);

    return true;
}

SearchFocusHotkeyKind DetectSearchFocusHotkeyPressedEdge(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0 || key == 0 || key->keyboard == 0)
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        return SearchFocusHotkeyKind_None;
    }

    const bool searchFocused = IsSearchEditFocused(searchEdit);
    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool altDown = key->keyboard->isKeyDown(OIS::KC_LMENU)
        || key->keyboard->isKeyDown(OIS::KC_RMENU);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool fDown = key->keyboard->isKeyDown(OIS::KC_F);

    const bool slashChordDown =
        !searchFocused && IsSlashCharacterChordDown(shiftDown, ctrlDown, altDown);
    const bool ctrlFChordDown = !searchFocused && ctrlDown && fDown;

    const bool slashPressedEdge = slashChordDown && !g_prevSearchSlashHotkeyDown;
    const bool ctrlFPressedEdge = ctrlFChordDown && !g_prevSearchCtrlFHotkeyDown;

    g_prevSearchSlashHotkeyDown = slashChordDown;
    g_prevSearchCtrlFHotkeyDown = ctrlFChordDown;

    if (slashPressedEdge)
    {
        return SearchFocusHotkeyKind_Slash;
    }
    if (ctrlFPressedEdge)
    {
        return SearchFocusHotkeyKind_CtrlF;
    }

    return SearchFocusHotkeyKind_None;
}

void ResetSearchQueryForTraderSwitch(const char* reason)
{
    const bool hadQuery = !g_searchQueryRaw.empty() || !g_searchQueryNormalized.empty();

    RestoreSortedInventoryLayoutIfNeeded();

    g_searchQueryRaw.clear();
    g_searchQueryNormalized.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    g_sortedEntriesRoot = 0;
    g_entryBaseCoords.clear();
    TraderState().search.g_lastSortInvestigationSignature.clear();
    ResetObservedTraderEntriesState();

    if (hadQuery)
    {
        std::stringstream line;
        line << "search query reset"
             << " reason=" << (reason == 0 ? "<unknown>" : reason);
        LogDebugLine(line.str());
    }
}

bool TryStripSingleSlashShortcutInsertion(
    const std::string& currentText,
    const std::string& baseText,
    std::string* outRestoredText)
{
    if (outRestoredText == 0 || currentText.size() != baseText.size() + 1)
    {
        return false;
    }

    for (std::size_t index = 0; index < currentText.size(); ++index)
    {
        if (currentText[index] != '/')
        {
            continue;
        }

        const std::string restored =
            currentText.substr(0, index) + currentText.substr(index + 1);
        if (restored == baseText)
        {
            *outRestoredText = restored;
            return true;
        }
    }

    return false;
}

void SetSearchQueryAndRefresh(
    MyGUI::EditBox* searchEdit,
    const std::string& rawText,
    const char* reason,
    bool focusAfterSet)
{
    if (searchEdit == 0)
    {
        return;
    }

    g_searchQueryRaw = rawText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;

    const std::string currentOnlyText = searchEdit->getOnlyText().asUTF8();
    if (currentOnlyText != rawText)
    {
        g_suppressNextSearchEditChangeEvent = true;
        searchEdit->setOnlyText(rawText);
    }

    if (focusAfterSet)
    {
        FocusSearchEdit(searchEdit, reason);
    }

    std::stringstream line;
    line << "search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\"";
    LogSearchDebugLine(line.str());

    MarkSearchFilterDirty(reason);
    ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    UpdateSearchUiState();
    RememberSearchEditSnapshot(searchEdit);
}

void ApplySearchShortcutQueryAndCursor(
    MyGUI::EditBox* searchEdit,
    const TraderSearchInputBehavior::EditResult& editResult,
    const char* reason)
{
    if (searchEdit == 0 || !editResult.handled || !editResult.rewriteText)
    {
        return;
    }

    const std::string rawText = ToMyGuiText(editResult.text).asUTF8();
    SetSearchQueryAndRefresh(searchEdit, rawText, reason, false);
    ApplySearchEditSelection(searchEdit, editResult.cursor, editResult.selection);
    RememberSearchEditSnapshotValue(rawText, editResult.cursor, editResult.selection);
}

bool ScheduleSearchEditMyGuiShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0 || !inputManager->isControlPressed())
    {
        return false;
    }

    const TraderSearchInputBehavior::ShortcutKind shortcut = ClassifySearchEditShortcut(keyCode);
    if (shortcut == TraderSearchInputBehavior::ShortcutKind_None)
    {
        return false;
    }

    ResetPendingSearchEditShortcut();
    g_pendingSearchEditShortcut.active = true;
    g_pendingSearchEditShortcut.keyValue = keyCode.getValue();
    g_pendingSearchEditShortcut.editResult = TraderSearchInputBehavior::ApplyShortcut(
        shortcut,
        BuildScheduledSearchShortcutSnapshot(searchEdit, shortcut));
    if (!g_pendingSearchEditShortcut.editResult.handled)
    {
        ResetPendingSearchEditShortcut();
        return false;
    }

    return true;
}

void ApplyPendingSearchEditShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
{
    if (!g_pendingSearchEditShortcut.active || g_pendingSearchEditShortcut.keyValue != keyCode.getValue())
    {
        return;
    }

    const PendingSearchEditShortcut pending = g_pendingSearchEditShortcut;
    ResetPendingSearchEditShortcut();

    if (searchEdit == 0)
    {
        return;
    }

    if (!pending.editResult.handled)
    {
        return;
    }

    if (pending.editResult.rewriteText)
    {
        ApplySearchShortcutQueryAndCursor(
            searchEdit,
            pending.editResult,
            "ctrl_shortcut");
        return;
    }

    ApplySearchEditSelection(searchEdit, pending.editResult.cursor, pending.editResult.selection);
    RememberSearchEditSnapshot(searchEdit);
}

void OnSearchClearButtonClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    if (g_searchQueryRaw.empty())
    {
        FocusSearchEdit(searchEdit, "clear_button_empty");
        return;
    }

    SetSearchQueryAndRefresh(searchEdit, "", "clear_button", true);
}

TraderSortMode ResolveSortModeFromComboIndex(std::size_t index)
{
    switch (index)
    {
    case 1:
        return TraderSortMode_Name;
    case 2:
        return TraderSortMode_UnitPrice;
    case 3:
        return TraderSortMode_StackValue;
    case 4:
        return TraderSortMode_UnitWeight;
    case 5:
        return TraderSortMode_StackWeight;
    case 6:
        return TraderSortMode_ValuePerWeight;
    default:
        return TraderSortMode_None;
    }
}

void SetSortStateAndRefresh(
    TraderSortMode requestedMode,
    TraderSortDirection requestedDirection,
    const char* reason)
{
    const TraderSortMode previousMode = g_sortMode;
    const TraderSortDirection previousDirection = g_sortDirection;
    if (previousMode == requestedMode
        && previousDirection == requestedDirection)
    {
        UpdateSearchUiState();
        return;
    }

    g_sortMode = requestedMode;
    g_sortDirection = requestedDirection;

    std::stringstream line;
    line << "search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " sort_mode="
         << TraderSortStateLabel(g_sortMode, g_sortDirection);
    LogSearchDebugLine(line.str());

    if (previousMode != TraderSortMode_None || requestedMode != TraderSortMode_None)
    {
        ObserveTraderEntriesStateForRefresh();
        MarkSearchFilterDirty(reason);
        ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    }
    UpdateSearchUiState();
}

void OnSortModeComboAccepted(MyGUI::ComboBox*, std::size_t index)
{
    const TraderSortMode requestedMode = ResolveSortModeFromComboIndex(index);
    const char* reason = "sort_mode_default";
    switch (requestedMode)
    {
    case TraderSortMode_Name:
        reason = "sort_mode_name";
        break;
    case TraderSortMode_UnitPrice:
        reason = "sort_mode_unit_price";
        break;
    case TraderSortMode_StackValue:
        reason = "sort_mode_stack_value";
        break;
    case TraderSortMode_UnitWeight:
        reason = "sort_mode_unit_weight";
        break;
    case TraderSortMode_StackWeight:
        reason = "sort_mode_stack_weight";
        break;
    case TraderSortMode_ValuePerWeight:
        reason = "sort_mode_value_weight";
        break;
    default:
        break;
    }
    SetSortStateAndRefresh(
        requestedMode,
        g_sortDirection,
        reason);
}

void OnSortDirectionButtonClicked(MyGUI::Widget*)
{
    const TraderSortDirection nextDirection = ToggleTraderSortDirection(g_sortDirection);
    SetSortStateAndRefresh(
        g_sortMode,
        nextDirection,
        nextDirection == TraderSortDirection_Descending
            ? "sort_direction_desc"
            : "sort_direction_asc");
}

void OnSearchPlaceholderClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    FocusSearchEdit(searchEdit, "placeholder_click");
}

void OnSearchEditKeyFocusChanged(MyGUI::Widget*, MyGUI::Widget*)
{
    ResetPendingSearchEditShortcut();
    UpdateSearchUiState();
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character)
{
    (void)character;

    if (sender == 0 || !IsInterestingSearchEditMyGuiKey(keyCode))
    {
        return;
    }

    MyGUI::EditBox* searchEdit = sender->castType<MyGUI::EditBox>(false);
    if (searchEdit == 0)
    {
        return;
    }

    if (keyCode.getValue() == MyGUI::KeyCode::Tab)
    {
        BlurSearchEdit(searchEdit, "tab_key");
        ResetPendingSearchEditShortcut();
        RememberSearchEditSnapshot(searchEdit);
        UpdateSearchUiState();
        return;
    }

    ScheduleSearchEditMyGuiShortcut(searchEdit, keyCode);
}

void OnSearchEditKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode)
{
    if (sender == 0)
    {
        ResetPendingSearchEditShortcut();
        ResetSearchEditSnapshot();
        return;
    }

    MyGUI::EditBox* searchEdit = sender->castType<MyGUI::EditBox>(false);
    if (searchEdit == 0)
    {
        ResetPendingSearchEditShortcut();
        return;
    }

    if (IsInterestingSearchEditMyGuiKey(keyCode))
    {
        ApplyPendingSearchEditShortcut(searchEdit, keyCode);
    }

    RememberSearchEditSnapshot(searchEdit);
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string captionText = sender->getCaption().asUTF8();
    const std::string onlyText = sender->getOnlyText().asUTF8();
    if (g_suppressNextSearchEditChangeEvent && onlyText == g_searchQueryRaw)
    {
        g_suppressNextSearchEditChangeEvent = false;
        return;
    }
    g_suppressNextSearchEditChangeEvent = false;

    if (g_pendingSlashFocusTextSuppression)
    {
        std::string restoredText;
        if (TryStripSingleSlashShortcutInsertion(
                onlyText,
                g_pendingSlashFocusBaseQuery,
                &restoredText))
        {
            g_pendingSlashFocusTextSuppression = false;
            g_suppressNextSearchEditChangeEvent = true;
            sender->setOnlyText(restoredText);
            return;
        }

        g_pendingSlashFocusTextSuppression = false;
    }

    g_searchQueryRaw = onlyText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;

    std::stringstream line;
    line << "search input changed"
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\""
         << " raw_len=" << g_searchQueryRaw.size()
         << " caption_len=" << captionText.size()
         << " only_len=" << onlyText.size();
    LogSearchDebugLine(line.str());

    MarkSearchFilterDirty("text_changed");
    ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    UpdateSearchUiState();
    RememberSearchEditSnapshot(sender);
}

void DestroyEmbeddedControlsIfPresent()
{
    // The game's tooltip owns a raw caller; clear it while our widgets live.
    const int tipControls[] = { 1, 4, 6 };
    for (int i = 0; i < 3; ++i)
        FullCaptionTip(AdaptiveUi().controls[tipControls[i]].get(), MyGUI::UString(), false);
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    MyGUI::Widget* sortControlsContainer = FindSortControlsContainer();
    if (controlsContainer == 0 && sortControlsContainer == 0)
    {
        RestoreSortedInventoryLayoutIfNeeded();
        ResetPendingSearchEditShortcut();
        ResetSearchEditSnapshot();
        g_searchContainerDragging = false;
        g_sortContainerDragging = false;
        g_searchContainerDragStartLeft = 0;
        g_searchContainerDragStartTop = 0;
        g_sortContainerDragStartLeft = 0;
        g_sortContainerDragStartTop = 0;
        g_controlsWereInjected = false;
        g_searchFilterDirty = false;
        g_sortedEntriesRoot = 0;
        g_entryBaseCoords.clear();
        TraderState().search.g_lastSortInvestigationSignature.clear();
        ResetObservedTraderEntriesState();
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
        g_suppressNextSearchEditChangeEvent = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        return;
    }

    if (controlsContainer != 0 && g_searchContainerPositionCustomized && DockPanelRef().get() == 0)
    {
        const MyGUI::IntCoord coord = controlsContainer->getCoord();
        g_searchContainerStoredLeft = coord.left;
        g_searchContainerStoredTop = coord.top;
    }
    if (sortControlsContainer != 0 && g_sortContainerPositionCustomized && DockPanelRef().get() == 0)
    {
        const MyGUI::IntCoord coord = sortControlsContainer->getCoord();
        g_sortContainerStoredLeft = coord.left;
        g_sortContainerStoredTop = coord.top;
    }
    g_searchContainerDragging = false;
    g_sortContainerDragging = false;
    g_searchContainerDragStartLeft = 0;
    g_searchContainerDragStartTop = 0;
    g_sortContainerDragStartLeft = 0;
    g_sortContainerDragStartTop = 0;

    RestoreSortedInventoryLayoutIfNeeded();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        if (sortControlsContainer != 0)
        {
            gui->destroyWidget(sortControlsContainer);
            LogDebugLine("sort controls container destroyed");
        }
        if (controlsContainer != 0)
        {
            gui->destroyWidget(controlsContainer);
            LogDebugLine("controls container destroyed");
        }
    }

    g_controlsWereInjected = false;
    ResetPendingSearchEditShortcut();
    ResetSearchEditSnapshot();
    g_searchFilterDirty = false;
    g_sortedEntriesRoot = 0;
    g_entryBaseCoords.clear();
    TraderState().search.g_lastSortInvestigationSignature.clear();
    ResetObservedTraderEntriesState();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_cachedHoveredWidgetInventory = 0;
    g_cachedHoveredWidgetInventorySignature.clear();
    ClearLockedKeysetSource();
    ClearInventoryGuiInventoryLinks();
    ClearTraderPanelInventoryBindings();
}

void DestroyControlsIfPresent()
{
    MyGUI::EditBox* edit = FindSearchEditBox();
    if (edit != 0) BlurSearchEdit(edit, "dock_destroyed");
    ClearTraderHiddenItems();
    DestroyEmbeddedControlsIfPresent();
    DestroyTradeBalance();
    MyGUI::Widget* panel = DockPanelRef().get();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (panel != 0 && gui != 0) gui->destroyWidget(panel);
    g_dockOwner = 0;
}

void ApplyRuntimeSearchUiConfig()
{
    DestroyControlsIfPresent();
    if (!g_controlsEnabled)
    {
        return;
    }

    g_loggedNoVisibleTraderTarget = false;
    EnsureControlsInjectedIfEnabled();
}

bool TryInjectControlsToTarget(MyGUI::Widget* anchor, MyGUI::Widget* parent, const char* sourceTag)
{
    if (anchor == 0 || parent == 0)
    {
        LogErrorLine("could not resolve anchor/parent widget for controls injection");
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
    const bool acceptedTarget = (sourceTag != 0 && std::string(sourceTag) == "hover-direct")
        || candidateScore > 0;

    if (!acceptedTarget)
    {
        std::stringstream line;
        line << "rejecting injection target reason=not_likely_trader_window"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " anchor=" << SafeWidgetName(anchor)
             << " parent=" << SafeWidgetName(parent)
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
             << " has_trader_structure=" << (HasTraderStructure(parent) ? "true" : "false");
        if (ShouldLogDebug())
        {
            LogWarnLine(line.str());
        }
        return false;
    }

    const std::string nextTraderTargetId = BuildTraderTargetIdentity(anchor, parent);
    if (!nextTraderTargetId.empty() && !g_activeTraderTargetId.empty() && nextTraderTargetId != g_activeTraderTargetId)
    {
        ResetSearchQueryForTraderSwitch("target_changed");
    }
    g_activeTraderTargetId = nextTraderTargetId;
    g_focusSearchEditOnNextInjection = g_autoFocusSearchInput;

    DestroyControlsIfPresent();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0) return false;
    g_dockOwner = anchor;
    const MyGUI::IntCoord inlineCoord = InlineSearchCoord(parent);
    if (inlineCoord.width < 160 || inlineCoord.height < 22) { g_dockOwner = 0; return false; }
    MyGUI::Widget* controlsParent = parent->createWidget<MyGUI::Widget>(
        "PanelEmpty", inlineCoord, MyGUI::Align::Default, kDockWindowName);
    if (controlsParent == 0) { g_dockOwner = 0; return false; }
    DockPanelRef().set(controlsParent);
    const int topOverride = 0;
    SearchUiCallbacks callbacks;
    callbacks.onSearchTextChanged = &OnSearchTextChanged;
    callbacks.onSearchEditFocusChanged = &OnSearchEditKeyFocusChanged;
    callbacks.onSearchPlaceholderClicked = &OnSearchPlaceholderClicked;
    callbacks.onSearchClearButtonClicked = &OnSearchClearButtonClicked;
    if (!BuildControlsScaffold(controlsParent, topOverride, callbacks))
    {
        LogErrorLine("failed to build inline controls");
        DestroyControlsIfPresent();
        return false;
    }

    PositionInlineControls();
    UpdateSearchUiState();
    g_controlsWereInjected = true;
    MarkSearchFilterDirty("controls_injected");
    if (ApplySearchFilterToTraderParent(parent, false, ShouldLogSearchDebug()))
    {
        g_searchFilterDirty = false;
    }

    std::stringstream line;
    line << "controls scaffold injected"
         << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogDebugLine(line.str());
    return true;
}

bool TryInjectControlsToHoveredWindowDirect()
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("manual attach failed: MyGUI InputManager unavailable");
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        LogWarnLine("manual attach failed: no mouse-focused widget; hover target window and press hotkey again");
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        std::stringstream line;
        line << "manual attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
        LogWarnLine(line.str());
        return false;
    }

    std::stringstream line;
    line << "manual attach using hovered window"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogDebugLine(line.str());

    if (ShouldLogDebug())
    {
        DumpHoveredAttachDiagnostics(hovered, anchor, parent);
    }

    return TryInjectControlsToTarget(anchor, parent, "hover-direct");
}

void EnsureControlsInjectedIfEnabled()
{
    if (!g_controlsEnabled)
    {
        return;
    }

    if (g_dockOwner != 0 && FindLiveDockOwner(true) == 0)
    {
        ApplySearchFilterFromControls(true, false);
        DestroyControlsIfPresent();
        ResetSearchQueryForTraderSwitch("trader_closed");
        g_activeTraderTargetId.clear();
    }
    PositionInlineControls();
    UpdateSearchUiState();

    if (AreControlsScaffoldPresent())
    {
        return;
    }

    if (FindControlsContainer() != 0 || FindSortControlsContainer() != 0)
    {
        DestroyControlsIfPresent();
    }

    // Окно торговли ищем не каждый кадр: панель появится с задержкой не
    // больше 0,1 с, а перебор окон в каждом кадре стоил впустую.
    static DWORD s_lastTargetScanMs = 0;
    const DWORD nowMs = GetTickCount();
    if (s_lastTargetScanMs != 0 && nowMs - s_lastTargetScanMs < 100)
    {
        return;
    }
    s_lastTargetScanMs = nowMs;

    MyGUI::Widget* anchor = 0;
    MyGUI::Widget* parent = 0;
    if (!TryResolveVisibleTraderTarget(&anchor, &parent))
    {
        if (TryResolveHoveredTarget(&anchor, &parent, false))
        {
            g_loggedNoVisibleTraderTarget = false;
            if (!TryInjectControlsToTarget(anchor, parent, "hover-auto"))
            {
                LogWarnLine("hover auto controls scaffold injection failed");
            }
            return;
        }

        if (!g_loggedNoVisibleTraderTarget)
        {
            if (ShouldLogDebug())
            {
                LogDebugLine("controls enabled but no visible trader target found yet");
                DumpTraderTargetProbe();
                DumpVisibleWindowCandidateDiagnostics();
            }
            g_loggedNoVisibleTraderTarget = true;
        }
        return;
    }
    g_loggedNoVisibleTraderTarget = false;
    if (!TryInjectControlsToTarget(anchor, parent, "auto"))
    {
        LogWarnLine("auto controls scaffold injection failed");
    }
}

bool IsToggleHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevToggleHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f8Down = key->keyboard->isKeyDown(OIS::KC_F8);

    const bool chordDown = ctrlDown && shiftDown && f8Down;
    const bool pressedEdge = chordDown && !g_prevToggleHotkeyDown;
    g_prevToggleHotkeyDown = chordDown;
    return pressedEdge;
}

bool IsDiagnosticsHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevDiagnosticsHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f9Down = key->keyboard->isKeyDown(OIS::KC_F9);

    const bool chordDown = ctrlDown && shiftDown && f9Down;
    const bool pressedEdge = chordDown && !g_prevDiagnosticsHotkeyDown;
    g_prevDiagnosticsHotkeyDown = chordDown;
    return pressedEdge;
}

void TickPhase2ControlsScaffold()
{
    TickSearchContainerDrag();
    TickSortContainerDrag();
    WidenOpenSortList();

    if (IsDiagnosticsHotkeyPressedEdge())
    {
        DumpOnDemandTraderDiagnosticsSnapshot();
    }

    if (IsToggleHotkeyPressedEdge())
    {
        if (g_controlsEnabled)
        {
            ApplySearchFilterFromControls(true, false);
            g_controlsEnabled = false;
            DestroyControlsIfPresent();
            g_loggedNoVisibleTraderTarget = false;
            LogDebugLine("controls toggled OFF");
            return;
        }

        g_controlsEnabled = true;
        LogDebugLine("controls toggled ON");

        g_loggedNoVisibleTraderTarget = false;
        EnsureControlsInjectedIfEnabled();
        if (!AreControlsScaffoldPresent())
        {
            if (!TryInjectControlsToHoveredWindowDirect())
            {
                LogWarnLine("manual attach did not inject controls");
            }
        }
    }

    if (!g_controlsEnabled)
    {
        return;
    }

    EnsureControlsInjectedIfEnabled();
    bool handledSearchShortcut = false;
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit != 0)
    {
        const SearchFocusHotkeyKind hotkeyKind =
            DetectSearchFocusHotkeyPressedEdge(searchEdit);
        if (hotkeyKind != SearchFocusHotkeyKind_None)
        {
            if (hotkeyKind == SearchFocusHotkeyKind_Slash)
            {
                g_pendingSlashFocusBaseQuery = g_searchQueryRaw;
                g_pendingSlashFocusTextSuppression = true;
            }
            else
            {
                g_pendingSlashFocusBaseQuery.clear();
                g_pendingSlashFocusTextSuppression = false;
            }
            FocusSearchEdit(searchEdit, "focus_hotkey");
            handledSearchShortcut = true;
        }
    }
    else
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
    }

    // Проверка «изменились ли вещи» собирает подпись всего ассортимента
    // (сортировка, строка на сотни вещей) - не каждый кадр, а раз в 150 мс
    // (leopard, 08.10.2026: просадка FPS в окне торговца).
    static DWORD s_lastObserveMs = 0;
    const DWORD observeNow = GetTickCount();
    if (AreControlsScaffoldPresent() && (g_searchFilterDirty || observeNow - s_lastObserveMs >= 150))
    {
        s_lastObserveMs = observeNow;
        ObserveTraderEntriesStateForRefresh();
    }

    if (AreControlsScaffoldPresent() && !handledSearchShortcut && g_searchFilterDirty)
    {
        ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    }

    if (g_controlsWereInjected && !AreControlsScaffoldPresent())
    {
        RestoreSortedInventoryLayoutIfNeeded();
        g_controlsWereInjected = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        ClearInventoryGuiInventoryLinks();
        ClearTraderPanelInventoryBindings();
        LogDebugLine("controls container no longer present (window likely closed/destroyed); hover target window and press Ctrl+Shift+F8 to attach again");
    }
}
