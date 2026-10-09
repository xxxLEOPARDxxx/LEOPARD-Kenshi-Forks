#include "hub_menu_bridge.h"

#include "hub_commit.h"
#include "hub_registry.h"
#include "hub_ui.h"

#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/InputHandler.h>
#include <kenshi/TitleScreen.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_ScrollBar.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

class ToolTip;
class ForgottenGUI;
class DatapanelGUI;

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

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef void (*FnOptionsInit)(OptionsWindow*);
typedef void (*FnOptionsSave)(OptionsWindow*);

namespace
{
const char* kHubTabName = "Mod Hub";
const char* kHubPanelName = "emc_mod_hub_options";
const int kHubPanelLineId = 0x454d43;
const char* kNamespaceButtonSkin = "Kenshi_Button1";
const char* kHeaderButtonSkin = "Kenshi_Button1";
const char* kValueButtonSkin = "Kenshi_Button1";
const char* kTextSkin = "Kenshi_TextboxStandardText";
const char* kEditBoxSkin = "Kenshi_EditBox";
const char* kNoMatchesText = "No matches in this tab. Try checking other tabs?";

bool g_hub_enabled = true;
bool g_hooks_installed = false;
HubMenuBridgeOptionsWindowInitObserver g_options_window_init_observer = 0;

FnCreateDatapanel g_fnCreateDatapanel = 0;
FnOptionsInit g_fnOptionsInit = 0;
FnOptionsSave g_fnOptionsSave = 0;
FnOptionsInit g_fnOptionsInitOrig = 0;
FnOptionsSave g_fnOptionsSaveOrig = 0;
ForgottenGUI* g_ptrKenshiGUI = 0;

void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;

OptionsWindow* g_active_options_window = 0;
DatapanelGUI* g_active_hub_panel = 0;
MyGUI::Widget* g_active_hub_panel_widget = 0;
std::string g_selected_namespace_id;
std::vector<MyGUI::Widget*> g_dynamic_widgets;
bool g_logged_missing_edit_box_skin = false;
bool g_logged_missing_scrollbar_skin = false;
bool g_restore_search_focus_after_rebuild = false;
std::string g_restore_search_focus_namespace_id;
int g_hub_scroll_offset = 0;
int g_hub_scroll_max_offset = 0;
int g_hub_scroll_page_step = 160;
bool g_ignore_scrollbar_position_event = false;

const int kHubScrollLineStep = 24;
const int kHubScrollControlWidth = 44;
const int kHubScrollGutterWidth = 56;

void OnHubMouseWheel(MyGUI::Widget* sender, int rel);
void OnHubScrollBarPositionChanged(MyGUI::ScrollBar* sender, size_t position);

struct RenderModGroup
{
    std::string mod_id;
    std::string mod_display_name;
    bool collapsed;
    std::vector<HubUiRowView> rows;
};

struct RenderNamespaceGroup
{
    std::string namespace_id;
    std::string namespace_display_name;
    std::vector<RenderModGroup> mods;
};

bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t base_addr)
{
    if (platform == 1u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC950);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003ECA90);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021337B0);
            return true;
        }
    }
    else if (platform == 0u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003EFD40);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC570);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC430);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021326E0);
            return true;
        }
    }

    return false;
}

void DestroyDynamicWidgets()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        g_dynamic_widgets.clear();
        return;
    }

    for (size_t index = g_dynamic_widgets.size(); index > 0; --index)
    {
        MyGUI::Widget* widget = g_dynamic_widgets[index - 1];
        if (widget != 0)
        {
            gui->destroyWidget(widget);
        }
    }

    g_dynamic_widgets.clear();
}

template <typename TWidget>
TWidget* CreateTrackedWidget(MyGUI::Widget* parent, const char* skin, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    TWidget* widget = parent->createWidget<TWidget>(skin, coord, MyGUI::Align::Default);
    if (widget != 0)
    {
        widget->eventMouseWheel += MyGUI::newDelegate(&OnHubMouseWheel);
        g_dynamic_widgets.push_back(widget);
    }
    return widget;
}

MyGUI::EditBox* CreateTrackedSearchBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    const char* skins[] = {
        kEditBoxSkin,
        "Kenshi_EditBoxStrechEmpty",
        "EditBoxLE",
        "EditBoxLEEmpty",
        "EditBox",
        "EditBoxEmpty"
    };
    for (size_t index = 0; index < sizeof(skins) / sizeof(skins[0]); ++index)
    {
        try
        {
            MyGUI::EditBox* widget = parent->createWidget<MyGUI::EditBox>(skins[index], coord, MyGUI::Align::Default);
            if (widget != 0)
            {
                widget->setTextColour(MyGUI::Colour(0.85f, 0.85f, 0.85f, 1.0f));
                widget->eventMouseWheel += MyGUI::newDelegate(&OnHubMouseWheel);
                g_dynamic_widgets.push_back(widget);
                return widget;
            }
        }
        catch (...)
        {
        }
    }

    if (!g_logged_missing_edit_box_skin)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create EditBox for Mod Hub (no compatible skin found)");
        g_logged_missing_edit_box_skin = true;
    }

    return 0;
}

MyGUI::ScrollBar* CreateTrackedScrollBar(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    const char* skins[] = {
        "VScroll",
        "VScrollBar",
        "ScrollBarV",
        "ScrollBar"
    };

    for (size_t index = 0; index < sizeof(skins) / sizeof(skins[0]); ++index)
    {
        try
        {
            MyGUI::ScrollBar* widget = parent->createWidget<MyGUI::ScrollBar>(skins[index], coord, MyGUI::Align::Default);
            if (widget != 0)
            {
                widget->setVerticalAlignment(true);
                g_dynamic_widgets.push_back(widget);
                return widget;
            }
        }
        catch (...)
        {
        }
    }

    if (!g_logged_missing_scrollbar_skin)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create ScrollBar for Mod Hub (no compatible skin found)");
        g_logged_missing_scrollbar_skin = true;
    }

    return 0;
}

