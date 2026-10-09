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
#include <mygui/MyGUI_ScrollView.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cstdlib>
#include <cctype>
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
typedef OptionsWindow* (*FnGetOptionsWindow)();
typedef void (*FnOpenOptionsWindow)(OptionsWindow*);

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
const char* kSearchHintText = "Search settings, or use mod:term (example: loot:enable)";
const char* kEmcNamespaceId = "emkej.qol";
const char* kEmcNamespaceDisplayName = "Emkej QoL";
const char* kEmcModId = "emkejs_mod_core";
const char* kEmcModDisplayName = "Emkejs Mod Core";
const char* kEmcConfigSectionName = "mod_hub";
const char* kEmcConfigFileName = "emkejs-mod-core.ini";
const char* kEmcPersistSearchSettingId = "persist_search_until_cleared";
const char* kEmcPersistCollapseSettingId = "persist_collapse_state_until_exit";
const char* kEmcAutoFocusSearchSettingId = "auto_focus_search_on_open";
const char* kEmcOpenModHubKeySettingId = "open_mod_hub_key";
const char* kEmcOpenModHubRequireCtrlSettingId = "open_mod_hub_require_ctrl";
const char* kEmcOpenModHubRequireShiftSettingId = "open_mod_hub_require_shift";
const char* kEmcOpenModHubRequireAltSettingId = "open_mod_hub_require_alt";
const int32_t kDefaultEmcOpenModHubKeycode = static_cast<int32_t>(OIS::KC_M);
const bool kDefaultEmcOpenModHubRequireCtrl = true;
const bool kDefaultEmcOpenModHubRequireShift = false;
const bool kDefaultEmcOpenModHubRequireAlt = false;

bool g_hub_enabled = true;
bool g_hooks_installed = false;
HubMenuBridgeOptionsWindowInitObserver g_options_window_init_observer = 0;

FnCreateDatapanel g_fnCreateDatapanel = 0;
FnOptionsInit g_fnOptionsInit = 0;
FnOptionsSave g_fnOptionsSave = 0;
FnGetOptionsWindow g_fnGetOptionsWindow = 0;
FnOpenOptionsWindow g_fnOpenOptionsWindow = 0;
FnOptionsInit g_fnOptionsInitOrig = 0;
FnOptionsSave g_fnOptionsSaveOrig = 0;
void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;
ForgottenGUI* g_ptrKenshiGUI = 0;

OptionsWindow* g_active_options_window = 0;
DatapanelGUI* g_active_hub_panel = 0;
MyGUI::Widget* g_active_hub_panel_widget = 0;
std::string g_selected_namespace_id;
std::vector<MyGUI::Widget*> g_dynamic_widgets;
bool g_logged_missing_edit_box_skin = false;
bool g_logged_missing_scrollbar_skin = false;
bool g_logged_missing_scroll_view_skin = false;
bool g_emc_config_loaded = false;
bool g_emc_settings_registered = false;
bool g_emc_persist_search_until_cleared = true;
bool g_emc_persist_collapse_state_until_exit = true;
bool g_emc_auto_focus_search_on_open = false;
int32_t g_emc_open_mod_hub_keycode = kDefaultEmcOpenModHubKeycode;
bool g_emc_open_mod_hub_require_ctrl = kDefaultEmcOpenModHubRequireCtrl;
bool g_emc_open_mod_hub_require_shift = kDefaultEmcOpenModHubRequireShift;
bool g_emc_open_mod_hub_require_alt = kDefaultEmcOpenModHubRequireAlt;
std::string g_emc_config_path;
EMC_ModHandle g_emc_mod_handle = 0;
bool g_restore_search_focus_after_rebuild = false;
std::string g_restore_search_focus_namespace_id;
bool g_restore_search_cursor_after_rebuild = false;
std::string g_restore_search_cursor_namespace_id;
size_t g_restore_search_cursor_position = 0;
int g_hub_scroll_offset = 0;
int g_hub_scroll_max_offset = 0;
int g_hub_scroll_page_step = 160;
int g_hub_final_content_height = 0;
int g_hub_final_viewport_height = 0;
bool g_ignore_scrollbar_position_event = false;
MyGUI::EditBox* g_active_hub_search_box = 0;
MyGUI::ScrollView* g_active_hub_scroll_view = 0;
MyGUI::Widget* g_active_hub_scroll_client = 0;
MyGUI::ScrollBar* g_active_hub_scroll_bar = 0;
MyGUI::Button* g_active_hub_scroll_line_up_button = 0;
MyGUI::Button* g_active_hub_scroll_line_down_button = 0;
MyGUI::Button* g_active_hub_scroll_page_up_button = 0;
MyGUI::Button* g_active_hub_scroll_page_down_button = 0;
MyGUI::TabControl* g_bound_options_tab_for_autofocus = 0;
struct PendingHubSearchShortcut
{
    bool active;
    bool rebuild_query;
    int key_value;
    std::string namespace_id;
    std::string query;
    size_t cursor_position;

    PendingHubSearchShortcut()
        : active(false)
        , rebuild_query(false)
        , key_value(0)
        , namespace_id()
        , query()
        , cursor_position(0)
    {
    }
};

PendingHubSearchShortcut g_pending_hub_search_shortcut;
bool g_have_hub_search_snapshot = false;
std::string g_hub_search_snapshot_namespace_id;
std::string g_hub_search_snapshot_query;
size_t g_hub_search_snapshot_cursor_position = 0;
bool g_select_mod_hub_on_next_options_init = false;

const int kHubScrollLineStep = 24;
const int kHubScrollControlWidth = 24;
const int kHubScrollGutterWidth = 56;

struct HubScrollableWidgetState
{
    MyGUI::Widget* widget;
    int left;
    int top;
    int width;
    int height;
};

std::vector<HubScrollableWidgetState> g_hub_scrollable_widgets;

struct HubPanelCreationResult
{
    MyGUI::TabItem* hub_tab;
    DatapanelGUI* hub_panel;
    MyGUI::Widget* hub_panel_widget;

    HubPanelCreationResult()
        : hub_tab(0)
        , hub_panel(0)
        , hub_panel_widget(0)
    {
    }
};

bool ApplyHubScrollOffsetWithoutRebuild(int offset);
bool IsWidgetWithinHubPanel(MyGUI::Widget* widget);
void ApplyHubScrollVisualState();
void TrackHubScrollableWidget(MyGUI::Widget* parent, MyGUI::Widget* widget, const MyGUI::IntCoord& coord);
void OnHubMouseWheel(MyGUI::Widget* sender, int rel);
void OnHubWidgetMouseWheelDebug(MyGUI::Widget* sender, int rel);
void OnHubScrollBarPositionChanged(MyGUI::ScrollBar* sender, size_t position);
void OnHubSearchKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode key_code, MyGUI::Char character);
void OnHubSearchKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode key_code);

void CopyHubErrorMessage(char* err_buf, uint32_t err_buf_size, const char* message)
{
    if (err_buf == 0 || err_buf_size == 0u)
    {
        return;
    }

    err_buf[0] = '\0';
    if (message == 0 || message[0] == '\0')
    {
        return;
    }

    const size_t copy_length = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, message, copy_length);
    err_buf[copy_length] = '\0';
}

bool IsValidEmcOpenModHubKeycode(int32_t keycode)
{
    if (keycode == EMC_KEY_UNBOUND)
    {
        return true;
    }

    if (keycode <= 0)
    {
        return false;
    }

    switch (keycode)
    {
    case OIS::KC_LSHIFT:
    case OIS::KC_RSHIFT:
    case OIS::KC_LCONTROL:
    case OIS::KC_RCONTROL:
    case OIS::KC_LMENU:
    case OIS::KC_RMENU:
        return false;
    default:
        return true;
    }
}

int32_t NormalizeEmcOpenModHubKeycode(int32_t keycode)
{
    if (IsValidEmcOpenModHubKeycode(keycode))
    {
        return keycode;
    }

    return kDefaultEmcOpenModHubKeycode;
}

std::string ResolveEmcConfigPath()
{
    HMODULE module = 0;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ResolveEmcConfigPath),
            &module))
    {
        return "";
    }

    char module_path[MAX_PATH] = {};
    const DWORD module_path_len = GetModuleFileNameA(module, module_path, MAX_PATH);
    if (module_path_len == 0 || module_path_len >= MAX_PATH)
    {
        return "";
    }

    std::string config_path(module_path, module_path_len);
    const size_t separator = config_path.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        return kEmcConfigFileName;
    }

    config_path.erase(separator + 1u);
    config_path += kEmcConfigFileName;
    return config_path;
}

