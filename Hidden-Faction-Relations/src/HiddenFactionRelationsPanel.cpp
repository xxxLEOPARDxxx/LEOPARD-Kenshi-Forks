// Тексты панели - через Tr (каталоги locale/<язык>/hidden_faction_relations.po).
#define KLOC_DOMAIN "hidden_faction_relations"
#include <Localization.h>

#include "HiddenFactionRelationsPanel.h"

#include "HiddenFactionRelations.h"
#include "HiddenFactionRelationsConfig.h"
#include "HiddenFactionRelationsUiModel.h"

#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/gui/TitleScreen.h>
#include <kenshi/util/lektor.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_ScrollView.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cctype>
#include <sstream>
#include <vector>

class ForgottenGUI;
class ToolTip;

class DatapanelGUI : public GUIWindow
{
public:
    virtual ~DatapanelGUI() {}
    virtual void vfunc0x68() {}
    virtual void vfunc0x70() {}
    virtual void vfunc0x78() {}
    virtual void vfunc0x80() {}
    virtual void vfunc0x88() {}
    virtual void vfunc0x90() {}
    virtual void vfunc0x98() {}
    virtual void vfunc0xa0() {}
    virtual void vfunc0xa8() {}
    virtual void vfunc0xb0() {}
    virtual void vfunc0xb8() {}
    virtual void vfunc0xc0(int) {}
    virtual void vfunc0xc8() {}
    virtual void vfunc0xd0() {}
    virtual void vfunc0xd8() {}
    virtual void vfunc0xe0(float) {}
    virtual void vfunc0xe8() {}
    virtual void vfunc0xf0() {}
};

class OptionsWindow : public GUIWindow, public wraps::BaseLayout
{
public:
    char _0xd0;
    lektor<std::string> _0xd8;
    int _0xf0;
    void* _0xf8;
    DatapanelGUI* datapanel;
    MyGUI::TabControl* optionsTab;
    bool _0x110;
    ToolTip* tooltip;
    bool _0x120;
};

namespace
{
const char* kPluginName = "Hidden-Faction-Relations";
const char* kHiddenFactionsTabName = "Hidden Factions";
const char* kHiddenFactionsPanelName = "hidden_faction_relations_panel";
const int kHiddenFactionsPanelLineId = 0x4846;

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef void (*FnOptionsInit)(OptionsWindow*);
typedef void (*FnOptionsSave)(OptionsWindow*);
typedef void (*FnInputHandlerKeyDown)(InputHandler*, OIS::KeyCode);
typedef OptionsWindow* (*FnGetOptionsWindow)();
typedef void (*FnOpenOptionsWindow)(OptionsWindow*);

FnCreateDatapanel g_fnCreateDatapanel = 0;
FnOptionsInit g_fnOptionsInit = 0;
FnOptionsSave g_fnOptionsSave = 0;
FnOptionsInit g_fnOptionsInitOrig = 0;
FnOptionsSave g_fnOptionsSaveOrig = 0;
FnInputHandlerKeyDown g_fnInputHandlerKeyDownOrig = 0;
FnGetOptionsWindow g_fnGetOptionsWindow = 0;
FnOpenOptionsWindow g_fnOpenOptionsWindow = 0;
ForgottenGUI* g_ptrKenshiGUI = 0;

OptionsWindow* g_activeOptionsWindow = 0;
DatapanelGUI* g_activePanel = 0;
MyGUI::Widget* g_activePanelWidget = 0;
// Список показан в странице ModConfigMenu (своя область, API v4); своей
// вкладки в окне настроек больше нет.
bool g_mcmAttached = false;
MyGUI::TabControl* g_boundOptionsTab = 0;
MyGUI::EditBox* g_activeSearchEdit = 0;
MyGUI::Gui* g_boundGui = 0;
std::vector<MyGUI::Widget*> g_dynamicWidgets;
HiddenFactionRelationsUiOptions g_uiOptions;
bool g_focusSearchOnNextRefresh = false;
bool g_refreshPending = false;
bool g_selectHiddenFactionsOnNextOptionsInit = false;
bool g_restoreSearchCursorOnNextRefresh = false;
size_t g_searchCursorPosition = 0;

struct PendingSearchEditShortcut
{
    bool active;
    int keyValue;
    bool rewriteText;
    MyGUI::UString text;
    size_t cursorPosition;

    PendingSearchEditShortcut()
        : active(false)
        , keyValue(0)
        , rewriteText(false)
        , cursorPosition(0)
    {
    }
};

PendingSearchEditShortcut g_pendingSearchEditShortcut;

void LogInfoLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " INFO: " << message;
    DebugLog(line.str().c_str());
}

void LogErrorLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " ERROR: " << message;
    ErrorLog(line.str().c_str());
}

template<typename TWidget>
TWidget* CreateWidgetWithFallback(
    MyGUI::Widget* parent,
    const char* const* skins,
    size_t skinCount,
    const MyGUI::IntCoord& coord,
    bool trackWidget)
{
    if (parent == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < skinCount; ++i)
    {
        try
        {
            TWidget* widget = parent->createWidget<TWidget>(skins[i], coord, MyGUI::Align::Default);
            if (widget != 0)
            {
                if (trackWidget)
                {
                    g_dynamicWidgets.push_back(widget);
                }
                return widget;
            }
        }
        catch (...)
        {
        }
    }

    return 0;
}

MyGUI::TextBox* CreateTrackedTextBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_TextboxStandardText",
        "TextBox"
    };
    return CreateWidgetWithFallback<MyGUI::TextBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, true);
}

