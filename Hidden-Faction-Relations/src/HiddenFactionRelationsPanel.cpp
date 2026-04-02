#include "HiddenFactionRelationsPanel.h"

#include "HiddenFactionRelations.h"
#include "HiddenFactionRelationsUiModel.h"

#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/TitleScreen.h>
#include <kenshi/util/lektor.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ScrollView.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

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

FnCreateDatapanel g_fnCreateDatapanel = 0;
FnOptionsInit g_fnOptionsInit = 0;
FnOptionsSave g_fnOptionsSave = 0;
FnOptionsInit g_fnOptionsInitOrig = 0;
FnOptionsSave g_fnOptionsSaveOrig = 0;
ForgottenGUI* g_ptrKenshiGUI = 0;

OptionsWindow* g_activeOptionsWindow = 0;
DatapanelGUI* g_activePanel = 0;
MyGUI::Widget* g_activePanelWidget = 0;
MyGUI::TabControl* g_boundOptionsTab = 0;
std::vector<MyGUI::Widget*> g_dynamicWidgets;

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
        MyGUI::IntCoord(12, 44, panelWidth - 24, 48));
    if (messageText != 0)
    {
        messageText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
        messageText->setCaption(message);
        messageText->setNeedMouseFocus(false);
    }
}

void BuildSummaryCard(
    MyGUI::Widget* parent,
    int left,
    int top,
    int width,
    const char* label,
    int value)
{
    std::stringstream valueText;
    valueText << value;

    MyGUI::TextBox* countText = CreateTrackedTextBox(parent, MyGUI::IntCoord(left, top, width, 28));
    if (countText != 0)
    {
        countText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        countText->setCaption(valueText.str());
        countText->setNeedMouseFocus(false);
    }

    MyGUI::TextBox* labelText = CreateTrackedTextBox(parent, MyGUI::IntCoord(left, top + 24, width, 18));
    if (labelText != 0)
    {
        labelText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        labelText->setCaption(label);
        labelText->setNeedMouseFocus(false);
    }
}