void AttachHubAction(MyGUI::Widget* widget, const char* action, const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id)
{
    if (widget == 0)
    {
        return;
    }

    widget->setUserString("emc_action", action != 0 ? action : "");
    widget->setUserString("emc_ns", namespace_id);
    widget->setUserString("emc_mod", mod_id);
    widget->setUserString("emc_setting", setting_id);
}

void AttachHubIdentity(MyGUI::Widget* widget, const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id)
{
    if (widget == 0)
    {
        return;
    }

    widget->setUserString("emc_ns", namespace_id);
    widget->setUserString("emc_mod", mod_id);
    widget->setUserString("emc_setting", setting_id);
}

void FindOrAddRenderNamespace(std::vector<RenderNamespaceGroup>* namespaces, const HubUiRowView& row, RenderNamespaceGroup** out_namespace)
{
    if (namespaces == 0 || out_namespace == 0)
    {
        return;
    }

    for (size_t index = 0; index < namespaces->size(); ++index)
    {
        RenderNamespaceGroup* existing = &(*namespaces)[index];
        if (existing->namespace_id == row.namespace_id)
        {
            *out_namespace = existing;
            return;
        }
    }

    RenderNamespaceGroup new_namespace;
    new_namespace.namespace_id = row.namespace_id != 0 ? row.namespace_id : "";
    new_namespace.namespace_display_name = row.namespace_display_name != 0 ? row.namespace_display_name : new_namespace.namespace_id;
    namespaces->push_back(new_namespace);
    *out_namespace = &namespaces->back();
}

void FindOrAddRenderMod(RenderNamespaceGroup* namespace_group, const HubUiRowView& row, RenderModGroup** out_mod)
{
    if (namespace_group == 0 || out_mod == 0)
    {
        return;
    }

    for (size_t index = 0; index < namespace_group->mods.size(); ++index)
    {
        RenderModGroup* existing = &namespace_group->mods[index];
        if (existing->mod_id == row.mod_id)
        {
            *out_mod = existing;
            return;
        }
    }

    RenderModGroup new_mod;
    new_mod.mod_id = row.mod_id != 0 ? row.mod_id : "";
    new_mod.mod_display_name = row.mod_display_name != 0 ? row.mod_display_name : new_mod.mod_id;
    new_mod.collapsed = false;
    HubUi_GetModCollapsed(row.namespace_id, row.mod_id, &new_mod.collapsed);
    namespace_group->mods.push_back(new_mod);
    *out_mod = &namespace_group->mods.back();
}

void BuildRenderNamespaces(std::vector<RenderNamespaceGroup>* out_namespaces)
{
    if (out_namespaces == 0)
    {
        return;
    }

    out_namespaces->clear();
    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        RenderNamespaceGroup* namespace_group = 0;
        FindOrAddRenderNamespace(out_namespaces, row, &namespace_group);
        if (namespace_group == 0)
        {
            continue;
        }

        RenderModGroup* mod_group = 0;
        FindOrAddRenderMod(namespace_group, row, &mod_group);
        if (mod_group == 0)
        {
            continue;
        }

        mod_group->rows.push_back(row);
    }
}

std::string FormatBoolButtonCaption(const HubUiRowView& row)
{
    if (row.pending_bool_value == 0)
    {
        return "Off";
    }

    return "On";
}

std::string FormatKeybindButtonCaption(const HubUiRowView& row)
{
    if (row.capture_active)
    {
        return "Press key...";
    }

    if (row.pending_keybind_value.keycode == EMC_KEY_UNBOUND)
    {
        return "Unbound";
    }

    std::string key_name;
    switch (row.pending_keybind_value.keycode)
    {
    case OIS::KC_ESCAPE: key_name = "Esc"; break;
    case OIS::KC_TAB: key_name = "Tab"; break;
    case OIS::KC_RETURN: key_name = "Enter"; break;
    case OIS::KC_SPACE: key_name = "Space"; break;
    case OIS::KC_BACK: key_name = "Backspace"; break;
    case OIS::KC_LSHIFT: key_name = "LShift"; break;
    case OIS::KC_RSHIFT: key_name = "RShift"; break;
    case OIS::KC_LCONTROL: key_name = "LCtrl"; break;
    case OIS::KC_RCONTROL: key_name = "RCtrl"; break;
    case OIS::KC_LMENU: key_name = "LAlt"; break;
    case OIS::KC_RMENU: key_name = "RAlt"; break;
    case OIS::KC_F1: key_name = "F1"; break;
    case OIS::KC_F2: key_name = "F2"; break;
    case OIS::KC_F3: key_name = "F3"; break;
    case OIS::KC_F4: key_name = "F4"; break;
    case OIS::KC_F5: key_name = "F5"; break;
    case OIS::KC_F6: key_name = "F6"; break;
    case OIS::KC_F7: key_name = "F7"; break;
    case OIS::KC_F8: key_name = "F8"; break;
    case OIS::KC_F9: key_name = "F9"; break;
    case OIS::KC_F10: key_name = "F10"; break;
    case OIS::KC_F11: key_name = "F11"; break;
    case OIS::KC_F12: key_name = "F12"; break;
    case OIS::KC_1: key_name = "1"; break;
    case OIS::KC_2: key_name = "2"; break;
    case OIS::KC_3: key_name = "3"; break;
    case OIS::KC_4: key_name = "4"; break;
    case OIS::KC_5: key_name = "5"; break;
    case OIS::KC_6: key_name = "6"; break;
    case OIS::KC_7: key_name = "7"; break;
    case OIS::KC_8: key_name = "8"; break;
    case OIS::KC_9: key_name = "9"; break;
    case OIS::KC_0: key_name = "0"; break;
    case OIS::KC_A: key_name = "A"; break;
    case OIS::KC_B: key_name = "B"; break;
    case OIS::KC_C: key_name = "C"; break;
    case OIS::KC_D: key_name = "D"; break;
    case OIS::KC_E: key_name = "E"; break;
    case OIS::KC_F: key_name = "F"; break;
    case OIS::KC_G: key_name = "G"; break;
    case OIS::KC_H: key_name = "H"; break;
    case OIS::KC_I: key_name = "I"; break;
    case OIS::KC_J: key_name = "J"; break;
    case OIS::KC_K: key_name = "K"; break;
    case OIS::KC_L: key_name = "L"; break;
    case OIS::KC_M: key_name = "M"; break;
    case OIS::KC_N: key_name = "N"; break;
    case OIS::KC_O: key_name = "O"; break;
    case OIS::KC_P: key_name = "P"; break;
    case OIS::KC_Q: key_name = "Q"; break;
    case OIS::KC_R: key_name = "R"; break;
    case OIS::KC_S: key_name = "S"; break;
    case OIS::KC_T: key_name = "T"; break;
    case OIS::KC_U: key_name = "U"; break;
    case OIS::KC_V: key_name = "V"; break;
    case OIS::KC_W: key_name = "W"; break;
    case OIS::KC_X: key_name = "X"; break;
    case OIS::KC_Y: key_name = "Y"; break;
    case OIS::KC_Z: key_name = "Z"; break;
    default:
        break;
    }

    if (key_name.empty())
    {
        if (row.pending_keybind_value.keycode >= 32 && row.pending_keybind_value.keycode <= 126)
        {
            key_name.assign(1, static_cast<char>(row.pending_keybind_value.keycode));
        }
        else
        {
            std::ostringstream fallback;
            fallback << "Key " << row.pending_keybind_value.keycode;
            key_name = fallback.str();
        }
    }

    std::ostringstream caption;
    caption << key_name;
    if (row.pending_keybind_value.modifiers != 0u)
    {
        caption << " +" << row.pending_keybind_value.modifiers;
    }
    return caption.str();
}