MyGUI::TextBox* CreateInlineTextBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_TextboxStandardText",
        "TextBox"
    };
    return CreateWidgetWithFallback<MyGUI::TextBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, false);
}

MyGUI::TextBox* CreateInlinePrimaryTextBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_TextboxPaintedText_Large",
        "Kenshi_TextboxPaintedText",
        "Kenshi_TextboxStandardText",
        "TextBox"
    };
    return CreateWidgetWithFallback<MyGUI::TextBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, false);
}

MyGUI::TextBox* CreateInlineSecondaryTextBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_TextboxStandardText",
        "TextBox"
    };
    return CreateWidgetWithFallback<MyGUI::TextBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, false);
}

MyGUI::EditBox* CreateTrackedEditBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_EditBox",
        "EditBox"
    };
    return CreateWidgetWithFallback<MyGUI::EditBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, true);
}

MyGUI::ComboBox* CreateTrackedComboBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_ComboBox",
        "ComboBox"
    };
    return CreateWidgetWithFallback<MyGUI::ComboBox>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, true);
}

MyGUI::Button* CreateTrackedButton(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_Button1",
        "Kenshi_Button",
        "Button"
    };
    return CreateWidgetWithFallback<MyGUI::Button>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, true);
}

MyGUI::ScrollView* CreateTrackedScrollView(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    const char* skins[] = {
        "Kenshi_ScrollView",
        "Kenshi_ScrollViewEmpty",
        "Kenshi_ScrollViewEmptyLight",
        "ScrollView"
    };
    return CreateWidgetWithFallback<MyGUI::ScrollView>(parent, skins, sizeof(skins) / sizeof(skins[0]), coord, true);
}

void DestroyDynamicWidgets()
{
    g_activeSearchEdit = 0;
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        g_dynamicWidgets.clear();
        return;
    }

    for (size_t i = g_dynamicWidgets.size(); i > 0; --i)
    {
        MyGUI::Widget* widget = g_dynamicWidgets[i - 1];
        if (widget != 0)
        {
            gui->destroyWidget(widget);
        }
    }

    g_dynamicWidgets.clear();
}

void ClearActiveUiState()
{
    DestroyDynamicWidgets();
    g_activeOptionsWindow = 0;
    g_activePanel = 0;
    g_activePanelWidget = 0;
    g_focusSearchOnNextRefresh = false;
    g_refreshPending = false;
    g_restoreSearchCursorOnNextRefresh = false;
    g_searchCursorPosition = 0;
    g_pendingSearchEditShortcut = PendingSearchEditShortcut();
}

void RefreshPanelContents();
bool IsHiddenFactionsTabCurrentlySelected(OptionsWindow* self);

void RequestPanelRefresh()
{
    g_refreshPending = true;
}

void RequestSearchFocusOnNextRefresh()
{
    g_focusSearchOnNextRefresh = true;
}

void RequestSearchCursorRestoreOnNextRefresh(size_t cursorPosition)
{
    g_restoreSearchCursorOnNextRefresh = true;
    g_searchCursorPosition = cursorPosition;
}

bool IsSearchTokenSeparator(MyGUI::UString::unicode_char value)
{
    if (value < 0x80u)
    {
        const unsigned char byte = static_cast<unsigned char>(value);
        return std::isspace(byte) != 0 || !std::isalnum(byte);
    }

    return false;
}

size_t ClampCursor(size_t cursor, size_t length)
{
    return cursor > length ? length : cursor;
}

size_t FindPreviousSearchTokenBoundary(const MyGUI::UString& text, size_t cursor)
{
    const size_t length = text.size();
    size_t position = ClampCursor(cursor, length);

    while (position > 0 && IsSearchTokenSeparator(text[position - 1]))
    {
        --position;
    }

    while (position > 0 && !IsSearchTokenSeparator(text[position - 1]))
    {
        --position;
    }

    return position;
}

size_t FindNextSearchTokenBoundary(const MyGUI::UString& text, size_t cursor)
{
    const size_t length = text.size();
    size_t position = ClampCursor(cursor, length);

    while (position < length && !IsSearchTokenSeparator(text[position]))
    {
        ++position;
    }

    while (position < length && IsSearchTokenSeparator(text[position]))
    {
        ++position;
    }

    return position;
}

void FocusSearchEditBestEffort()
{
    if (g_activeSearchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return;
    }

    inputManager->setKeyFocusWidget(g_activeSearchEdit);
}

void FocusSearchEditIfRequested()
{
    if (!g_focusSearchOnNextRefresh)
    {
        return;
    }

    g_focusSearchOnNextRefresh = false;
    FocusSearchEditBestEffort();
}

bool IsInterestingSearchEditShortcutKey(MyGUI::KeyCode keyCode)
{
    const int value = keyCode.getValue();
    return value == MyGUI::KeyCode::ArrowLeft
        || value == MyGUI::KeyCode::ArrowRight
        || value == MyGUI::KeyCode::Delete;
}

void ApplySearchEditCursor(MyGUI::EditBox* searchEdit, size_t cursorPosition)
{
    if (searchEdit == 0)
    {
        return;
    }

    const size_t clampedCursor = ClampCursor(cursorPosition, searchEdit->getTextLength());
    searchEdit->setTextCursor(clampedCursor);
    searchEdit->setTextSelection(clampedCursor, clampedCursor);
}