void EnsureEmcConfigLoaded()
{
    if (g_emc_config_loaded)
    {
        HubUi_SetSearchPersistenceEnabled(g_emc_persist_search_until_cleared);
        HubUi_SetCollapsePersistenceEnabled(g_emc_persist_collapse_state_until_exit);
        return;
    }

    g_emc_config_loaded = true;
    g_emc_persist_search_until_cleared = true;
    g_emc_persist_collapse_state_until_exit = true;
    g_emc_auto_focus_search_on_open = false;
    g_emc_open_mod_hub_keycode = kDefaultEmcOpenModHubKeycode;
    g_emc_open_mod_hub_require_ctrl = kDefaultEmcOpenModHubRequireCtrl;
    g_emc_open_mod_hub_require_shift = kDefaultEmcOpenModHubRequireShift;
    g_emc_open_mod_hub_require_alt = kDefaultEmcOpenModHubRequireAlt;
    g_emc_config_path = ResolveEmcConfigPath();
    if (!g_emc_config_path.empty())
    {
        g_emc_persist_search_until_cleared =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcPersistSearchSettingId,
                1,
                g_emc_config_path.c_str()) != 0;
        g_emc_persist_collapse_state_until_exit =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcPersistCollapseSettingId,
                1,
                g_emc_config_path.c_str()) != 0;
        g_emc_auto_focus_search_on_open =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcAutoFocusSearchSettingId,
                0,
                g_emc_config_path.c_str()) != 0;
        g_emc_open_mod_hub_keycode = NormalizeEmcOpenModHubKeycode(
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcOpenModHubKeySettingId,
                kDefaultEmcOpenModHubKeycode,
                g_emc_config_path.c_str()));
        g_emc_open_mod_hub_require_ctrl =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcOpenModHubRequireCtrlSettingId,
                kDefaultEmcOpenModHubRequireCtrl ? 1 : 0,
                g_emc_config_path.c_str()) != 0;
        g_emc_open_mod_hub_require_shift =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcOpenModHubRequireShiftSettingId,
                kDefaultEmcOpenModHubRequireShift ? 1 : 0,
                g_emc_config_path.c_str()) != 0;
        g_emc_open_mod_hub_require_alt =
            GetPrivateProfileIntA(
                kEmcConfigSectionName,
                kEmcOpenModHubRequireAltSettingId,
                kDefaultEmcOpenModHubRequireAlt ? 1 : 0,
                g_emc_config_path.c_str()) != 0;
    }

    HubUi_SetSearchPersistenceEnabled(g_emc_persist_search_until_cleared);
    HubUi_SetCollapsePersistenceEnabled(g_emc_persist_collapse_state_until_exit);
}