bool TryFindRowViewById(const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id, HubUiRowView* out_row)
{
    if (out_row == 0)
    {
        return false;
    }

    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        if (namespace_id == (row.namespace_id != 0 ? row.namespace_id : "")
            && mod_id == (row.mod_id != 0 ? row.mod_id : "")
            && setting_id == (row.setting_id != 0 ? row.setting_id : ""))
        {
            *out_row = row;
            return true;
        }
    }

    return false;
}

void EnsureSelectedNamespace(const std::vector<RenderNamespaceGroup>& namespaces)
{
    if (namespaces.empty())
    {
        g_selected_namespace_id.clear();
        return;
    }

    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        if (namespaces[index].namespace_id == g_selected_namespace_id)
        {
            return;
        }
    }

    g_selected_namespace_id = namespaces[0].namespace_id;
}

void RebuildHubPanelWidgets();
void SetHubScrollOffset(int offset);

void OnHubSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    if (namespace_id.empty())
    {
        return;
    }

    const std::string query = sender->getOnlyText().asUTF8();
    const char* existing_query = "";
    if (HubUi_GetNamespaceSearchQuery(namespace_id.c_str(), &existing_query)
        && existing_query != 0
        && query == existing_query)
    {
        return;
    }

    if (HubUi_SetNamespaceSearchQuery(namespace_id.c_str(), query.c_str()) == EMC_OK)
    {
        g_hub_scroll_offset = 0;
        g_restore_search_focus_after_rebuild = true;
        g_restore_search_focus_namespace_id = namespace_id;
        RebuildHubPanelWidgets();
    }
}

void NormalizeHubIntEditText(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    HubUiRowView before_row;
    const bool have_before_row = TryFindRowViewById(namespace_id, mod_id, setting_id, &before_row);
    const std::string previous_text = have_before_row && before_row.pending_int_text != 0 ? before_row.pending_int_text : "";
    const bool previous_error = have_before_row ? before_row.int_text_parse_error : false;

    if (HubUi_NormalizePendingIntText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str()) != EMC_OK)
    {
        return;
    }

    HubUiRowView after_row;
    if (!TryFindRowViewById(namespace_id, mod_id, setting_id, &after_row))
    {
        return;
    }

    const std::string normalized_text = after_row.pending_int_text != 0 ? after_row.pending_int_text : "";
    if (previous_error || previous_text != normalized_text)
    {
        sender->setOnlyText(normalized_text);
    }
}

void NormalizeHubFloatEditText(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    HubUiRowView before_row;
    const bool have_before_row = TryFindRowViewById(namespace_id, mod_id, setting_id, &before_row);
    const std::string previous_text = have_before_row && before_row.pending_float_text != 0 ? before_row.pending_float_text : "";
    const bool previous_error = have_before_row ? before_row.float_text_parse_error : false;

    if (HubUi_NormalizePendingFloatText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str()) != EMC_OK)
    {
        return;
    }

    HubUiRowView after_row;
    if (!TryFindRowViewById(namespace_id, mod_id, setting_id, &after_row))
    {
        return;
    }

    const std::string normalized_text = after_row.pending_float_text != 0 ? after_row.pending_float_text : "";
    if (previous_error || previous_text != normalized_text)
    {
        sender->setOnlyText(normalized_text);
    }
}

void OnHubIntTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    const std::string text = sender->getOnlyText().asUTF8();

    HubUiRowView row;
    if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row)
        && row.pending_int_text != 0
        && text == row.pending_int_text)
    {
        return;
    }

    HubUi_SetPendingIntFromText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), text.c_str());
}

void OnHubIntEditAccepted(MyGUI::EditBox* sender)
{
    NormalizeHubIntEditText(sender);
}

void OnHubIntEditLostFocus(MyGUI::Widget* sender, MyGUI::Widget*)
{
    NormalizeHubIntEditText(sender != 0 ? sender->castType<MyGUI::EditBox>(false) : 0);
}

void OnHubFloatTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    const std::string text = sender->getOnlyText().asUTF8();

    HubUiRowView row;
    if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row)
        && row.pending_float_text != 0
        && text == row.pending_float_text)
    {
        return;
    }

    HubUi_SetPendingFloatFromText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), text.c_str());
}