bool ScheduleSearchEditShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
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

    const MyGUI::UString text = searchEdit->getOnlyText();
    const size_t textLength = text.size();
    const size_t cursor = ClampCursor(searchEdit->getTextCursor(), textLength);

    g_pendingSearchEditShortcut = PendingSearchEditShortcut();
    g_pendingSearchEditShortcut.active = true;
    g_pendingSearchEditShortcut.keyValue = keyCode.getValue();

    if (keyCode.getValue() == MyGUI::KeyCode::ArrowLeft)
    {
        g_pendingSearchEditShortcut.cursorPosition = FindPreviousSearchTokenBoundary(text, cursor);
        return true;
    }

    if (keyCode.getValue() == MyGUI::KeyCode::ArrowRight)
    {
        g_pendingSearchEditShortcut.cursorPosition = FindNextSearchTokenBoundary(text, cursor);
        return true;
    }

    if (keyCode.getValue() != MyGUI::KeyCode::Delete)
    {
        g_pendingSearchEditShortcut = PendingSearchEditShortcut();
        return false;
    }

    g_pendingSearchEditShortcut.rewriteText = true;
    MyGUI::UString updated = text;

    if (searchEdit->isTextSelection())
    {
        size_t selectionStart = searchEdit->getTextSelectionStart();
        size_t selectionLength = searchEdit->getTextSelectionLength();
        if (selectionStart != MyGUI::ITEM_NONE && selectionLength != 0)
        {
            selectionStart = ClampCursor(selectionStart, textLength);
            if (selectionStart + selectionLength > textLength)
            {
                selectionLength = textLength - selectionStart;
            }

            updated.erase(selectionStart, selectionLength);
            g_pendingSearchEditShortcut.cursorPosition = selectionStart;
            g_pendingSearchEditShortcut.text = updated;
            return true;
        }
    }

    const size_t deleteEnd = FindNextSearchTokenBoundary(text, cursor);
    if (deleteEnd != cursor)
    {
        updated.erase(cursor, deleteEnd - cursor);
    }

    g_pendingSearchEditShortcut.cursorPosition = cursor;
    g_pendingSearchEditShortcut.text = updated;
    return true;
}

void ApplyPendingSearchEditShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
{
    if (!g_pendingSearchEditShortcut.active
        || g_pendingSearchEditShortcut.keyValue != keyCode.getValue())
    {
        return;
    }

    const PendingSearchEditShortcut pending = g_pendingSearchEditShortcut;
    g_pendingSearchEditShortcut = PendingSearchEditShortcut();

    if (searchEdit == 0)
    {
        return;
    }

    if (pending.rewriteText)
    {
        searchEdit->setOnlyText(pending.text);
    }

    ApplySearchEditCursor(searchEdit, pending.cursorPosition);
}

const char* GetSortLabel(HiddenFactionRelationsUiSortMode sortMode)
{
    switch (sortMode)
    {
    case HiddenFactionRelationsUiSort_RelationDescending:
        return Tr("Relation descending");
    case HiddenFactionRelationsUiSort_NameAscending:
        return Tr("Name A-Z");
    case HiddenFactionRelationsUiSort_NameDescending:
        return Tr("Name Z-A");
    case HiddenFactionRelationsUiSort_RelationAscending:
    default:
        return Tr("Relation ascending");
    }
}

// На кнопке - текущий охват (раньше было «Scope: hidden», не влезало).
const char* GetScopeLabel(HiddenFactionRelationsUiScopeMode scopeMode)
{
    switch (scopeMode)
    {
    case HiddenFactionRelationsUiScope_AllFactions:
        return Tr("All factions");
    case HiddenFactionRelationsUiScope_HiddenOnly:
    default:
        return Tr("Hidden only");
    }
}

const char* GetNonZeroLabel(bool nonZeroOnly)
{
    return nonZeroOnly ? Tr("Non-zero only") : Tr("All values");
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    g_uiOptions.searchText = sender->getOnlyText();
    RequestSearchFocusOnNextRefresh();
    RequestSearchCursorRestoreOnNextRefresh(sender->getTextCursor());
    RequestPanelRefresh();
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character)
{
    (void)character;

    if (sender == 0 || !IsInterestingSearchEditShortcutKey(keyCode))
    {
        return;
    }

    ScheduleSearchEditShortcut(sender->castType<MyGUI::EditBox>(false), keyCode);
}

void OnSearchEditKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode)
{
    if (sender == 0 || !IsInterestingSearchEditShortcutKey(keyCode))
    {
        g_pendingSearchEditShortcut = PendingSearchEditShortcut();
        return;
    }

    ApplyPendingSearchEditShortcut(sender->castType<MyGUI::EditBox>(false), keyCode);
}

void OnSortChanged(MyGUI::ComboBox* sender, size_t index)
{
    if (sender == 0 || index == MyGUI::ITEM_NONE)
    {
        return;
    }

    if (index > static_cast<size_t>(HiddenFactionRelationsUiSort_NameDescending))
    {
        return;
    }

    g_uiOptions.sortMode = static_cast<HiddenFactionRelationsUiSortMode>(index);
    RequestPanelRefresh();
}

void OnNonZeroToggleClick(MyGUI::Widget* sender)
{
    MyGUI::Button* button = sender != 0 ? sender->castType<MyGUI::Button>(false) : 0;
    if (button == 0)
    {
        return;
    }

    g_uiOptions.nonZeroOnly = !g_uiOptions.nonZeroOnly;
    button->setStateSelected(g_uiOptions.nonZeroOnly);
    button->setCaption(GetNonZeroLabel(g_uiOptions.nonZeroOnly));
    RequestPanelRefresh();
}