bool SaveEmcConfig(const char** out_error)
{
    if (out_error != 0)
    {
        *out_error = "";
    }

    EnsureEmcConfigLoaded();
    if (g_emc_config_path.empty())
    {
        if (out_error != 0)
        {
            *out_error = "config_path_unavailable";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcPersistSearchSettingId,
            g_emc_persist_search_until_cleared ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcPersistCollapseSettingId,
            g_emc_persist_collapse_state_until_exit ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcAutoFocusSearchSettingId,
            g_emc_auto_focus_search_on_open ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    std::ostringstream open_mod_hub_keycode_stream;
    open_mod_hub_keycode_stream << g_emc_open_mod_hub_keycode;
    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcOpenModHubKeySettingId,
            open_mod_hub_keycode_stream.str().c_str(),
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcOpenModHubRequireCtrlSettingId,
            g_emc_open_mod_hub_require_ctrl ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcOpenModHubRequireShiftSettingId,
            g_emc_open_mod_hub_require_shift ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    if (!WritePrivateProfileStringA(
            kEmcConfigSectionName,
            kEmcOpenModHubRequireAltSettingId,
            g_emc_open_mod_hub_require_alt ? "1" : "0",
            g_emc_config_path.c_str()))
    {
        if (out_error != 0)
        {
            *out_error = "config_write_failed";
        }
        return false;
    }

    return true;
}

EMC_Result __cdecl GetEmcPersistSearchUntilCleared(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_persist_search_until_cleared ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcPersistSearchUntilCleared(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_persist_search_until_cleared;
    g_emc_persist_search_until_cleared = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_persist_search_until_cleared = previous_value;
        HubUi_SetSearchPersistenceEnabled(previous_value);
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    HubUi_SetSearchPersistenceEnabled(g_emc_persist_search_until_cleared);
    return EMC_OK;
}

EMC_Result __cdecl GetEmcPersistCollapseStateUntilExit(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_persist_collapse_state_until_exit ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcPersistCollapseStateUntilExit(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_persist_collapse_state_until_exit;
    g_emc_persist_collapse_state_until_exit = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_persist_collapse_state_until_exit = previous_value;
        HubUi_SetCollapsePersistenceEnabled(previous_value);
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    HubUi_SetCollapsePersistenceEnabled(g_emc_persist_collapse_state_until_exit);
    return EMC_OK;
}

EMC_Result __cdecl GetEmcAutoFocusSearchOnOpen(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_auto_focus_search_on_open ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcAutoFocusSearchOnOpen(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_auto_focus_search_on_open;
    g_emc_auto_focus_search_on_open = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_auto_focus_search_on_open = previous_value;
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetEmcOpenModHubKeybind(void*, EMC_KeybindValueV1* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    out_value->keycode = g_emc_open_mod_hub_keycode;
    out_value->modifiers = 0u;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcOpenModHubKeybind(void*, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
{
    if (value.modifiers != 0u)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "use_modifier_toggles");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (!IsValidEmcOpenModHubKeycode(value.keycode))
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "invalid_keybind");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const int32_t previous_value = g_emc_open_mod_hub_keycode;
    g_emc_open_mod_hub_keycode = value.keycode;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_open_mod_hub_keycode = previous_value;
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetEmcOpenModHubRequireCtrl(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_open_mod_hub_require_ctrl ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcOpenModHubRequireCtrl(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_open_mod_hub_require_ctrl;
    g_emc_open_mod_hub_require_ctrl = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_open_mod_hub_require_ctrl = previous_value;
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetEmcOpenModHubRequireShift(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_open_mod_hub_require_shift ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcOpenModHubRequireShift(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_open_mod_hub_require_shift;
    g_emc_open_mod_hub_require_shift = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_open_mod_hub_require_shift = previous_value;
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetEmcOpenModHubRequireAlt(void*, int32_t* out_value)
{
    if (out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    *out_value = g_emc_open_mod_hub_require_alt ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetEmcOpenModHubRequireAlt(void*, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (value != 0 && value != 1)
    {
        CopyHubErrorMessage(err_buf, err_buf_size, "value_must_be_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureEmcConfigLoaded();
    const bool previous_value = g_emc_open_mod_hub_require_alt;
    g_emc_open_mod_hub_require_alt = value != 0;

    const char* save_error = "";
    if (!SaveEmcConfig(&save_error))
    {
        g_emc_open_mod_hub_require_alt = previous_value;
        CopyHubErrorMessage(err_buf, err_buf_size, save_error);
        return EMC_ERR_CALLBACK_FAILED;
    }

    return EMC_OK;
}

void EnsureEmcSettingsRegistered()
{
    EnsureEmcConfigLoaded();
    if (g_emc_settings_registered)
    {
        return;
    }

    const EMC_ModDescriptorV1 mod_desc = {
        kEmcNamespaceId,
        kEmcNamespaceDisplayName,
        kEmcModId,
        kEmcModDisplayName,
        0
    };

    const EMC_Result register_mod_result = HubRegistry_RegisterMod(&mod_desc, &g_emc_mod_handle);
    if (register_mod_result != EMC_OK || g_emc_mod_handle == 0)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal Mod Hub settings mod result=" << register_mod_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_BoolSettingDefV1 persist_search_setting = {
        kEmcPersistSearchSettingId,
        "Persist search until cleared",
        "Keep Mod Hub search text when Options is closed and reopened during the current game session. Search resets when Kenshi exits.",
        0,
        &GetEmcPersistSearchUntilCleared,
        &SetEmcPersistSearchUntilCleared
    };
    const EMC_BoolSettingDefV1 persist_collapse_setting = {
        kEmcPersistCollapseSettingId,
        "Persist collapse state until exit",
        "Keep mod sections expanded or collapsed when Options is closed and reopened during the current Kenshi session.",
        0,
        &GetEmcPersistCollapseStateUntilExit,
        &SetEmcPersistCollapseStateUntilExit
    };
    const EMC_BoolSettingDefV1 auto_focus_search_setting = {
        kEmcAutoFocusSearchSettingId,
        "Auto focus search on open",
        "Move keyboard focus to the Mod Hub search box when the Mod Hub tab is opened.",
        0,
        &GetEmcAutoFocusSearchOnOpen,
        &SetEmcAutoFocusSearchOnOpen
    };
    const EMC_KeybindSettingDefV1 open_mod_hub_key_setting = {
        kEmcOpenModHubKeySettingId,
        "Open Mod Hub key",
        "Primary key used to open Mod Hub from gameplay or switch to the Mod Hub tab when Options is already open. Use the modifier toggles below for combos.",
        0,
        &GetEmcOpenModHubKeybind,
        &SetEmcOpenModHubKeybind
    };
    const EMC_BoolSettingDefV1 open_mod_hub_require_ctrl_setting = {
        kEmcOpenModHubRequireCtrlSettingId,
        "Require Ctrl for open shortcut",
        "Require Ctrl to be held with the Open Mod Hub key.",
        0,
        &GetEmcOpenModHubRequireCtrl,
        &SetEmcOpenModHubRequireCtrl
    };
    const EMC_BoolSettingDefV1 open_mod_hub_require_shift_setting = {
        kEmcOpenModHubRequireShiftSettingId,
        "Require Shift for open shortcut",
        "Require Shift to be held with the Open Mod Hub key.",
        0,
        &GetEmcOpenModHubRequireShift,
        &SetEmcOpenModHubRequireShift
    };
    const EMC_BoolSettingDefV1 open_mod_hub_require_alt_setting = {
        kEmcOpenModHubRequireAltSettingId,
        "Require Alt for open shortcut",
        "Require Alt to be held with the Open Mod Hub key.",
        0,
        &GetEmcOpenModHubRequireAlt,
        &SetEmcOpenModHubRequireAlt
    };

    const EMC_Result register_setting_result = HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &persist_search_setting);
    if (register_setting_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal search persistence setting result=" << register_setting_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_collapse_result = HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &persist_collapse_setting);
    if (register_collapse_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal collapse persistence setting result=" << register_collapse_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_focus_result = HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &auto_focus_search_setting);
    if (register_focus_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal auto-focus setting result=" << register_focus_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_open_key_result = HubRegistry_RegisterKeybindSetting(g_emc_mod_handle, &open_mod_hub_key_setting);
    if (register_open_key_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal open shortcut key setting result=" << register_open_key_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_open_ctrl_result =
        HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &open_mod_hub_require_ctrl_setting);
    if (register_open_ctrl_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal open shortcut Ctrl setting result=" << register_open_ctrl_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_open_shift_result =
        HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &open_mod_hub_require_shift_setting);
    if (register_open_shift_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal open shortcut Shift setting result=" << register_open_shift_result;
        ErrorLog(line.str().c_str());
        return;
    }

    const EMC_Result register_open_alt_result =
        HubRegistry_RegisterBoolSetting(g_emc_mod_handle, &open_mod_hub_require_alt_setting);
    if (register_open_alt_result != EMC_OK)
    {
        std::ostringstream line;
        line << "Emkejs-Mod-Core: failed to register internal open shortcut Alt setting result=" << register_open_alt_result;
        ErrorLog(line.str().c_str());
        return;
    }

    g_emc_settings_registered = true;
}

int MeasureTrackedHubScrollContentHeight()
{
    int max_bottom = 0;
    for (size_t index = 0; index < g_hub_scrollable_widgets.size(); ++index)
    {
        const HubScrollableWidgetState& state = g_hub_scrollable_widgets[index];
        if (state.widget == 0)
        {
            continue;
        }

        const int bottom = state.top + state.height;
        if (bottom > max_bottom)
        {
            max_bottom = bottom;
        }
    }

    return max_bottom;
}

void OnHubWidgetMouseWheelDebug(MyGUI::Widget* sender, int rel)
{
    if (IsWidgetWithinHubPanel(sender))
    {
        OnHubMouseWheel(sender, rel);
    }
}

void TrackHubScrollableWidget(MyGUI::Widget* parent, MyGUI::Widget* widget, const MyGUI::IntCoord& coord)
{
    if (parent != g_active_hub_scroll_client || widget == 0)
    {
        return;
    }

    HubScrollableWidgetState state;
    state.widget = widget;
    state.left = coord.left;
    state.top = coord.top;
    state.width = coord.width;
    state.height = coord.height;
    g_hub_scrollable_widgets.push_back(state);
}

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
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(base_addr + 0x00406B90);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(base_addr + 0x003FB250);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003ECA90);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(base_addr + 0x00406F30);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(base_addr + 0x003FB570);
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
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(base_addr + 0x004067B0);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(base_addr + 0x003FAE70);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC430);
            g_fnGetOptionsWindow = reinterpret_cast<FnGetOptionsWindow>(base_addr + 0x004068D0);
            g_fnOpenOptionsWindow = reinterpret_cast<FnOpenOptionsWindow>(base_addr + 0x003FAF10);
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
    g_active_hub_search_box = 0;
    g_active_hub_scroll_view = 0;
    g_active_hub_scroll_client = 0;
    g_active_hub_scroll_bar = 0;
    g_active_hub_scroll_line_up_button = 0;
    g_active_hub_scroll_line_down_button = 0;
    g_active_hub_scroll_page_up_button = 0;
    g_active_hub_scroll_page_down_button = 0;
    g_hub_scrollable_widgets.clear();
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
        widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
        g_dynamic_widgets.push_back(widget);
        TrackHubScrollableWidget(parent, widget, coord);
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
                widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
                widget->eventKeyButtonPressed += MyGUI::newDelegate(&OnHubSearchKeyPressed);
                widget->eventKeyButtonReleased += MyGUI::newDelegate(&OnHubSearchKeyReleased);
                g_dynamic_widgets.push_back(widget);
                TrackHubScrollableWidget(parent, widget, coord);
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
        "Kenshi_ScrollBarV",
        "Kenshi_ScrollBar",
        "Kenshi_ScrollBarVBig",
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
                widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
                g_dynamic_widgets.push_back(widget);
                TrackHubScrollableWidget(parent, widget, coord);
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

MyGUI::ScrollView* CreateTrackedScrollView(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    const char* skins[] = {
        "Kenshi_ScrollView",
        "Kenshi_ScrollViewEmpty",
        "Kenshi_ScrollViewEmptyLight",
        "ScrollView"
    };

    for (size_t index = 0; index < sizeof(skins) / sizeof(skins[0]); ++index)
    {
        try
        {
            MyGUI::ScrollView* widget = parent->createWidget<MyGUI::ScrollView>(skins[index], coord, MyGUI::Align::Default);
            if (widget != 0)
            {
                widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
                g_dynamic_widgets.push_back(widget);
                return widget;
            }
        }
        catch (...)
        {
        }
    }

    if (!g_logged_missing_scroll_view_skin)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create native ScrollView for Mod Hub (no compatible skin found)");
        g_logged_missing_scroll_view_skin = true;
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

void AttachHubExactIntDelta(
    MyGUI::Widget* widget,
    int32_t delta,
    const std::string& namespace_id,
    const std::string& mod_id,
    const std::string& setting_id)
{
    if (widget == 0)
    {
        return;
    }

    AttachHubAction(widget, "int_delta_custom", namespace_id, mod_id, setting_id);

    std::ostringstream value;
    value << delta;
    widget->setUserString("emc_int_delta", value.str());
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

bool AreAllModsCollapsed(const RenderNamespaceGroup& namespace_group)
{
    if (namespace_group.mods.empty())
    {
        return false;
    }

    for (size_t index = 0; index < namespace_group.mods.size(); ++index)
    {
        if (!namespace_group.mods[index].collapsed)
        {
            return false;
        }
    }

    return true;
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

bool IsInterestingHubSearchMyGuiKey(MyGUI::KeyCode key_code)
{
    const int value = key_code.getValue();
    return value == MyGUI::KeyCode::LeftControl
        || value == MyGUI::KeyCode::RightControl
        || value == MyGUI::KeyCode::ArrowLeft
        || value == MyGUI::KeyCode::ArrowRight
        || value == MyGUI::KeyCode::Backspace;
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
void FocusHubSearchBoxBestEffort();
bool IsModHubTabCurrentlySelected(OptionsWindow* self);

void ResetPendingHubSearchShortcut()
{
    g_pending_hub_search_shortcut = PendingHubSearchShortcut();
}

void ResetHubSearchSnapshot()
{
    g_have_hub_search_snapshot = false;
    g_hub_search_snapshot_namespace_id.clear();
    g_hub_search_snapshot_query.clear();
    g_hub_search_snapshot_cursor_position = 0;
}

bool IsCtrlModifierDown(const InputHandler* input_handler)
{
    return input_handler != 0 && input_handler->ctrl;
}

bool IsShiftModifierDown(const InputHandler* input_handler)
{
    return input_handler != 0 && input_handler->shift;
}

bool IsAltModifierDown(const InputHandler* input_handler)
{
    return input_handler != 0 && input_handler->alt;
}

bool AreOpenModHubShortcutModifiersSatisfied(const InputHandler* input_handler)
{
    if (g_emc_open_mod_hub_require_ctrl && !IsCtrlModifierDown(input_handler))
    {
        return false;
    }

    if (g_emc_open_mod_hub_require_shift && !IsShiftModifierDown(input_handler))
    {
        return false;
    }

    if (g_emc_open_mod_hub_require_alt && !IsAltModifierDown(input_handler))
    {
        return false;
    }

    return true;
}

bool IsSearchTokenSeparator(MyGUI::UString::unicode_char value)
{
    if (value < 0x80u)
    {
        const unsigned char byte = static_cast<unsigned char>(value);
        return byte == ':' || std::isspace(byte) || !std::isalnum(byte);
    }

    return false;
}

size_t FindPreviousSearchTokenBoundary(const MyGUI::UString& text, size_t cursor)
{
    const size_t length = text.size();
    if (cursor > length)
    {
        cursor = length;
    }

    size_t position = cursor;
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
    if (cursor > length)
    {
        cursor = length;
    }

    size_t position = cursor;
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

void RequestSearchRestoreAfterRebuild(const std::string& namespace_id, size_t cursor_position)
{
    g_restore_search_focus_after_rebuild = true;
    g_restore_search_focus_namespace_id = namespace_id;
    g_restore_search_cursor_after_rebuild = true;
    g_restore_search_cursor_namespace_id = namespace_id;
    g_restore_search_cursor_position = cursor_position;
}

bool ApplyHubSearchQueryAndRebuild(const std::string& namespace_id, const std::string& query, size_t cursor_position)
{
    const char* existing_query = "";
    if (HubUi_GetNamespaceSearchQuery(namespace_id.c_str(), &existing_query)
        && existing_query != 0
        && query == existing_query)
    {
        return false;
    }

    if (HubUi_SetNamespaceSearchQuery(namespace_id.c_str(), query.c_str()) != EMC_OK)
    {
        return false;
    }

    g_hub_scroll_offset = 0;
    RequestSearchRestoreAfterRebuild(namespace_id, cursor_position);
    RebuildHubPanelWidgets();
    return true;
}

MyGUI::EditBox* FindFocusedHubSearchBox(std::string* out_namespace_id)
{
    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return 0;
    }

    MyGUI::Widget* focused_widget = input->getKeyFocusWidget();
    if (focused_widget == 0)
    {
        return 0;
    }

    MyGUI::EditBox* search_box = 0;
    for (MyGUI::Widget* widget = focused_widget; widget != 0; widget = widget->getParent())
    {
        if (widget != g_active_hub_search_box
            && widget->getUserString("emc_search_box") != "1")
        {
            continue;
        }

        search_box = widget->castType<MyGUI::EditBox>(false);
        if (search_box != 0)
        {
            break;
        }
    }

    if (search_box == 0)
    {
        return 0;
    }

    if (out_namespace_id != 0)
    {
        *out_namespace_id = search_box->getUserString("emc_ns");
    }

    return search_box;
}

void RememberHubSearchSnapshot(MyGUI::EditBox* search_box)
{
    if (search_box == 0)
    {
        ResetHubSearchSnapshot();
        return;
    }

    size_t cursor_position = search_box->getTextCursor();
    const size_t text_length = search_box->getTextLength();
    if (cursor_position > text_length)
    {
        cursor_position = text_length;
    }

    g_have_hub_search_snapshot = true;
    g_hub_search_snapshot_namespace_id = search_box->getUserString("emc_ns");
    g_hub_search_snapshot_query = search_box->getOnlyText().asUTF8();
    g_hub_search_snapshot_cursor_position = cursor_position;
}

void RememberHubSearchSnapshotValue(const std::string& namespace_id, const std::string& query, size_t cursor_position)
{
    g_have_hub_search_snapshot = true;
    g_hub_search_snapshot_namespace_id = namespace_id;
    g_hub_search_snapshot_query = query;

    if (cursor_position > query.size())
    {
        cursor_position = query.size();
    }

    g_hub_search_snapshot_cursor_position = cursor_position;
}

bool ScheduleHubSearchMyGuiShortcut(MyGUI::EditBox* search_box, MyGUI::KeyCode key_code)
{
    if (search_box == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0 || !input->isControlPressed())
    {
        return false;
    }

    const std::string namespace_id = search_box->getUserString("emc_ns");
    if (namespace_id.empty() || search_box->getUserString("emc_search_box") != "1")
    {
        return false;
    }

    MyGUI::UString text = search_box->getOnlyText();
    size_t text_length = text.size();
    size_t cursor = search_box->getTextCursor();
    if (cursor > text_length)
    {
        cursor = text_length;
    }

    ResetPendingHubSearchShortcut();
    g_pending_hub_search_shortcut.active = true;
    g_pending_hub_search_shortcut.key_value = key_code.getValue();
    g_pending_hub_search_shortcut.namespace_id = namespace_id;

    if (key_code.getValue() == MyGUI::KeyCode::ArrowLeft)
    {
        g_pending_hub_search_shortcut.rebuild_query = false;
        g_pending_hub_search_shortcut.cursor_position = FindPreviousSearchTokenBoundary(text, cursor);
        return true;
    }

    if (key_code.getValue() == MyGUI::KeyCode::ArrowRight)
    {
        g_pending_hub_search_shortcut.rebuild_query = false;
        g_pending_hub_search_shortcut.cursor_position = FindNextSearchTokenBoundary(text, cursor);
        return true;
    }

    if (key_code.getValue() != MyGUI::KeyCode::Backspace)
    {
        ResetPendingHubSearchShortcut();
        return false;
    }

    if (g_have_hub_search_snapshot && g_hub_search_snapshot_namespace_id == namespace_id)
    {
        text = MyGUI::UString(g_hub_search_snapshot_query);
        text_length = text.size();
        cursor = g_hub_search_snapshot_cursor_position;
        if (cursor > text_length)
        {
            cursor = text_length;
        }
    }

    g_pending_hub_search_shortcut.rebuild_query = true;
    MyGUI::UString updated = text;

    if (search_box->isTextSelection())
    {
        size_t selection_start = search_box->getTextSelectionStart();
        size_t selection_length = search_box->getTextSelectionLength();
        if (selection_start != MyGUI::ITEM_NONE)
        {
            if (selection_start > text_length)
            {
                selection_start = text_length;
            }
            if (selection_start + selection_length > text_length)
            {
                selection_length = text_length - selection_start;
            }
        }

        if (selection_start != MyGUI::ITEM_NONE && selection_length != 0)
        {
            updated.erase(selection_start, selection_length);
            g_pending_hub_search_shortcut.cursor_position = selection_start;
        }
        else
        {
            g_pending_hub_search_shortcut.cursor_position = cursor;
        }
    }
    else
    {
        const size_t delete_start = FindPreviousSearchTokenBoundary(text, cursor);
        if (delete_start != cursor)
        {
            updated.erase(delete_start, cursor - delete_start);
        }
        g_pending_hub_search_shortcut.cursor_position = delete_start;
    }

    g_pending_hub_search_shortcut.query = updated.asUTF8();
    return true;
}

void ApplyPendingHubSearchShortcut(MyGUI::EditBox* search_box, MyGUI::KeyCode key_code)
{
    if (!g_pending_hub_search_shortcut.active || g_pending_hub_search_shortcut.key_value != key_code.getValue())
    {
        return;
    }

    const PendingHubSearchShortcut pending = g_pending_hub_search_shortcut;
    ResetPendingHubSearchShortcut();

    if (search_box == 0)
    {
        return;
    }

    if (pending.rebuild_query)
    {
        RememberHubSearchSnapshotValue(pending.namespace_id, pending.query, pending.cursor_position);
        const bool applied = ApplyHubSearchQueryAndRebuild(pending.namespace_id, pending.query, pending.cursor_position);
        if (!applied)
        {
            search_box->setOnlyText(pending.query);
            size_t cursor_position = pending.cursor_position;
            const size_t text_length = search_box->getTextLength();
            if (cursor_position > text_length)
            {
                cursor_position = text_length;
            }
            search_box->setTextCursor(cursor_position);
            search_box->setTextSelection(cursor_position, cursor_position);
        }

        return;
    }

    size_t cursor_position = pending.cursor_position;
    const size_t text_length = search_box->getTextLength();
    if (cursor_position > text_length)
    {
        cursor_position = text_length;
    }
    search_box->setTextCursor(cursor_position);
    search_box->setTextSelection(cursor_position, cursor_position);
}

bool HandleHubSearchCtrlShortcut(InputHandler* input_handler, OIS::KeyCode key_code)
{
    if (!IsCtrlModifierDown(input_handler))
    {
        return false;
    }

    std::string namespace_id;
    MyGUI::EditBox* search_box = FindFocusedHubSearchBox(&namespace_id);
    if (search_box == 0 || namespace_id.empty())
    {
        return false;
    }

    const MyGUI::UString text = search_box->getOnlyText();
    const size_t cursor = search_box->getTextCursor();

    if (key_code == OIS::KC_LEFT)
    {
        const size_t next_cursor = FindPreviousSearchTokenBoundary(text, cursor);
        if (next_cursor != cursor)
        {
            search_box->setTextCursor(next_cursor);
        }
        return true;
    }

    if (key_code == OIS::KC_RIGHT)
    {
        const size_t next_cursor = FindNextSearchTokenBoundary(text, cursor);
        if (next_cursor != cursor)
        {
            search_box->setTextCursor(next_cursor);
        }
        return true;
    }

    if (key_code != OIS::KC_BACK)
    {
        return false;
    }

    if (search_box->isTextSelection())
    {
        const size_t selection_start = search_box->getTextSelectionStart();
        const size_t selection_length = search_box->getTextSelectionLength();
        if (selection_start == MyGUI::ITEM_NONE || selection_length == 0)
        {
            return true;
        }

        MyGUI::UString updated = text;
        updated.erase(selection_start, selection_length);
        const std::string updated_query = updated.asUTF8();
        ApplyHubSearchQueryAndRebuild(namespace_id, updated_query, selection_start);
        return true;
    }

    const size_t delete_start = FindPreviousSearchTokenBoundary(text, cursor);
    if (delete_start == cursor)
    {
        return true;
    }

    MyGUI::UString updated = text;
    updated.erase(delete_start, cursor - delete_start);
    const std::string updated_query = updated.asUTF8();
    ApplyHubSearchQueryAndRebuild(namespace_id, updated_query, delete_start);
    return true;
}

bool HandleHubSearchFocusShortcut(InputHandler* input_handler, OIS::KeyCode key_code)
{
    if (!g_hub_enabled || !HubUi_IsOptionsWindowOpen() || !IsModHubTabCurrentlySelected(g_active_options_window))
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    MyGUI::Widget* focused_widget = input != 0 ? input->getKeyFocusWidget() : 0;
    const bool search_box_already_focused = focused_widget == g_active_hub_search_box;

    if (key_code == OIS::KC_SLASH)
    {
        if (search_box_already_focused)
        {
            return false;
        }

        FocusHubSearchBoxBestEffort();
        return true;
    }

    if (key_code == OIS::KC_F && IsCtrlModifierDown(input_handler))
    {
        FocusHubSearchBoxBestEffort();
        return true;
    }

    return false;
}

bool SelectModHubTabUnsafe(MyGUI::TabControl* options_tab)
{
    if (options_tab == 0)
    {
        return false;
    }

    const size_t hub_index = options_tab->findItemIndexWith(kHubTabName);
    if (hub_index == MyGUI::ITEM_NONE)
    {
        return false;
    }

    if (options_tab->getIndexSelected() == hub_index)
    {
        return false;
    }

    options_tab->setIndexSelected(hub_index);
    return true;
}

bool TrySelectModHubTabBestEffort()
{
    if (g_active_options_window == 0 || g_active_options_window->optionsTab == 0)
    {
        return false;
    }

    __try
    {
        return SelectModHubTabUnsafe(g_active_options_window->optionsTab);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub shortcut failed while selecting tab");
        return false;
    }
}

bool OpenOptionsWindowForHubUnsafe()
{
    if (g_fnGetOptionsWindow == 0 || g_fnOpenOptionsWindow == 0)
    {
        return false;
    }

    OptionsWindow* options_window = g_fnGetOptionsWindow();
    if (options_window == 0)
    {
        return false;
    }

    g_fnOpenOptionsWindow(options_window);
    return true;
}

bool TryOpenOptionsWindowForHubBestEffort()
{
    __try
    {
        return OpenOptionsWindowForHubUnsafe();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub shortcut failed while opening native OptionsWindow");
        return false;
    }
}

bool HandleHubOpenShortcut(InputHandler* input_handler, OIS::KeyCode key_code)
{
    if (!g_hub_enabled || input_handler == 0)
    {
        return false;
    }

    if (!g_emc_config_loaded)
    {
        EnsureEmcConfigLoaded();
    }

    if (g_emc_open_mod_hub_keycode == EMC_KEY_UNBOUND
        || static_cast<int32_t>(key_code) != g_emc_open_mod_hub_keycode
        || !AreOpenModHubShortcutModifiersSatisfied(input_handler))
    {
        return false;
    }

    if (HubUi_IsOptionsWindowOpen())
    {
        return TrySelectModHubTabBestEffort();
    }

    g_select_mod_hub_on_next_options_init = true;
    const bool opened = TryOpenOptionsWindowForHubBestEffort();
    if (opened)
    {
        return true;
    }

    g_select_mod_hub_on_next_options_init = false;
    return false;
}

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
    size_t cursor_position = sender->getTextCursor();
    const size_t text_length = sender->getTextLength();
    if (cursor_position > text_length)
    {
        cursor_position = text_length;
    }

    RememberHubSearchSnapshotValue(namespace_id, query, cursor_position);
    ApplyHubSearchQueryAndRebuild(namespace_id, query, cursor_position);
}

void OnHubSearchKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode key_code, MyGUI::Char character)
{
    (void)character;

    if (sender == 0 || !IsInterestingHubSearchMyGuiKey(key_code))
    {
        return;
    }

    ScheduleHubSearchMyGuiShortcut(sender->castType<MyGUI::EditBox>(false), key_code);
}

void OnHubSearchKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode key_code)
{
    if (sender == 0 || !IsInterestingHubSearchMyGuiKey(key_code))
    {
        return;
    }

    MyGUI::EditBox* search_box = sender->castType<MyGUI::EditBox>(false);
    const bool pending_rebuild_query =
        g_pending_hub_search_shortcut.active
        && g_pending_hub_search_shortcut.key_value == key_code.getValue()
        && g_pending_hub_search_shortcut.rebuild_query;
    ApplyPendingHubSearchShortcut(search_box, key_code);
    if (!pending_rebuild_query)
    {
        RememberHubSearchSnapshot(search_box);
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

    int delta = kHubScrollLineStep;
    const int magnitude = rel > 0 ? rel : -rel;
    if (magnitude > 120)
    {
        const int steps = (magnitude + 119) / 120;
        delta *= steps;
    }
    if (rel > 0)
    {
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset - delta);
    }
    else
    {
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset + delta);
    }

}

void OnHubScrollBarPositionChanged(MyGUI::ScrollBar*, size_t position)
{
    if (g_ignore_scrollbar_position_event)
    {
        return;
    }

    SetHubScrollOffset(static_cast<int>(position));
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
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset - kHubScrollLineStep);
        return;
    }

    if (action == "scroll_line_down")
    {
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset + kHubScrollLineStep);
        return;
    }

    if (action == "scroll_page_up")
    {
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset - g_hub_scroll_page_step);
        return;
    }

    if (action == "scroll_page_down")
    {
        ApplyHubScrollOffsetWithoutRebuild(g_hub_scroll_offset + g_hub_scroll_page_step);
        return;
    }

    if (action == "namespace_select")
    {
        g_selected_namespace_id = namespace_id;
        g_hub_scroll_offset = 0;
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "search_clear")
    {
        ApplyHubSearchQueryAndRebuild(namespace_id, "", 0);
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

    if (action == "mods_toggle_all")
    {
        std::vector<RenderNamespaceGroup> namespaces;
        BuildRenderNamespaces(&namespaces);
        EnsureSelectedNamespace(namespaces);

        const RenderNamespaceGroup* target_namespace = 0;
        for (size_t index = 0; index < namespaces.size(); ++index)
        {
            if (namespaces[index].namespace_id == namespace_id)
            {
                target_namespace = &namespaces[index];
                break;
            }
        }

        if (target_namespace != 0)
        {
            const bool collapse_all = !AreAllModsCollapsed(*target_namespace);
            for (size_t index = 0; index < target_namespace->mods.size(); ++index)
            {
                HubUi_SetModCollapsed(
                    namespace_id.c_str(),
                    target_namespace->mods[index].mod_id.c_str(),
                    collapse_all);
            }
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

    if (action == "int_delta_custom")
    {
        const std::string delta_text = sender->getUserString("emc_int_delta");
        const int32_t delta = static_cast<int32_t>(std::atoi(delta_text.c_str()));
        HubUi_AdjustPendingIntDelta(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), delta);
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

std::string BuildNumericFooterText(const HubUiRowView& row)
{
    if (row.kind == HUB_UI_ROW_KIND_INT || row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        if (row.description != 0 && row.description[0] != '\0')
        {
            return row.description;
        }

        return BuildNumericRangeHint(row);
    }

    return std::string();
}

int CountEnabledIntButtons(const int32_t deltas[3])
{
    int count = 0;
    for (int32_t index = 0; index < 3; ++index)
    {
        if (deltas[index] > 0)
        {
            count += 1;
        }
    }

    return count;
}

int GetIntControlGroupWidth(const HubUiRowView& row)
{
    const int button_width = 44;
    const int button_gap = 4;
    const int value_box_width = 96;
    int button_count = 6;
    if (row.int_use_custom_buttons)
    {
        button_count = CountEnabledIntButtons(row.int_dec_button_deltas) + CountEnabledIntButtons(row.int_inc_button_deltas);
    }

    return (button_width * button_count) + (button_gap * button_count) + value_box_width;
}

int GetIntControlX(const HubUiRowView& row, int panel_width, int value_x)
{
    int control_x = panel_width - 24 - GetIntControlGroupWidth(row);
    if (control_x < value_x - 190)
    {
        control_x = value_x - 190;
    }

    return control_x;
}

std::string FormatExactIntDeltaButtonCaption(int32_t delta)
{
    if (delta == -1)
    {
        return "-";
    }

    if (delta == 1)
    {
        return "+";
    }

    std::ostringstream caption;
    if (delta > 0)
    {
        caption << '+';
    }
    caption << delta;
    return caption.str();
}

void CreateNumericRangeHintLabel(MyGUI::Widget* parent, int x, int y, int width, const HubUiRowView& row)
{
    if (parent == 0)
    {
        return;
    }

    const std::string hint = BuildNumericFooterText(row);
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
    hint_text->setTextAlign((row.description != 0 && row.description[0] != '\0')
        ? MyGUI::Align::Left
        : MyGUI::Align::Right);
    hint_text->setTextColour(MyGUI::Colour(0.56f, 0.56f, 0.56f, 1.0f));
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
        label->setFontHeight(18);
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
        int control_x = GetIntControlX(row, panel_width, value_x);

        if (row.int_use_custom_buttons)
        {
            for (int32_t index = 0; index < 3; ++index)
            {
                const int32_t delta = row.int_dec_button_deltas[index];
                if (delta <= 0)
                {
                    continue;
                }

                MyGUI::Button* button = CreateTrackedWidget<MyGUI::Button>(
                    parent,
                    kValueButtonSkin,
                    MyGUI::IntCoord(control_x, y, button_width, button_height));
                if (button != 0)
                {
                    button->setCaption(FormatExactIntDeltaButtonCaption(-delta));
                    button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                    AttachHubExactIntDelta(
                        button,
                        -delta,
                        row.namespace_id != 0 ? row.namespace_id : "",
                        row.mod_id != 0 ? row.mod_id : "",
                        row.setting_id != 0 ? row.setting_id : "");
                }
                control_x += button_width + button_gap;
            }
        }
        else
        {
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
        }

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

        if (row.int_use_custom_buttons)
        {
            for (int32_t index = 0; index < 3; ++index)
            {
                const int32_t delta = row.int_inc_button_deltas[index];
                if (delta <= 0)
                {
                    continue;
                }

                MyGUI::Button* button = CreateTrackedWidget<MyGUI::Button>(
                    parent,
                    kValueButtonSkin,
                    MyGUI::IntCoord(control_x, y, button_width, button_height));
                if (button != 0)
                {
                    button->setCaption(FormatExactIntDeltaButtonCaption(delta));
                    button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                    AttachHubExactIntDelta(
                        button,
                        delta,
                        row.namespace_id != 0 ? row.namespace_id : "",
                        row.mod_id != 0 ? row.mod_id : "",
                        row.setting_id != 0 ? row.setting_id : "");
                }
                control_x += button_width + button_gap;
            }
        }
        else
        {
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
        int hint_x = label_x + 12;
        int hint_width = panel_width - 80;
        if (row.kind == HUB_UI_ROW_KIND_INT)
        {
            const int group_width = GetIntControlGroupWidth(row);
            const bool use_description_footer = row.description != 0 && row.description[0] != '\0';
            if (use_description_footer)
            {
                hint_x = label_x + 12;
                hint_width = panel_width - 80;
            }
            else
            {
                hint_x = GetIntControlX(row, panel_width, value_x);
                hint_width = group_width;
            }
        }
        else
        {
            hint_x = value_x - 78;
            hint_width = 224;
        }

        CreateNumericRangeHintLabel(parent, hint_x, next_y, hint_width, row);
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

int GetHubVisibleViewportHeight(int fallback_height)
{
    if (g_active_hub_scroll_view != 0)
    {
        const int visible_height = g_active_hub_scroll_view->getHeight();
        if (visible_height > 0)
        {
            return visible_height;
        }
    }

    return fallback_height;
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

void SetHubFinalScrollMetrics(int content_height, int viewport_height)
{
    g_hub_final_content_height = content_height > 0 ? content_height : 0;
    g_hub_final_viewport_height = viewport_height > 0 ? viewport_height : 0;
}

void SyncHubScrollBarMetrics(int content_height, int viewport_height)
{
    if (g_active_hub_scroll_bar == 0)
    {
        return;
    }

    const size_t range = static_cast<size_t>(g_hub_scroll_max_offset + 1);
    const size_t line_step = static_cast<size_t>(kHubScrollLineStep);
    size_t page_step = static_cast<size_t>(g_hub_scroll_page_step);
    if (page_step == 0u)
    {
        page_step = line_step;
    }

    g_ignore_scrollbar_position_event = true;
    g_active_hub_scroll_bar->setScrollRange(range);
    g_active_hub_scroll_bar->setScrollPage(line_step);
    g_active_hub_scroll_bar->setScrollViewPage(page_step);
    g_active_hub_scroll_bar->setScrollWheelPage(line_step);
    g_active_hub_scroll_bar->setScrollPosition(static_cast<size_t>(g_hub_scroll_offset));

    int thumb_height = 0;
    const int scroll_bar_height = g_active_hub_scroll_bar->getHeight();
    if (content_height > 0)
    {
        thumb_height = (scroll_bar_height * viewport_height) / content_height;
    }
    if (thumb_height < 28)
    {
        thumb_height = 28;
    }
    if (thumb_height > scroll_bar_height)
    {
        thumb_height = scroll_bar_height;
    }
    g_active_hub_scroll_bar->setTrackSize(thumb_height);
    g_ignore_scrollbar_position_event = false;
}

void SetHubScrollOffset(int offset)
{
    g_hub_scroll_offset = ClampInt(offset, 0, g_hub_scroll_max_offset);
    ApplyHubScrollVisualState();
}

bool ApplyHubScrollOffsetWithoutRebuild(int offset)
{
    const int previous_offset = g_hub_scroll_offset;
    SetHubScrollOffset(offset);
    if (g_hub_scroll_offset == previous_offset)
    {
        return false;
    }

    if (g_active_hub_scroll_client == 0)
    {
        RebuildHubPanelWidgets();
    }

    return true;
}

bool IsWidgetWithinHubPanel(MyGUI::Widget* widget)
{
    while (widget != 0)
    {
        if (widget == g_active_hub_panel_widget)
        {
            return true;
        }
        widget = widget->getParent();
    }

    return false;
}

void ApplyHubScrollVisualState()
{
    int visible_widgets = 0;
    int hidden_widgets = 0;
    int viewport_height = GetHubVisibleViewportHeight(g_hub_final_viewport_height);
    int client_visual_offset_y = 0;
    if (g_active_hub_scroll_client != 0 && g_active_hub_scroll_view != 0)
    {
        if (g_active_hub_scroll_client != g_active_hub_scroll_view)
        {
            client_visual_offset_y = g_active_hub_scroll_client->getCoord().top;
        }

        for (size_t index = 0; index < g_hub_scrollable_widgets.size(); ++index)
        {
            HubScrollableWidgetState& state = g_hub_scrollable_widgets[index];
            if (state.widget == 0)
            {
                continue;
            }

            const int top = state.top - g_hub_scroll_offset - client_visual_offset_y;
            state.widget->setCoord(state.left, top, state.width, state.height);
            const int actual_top = top + client_visual_offset_y;
            const bool visible = (actual_top + state.height) > 0 && actual_top < viewport_height;
            state.widget->setVisible(visible);
            if (visible)
            {
                ++visible_widgets;
            }
            else
            {
                ++hidden_widgets;
            }
        }
    }

    if (g_active_hub_scroll_bar != 0)
    {
        g_ignore_scrollbar_position_event = true;
        g_active_hub_scroll_bar->setScrollPosition(static_cast<size_t>(g_hub_scroll_offset));
        g_ignore_scrollbar_position_event = false;
    }

    if (g_active_hub_scroll_line_up_button != 0)
    {
        g_active_hub_scroll_line_up_button->setEnabled(g_hub_scroll_offset > 0);
    }
    if (g_active_hub_scroll_line_down_button != 0)
    {
        g_active_hub_scroll_line_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
    }
    if (g_active_hub_scroll_page_up_button != 0)
    {
        g_active_hub_scroll_page_up_button->setEnabled(g_hub_scroll_offset > 0);
    }
    if (g_active_hub_scroll_page_down_button != 0)
    {
        g_active_hub_scroll_page_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
    }

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
        && (g_restore_search_focus_namespace_id.empty()
            || selected_namespace->namespace_id == g_restore_search_focus_namespace_id);
    const bool should_restore_search_cursor =
        g_restore_search_cursor_after_rebuild
        && selected_namespace->namespace_id == g_restore_search_cursor_namespace_id;
    const size_t restore_search_cursor_position = g_restore_search_cursor_position;
    g_restore_search_focus_after_rebuild = false;
    g_restore_search_focus_namespace_id.clear();
    g_restore_search_cursor_after_rebuild = false;
    g_restore_search_cursor_namespace_id.clear();
    g_restore_search_cursor_position = 0;

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

    const std::string search_query_text = search_query != 0 ? search_query : "";
    const bool show_search_clear_button = !search_query_text.empty();
    const bool all_mods_collapsed = AreAllModsCollapsed(*selected_namespace);
    const int clear_button_gap = 6;
    const int clear_button_width = 32;
    const int search_box_width = search_width - clear_button_width - clear_button_gap;
    const int toggle_all_button_width = 146;
    const int toggle_all_button_x = panel_width - 20 - toggle_all_button_width;

    MyGUI::EditBox* search_box = CreateTrackedSearchBox(
        g_active_hub_panel_widget,
        MyGUI::IntCoord(96, y, search_box_width, 40));
    if (search_box != 0)
    {
        g_active_hub_search_box = search_box;
        search_box->setEditMultiLine(false);
        search_box->setOnlyText(search_query_text);
        search_box->setUserString("emc_ns", selected_namespace->namespace_id);
        search_box->setUserString("emc_search_box", "1");
        search_box->eventEditTextChange += MyGUI::newDelegate(&OnHubSearchTextChanged);
        if (should_restore_search_focus)
        {
            MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
            if (input != 0)
            {
                input->setKeyFocusWidget(search_box);
            }
        }

        if (should_restore_search_cursor)
        {
            size_t cursor_position = restore_search_cursor_position;
            const size_t text_length = search_box->getTextLength();
            if (cursor_position > text_length)
            {
                cursor_position = text_length;
            }
            search_box->setTextCursor(cursor_position);
        }

        RememberHubSearchSnapshot(search_box);
    }

    if (show_search_clear_button)
    {
        MyGUI::Button* clear_search_button = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kValueButtonSkin,
            MyGUI::IntCoord(96 + search_box_width + clear_button_gap, y + 4, clear_button_width, 32));
        if (clear_search_button != 0)
        {
            clear_search_button->setCaption("x");
            clear_search_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(clear_search_button, "search_clear", selected_namespace->namespace_id, "", "");
        }
    }

    MyGUI::Button* toggle_all_button = CreateTrackedWidget<MyGUI::Button>(
        g_active_hub_panel_widget,
        kValueButtonSkin,
        MyGUI::IntCoord(toggle_all_button_x, y + 4, toggle_all_button_width, 32));
    if (toggle_all_button != 0)
    {
        toggle_all_button->setCaption(all_mods_collapsed ? "Expand all" : "Collapse all");
        toggle_all_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
        AttachHubAction(toggle_all_button, "mods_toggle_all", selected_namespace->namespace_id, "", "");
    }

    MyGUI::TextBox* search_hint = CreateTrackedWidget<MyGUI::TextBox>(
        g_active_hub_panel_widget,
        kTextSkin,
        MyGUI::IntCoord(96, y + 42, panel_width - 120, 22));
    if (search_hint != 0)
    {
        search_hint->setCaption(kSearchHintText);
        search_hint->setTextColour(MyGUI::Colour(0.65f, 0.65f, 0.65f, 1.0f));
    }

    const int content_top = y + 68;
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
        SetHubFinalScrollMetrics(0, viewport_height);
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
    const int preserved_scroll_offset = g_hub_scroll_offset;
    const bool show_scroll_controls = g_hub_scroll_max_offset > 0;
    int content_panel_width = panel_width - 40;
    if (show_scroll_controls)
    {
        content_panel_width -= kHubScrollGutterWidth;
    }
    if (content_panel_width < 280)
    {
        content_panel_width = panel_width - 40;
    }

    MyGUI::Widget* content_parent = g_active_hub_panel_widget;
    MyGUI::ScrollView* scroll_view = CreateTrackedScrollView(
        g_active_hub_panel_widget,
        MyGUI::IntCoord(20, content_top, content_panel_width, viewport_height));
    if (scroll_view != 0)
    {
        scroll_view->setVisibleHScroll(false);
        scroll_view->setVisibleVScroll(false);
        g_active_hub_scroll_view = scroll_view;

        MyGUI::Widget* client_widget = scroll_view->getClientWidget();
        if (client_widget != 0)
        {
            client_widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
            g_active_hub_scroll_client = client_widget;
            content_parent = client_widget;
            content_panel_width = scroll_view->getClientCoord().width;
        }
        else
        {
            g_active_hub_scroll_client = scroll_view;
            content_parent = scroll_view;
        }

        if (content_panel_width < 280)
        {
            content_panel_width = panel_width - 40;
            if (show_scroll_controls)
            {
                content_panel_width -= kHubScrollGutterWidth;
            }
        }

        int canvas_height = content_total_height;
        if (canvas_height < viewport_height)
        {
            canvas_height = viewport_height;
        }
        scroll_view->setCanvasSize(content_panel_width, canvas_height);
    }

    if (show_scroll_controls)
    {
        const int control_x = content_panel_width + ((panel_width - content_panel_width - kHubScrollControlWidth) / 2);
        const int button_height = 28;
        const int button_gap = 4;
        const int top_line_y = content_top;
        const int bottom_line_y = content_bottom - button_height;

        const int scroll_bar_y = content_top;
        int scroll_bar_height = content_bottom - content_top;
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
                g_active_hub_scroll_bar = scroll_bar;
                SyncHubScrollBarMetrics(content_total_height, viewport_height);
                scroll_bar->eventScrollChangePosition += MyGUI::newDelegate(&OnHubScrollBarPositionChanged);
                has_scrollbar = true;
            }
        }

        if (!has_scrollbar)
        {
            MyGUI::Button* line_up_button = CreateTrackedWidget<MyGUI::Button>(
                g_active_hub_panel_widget,
                kValueButtonSkin,
                MyGUI::IntCoord(control_x, top_line_y, kHubScrollControlWidth, button_height));
            if (line_up_button != 0)
            {
                g_active_hub_scroll_line_up_button = line_up_button;
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
                g_active_hub_scroll_line_down_button = line_down_button;
                line_down_button->setCaption("v");
                line_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
                line_down_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                AttachHubAction(line_down_button, "scroll_line_down", "", "", "");
            }

            const int top_page_y = content_top + 34;
            const int bottom_page_y = content_bottom - 62;

            MyGUI::Button* page_up_button = CreateTrackedWidget<MyGUI::Button>(
                g_active_hub_panel_widget,
                kValueButtonSkin,
                MyGUI::IntCoord(control_x, top_page_y, kHubScrollControlWidth, button_height));
            if (page_up_button != 0)
            {
                g_active_hub_scroll_page_up_button = page_up_button;
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
                g_active_hub_scroll_page_down_button = page_down_button;
                page_down_button->setCaption("Pgv");
                page_down_button->setEnabled(g_hub_scroll_offset < g_hub_scroll_max_offset);
                page_down_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
                AttachHubAction(page_down_button, "scroll_page_down", "", "", "");
            }
        }
    }

    int logical_y = 0;
    const bool has_persistent_scroll = g_active_hub_scroll_client != 0;
    for (size_t mod_index = 0; mod_index < filtered_mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = filtered_mods[mod_index];
        const int header_height = 32;
        const int header_y = has_persistent_scroll
            ? logical_y
            : content_top + logical_y - g_hub_scroll_offset;

        if (has_persistent_scroll || IsBlockFullyVisible(header_y, header_height, content_top, content_bottom))
        {
            MyGUI::Button* mod_header = CreateTrackedWidget<MyGUI::Button>(
                content_parent,
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
            const int row_y = has_persistent_scroll
                ? logical_y
                : content_top + logical_y - g_hub_scroll_offset;
            if (has_persistent_scroll || IsBlockFullyVisible(row_y, row_height, content_top, content_bottom))
            {
                int next_y = row_y;
                CreateRowWidgets(content_parent, content_panel_width, row_y, row, &next_y);
                if (has_persistent_scroll)
                {
                    logical_y = next_y;
                    continue;
                }
            }
            logical_y += row_height;
        }

        logical_y += 8;
    }

    logical_y += 12;

    int final_viewport_height = viewport_height;
    final_viewport_height = GetHubVisibleViewportHeight(final_viewport_height);
    int final_content_height = logical_y;
    if (final_content_height < 0)
    {
        final_content_height = 0;
    }
    ConfigureHubScrollRange(final_content_height, final_viewport_height);
    SetHubFinalScrollMetrics(final_content_height, final_viewport_height);
    SyncHubScrollBarMetrics(final_content_height, final_viewport_height);
    SetHubScrollOffset(preserved_scroll_offset);
}

void ClearActiveUiState()
{
    DestroyDynamicWidgets();
    ResetPendingHubSearchShortcut();
    ResetHubSearchSnapshot();
    g_selected_namespace_id.clear();
    g_hub_scroll_offset = 0;
    g_hub_scroll_max_offset = 0;
    g_hub_scroll_page_step = 160;
    g_hub_final_content_height = 0;
    g_hub_final_viewport_height = 0;
    g_active_hub_panel_widget = 0;
    g_active_hub_panel = 0;
    g_active_options_window = 0;
    g_select_mod_hub_on_next_options_init = false;
}

void BindHubPanelWheelDelegateBestEffort(MyGUI::Widget* hub_panel_widget)
{
    if (hub_panel_widget == 0)
    {
        return;
    }

    __try
    {
        hub_panel_widget->eventMouseWheel += MyGUI::newDelegate(&OnHubWidgetMouseWheelDebug);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core: EnsureHubPanel step=bind_wheel faulted; continuing without panel wheel delegate");
    }
}

void FocusHubSearchBoxBestEffort_WithSeh(MyGUI::InputManager* input, MyGUI::EditBox* search_box)
{
    __try
    {
        input->setKeyFocusWidget(search_box);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub search auto-focus faulted; continuing without focus change");
    }
}

void FocusHubSearchBoxBestEffort()
{
    if (g_active_hub_search_box == 0)
    {
        g_restore_search_focus_after_rebuild = true;
        g_restore_search_focus_namespace_id.clear();
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return;
    }

    FocusHubSearchBoxBestEffort_WithSeh(input, g_active_hub_search_box);
}

void OnOptionsTabChangeSelect(MyGUI::TabControl* sender, size_t index)
{
    if (!g_hub_enabled || !g_emc_auto_focus_search_on_open || sender == 0)
    {
        return;
    }

    const MyGUI::UString& selected_name = sender->getItemNameAt(index);
    if (selected_name != kHubTabName)
    {
        return;
    }

    FocusHubSearchBoxBestEffort();
}

void BindHubTabSelectDelegateBestEffort(MyGUI::TabControl* options_tab)
{
    if (options_tab == 0 || g_bound_options_tab_for_autofocus == options_tab)
    {
        return;
    }

    __try
    {
        options_tab->eventTabChangeSelect += MyGUI::newDelegate(&OnOptionsTabChangeSelect);
        g_bound_options_tab_for_autofocus = options_tab;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core: EnsureHubPanel step=bind_tab_select faulted; continuing without Mod Hub auto-focus delegate");
    }
}

bool BuildHubPanelUnsafe(OptionsWindow* self, HubPanelCreationResult* out_result, volatile long* out_fault_stage)
{
    if (out_fault_stage != 0)
    {
        *out_fault_stage = 1;
    }
    MyGUI::TabItem* hub_tab = self->optionsTab->addItem(kHubTabName);
    if (hub_tab == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to add Mod Hub tab item");
        return false;
    }

    if (out_fault_stage != 0)
    {
        *out_fault_stage = 2;
    }
    DatapanelGUI* hub_panel = g_fnCreateDatapanel(g_ptrKenshiGUI, kHubPanelName, hub_tab, false);
    if (hub_panel == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create Mod Hub datapanel");
        return false;
    }

    hub_panel->vfunc0xc0(kHubPanelLineId);
    hub_panel->vfunc0xe0(25.0f);

    if (out_fault_stage != 0)
    {
        *out_fault_stage = 3;
    }
    MyGUI::Widget* hub_panel_widget = hub_panel->getWidget();
    if (hub_panel_widget == 0)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub panel widget is null");
        return false;
    }

    BindHubPanelWheelDelegateBestEffort(hub_panel_widget);

    if (out_fault_stage != 0)
    {
        *out_fault_stage = 4;
    }
    hub_tab->setVisible(false);
    self->optionsTab->setItemData(hub_tab, hub_panel);

    if (out_result != 0)
    {
        out_result->hub_tab = hub_tab;
        out_result->hub_panel = hub_panel;
        out_result->hub_panel_widget = hub_panel_widget;
    }

    return true;
}

bool TryBuildHubPanel(OptionsWindow* self, HubPanelCreationResult* out_result)
{
    volatile long fault_stage = 0;
    __try
    {
        return BuildHubPanelUnsafe(self, out_result, &fault_stage);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (fault_stage == 1)
        {
            ErrorLog("Emkejs-Mod-Core: EnsureHubPanel UI mutation faulted at stage=add_tab; skipping Mod Hub attach");
        }
        else if (fault_stage == 2)
        {
            ErrorLog("Emkejs-Mod-Core: EnsureHubPanel UI mutation faulted at stage=create_panel; skipping Mod Hub attach");
        }
        else if (fault_stage == 3)
        {
            ErrorLog("Emkejs-Mod-Core: EnsureHubPanel UI mutation faulted at stage=get_widget; skipping Mod Hub attach");
        }
        else if (fault_stage == 4)
        {
            ErrorLog("Emkejs-Mod-Core: EnsureHubPanel UI mutation faulted at stage=finalize; skipping Mod Hub attach");
        }
        else
        {
            ErrorLog("Emkejs-Mod-Core: EnsureHubPanel UI mutation faulted at stage=unknown; skipping Mod Hub attach");
        }
        if (out_result != 0)
        {
            *out_result = HubPanelCreationResult();
        }
        return false;
    }
}

bool IsModHubTabCurrentlySelected(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0)
    {
        return false;
    }

    const size_t selected_index = self->optionsTab->getIndexSelected();
    const MyGUI::UString& selected_name = self->optionsTab->getItemNameAt(selected_index);
    return selected_name == kHubTabName;
}

bool EnsureHubPanel(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0 || g_ptrKenshiGUI == 0 || g_fnCreateDatapanel == 0)
    {
        return false;
    }

    if (g_active_options_window == self && g_active_hub_panel_widget != 0)
    {
        return true;
    }

    HubPanelCreationResult result;
    if (!TryBuildHubPanel(self, &result))
    {
        return false;
    }

    g_active_options_window = self;
    g_active_hub_panel = result.hub_panel;
    g_active_hub_panel_widget = result.hub_panel_widget;
    return true;
}

void OptionsWindowInitHook(OptionsWindow* self)
{
    const bool select_mod_hub_after_init = g_select_mod_hub_on_next_options_init;
    g_select_mod_hub_on_next_options_init = false;

    if (g_fnOptionsInitOrig != 0)
    {
        g_fnOptionsInitOrig(self);
    }

    const bool ensured_hub_panel = !g_hub_enabled || EnsureHubPanel(self);
    if (!ensured_hub_panel)
    {
        HubMenuBridge_OnOptionsWindowInit();
        return;
    }

    HubMenuBridge_OnOptionsWindowInit();
    if (!g_hub_enabled)
    {
        return;
    }

    RebuildHubPanelWidgets();
    if (select_mod_hub_after_init)
    {
        TrySelectModHubTabBestEffort();
    }
    if (g_emc_auto_focus_search_on_open && IsModHubTabCurrentlySelected(self))
    {
        FocusHubSearchBoxBestEffort();
    }
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
    if (g_hub_enabled && HubUi_IsOptionsWindowOpen())
    {
        if (HubUi_IsAnyKeybindCaptureActive())
        {
            if (HubUi_ApplyCapturedKeycodeToActiveRow(static_cast<int32_t>(key_code)) == EMC_OK)
            {
                RebuildHubPanelWidgets();
                return;
            }
        }

        if (HandleHubSearchFocusShortcut(thisptr, key_code))
        {
            return;
        }

        if (HandleHubOpenShortcut(thisptr, key_code))
        {
            return;
        }

        if (HandleHubSearchCtrlShortcut(thisptr, key_code))
        {
            return;
        }
    }
    else if (g_hub_enabled && HandleHubOpenShortcut(thisptr, key_code))
    {
        return;
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

    EnsureEmcSettingsRegistered();
    if (g_active_options_window != 0)
    {
        BindHubTabSelectDelegateBestEffort(g_active_options_window->optionsTab);
    }
    HubUi_SetOptionsWindowOpen(true);
    HubRegistry_SetRegistrationLocked(true);
    HubUi_RebuildSessionModelFromRegistry();
    HubUi_PerformInitialSync();
    if (g_emc_auto_focus_search_on_open)
    {
        g_restore_search_focus_after_rebuild = true;
        g_restore_search_focus_namespace_id.clear();
    }
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
    g_select_mod_hub_on_next_options_init = false;
}