void OnHubFloatEditAccepted(MyGUI::EditBox* sender)
{
    NormalizeHubFloatEditText(sender);
}

void OnHubFloatEditLostFocus(MyGUI::Widget* sender, MyGUI::Widget*)
{
    NormalizeHubFloatEditText(sender != 0 ? sender->castType<MyGUI::EditBox>(false) : 0);
}

void OnHubMouseWheel(MyGUI::Widget*, int rel)
{
    if (!g_hub_enabled || !HubUi_IsOptionsWindowOpen() || g_hub_scroll_max_offset <= 0 || rel == 0)
    {
        return;
    }

    const int previous_offset = g_hub_scroll_offset;
    if (rel > 0)
    {
        SetHubScrollOffset(g_hub_scroll_offset - kHubScrollLineStep);
    }
    else
    {
        SetHubScrollOffset(g_hub_scroll_offset + kHubScrollLineStep);
    }

    if (g_hub_scroll_offset != previous_offset)
    {
        RebuildHubPanelWidgets();
    }
}

void OnHubScrollBarPositionChanged(MyGUI::ScrollBar*, size_t position)
{
    if (g_ignore_scrollbar_position_event)
    {
        return;
    }

    const int previous_offset = g_hub_scroll_offset;
    SetHubScrollOffset(static_cast<int>(position));
    if (g_hub_scroll_offset != previous_offset)
    {
        RebuildHubPanelWidgets();
    }
}

void OnHubButtonClicked(MyGUI::Widget* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string action = sender->getUserString("emc_action");
    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");

    if (action == "scroll_line_up")
    {
        SetHubScrollOffset(g_hub_scroll_offset - kHubScrollLineStep);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "scroll_line_down")
    {
        SetHubScrollOffset(g_hub_scroll_offset + kHubScrollLineStep);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "scroll_page_up")
    {
        SetHubScrollOffset(g_hub_scroll_offset - g_hub_scroll_page_step);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "scroll_page_down")
    {
        SetHubScrollOffset(g_hub_scroll_offset + g_hub_scroll_page_step);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "namespace_select")
    {
        g_selected_namespace_id = namespace_id;
        g_hub_scroll_offset = 0;
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "mod_toggle")
    {
        bool collapsed = false;
        if (HubUi_GetModCollapsed(namespace_id.c_str(), mod_id.c_str(), &collapsed))
        {
            HubUi_SetModCollapsed(namespace_id.c_str(), mod_id.c_str(), !collapsed);
        }
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "bool_toggle")
    {
        HubUiRowView row;
        if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row))
        {
            const int32_t next_value = row.pending_bool_value == 0 ? 1 : 0;
            HubUi_SetPendingBool(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), next_value);
        }
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "keybind_bind")
    {
        HubUi_BeginKeybindCapture(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "keybind_clear")
    {
        HubUi_ClearPendingKeybind(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_dec_10")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -10);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_dec_5")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -5);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_dec")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_inc")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_inc_5")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 5);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_inc_10")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 10);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "float_step_dec")
    {
        HubUi_AdjustPendingFloatStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "float_step_inc")
    {
        HubUi_AdjustPendingFloatStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "action_invoke")
    {
        HubUi_InvokeActionRow(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
    }
}

void CreateInlineErrorLabel(MyGUI::Widget* parent, int x, int y, int width, const char* message)
{
    if (parent == 0 || message == 0 || message[0] == '\0')
    {
        return;
    }

    MyGUI::TextBox* error_text = CreateTrackedWidget<MyGUI::TextBox>(
        parent,
        kTextSkin,
        MyGUI::IntCoord(x, y, width, 18));
    if (error_text == 0)
    {
        return;
    }

    error_text->setCaption(message);
    error_text->setTextColour(MyGUI::Colour(1.0f, 0.3f, 0.3f, 1.0f));
}

std::string FormatHubRangeFloatValue(float value, uint32_t display_decimals)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(static_cast<std::streamsize>(display_decimals));
    stream << value;
    return stream.str();
}

std::string BuildNumericRangeHint(const HubUiRowView& row)
{
    std::ostringstream hint;
    hint << "Allowed range: ";

    if (row.kind == HUB_UI_ROW_KIND_INT)
    {
        hint << row.int_min_value << " to " << row.int_max_value;
        return hint.str();
    }

    if (row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        hint << FormatHubRangeFloatValue(row.float_min_value, row.float_display_decimals)
             << " to "
             << FormatHubRangeFloatValue(row.float_max_value, row.float_display_decimals);
        return hint.str();
    }

    return std::string();
}

void CreateNumericRangeHintLabel(MyGUI::Widget* parent, int x, int y, int width, const HubUiRowView& row)
{
    if (parent == 0)
    {
        return;
    }

    const std::string hint = BuildNumericRangeHint(row);
    if (hint.empty())
    {
        return;
    }

    MyGUI::TextBox* hint_text = CreateTrackedWidget<MyGUI::TextBox>(
        parent,
        kTextSkin,
        MyGUI::IntCoord(x, y, width, 18));
    if (hint_text == 0)
    {
        return;
    }

    hint_text->setCaption(hint);
    hint_text->setTextColour(MyGUI::Colour(0.65f, 0.65f, 0.65f, 1.0f));
}