void OnScopeToggleClick(MyGUI::Widget* sender)
{
    MyGUI::Button* button = sender != 0 ? sender->castType<MyGUI::Button>(false) : 0;
    if (button == 0)
    {
        return;
    }

    if (g_uiOptions.scopeMode == HiddenFactionRelationsUiScope_HiddenOnly)
    {
        g_uiOptions.scopeMode = HiddenFactionRelationsUiScope_AllFactions;
    }
    else
    {
        g_uiOptions.scopeMode = HiddenFactionRelationsUiScope_HiddenOnly;
    }

    button->setCaption(GetScopeLabel(g_uiOptions.scopeMode));
    RequestPanelRefresh();
}

void BuildUnavailableState(const std::string& message)
{
    if (g_activePanelWidget == 0)
    {
        return;
    }

    const MyGUI::IntCoord panelCoord = g_activePanelWidget->getCoord();
    const int panelWidth = panelCoord.width > 0 ? panelCoord.width : 720;

    MyGUI::TextBox* messageText = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(12, 8, panelWidth - 24, 48));
    if (messageText != 0)
    {
        messageText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
        messageText->setCaption(Tr(message.c_str()));
        messageText->setNeedMouseFocus(false);
    }
}

void BuildSummaryCard(
    MyGUI::Widget* parent,
    int left,
    int top,
    int width,
    const char* label,
    int value,
    HiddenFactionRelationsUiTone tone)
{
    std::stringstream valueText;
    valueText << value;

    MyGUI::TextBox* countText = CreateTrackedTextBox(parent, MyGUI::IntCoord(left, top, width, 28));
    if (countText != 0)
    {
        countText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        countText->setCaption(valueText.str());
        switch (tone)
        {
        case HiddenFactionRelationsUiTone_Hostile:
            countText->setTextColour(MyGUI::Colour(0.95f, 0.33f, 0.28f, 1.0f));
            break;
        case HiddenFactionRelationsUiTone_Friendly:
            countText->setTextColour(MyGUI::Colour(0.33f, 0.86f, 0.55f, 1.0f));
            break;
        case HiddenFactionRelationsUiTone_Neutral:
            countText->setTextColour(MyGUI::Colour(0.90f, 0.78f, 0.38f, 1.0f));
            break;
        case HiddenFactionRelationsUiTone_Default:
        default:
            countText->setTextColour(MyGUI::Colour(0.92f, 0.92f, 0.92f, 1.0f));
            break;
        }
        countText->setNeedMouseFocus(false);
    }

    MyGUI::TextBox* labelText = CreateTrackedTextBox(parent, MyGUI::IntCoord(left, top + 24, width, 18));
    if (labelText != 0)
    {
        labelText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        labelText->setCaption(Tr(label));
        labelText->setTextColour(MyGUI::Colour(0.72f, 0.72f, 0.72f, 1.0f));
        labelText->setNeedMouseFocus(false);
    }
}

MyGUI::Colour ResolveRelationToneColour(HiddenFactionRelationsUiTone tone)
{
    switch (tone)
    {
    case HiddenFactionRelationsUiTone_Hostile:
        return MyGUI::Colour(0.95f, 0.33f, 0.28f, 1.0f);
    case HiddenFactionRelationsUiTone_Friendly:
        return MyGUI::Colour(0.33f, 0.86f, 0.55f, 1.0f);
    case HiddenFactionRelationsUiTone_Neutral:
        return MyGUI::Colour(0.90f, 0.78f, 0.38f, 1.0f);
    case HiddenFactionRelationsUiTone_Default:
    default:
        return MyGUI::Colour(0.92f, 0.92f, 0.92f, 1.0f);
    }
}