void BuildEmptyResultsState(
    MyGUI::Widget* parent,
    int contentWidth,
    int& rowY,
    const HiddenFactionRelationsUiView& view)
{
    const char* message = "No hidden factions match the current view.";
    if (view.summary.scannedHiddenFactions == 0)
    {
        message = "No hidden factions were available in the current world.";
    }

    MyGUI::TextBox* emptyText = CreateInlineTextBox(
        parent,
        MyGUI::IntCoord(0, 0, contentWidth, 24));
    if (emptyText != 0)
    {
        emptyText->setTextAlign(MyGUI::Align::HCenter | MyGUI::Align::VCenter);
        emptyText->setCaption(message);
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
    const int rowValueWidth = 96;
    const int rowRightPadding = 16;

    MyGUI::TextBox* titleText = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 10, contentWidth, 24));
    if (titleText != 0)
    {
        titleText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        titleText->setCaption("HIDDEN FACTION RELATIONS");
        titleText->setNeedMouseFocus(false);
    }

    std::stringstream subtitleText;
    subtitleText << "Showing " << view.summary.shownHiddenFactions
                 << " / " << view.summary.scannedHiddenFactions
                 << " hidden factions";
    MyGUI::TextBox* subtitle = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 34, contentWidth, 20));
    if (subtitle != 0)
    {
        subtitle->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        subtitle->setCaption(subtitleText.str());
        subtitle->setNeedMouseFocus(false);
    }

    std::stringstream playerText;
    playerText << "Player faction: " << view.playerFactionName;
    if (!view.playerFactionId.empty())
    {
        playerText << " [" << view.playerFactionId << "]";
    }

    MyGUI::TextBox* playerContextText = CreateTrackedTextBox(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 52, contentWidth, 20));
    if (playerContextText != 0)
    {
        playerContextText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        playerContextText->setCaption(playerText.str());
        playerContextText->setNeedMouseFocus(false);
    }

    const int summaryGap = 6;
    const int summaryTop = 76;
    const int summaryWidth = (contentWidth - (summaryGap * 3)) / 4;
    BuildSummaryCard(g_activePanelWidget, contentLeft, summaryTop, summaryWidth, "TOTAL HIDDEN", view.summary.shownHiddenFactions);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + (summaryWidth + summaryGap),
        summaryTop,
        summaryWidth,
        "HOSTILE",
        view.summary.hostileCount);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + ((summaryWidth + summaryGap) * 2),
        summaryTop,
        summaryWidth,
        "FRIENDLY",
        view.summary.friendlyCount);
    BuildSummaryCard(
        g_activePanelWidget,
        contentLeft + ((summaryWidth + summaryGap) * 3),
        summaryTop,
        summaryWidth,
        "NEUTRAL",
        view.summary.neutralCount);

    MyGUI::ScrollView* scrollView = CreateTrackedScrollView(
        g_activePanelWidget,
        MyGUI::IntCoord(contentLeft, 124, contentWidth, panelHeight - 136));
    if (scrollView == 0)
    {
        BuildUnavailableState("Failed to create the hidden faction scroll view.");
        LogErrorLine("failed to create hidden faction scroll view");
        return;
    }

    MyGUI::Widget* contentParent = scrollView->getClientWidget();
    if (contentParent == 0)
    {
        contentParent = scrollView;
    }

    const MyGUI::IntCoord clientCoord = scrollView->getClientCoord();
    int clientWidth = clientCoord.width > 0 ? clientCoord.width : panelWidth - 48;
    if (clientWidth < 240)
    {
        clientWidth = panelWidth - 48;
    }

    int rowY = 0;
    for (size_t i = 0; i < view.rows.size(); ++i)
    {
        const HiddenFactionRelationsUiRow& row = view.rows[i];

        MyGUI::TextBox* nameText = CreateInlineTextBox(
            contentParent,
            MyGUI::IntCoord(0, rowY, clientWidth - rowValueWidth - rowRightPadding, 22));
        if (nameText != 0)
        {
            nameText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            nameText->setCaption(row.factionName);
            nameText->setNeedMouseFocus(false);
        }

        MyGUI::TextBox* relationText = CreateInlineTextBox(
            contentParent,
            MyGUI::IntCoord(clientWidth - rowValueWidth - rowRightPadding, rowY, rowValueWidth, 22));
        if (relationText != 0)
        {
            relationText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
            relationText->setCaption(row.relationValueText);
            relationText->setNeedMouseFocus(false);
        }

        MyGUI::TextBox* badgeText = CreateInlineTextBox(
            contentParent,
            MyGUI::IntCoord(12, rowY + 20, clientWidth - 12, 18));
        if (badgeText != 0)
        {
            badgeText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            badgeText->setCaption(row.relationBadgeText);
            badgeText->setNeedMouseFocus(false);
        }

        rowY += 42;
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
    if (!HiddenFactionRelationsUiModel_BuildDefaultView(snapshot, &view))
    {
        BuildUnavailableState("Hidden faction relations could not be prepared for display.");
        return;
    }

    BuildRows(view);
}

void RefreshPanelContents()
{
    RefreshPanelContentsUnsafe();
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

    RefreshPanelContents();
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
    MyGUI::TabItem* panelTab = self->optionsTab->addItem(kHiddenFactionsTabName);
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

    if (!EnsurePanel(self))
    {
        return;
    }

    BindTabSelectDelegateBestEffort(self->optionsTab);
    if (IsHiddenFactionsTabCurrentlySelected(self))
    {
        RefreshPanelContents();
    }
}

void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig != 0)
    {
        g_fnOptionsSaveOrig(self);
    }

    ClearActiveUiState();
}

bool ResolveUiFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddress)
{
    if (platform == 1u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003EC950);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003ECA90);
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
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddress + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddress + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddress + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddress + 0x003EC430);
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

    LogInfoLine("hidden faction panel hooks installed");
    return true;
}