void CreateRowWidgets(MyGUI::Widget* parent, int panel_width, int y, const HubUiRowView& row, int* out_next_y)
{
    if (parent == 0 || out_next_y == 0)
    {
        return;
    }

    const int label_x = 34;
    int label_width = panel_width - 290;
    if (row.kind == HUB_UI_ROW_KIND_INT)
    {
        label_width = panel_width - 430;
    }
    if (label_width < 140)
    {
        label_width = 140;
    }
    const int value_x = panel_width - 240;
    const int row_height = 34;

    MyGUI::TextBox* label = CreateTrackedWidget<MyGUI::TextBox>(
        parent,
        kTextSkin,
        MyGUI::IntCoord(label_x, y + 6, label_width, 22));
    if (label != 0)
    {
        label->setCaption(row.label != 0 ? row.label : row.setting_id);
    }

    if (row.kind == HUB_UI_ROW_KIND_BOOL)
    {
        MyGUI::Button* button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x, y, 200, 30));
        if (button != 0)
        {
            button->setCaption(FormatBoolButtonCaption(row));
            button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(button, "bool_toggle", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_KEYBIND)
    {
        MyGUI::Button* bind_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x - 70, y, 180, 30));
        if (bind_button != 0)
        {
            bind_button->setCaption(FormatKeybindButtonCaption(row));
            bind_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(bind_button, "keybind_bind", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }

        MyGUI::Button* clear_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 120, y, 80, 30));
        if (clear_button != 0)
        {
            clear_button->setCaption("Clear");
            clear_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(clear_button, "keybind_clear", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_INT)
    {
        const int button_width = 44;
        const int button_height = 34;
        const int button_gap = 4;
        const int value_box_width = 96;
        const int group_width = (button_width * 6) + (button_gap * 6) + value_box_width;
        int control_x = panel_width - 24 - group_width;
        if (control_x < value_x - 190)
        {
            control_x = value_x - 190;
        }

        MyGUI::Button* minus_ten_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (minus_ten_button != 0)
        {
            minus_ten_button->setCaption("-10");
            minus_ten_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_ten_button, "int_step_dec_10", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
        control_x += button_width + button_gap;

        MyGUI::Button* minus_five_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (minus_five_button != 0)
        {
            minus_five_button->setCaption("-5");
            minus_five_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_five_button, "int_step_dec_5", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
        control_x += button_width + button_gap;

        MyGUI::Button* minus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (minus_button != 0)
        {
            minus_button->setCaption("-");
            minus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_button, "int_step_dec", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
        control_x += button_width + button_gap;

        MyGUI::EditBox* value_box = CreateTrackedSearchBox(
            parent,
            MyGUI::IntCoord(control_x, y, value_box_width, button_height));
        if (value_box != 0)
        {
            value_box->setEditMultiLine(false);
            value_box->setOnlyText(row.pending_int_text != 0 ? row.pending_int_text : "");
            AttachHubIdentity(value_box, row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
            value_box->eventEditTextChange += MyGUI::newDelegate(&OnHubIntTextChanged);
            value_box->eventEditSelectAccept += MyGUI::newDelegate(&OnHubIntEditAccepted);
            value_box->eventKeyLostFocus += MyGUI::newDelegate(&OnHubIntEditLostFocus);
        }
        control_x += value_box_width + button_gap;

        MyGUI::Button* plus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (plus_button != 0)
        {
            plus_button->setCaption("+");
            plus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_button, "int_step_inc", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
        control_x += button_width + button_gap;

        MyGUI::Button* plus_five_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (plus_five_button != 0)
        {
            plus_five_button->setCaption("+5");
            plus_five_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_five_button, "int_step_inc_5", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
        control_x += button_width + button_gap;

        MyGUI::Button* plus_ten_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, y, button_width, button_height));
        if (plus_ten_button != 0)
        {
            plus_ten_button->setCaption("+10");
            plus_ten_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_ten_button, "int_step_inc_10", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        MyGUI::Button* minus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x - 78, y, 44, 34));
        if (minus_button != 0)
        {
            minus_button->setCaption("-");
            minus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_button, "float_step_dec", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }

        MyGUI::EditBox* value_box = CreateTrackedSearchBox(
            parent,
            MyGUI::IntCoord(value_x - 28, y, 124, 34));
        if (value_box != 0)
        {
            value_box->setEditMultiLine(false);
            value_box->setOnlyText(row.pending_float_text != 0 ? row.pending_float_text : "");
            AttachHubIdentity(value_box, row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
            value_box->eventEditTextChange += MyGUI::newDelegate(&OnHubFloatTextChanged);
            value_box->eventEditSelectAccept += MyGUI::newDelegate(&OnHubFloatEditAccepted);
            value_box->eventKeyLostFocus += MyGUI::newDelegate(&OnHubFloatEditLostFocus);
        }

        MyGUI::Button* plus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 102, y, 44, 34));
        if (plus_button != 0)
        {
            plus_button->setCaption("+");
            plus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_button, "float_step_inc", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_ACTION)
    {
        MyGUI::Button* action_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 40, y, 160, 30));
        if (action_button != 0)
        {
            action_button->setCaption("Run");
            action_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(action_button, "action_invoke", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }

    int next_y = y + row_height;
    if (row.kind == HUB_UI_ROW_KIND_INT || row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        CreateNumericRangeHintLabel(parent, label_x + 12, next_y, panel_width - 80, row);
        next_y += 20;
    }

    if (row.inline_error != 0 && row.inline_error[0] != '\0' && std::strcmp(row.inline_error, "none") != 0)
    {
        CreateInlineErrorLabel(parent, label_x + 12, next_y, panel_width - 80, row.inline_error);
        next_y += 20;
    }

    *out_next_y = next_y;
}

void BuildFilteredModsForNamespace(const RenderNamespaceGroup& namespace_group, std::vector<RenderModGroup>* out_mods)
{
    if (out_mods == 0)
    {
        return;
    }

    out_mods->clear();
    for (size_t mod_index = 0; mod_index < namespace_group.mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = namespace_group.mods[mod_index];
        RenderModGroup filtered_mod = mod_group;
        filtered_mod.rows.clear();

        for (size_t row_index = 0; row_index < mod_group.rows.size(); ++row_index)
        {
            const HubUiRowView& row = mod_group.rows[row_index];
            if (HubUi_DoesRowMatchNamespaceSearch(&row))
            {
                filtered_mod.rows.push_back(row);
            }
        }

        if (!filtered_mod.rows.empty())
        {
            out_mods->push_back(filtered_mod);
        }
    }
}

int ClampInt(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

int MeasureRowBlockHeight(const HubUiRowView& row)
{
    int height = 34;
    if (row.kind == HUB_UI_ROW_KIND_INT || row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        height += 20;
    }

    if (row.inline_error != 0 && row.inline_error[0] != '\0' && std::strcmp(row.inline_error, "none") != 0)
    {
        height += 20;
    }

    return height;
}

int MeasureFilteredModsContentHeight(const std::vector<RenderModGroup>& filtered_mods)
{
    int total_height = 0;
    for (size_t mod_index = 0; mod_index < filtered_mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = filtered_mods[mod_index];
        total_height += 38;
        if (mod_group.collapsed)
        {
            continue;
        }

        for (size_t row_index = 0; row_index < mod_group.rows.size(); ++row_index)
        {
            total_height += MeasureRowBlockHeight(mod_group.rows[row_index]);
        }

        total_height += 8;
    }

    return total_height;
}

void ConfigureHubScrollRange(int content_height, int viewport_height)
{
    if (viewport_height <= 0)
    {
        g_hub_scroll_max_offset = 0;
        g_hub_scroll_offset = 0;
        g_hub_scroll_page_step = 160;
        return;
    }

    const int overflow = content_height - viewport_height;
    g_hub_scroll_max_offset = overflow > 0 ? overflow : 0;
    g_hub_scroll_offset = ClampInt(g_hub_scroll_offset, 0, g_hub_scroll_max_offset);

    int page_step = viewport_height - 32;
    if (page_step < 120)
    {
        page_step = 120;
    }
    g_hub_scroll_page_step = page_step;
}

void SetHubScrollOffset(int offset)
{
    g_hub_scroll_offset = ClampInt(offset, 0, g_hub_scroll_max_offset);
}

bool IsBlockFullyVisible(int block_y, int block_height, int viewport_top, int viewport_bottom)
{
    if (block_height <= 0)
    {
        return false;
    }

    if (block_y < viewport_top)
    {
        return false;
    }

    const int block_bottom = block_y + block_height;
    return block_bottom <= viewport_bottom;
}

void RebuildHubPanelWidgets()
{
    if (g_active_hub_panel_widget == 0)
    {
        return;
    }

    DestroyDynamicWidgets();

    std::vector<RenderNamespaceGroup> namespaces;
    BuildRenderNamespaces(&namespaces);
    EnsureSelectedNamespace(namespaces);

    const int panel_width = g_active_hub_panel_widget->getWidth();
    const int panel_height = g_active_hub_panel_widget->getHeight();
    int y = 12;

    if (namespaces.empty())
    {
        g_hub_scroll_offset = 0;
        g_hub_scroll_max_offset = 0;
        MyGUI::TextBox* empty_text = CreateTrackedWidget<MyGUI::TextBox>(
            g_active_hub_panel_widget,
            kTextSkin,
            MyGUI::IntCoord(24, y, panel_width - 48, 28));
        if (empty_text != 0)
        {
            empty_text->setCaption("No registered Mod Hub settings.");
        }
        return;
    }

    int namespace_x = 20;
    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        const RenderNamespaceGroup& namespace_group = namespaces[index];
        MyGUI::Button* namespace_button = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kNamespaceButtonSkin,
            MyGUI::IntCoord(namespace_x, y, 170, 30));
        if (namespace_button != 0)
        {
            std::string caption = namespace_group.namespace_display_name;
            if (namespace_group.namespace_id == g_selected_namespace_id)
            {
                caption = "[" + caption + "]";
            }
            namespace_button->setCaption(caption);
            namespace_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(namespace_button, "namespace_select", namespace_group.namespace_id, "", "");
        }

        namespace_x += 178;
    }

    y += 56;

    const RenderNamespaceGroup* selected_namespace = 0;
    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        if (namespaces[index].namespace_id == g_selected_namespace_id)
        {
            selected_namespace = &namespaces[index];
            break;
        }
    }

    if (selected_namespace == 0)
    {
        selected_namespace = &namespaces[0];
    }

    const char* search_query = "";
    HubUi_GetNamespaceSearchQuery(selected_namespace->namespace_id.c_str(), &search_query);
    const bool should_restore_search_focus =
        g_restore_search_focus_after_rebuild
        && selected_namespace->namespace_id == g_restore_search_focus_namespace_id;
    g_restore_search_focus_after_rebuild = false;
    g_restore_search_focus_namespace_id.clear();

    MyGUI::TextBox* search_label = CreateTrackedWidget<MyGUI::TextBox>(
        g_active_hub_panel_widget,
        kTextSkin,
        MyGUI::IntCoord(24, y + 10, 70, 24));
    if (search_label != 0)
    {
        search_label->setCaption("Search:");
    }

    int search_width = (panel_width - 120) / 2;
    if (search_width < 220)
    {
        search_width = 220;
    }

    MyGUI::EditBox* search_box = CreateTrackedSearchBox(
        g_active_hub_panel_widget,
        MyGUI::IntCoord(96, y, search_width, 40));
    if (search_box != 0)
    {
        search_box->setEditMultiLine(false);
        search_box->setOnlyText(search_query != 0 ? search_query : "");
        search_box->setUserString("emc_ns", selected_namespace->namespace_id);
        search_box->eventEditTextChange += MyGUI::newDelegate(&OnHubSearchTextChanged);
        if (should_restore_search_focus)
        {
            MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
            if (input != 0)
            {
                input->setKeyFocusWidget(search_box);
            }
        }
    }

    const int content_top = y + 50;
    int content_bottom = panel_height - 12;
    if (content_bottom <= content_top)
    {
        content_bottom = content_top + 1;
    }
    const int viewport_height = content_bottom - content_top;

    std::vector<RenderModGroup> filtered_mods;
    BuildFilteredModsForNamespace(*selected_namespace, &filtered_mods);
    if (filtered_mods.empty())
    {
        ConfigureHubScrollRange(0, viewport_height);
        MyGUI::TextBox* no_matches_text = CreateTrackedWidget<MyGUI::TextBox>(
            g_active_hub_panel_widget,
            kTextSkin,
            MyGUI::IntCoord(24, content_top, panel_width - 48, 28));
        if (no_matches_text != 0)
        {
            no_matches_text->setCaption(kNoMatchesText);
        }
        return;
    }

    const int content_total_height = MeasureFilteredModsContentHeight(filtered_mods);
    ConfigureHubScrollRange(content_total_height, viewport_height);

    const bool show_scroll_controls = g_hub_scroll_max_offset > 0;
    int content_panel_width = panel_width;
    if (show_scroll_controls)
    {
        content_panel_width -= kHubScrollGutterWidth;
    }
    if (content_panel_width < 280)
    {
        content_panel_width = panel_width;
    }

    if (show_scroll_controls)
    {
        const int control_x = content_panel_width + ((panel_width - content_panel_width - kHubScrollControlWidth) / 2);
        const int button_height = 28;
        const int button_gap = 4;
        const int top_line_y = content_top;
        const int bottom_line_y = content_bottom - button_height;

        MyGUI::Button* line_up_button = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, top_line_y, kHubScrollControlWidth, button_height));
        if (line_up_button != 0)
        {
            line_up_button->setCaption("^");
            line_up_button->setEnabled(g_hub_scroll_offset > 0);
            line_up_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(line_up_button, "scroll_line_up", "", "", "");
        }

        MyGUI::Button* line_down_button = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kValueButtonSkin,
            MyGUI::IntCoord(control_x, bottom_line_y, kHubScrollControlWidth, button_height));
        if (line_down_button != 0)
        {
            line_down_button->setCaption("v");
            line_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
            line_down_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(line_down_button, "scroll_line_down", "", "", "");
        }

        const int scroll_bar_y = top_line_y + button_height + button_gap;
        int scroll_bar_height = bottom_line_y - button_gap - scroll_bar_y;
        if (scroll_bar_height < 40)
        {
            scroll_bar_height = 0;
        }

        bool has_scrollbar = false;
        if (scroll_bar_height > 0)
        {
            MyGUI::ScrollBar* scroll_bar = CreateTrackedScrollBar(
                g_active_hub_panel_widget,
                MyGUI::IntCoord(control_x, scroll_bar_y, kHubScrollControlWidth, scroll_bar_height));
            if (scroll_bar != 0)
            {
                const size_t range = static_cast<size_t>(g_hub_scroll_max_offset + 1);
                const size_t line_step = static_cast<size_t>(kHubScrollLineStep);
                size_t page_step = static_cast<size_t>(g_hub_scroll_page_step);
                if (page_step == 0u)
                {
                    page_step = line_step;
                }

                g_ignore_scrollbar_position_event = true;
                scroll_bar->setScrollRange(range);
                scroll_bar->setScrollPage(line_step);
                scroll_bar->setScrollViewPage(page_step);
                scroll_bar->setScrollWheelPage(line_step);
                scroll_bar->setScrollPosition(static_cast<size_t>(g_hub_scroll_offset));
                g_ignore_scrollbar_position_event = false;

                scroll_bar->eventScrollChangePosition += MyGUI::newDelegate(&OnHubScrollBarPositionChanged);
                has_scrollbar = true;
            }
        }

        if (!has_scrollbar)
        {
            const int top_page_y = content_top + 34;
            const int bottom_page_y = content_bottom - 62;

            MyGUI::Button* page_up_button = CreateTrackedWidget<MyGUI::Button>(
                g_active_hub_panel_widget,
                kValueButtonSkin,
                MyGUI::IntCoord(control_x, top_page_y, kHubScrollControlWidth, button_height));
            if (page_up_button != 0)
            {
                page_up_button->setCaption("Pg^");
                page_up_button->setEnabled(g_hub_scroll_offset > 0);
                page_up_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                AttachHubAction(page_up_button, "scroll_page_up", "", "", "");
            }

            MyGUI::Button* page_down_button = CreateTrackedWidget<MyGUI::Button>(
                g_active_hub_panel_widget,
                kValueButtonSkin,
                MyGUI::IntCoord(control_x, bottom_page_y, kHubScrollControlWidth, button_height));
            if (page_down_button != 0)
            {
                page_down_button->setCaption("Pgv");
                page_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
                page_down_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                AttachHubAction(page_down_button, "scroll_page_down", "", "", "");
            }
        }
    }

    int logical_y = 0;
    for (size_t mod_index = 0; mod_index < filtered_mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = filtered_mods[mod_index];
        const int header_height = 32;
        const int header_y = content_top + logical_y - g_hub_scroll_offset;

        if (IsBlockFullyVisible(header_y, header_height, content_top, content_bottom))
        {
            MyGUI::Button* mod_header = CreateTrackedWidget<MyGUI::Button>(
                g_active_hub_panel_widget,
                kHeaderButtonSkin,
                MyGUI::IntCoord(20, header_y, content_panel_width - 40, header_height));
            if (mod_header != 0)
            {
                const std::string prefix = mod_group.collapsed ? "[+] " : "[-] ";
                mod_header->setCaption(prefix + mod_group.mod_display_name);
                mod_header->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                AttachHubAction(mod_header, "mod_toggle", selected_namespace->namespace_id, mod_group.mod_id, "");
            }
        }

        logical_y += 38;

        if (mod_group.collapsed)
        {
            continue;
        }

        for (size_t row_index = 0; row_index < mod_group.rows.size(); ++row_index)
        {
            const HubUiRowView& row = mod_group.rows[row_index];
            const int row_height = MeasureRowBlockHeight(row);
            const int row_y = content_top + logical_y - g_hub_scroll_offset;
            if (IsBlockFullyVisible(row_y, row_height, content_top, content_bottom))
            {
                int next_y = row_y;
                CreateRowWidgets(g_active_hub_panel_widget, content_panel_width, row_y, row, &next_y);
            }
            logical_y += row_height;
        }

        logical_y += 8;
    }
}