void BuildControlsRow(MyGUI::Widget* parent, int left, int top, int width)
{
    // Ширины - долями от области (страница MCM уже прежней вкладки), поиск
    // не шире, чем задано в настройках.
    const int gap = 8;
    const int searchHeight = HiddenFactionRelationsConfig_GetSearchInputHeight();
    const int usable = width - gap * 3;
    int searchWidth = usable * 30 / 100;
    if (searchWidth > HiddenFactionRelationsConfig_GetSearchInputWidth())
    {
        searchWidth = HiddenFactionRelationsConfig_GetSearchInputWidth();
    }
    if (searchWidth < 120)
    {
        searchWidth = 120;
    }
    const int rest = usable - searchWidth;
    const int sortWidth = rest * 38 / 100;
    const int toggleWidth = rest * 31 / 100;
    const int scopeWidth = rest - sortWidth - toggleWidth;
    const int labelTop = top;
    const int controlTop = top + 16;

    MyGUI::TextBox* searchLabel = CreateTrackedTextBox(parent, MyGUI::IntCoord(left, labelTop, searchWidth, 14));
    if (searchLabel != 0)
    {
        searchLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        searchLabel->setCaption(Tr("Search"));
        searchLabel->setNeedMouseFocus(false);
    }

    MyGUI::EditBox* searchBox = CreateTrackedEditBox(parent, MyGUI::IntCoord(left, controlTop, searchWidth, searchHeight));
    if (searchBox != 0)
    {
        searchBox->setEditMultiLine(false);
        searchBox->setOnlyText(g_uiOptions.searchText);
        searchBox->eventEditTextChange += MyGUI::newDelegate(&OnSearchTextChanged);
        searchBox->eventKeyButtonPressed += MyGUI::newDelegate(&OnSearchEditKeyPressed);
        searchBox->eventKeyButtonReleased += MyGUI::newDelegate(&OnSearchEditKeyReleased);
        if (g_restoreSearchCursorOnNextRefresh)
        {
            ApplySearchEditCursor(searchBox, g_searchCursorPosition);
            g_restoreSearchCursorOnNextRefresh = false;
        }
        g_activeSearchEdit = searchBox;
    }

    const int sortLeft = left + searchWidth + gap;
    MyGUI::TextBox* sortLabel = CreateTrackedTextBox(parent, MyGUI::IntCoord(sortLeft, labelTop, sortWidth, 14));
    if (sortLabel != 0)
    {
        sortLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        sortLabel->setCaption(Tr("Sort"));
        sortLabel->setNeedMouseFocus(false);
    }

    MyGUI::ComboBox* sortCombo = CreateTrackedComboBox(parent, MyGUI::IntCoord(sortLeft, controlTop, sortWidth, searchHeight));
    if (sortCombo != 0)
    {
        sortCombo->setComboModeDrop(true);
        sortCombo->addItem(Tr("Relation ascending"));
        sortCombo->addItem(Tr("Relation descending"));
        sortCombo->addItem(Tr("Name A-Z"));
        sortCombo->addItem(Tr("Name Z-A"));
        sortCombo->setIndexSelected(static_cast<size_t>(g_uiOptions.sortMode));
        sortCombo->setOnlyText(GetSortLabel(g_uiOptions.sortMode));
        sortCombo->eventComboChangePosition += MyGUI::newDelegate(&OnSortChanged);
    }

    const int toggleLeft = sortLeft + sortWidth + gap;
    MyGUI::TextBox* toggleLabel = CreateTrackedTextBox(parent, MyGUI::IntCoord(toggleLeft, labelTop, toggleWidth, 14));
    if (toggleLabel != 0)
    {
        toggleLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        toggleLabel->setCaption(Tr("Filter"));
        toggleLabel->setNeedMouseFocus(false);
    }

    MyGUI::Button* toggleButton = CreateTrackedButton(parent, MyGUI::IntCoord(toggleLeft, controlTop, toggleWidth, searchHeight));
    if (toggleButton != 0)
    {
        toggleButton->setStateSelected(g_uiOptions.nonZeroOnly);
        toggleButton->setCaption(GetNonZeroLabel(g_uiOptions.nonZeroOnly));
        toggleButton->setNeedMouseFocus(true);
        toggleButton->eventMouseButtonClick += MyGUI::newDelegate(&OnNonZeroToggleClick);
    }

    const int scopeLeft = toggleLeft + toggleWidth + gap;
    MyGUI::TextBox* scopeLabel = CreateTrackedTextBox(parent, MyGUI::IntCoord(scopeLeft, labelTop, scopeWidth, 14));
    if (scopeLabel != 0)
    {
        scopeLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        scopeLabel->setCaption(Tr("Scope"));
        scopeLabel->setNeedMouseFocus(false);
    }

    MyGUI::Button* scopeButton = CreateTrackedButton(parent, MyGUI::IntCoord(scopeLeft, controlTop, scopeWidth, searchHeight));
    if (scopeButton != 0)
    {
        scopeButton->setCaption(GetScopeLabel(g_uiOptions.scopeMode));
        scopeButton->setNeedMouseFocus(true);
        scopeButton->eventMouseButtonClick += MyGUI::newDelegate(&OnScopeToggleClick);
    }
}

void BuildEmptyResultsState(
    MyGUI::Widget* parent,
    int contentWidth,
    int& rowY,
    const HiddenFactionRelationsUiView& view)
{
    const char* message = view.showingAllFactions
        ? "No factions match the current view."
        : "No hidden factions match the current view.";
    if (view.summary.scannedFactionCount == 0)
    {
        message = view.showingAllFactions
            ? "No factions with relation data were available in the current world."
            : "No hidden factions were available in the current world.";
    }

    MyGUI::TextBox* emptyText = CreateInlineTextBox(
        parent,
        MyGUI::IntCoord(0, 0, contentWidth, 24));
    if (emptyText != 0)
    {
        emptyText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        emptyText->setCaption(Tr(message));
        emptyText->setNeedMouseFocus(false);
    }
    rowY = 28;
}

