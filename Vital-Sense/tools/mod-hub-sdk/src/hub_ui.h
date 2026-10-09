#ifndef EMC_HUB_UI_H
#define EMC_HUB_UI_H

#include "emc/mod_hub_api.h"

enum HubUiRowKind
{
    HUB_UI_ROW_KIND_BOOL = 0,
    HUB_UI_ROW_KIND_KEYBIND = 1,
    HUB_UI_ROW_KIND_ACTION = 2,
    HUB_UI_ROW_KIND_INT = 3,
    HUB_UI_ROW_KIND_FLOAT = 4,
    HUB_UI_ROW_KIND_SELECT = 5,
    HUB_UI_ROW_KIND_TEXT = 6,
    HUB_UI_ROW_KIND_COLOR = 7
};

struct HubUiRowView
{
    void* token;
    int32_t kind;
    bool dirty;
    bool capture_active;

    const char* namespace_id;
    const char* namespace_display_name;
    const char* mod_id;
    const char* mod_display_name;
    const char* setting_id;
    const char* label;
    const char* description;
    const char* hover_hint;
    const char* inline_error;
    const char* section_id;
    const char* section_display_name;

    void* user_data;
    EMC_GetBoolCallback get_bool;
    EMC_SetBoolCallback set_bool;
    int32_t pending_bool_value;
    EMC_GetKeybindCallback get_keybind;
    EMC_SetKeybindCallback set_keybind;
    EMC_KeybindValueV1 pending_keybind_value;

    EMC_GetIntCallback get_int;
    EMC_SetIntCallback set_int;
    int32_t int_min_value;
    int32_t int_max_value;
    int32_t int_step;
    bool int_use_custom_buttons;
    int32_t int_dec_button_deltas[3];
    int32_t int_inc_button_deltas[3];
    int32_t pending_int_value;
    const char* pending_int_text;
    bool int_text_parse_error;

    EMC_GetFloatCallback get_float;
    EMC_SetFloatCallback set_float;
    float float_min_value;
    float float_max_value;
    float float_step;
    uint32_t float_display_decimals;
    float pending_float_value;
    const char* pending_float_text;
    bool float_text_parse_error;

    EMC_GetSelectCallback get_select;
    EMC_SetSelectCallback set_select;
    const EMC_SelectOptionV1* select_options;
    uint32_t select_option_count;
    int32_t pending_select_value;

    EMC_GetTextCallback get_text;
    EMC_SetTextCallback set_text;
    uint32_t text_max_length;
    const char* pending_text;
    const char* pending_color_text;
    bool color_text_parse_error;

    uint32_t color_preview_kind;
    const EMC_ColorPresetV1* color_presets;
    uint32_t color_preset_count;
    bool color_hex_mode;
    bool color_palette_expanded;
};

void HubUi_SetOptionsWindowOpen(bool is_open);
bool HubUi_IsOptionsWindowOpen();

void HubUi_ClearSessionModel();
void HubUi_RebuildSessionModelFromRegistry();
void HubUi_PerformInitialSync();
void HubUi_SetSearchPersistenceEnabled(bool is_enabled);
bool HubUi_IsSearchPersistenceEnabled();
void HubUi_SetCollapsePersistenceEnabled(bool is_enabled);
bool HubUi_IsCollapsePersistenceEnabled();

bool HubUi_IsAnyKeybindCaptureActive();

EMC_Result HubUi_SetModCollapsed(const char* namespace_id, const char* mod_id, bool is_collapsed);
bool HubUi_GetModCollapsed(const char* namespace_id, const char* mod_id, bool* out_is_collapsed);
EMC_Result HubUi_SetSectionCollapsed(const char* namespace_id, const char* mod_id, const char* section_id, bool is_collapsed);
bool HubUi_GetSectionCollapsed(const char* namespace_id, const char* mod_id, const char* section_id, bool* out_is_collapsed);
EMC_Result HubUi_SetNamespaceSearchQuery(const char* namespace_id, const char* search_query);
bool HubUi_GetNamespaceSearchQuery(const char* namespace_id, const char** out_search_query);
bool HubUi_DoesRowMatchNamespaceSearch(const HubUiRowView* row);
bool HubUi_DoesSettingMatchNamespaceSearch(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    bool* out_matches);

EMC_Result HubUi_SetPendingBool(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t value);
EMC_Result HubUi_AdjustPendingIntStep(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t step_delta);
EMC_Result HubUi_AdjustPendingIntDelta(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t delta);
EMC_Result HubUi_SetPendingIntFromText(const char* namespace_id, const char* mod_id, const char* setting_id, const char* text);
EMC_Result HubUi_NormalizePendingIntText(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_AdjustPendingFloatStep(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t step_delta);
EMC_Result HubUi_SetPendingFloatFromText(const char* namespace_id, const char* mod_id, const char* setting_id, const char* text);
EMC_Result HubUi_NormalizePendingFloatText(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_SetPendingSelect(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t value);
EMC_Result HubUi_SetPendingText(const char* namespace_id, const char* mod_id, const char* setting_id, const char* text);
EMC_Result HubUi_SetPendingColor(const char* namespace_id, const char* mod_id, const char* setting_id, const char* value);
EMC_Result HubUi_SetPendingColorFromText(const char* namespace_id, const char* mod_id, const char* setting_id, const char* text);
EMC_Result HubUi_NormalizePendingColorText(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_SetColorHexMode(const char* namespace_id, const char* mod_id, const char* setting_id, bool is_hex_mode);
EMC_Result HubUi_SetColorPaletteExpanded(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    bool is_expanded);
EMC_Result HubUi_BeginKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_CancelKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_ApplyCapturedKeycode(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t keycode);
EMC_Result HubUi_ApplyCapturedKeycodeToActiveRow(int32_t keycode);
EMC_Result HubUi_ClearPendingKeybind(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_InvokeActionRow(const char* namespace_id, const char* mod_id, const char* setting_id);

uint32_t HubUi_GetRowCount();
bool HubUi_GetRowViewByIndex(uint32_t index, HubUiRowView* out_view);

void HubUi_OnCommitSetFailure(void* token, const char* message);
void HubUi_OnCommitSyncBool(void* token, int32_t canonical_value);
void HubUi_OnCommitSyncKeybind(void* token, EMC_KeybindValueV1 canonical_value);
void HubUi_OnCommitSyncInt(void* token, int32_t canonical_value);
void HubUi_OnCommitSyncFloat(void* token, float canonical_value);
void HubUi_OnCommitSyncSelect(void* token, int32_t canonical_value);
void HubUi_OnCommitSyncText(void* token, const char* canonical_value);
void HubUi_OnCommitSyncColor(void* token, const char* canonical_value);

#endif