void ClearActiveUiState()
{
    DestroyDynamicWidgets();
    g_selected_namespace_id.clear();
    g_hub_scroll_offset = 0;
    g_hub_scroll_max_offset = 0;
    g_hub_scroll_page_step = 160;
    g_active_hub_panel_widget = 0;
    g_active_hub_panel = 0;
    g_active_options_window = 0;
}

bool EnsureHubPanel(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0 || g_ptrKenshiGUI == 0 || g_fnCreateDatapanel == 0)
    {
        return false;
    }

    if (self->optionsTab->findItemWith(kHubTabName))
    {
        return g_active_hub_panel_widget != 0;
    }

    MyGUI::TabItem* hub_tab = self->optionsTab->addItem(kHubTabName);
    if (hub_tab == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to add Mod Hub tab item");
        return false;
    }

    DatapanelGUI* hub_panel = g_fnCreateDatapanel(g_ptrKenshiGUI, kHubPanelName, hub_tab, false);
    if (hub_panel == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create Mod Hub datapanel");
        return false;
    }

    hub_panel->vfunc0xc0(kHubPanelLineId);
    hub_panel->vfunc0xe0(25.0f);

    g_active_options_window = self;
    g_active_hub_panel = hub_panel;
    g_active_hub_panel_widget = hub_panel->getWidget();
    if (g_active_hub_panel_widget == 0)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub panel widget is null");
        return false;
    }
    g_active_hub_panel_widget->eventMouseWheel += MyGUI::newDelegate(&OnHubMouseWheel);

    hub_tab->setVisible(false);
    self->optionsTab->setItemData(hub_tab, hub_panel);
    return true;
}