void BuildRows(const HiddenFactionRelationsUiView& view)
{
    if (g_activePanelWidget == 0)
    {
        return;
    }

    const MyGUI::IntCoord panelCoord = g_activePanelWidget->getCoord();
    const int panelWidth = panelCoord.width > 0 ? panelCoord.width : 720;
    const int panelHeight = panelCoord.height > 0 ? panelCoord.height : 520;
    const int contentLeft = 12;
    const int contentWidth = panelWidth - 24;
    const int rowValueWidth = 56;            // «-100» помещается
    const int rowRightPadding = 4;

    // Название - у страницы MCM; здесь только сводка в две строки.
    char subtitleText[256];
    sprintf_s(subtitleText,
              view.showingAllFactions ? Tr("Showing %d of %d factions") : Tr("Showing %d of %d hidden factions"),
              static_cast<int>(view.summary.shownFactionCount), static_cast<int>(view.summary.scannedFactionCount));
    MyGUI::TextBox* subtitle = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 2, contentWidth, 20));
    if (subtitle != 0)
    {
        subtitle->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        subtitle->setCaption(subtitleText);
        subtitle->setNeedMouseFocus(false);
    }

    MyGUI::TextBox* playerContextText = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 20, contentWidth, 20));
    if (playerContextText != 0)
    {
        playerContextText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        playerContextText->setCaption(std::string(Tr("Player faction:")) + " " + view.playerFactionName);
        playerContextText->setNeedMouseFocus(false);
    }

    const int summaryGap = 6;
    const int summaryTop = 40;
    const int summaryWidth = (contentWidth - (summaryGap * 3)) / 4;
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft,
        summaryTop,
        summaryWidth,
        view.showingAllFactions ? "TOTAL SHOWN" : "TOTAL HIDDEN",
        view.summary.shownFactionCount,
        HiddenFactionRelationsUiTone_Default);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + (summaryWidth + summaryGap),
        summaryTop,
        summaryWidth,
        "HOSTILE",
        view.summary.hostileCount,
        HiddenFactionRelationsUiTone_Hostile);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + ((summaryWidth + summaryGap) * 2),
        summaryTop,
        summaryWidth,
        "FRIENDLY",
        view.summary.friendlyCount,
        HiddenFactionRelationsUiTone_Friendly);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + ((summaryWidth + summaryGap) * 3),
        summaryTop,
        summaryWidth,
        "NEUTRAL",
        view.summary.neutralCount,
        HiddenFactionRelationsUiTone_Neutral);

    const int controlsTop = 88;
    const int searchHeight = HiddenFactionRelationsConfig_GetSearchInputHeight();
    const int scrollTop = controlsTop + 16 + searchHeight + 16;
    int scrollHeight = panelHeight - (scrollTop + 4);
    if (scrollHeight < 120)
    {
        scrollHeight = 120;
    }
    BuildControlsRow(g_activePanelWidget, contentLeft, controlsTop, contentWidth);

    MyGUI::ScrollView* scrollView = CreateTrackedScrollView(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, scrollTop, contentWidth, scrollHeight));
    if (scrollView == 0)
    {
        BuildUnavailableState("Failed to create the hidden faction scroll view.");
        LogErrorLine("failed to create hidden faction scroll view");
        return;
    }

    scrollView->setCanvasAlign(MyGUI::Align::Left | MyGUI::Align::Top);
    // Только вертикальная прокрутка: ширину строк считаем от видимой ширины
    // за вычетом полосы - раньше её считали до появления полосы, строки
    // выходили шире окна, правый край (число отношения) обрезался.
    scrollView->setVisibleHScroll(false);

    MyGUI::Widget* contentParent = scrollView->getClientWidget();
    if (contentParent == 0)
    {
        contentParent = scrollView;
    }

    // Ширину строк берём у самого ScrollView: сначала задаём холст нужной
    // высоты, MyGUI показывает полосу и сужает клиентскую часть на её
    // ширину - её и читаем. Прежний расчёт «минус 24» был меньше полосы игры,
    // число отношения уходило под неё.
    scrollView->setCanvasSize(contentWidth / 2, static_cast<int>(view.rows.size()) * 28 + 4);
    const int viewWidth = scrollView->getViewCoord().width;
    int clientWidth = (viewWidth < contentWidth - 24 ? viewWidth : contentWidth - 24) - 8;
    {
        std::ostringstream note;
        note << "rows layout: content=" << contentWidth << " view=" << viewWidth << " client=" << clientWidth;
        LogInfoLine(note.str());
    }
    if (clientWidth < 240)
    {
        clientWidth = 240;
    }

    int rowY = 0;
    for (size_t i = 0; i < view.rows.size(); ++i)
    {
        const HiddenFactionRelationsUiRow& row = view.rows[i];
        const int relationLeft = clientWidth - rowValueWidth - rowRightPadding;
        const int originWidth = clientWidth * 26 / 100;
        const int badgeWidth = 110;              // НЕНАВИСТЬ, ДРУЖЕЛЮБНЫ
        const int originLeft = relationLeft - originWidth;
        const int badgeLeft = originLeft - 8 - badgeWidth;
        const int nameWidth = badgeLeft > 96 ? badgeLeft - 8 : 96;

        MyGUI::TextBox* nameText = CreateInlinePrimaryTextBox(
            contentParent,
            MyGUI::IntCoord(0, rowY, nameWidth, 24));
        if (nameText != 0)
        {
            nameText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            nameText->setCaption(row.factionName);
            nameText->setNeedMouseFocus(false);
        }

        MyGUI::TextBox* relationText = CreateInlinePrimaryTextBox(
            contentParent,
            MyGUI::IntCoord(relationLeft, rowY, rowValueWidth, 24));
        if (relationText != 0)
        {
            relationText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
            relationText->setCaption(row.relationValueText);
            relationText->setTextColour(ResolveRelationToneColour(row.relationTone));
            relationText->setNeedMouseFocus(false);
        }

        MyGUI::TextBox* badgeText = CreateInlineSecondaryTextBox(
            contentParent,
            MyGUI::IntCoord(badgeLeft, rowY + 4, badgeWidth, 16));
        if (badgeText != 0)
        {
            badgeText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
            badgeText->setCaption(Tr(row.relationBadgeText.c_str()));
            badgeText->setTextColour(ResolveRelationToneColour(row.relationTone));
            badgeText->setNeedMouseFocus(false);
        }

        MyGUI::TextBox* originText = CreateInlineSecondaryTextBox(
            contentParent,
            MyGUI::IntCoord(originLeft, rowY + 4, originWidth, 16));
        if (originText != 0)
        {
            originText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            originText->setCaption(Tr(row.originText.c_str()));
            originText->setTextColour(MyGUI::Colour(0.48f, 0.48f, 0.48f, 1.0f));
            originText->setNeedMouseFocus(false);
        }

        rowY += 28;
    }

    if (view.rows.empty())
    {
        BuildEmptyResultsState(contentParent, clientWidth, rowY, view);
    }

    if (rowY < scrollView->getClientCoord().height)
    {
        rowY = scrollView->getClientCoord().height;
    }
    scrollView->setCanvasSize(clientWidth, rowY);
    scrollView->setViewOffset(MyGUI::IntPoint(0, 0));
}