void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig != 0)
    {
        g_fnOptionsInitOrig(self);
    }

    HubMenuBridge_OnOptionsWindowInit();
    if (!g_hub_enabled)
    {
        return;
    }

    if (!EnsureHubPanel(self))
    {
        return;
    }

    RebuildHubPanelWidgets();
}

void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig != 0)
    {
        g_fnOptionsSaveOrig(self);
    }

    HubMenuBridge_OnOptionsWindowSave();
    HubMenuBridge_OnOptionsWindowClose();
    ClearActiveUiState();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode key_code)
{
    if (g_hub_enabled && HubUi_IsOptionsWindowOpen() && HubUi_IsAnyKeybindCaptureActive())
    {
        HubUi_ApplyCapturedKeycodeToActiveRow(static_cast<int32_t>(key_code));
        RebuildHubPanelWidgets();
        return;
    }

    if (g_hub_enabled && HubUi_IsOptionsWindowOpen() && g_active_hub_panel_widget != 0)
    {
        if (key_code == OIS::KC_PGUP)
        {
            SetHubScrollOffset(g_hub_scroll_offset - g_hub_scroll_page_step);
            RebuildHubPanelWidgets();
            return;
        }

        if (key_code == OIS::KC_PGDOWN)
        {
            SetHubScrollOffset(g_hub_scroll_offset + g_hub_scroll_page_step);
            RebuildHubPanelWidgets();
            return;
        }

        if (key_code == OIS::KC_UP)
        {
            SetHubScrollOffset(g_hub_scroll_offset - kHubScrollLineStep);
            RebuildHubPanelWidgets();
            return;
        }

        if (key_code == OIS::KC_DOWN)
        {
            SetHubScrollOffset(g_hub_scroll_offset + kHubScrollLineStep);
            RebuildHubPanelWidgets();
            return;
        }
    }

    if (InputHandler_keyDownEvent_orig != 0)
    {
        InputHandler_keyDownEvent_orig(thisptr, key_code);
    }
}
}