void RefreshPanelContentsUnsafe()
{
    if (g_activePanelWidget == 0)
    {
        return;
    }

    DestroyDynamicWidgets();

    HiddenFactionRelationsSnapshot snapshot;
    if (!HiddenFactionRelations_TryCollectSnapshot(ou, &snapshot))
    {
        BuildUnavailableState("Load or resume a game, then open this tab to inspect hidden faction relations.");
        return;
    }

    HiddenFactionRelationsUiView view;
    if (!HiddenFactionRelationsUiModel_BuildView(snapshot, g_uiOptions, &view))
    {
        BuildUnavailableState("Hidden faction relations could not be prepared for display.");
        return;
    }

    BuildRows(view);
    FocusSearchEditIfRequested();
}

void RefreshPanelContents()
{
    RefreshPanelContentsUnsafe();
}

void OnGuiFrameStart(float)
{
    if (!g_refreshPending)
    {
        return;
    }

    g_refreshPending = false;
    if (g_activePanelWidget == 0 || !g_mcmAttached)
    {
        return;
    }

    RefreshPanelContents();
}

bool IsHiddenFactionsTabCurrentlySelected(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0)
    {
        return false;
    }

    const size_t selectedIndex = self->optionsTab->getIndexSelected();
    const MyGUI::UString& selectedName = self->optionsTab->getItemNameAt(selectedIndex);
    return selectedName == kHiddenFactionsTabName;
}

void OnOptionsTabChangeSelect(MyGUI::TabControl* sender, size_t index)
{
    if (sender == 0)
    {
        return;
    }

    const MyGUI::UString& selectedName = sender->getItemNameAt(index);
    if (selectedName != kHiddenFactionsTabName)
    {
        return;
    }

    if (HiddenFactionRelationsConfig_ShouldAutoFocusSearchOnOpen())
    {
        RequestSearchFocusOnNextRefresh();
    }
    RequestPanelRefresh();
}

bool HandleSearchFocusShortcut(InputHandler* inputHandler, OIS::KeyCode keyCode)
{
    if (inputHandler == 0
        || keyCode != OIS::KC_F
        || !inputHandler->ctrl
        || !g_mcmAttached)
    {
        return false;
    }

    if (g_activeSearchEdit != 0)
    {
        FocusSearchEditBestEffort();
        return true;
    }

    RequestSearchFocusOnNextRefresh();
    RequestPanelRefresh();
    return true;
}

bool AreOpenMenuShortcutModifiersSatisfied(InputHandler* inputHandler)
{
    if (inputHandler == 0)
    {
        return false;
    }

    if (HiddenFactionRelationsConfig_ShouldRequireCtrlForOpenMenu() && !inputHandler->ctrl)
    {
        return false;
    }

    if (HiddenFactionRelationsConfig_ShouldRequireShiftForOpenMenu() && !inputHandler->shift)
    {
        return false;
    }

    if (HiddenFactionRelationsConfig_ShouldRequireAltForOpenMenu() && !inputHandler->alt)
    {
        return false;
    }

    return true;
}

bool SelectHiddenFactionsTabUnsafe(MyGUI::TabControl* optionsTab)
{
    if (optionsTab == 0)
    {
        return false;
    }

    const size_t tabIndex = optionsTab->findItemIndexWith(kHiddenFactionsTabName);
    if (tabIndex == MyGUI::ITEM_NONE)
    {
        return false;
    }

    optionsTab->setIndexSelected(tabIndex);
    return true;
}

bool TrySelectHiddenFactionsTabBestEffort()
{
    if (g_activeOptionsWindow == 0 || g_activeOptionsWindow->optionsTab == 0)
    {
        return false;
    }

    return SelectHiddenFactionsTabUnsafe(g_activeOptionsWindow->optionsTab);
}

bool OpenOptionsWindowForHiddenFactionsUnsafe()
{
    if (g_fnGetOptionsWindow == 0 || g_fnOpenOptionsWindow == 0)
    {
        return false;
    }

    OptionsWindow* optionsWindow = g_fnGetOptionsWindow();
    if (optionsWindow == 0)
    {
        return false;
    }

    g_fnOpenOptionsWindow(optionsWindow);
    return true;
}

bool HandleOpenHiddenFactionsShortcut(InputHandler* inputHandler, OIS::KeyCode keyCode)
{
    // Клавиша открытия убрана (07.10.2026): список - на странице MCM, а
    // клавиша у игроков не срабатывала. Код ниже оставлен на случай возврата.
    (void)inputHandler;
    (void)keyCode;
    return false;
    if (inputHandler == 0
        || HiddenFactionRelationsConfig_GetOpenMenuKeycode() < 0
        || static_cast<int>(keyCode) != HiddenFactionRelationsConfig_GetOpenMenuKeycode()
        || !AreOpenMenuShortcutModifiersSatisfied(inputHandler))
    {
        return false;
    }

    // Окно настроек на странице этого мода во вкладке MCM.
    typedef void (__cdecl *OpenPageFn)(const char*);
    HMODULE mcm = GetModuleHandleA("ModConfigMenu.dll");
    OpenPageFn openPage = mcm != 0 ? reinterpret_cast<OpenPageFn>(GetProcAddress(mcm, "MCM_OpenPage")) : 0;
    if (openPage == 0)
    {
        LogErrorLine("ModConfigMenu is not installed: the hidden factions list lives on its MCM page");
        return false;
    }
    openPage("hidden_faction_relations");
    return true;
}