void HubMenuBridge_SetHubEnabled(bool is_enabled)
{
    g_hub_enabled = is_enabled;
    if (!g_hub_enabled)
    {
        HubUi_SetOptionsWindowOpen(false);
        HubUi_ClearSessionModel();
        HubRegistry_SetRegistrationLocked(false);
        ClearActiveUiState();
    }
}

bool HubMenuBridge_IsHubEnabled()
{
    return g_hub_enabled;
}

void HubMenuBridge_SetOptionsWindowInitObserver(HubMenuBridgeOptionsWindowInitObserver observer)
{
    g_options_window_init_observer = observer;
}

bool HubMenuBridge_InstallHooks(unsigned int platform, const std::string& version)
{
    if (g_hooks_installed)
    {
        return true;
    }

    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPluginMenuFunctions(platform, version, base_addr))
    {
        ErrorLog("Emkejs-Mod-Core: failed to initialize menu function pointers");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook options init");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsSave, OptionsWindowSaveHook, &g_fnOptionsSaveOrig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook options save");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook InputHandler::keyDownEvent");
        return false;
    }

    g_hooks_installed = true;
    return true;
}

void HubMenuBridge_OnOptionsWindowInit()
{
    if (g_options_window_init_observer != 0)
    {
        g_options_window_init_observer();
    }

    if (!g_hub_enabled)
    {
        HubUi_SetOptionsWindowOpen(false);
        HubUi_ClearSessionModel();
        HubRegistry_SetRegistrationLocked(false);
        return;
    }

    HubUi_SetOptionsWindowOpen(true);
    HubRegistry_SetRegistrationLocked(true);
    HubUi_RebuildSessionModelFromRegistry();
    HubUi_PerformInitialSync();
}

void HubMenuBridge_OnOptionsWindowSave()
{
    if (!g_hub_enabled || !HubUi_IsOptionsWindowOpen())
    {
        return;
    }

    HubCommit_RunOptionsSave();
}

void HubMenuBridge_OnOptionsWindowClose()
{
    HubUi_SetOptionsWindowOpen(false);
    HubUi_ClearSessionModel();
    HubRegistry_SetRegistrationLocked(false);
}