void BindGuiFrameStartBestEffort()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0 || g_boundGui == gui)
    {
        return;
    }

    gui->eventFrameStart += MyGUI::newDelegate(&OnGuiFrameStart);
    g_boundGui = gui;
}

void BindTabSelectDelegateBestEffort(MyGUI::TabControl* optionsTab)
{
    if (optionsTab == 0 || g_boundOptionsTab == optionsTab)
    {
        return;
    }

    optionsTab->eventTabChangeSelect += MyGUI::newDelegate(&OnOptionsTabChangeSelect);
    g_boundOptionsTab = optionsTab;
}

bool BuildPanelUnsafe(OptionsWindow* self)
{
    // Перед последней вкладкой (ванильной «Моды»): RE_Kenshi вешает свои
    // кнопки на последнюю вкладку окна - стань мы последними, они легли бы
    // поверх списка фракций. Так же встают KEP и MCM.
    const size_t tabCount = self->optionsTab->getItemCount();
    MyGUI::TabItem* panelTab = tabCount > 0
        ? self->optionsTab->insertItemAt(tabCount - 1, kHiddenFactionsTabName)
        : self->optionsTab->addItem(kHiddenFactionsTabName);
    if (panelTab == 0)
    {
        LogErrorLine("failed to add hidden factions tab item");
        return false;
    }

    DatapanelGUI* panel = g_fnCreateDatapanel(g_ptrKenshiGUI, kHiddenFactionsPanelName, panelTab, false);
    if (panel == 0)
    {
        LogErrorLine("failed to create hidden factions datapanel");
        return false;
    }

    panel->vfunc0xc0(kHiddenFactionsPanelLineId);
    panel->vfunc0xe0(25.0f);

    MyGUI::Widget* panelWidget = panel->getWidget();
    if (panelWidget == 0)
    {
        LogErrorLine("hidden factions panel widget is null");
        return false;
    }

    panelTab->setVisible(false);
    self->optionsTab->setItemData(panelTab, panel);

    g_activeOptionsWindow = self;
    g_activePanel = panel;
    g_activePanelWidget = panelWidget;
    return true;
}

bool EnsurePanel(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0 || g_ptrKenshiGUI == 0 || g_fnCreateDatapanel == 0)
    {
        return false;
    }

    if (g_activeOptionsWindow == self && g_activePanelWidget != 0)
    {
        return true;
    }

    return BuildPanelUnsafe(self);
}

void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig != 0)
    {
        g_fnOptionsInitOrig(self);
    }

    // Своей вкладки нет: список рисуется в странице MCM, когда она открыта
    // (HiddenFactionRelationsPanel_McmAttach).
    BindGuiFrameStartBestEffort();
    g_activeOptionsWindow = self;
}

void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig != 0)
    {
        g_fnOptionsSaveOrig(self);
    }

    ClearActiveUiState();
}

void InputHandlerKeyDownHook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    if (HandleOpenHiddenFactionsShortcut(thisptr, keyCode))
    {
        return;
    }

    if (HandleSearchFocusShortcut(thisptr, keyCode))
    {
        return;
    }

    if (g_fnInputHandlerKeyDownOrig != 0)
    {
        g_fnInputHandlerKeyDownOrig(thisptr, keyCode);
    }
}

bool ResolveUiFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddress)
{
    if (platform == 1u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003EC950);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(baseAddress + 0x00406B90);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(baseAddress + 0x003FB250);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003ECA90);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(baseAddress + 0x00406F30);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(baseAddress + 0x003FB570);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x021337B0);
            return true;
        }
    }
    else if (platform == 0u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003EFD40);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003EC570);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(baseAddress + 0x004067B0);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(baseAddress + 0x003FAE70);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003EC430);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(baseAddress + 0x004068D0);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(baseAddress + 0x003FAF10);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x021326E0);
            return true;
        }
    }

    return false;
}
}

bool HiddenFactionRelationsPanel_Initialize(
    unsigned int platform,
    const std::string& version,
    uintptr_t baseAddress)
{
    if (!ResolveUiFunctions(platform, version, baseAddress))
    {
        LogErrorLine("failed to resolve hidden faction panel UI functions");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
    {
        LogErrorLine("could not hook OptionsWindow init for hidden faction panel");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsSave, OptionsWindowSaveHook, &g_fnOptionsSaveOrig))
    {
        LogErrorLine("could not hook OptionsWindow save for hidden faction panel");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
            InputHandlerKeyDownHook,
            &g_fnInputHandlerKeyDownOrig))
    {
        LogErrorLine("could not hook InputHandler::keyDownEvent for hidden faction panel");
        return false;
    }

    LogInfoLine("hidden faction panel hooks installed");
    return true;
}


// Страница MCM открыта: рисуем список в выданной области.
void HiddenFactionRelationsPanel_McmAttach(void* parent)
{
    BindGuiFrameStartBestEffort();
    if (g_activePanelWidget != static_cast<MyGUI::Widget*>(parent))
        DestroyDynamicWidgets();
    g_activePanelWidget = static_cast<MyGUI::Widget*>(parent);
    g_mcmAttached = g_activePanelWidget != 0;
    if (HiddenFactionRelationsConfig_ShouldAutoFocusSearchOnOpen())
        RequestSearchFocusOnNextRefresh();
    RequestPanelRefresh();
}

// Страницу закрыли или ушли с неё: виджеты списка убрать, область отдать.
void HiddenFactionRelationsPanel_McmDetach()
{
    DestroyDynamicWidgets();
    g_mcmAttached = false;
    g_activePanelWidget = 0;
    g_activeSearchEdit = 0;
}
