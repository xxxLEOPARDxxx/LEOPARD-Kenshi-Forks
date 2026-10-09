# Mod Hub SDK (v1)

Consumer-facing SDK reference for integrating with `Emkejs-Mod-Core` Mod Hub.

Need the shortest integration path first? Start with [Mod Hub SDK Quick Start](mod-hub-sdk-quickstart.md).

Recommended path:

1. Start from `src/mod_hub_consumer_adapter.*`.
2. Keep `samples/mod_hub_consumer_single_tu.cpp` as a reference-only sample.
3. Replace only the example rows you actually need.
4. Keep persistence in one local `PersistExampleModState(...)` seam while shared helpers handle validation and rollback.

## SDK SSOT

Canonical SDK surfaces:

- C ABI header: `include/emc/mod_hub_api.h`
- C++ helper API: `include/emc/mod_hub_client.h`
- Consumer callback helpers: `include/emc/mod_hub_consumer_helpers.h`
- Helper implementation: `src/mod_hub_client.cpp`

Do not include or copy internal hub implementation files (`src/hub_*`) in consumer mods.

## ABI Compatibility Contract

Handshake constants:

- `EMC_HUB_API_VERSION_1`
- `EMC_HUB_API_V1_MIN_SIZE`
- `EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE`
- `EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE`
- `EMC_HUB_API_V1_SELECT_SETTING_MIN_SIZE`
- `EMC_HUB_API_V1_TEXT_SETTING_MIN_SIZE`
- `EMC_HUB_API_V1_COLOR_SETTING_MIN_SIZE`

Handshake entrypoint:

- `EMC_ModHub_GetApi(uint32_t requested_version, uint32_t caller_api_size, const EMC_HubApiV1** out_api, uint32_t* out_api_size)`
- Call with `requested_version = EMC_HUB_API_VERSION_1`.
- Call with `caller_api_size = EMC_HUB_API_V1_MIN_SIZE`.
- On success, `out_api` points to a valid `EMC_HubApiV1` table and `out_api_size` is at least `EMC_HUB_API_V1_MIN_SIZE`.

Observer extension (Phase 14):

- Callback type: `EMC_OptionsWindowInitObserverFn`.
- Feature-detect the observer extension with `out_api_size >= EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE`.
- When present, `EMC_HubApiV1` also exposes:
  - `register_options_window_init_observer`
  - `unregister_options_window_init_observer`
- Callbacks run on the main thread during the options-window-init lifecycle point and must be non-reentrant.

Export stability policy (Phase 13):

- Canonical export symbol is `EMC_ModHub_GetApi` (`EMC_MOD_HUB_GET_API_EXPORT_NAME`).
- Helper lookup tries canonical first, then compatibility aliases.
- Current temporary alias: `EMC_ModHub_GetApi_v1_compat` (`EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME`).
- Alias removal target: `EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET` (`v1.2.0`).
- Alias usage keeps a one-time deprecation-warning guard internally.
- When debug logging is enabled in a Kenshi process, alias usage emits at most one deprecation warning event per process: `event=hub_get_api_alias_deprecated`.

Public result codes (`EMC_Result`):

- `EMC_OK`
- `EMC_ERR_INVALID_ARGUMENT`
- `EMC_ERR_UNSUPPORTED_VERSION`
- `EMC_ERR_API_SIZE_MISMATCH`
- `EMC_ERR_CONFLICT`
- `EMC_ERR_NOT_FOUND`
- `EMC_ERR_CALLBACK_FAILED`
- `EMC_ERR_INTERNAL`

Public value/row constants:

- `EMC_KEY_UNBOUND`
- `EMC_ACTION_FORCE_REFRESH`
- `EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT`
- `EMC_COLOR_PREVIEW_KIND_SWATCH`
- `EMC_COLOR_PREVIEW_KIND_TEXT`

Core value type:

- `EMC_KeybindValueV1 { int32_t keycode; uint32_t modifiers; }`

## Registration Model (Phase 8 SSOT)

`mod_hub_client.h` table schema:

- `emc::ModHubClientSettingKind`
- `emc::ModHubClientSettingRowV1`
- `emc::ModHubClientTableRegistrationV1`
- `emc::RegisterSettingsTableV1(...)`
- `emc::RegisterSettingsTableWithApiSizeV1(...)`

Supported row kinds:

- `MOD_HUB_CLIENT_SETTING_KIND_BOOL`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND`
- `MOD_HUB_CLIENT_SETTING_KIND_INT`
- `MOD_HUB_CLIENT_SETTING_KIND_INT_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_FLOAT`
- `MOD_HUB_CLIENT_SETTING_KIND_SELECT`
- `MOD_HUB_CLIENT_SETTING_KIND_TEXT`
- `MOD_HUB_CLIENT_SETTING_KIND_COLOR`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION`

Deterministic helper behavior:

1. `register_mod` first.
2. Row registration in table order.
3. Fail-fast on first non-`EMC_OK`.

Identity format constraints enforced by the hub registry:

- `namespace_id`, `mod_id`, and `setting_id` accept only lowercase `a-z`, digits `0-9`, `_`, `.`, `-`
- camelCase IDs are rejected with `EMC_ERR_INVALID_ARGUMENT`
- recommended convention: dotted namespace IDs such as `myteam.qol`, snake_case mod/setting IDs such as `faster_looting` and `show_overlay`

## Helper Integration (Phase 7 / Phase 14 SSOT)

`ModHubClient` entrypoints:

- `OnStartup()`
- `OnOptionsWindowInit()`
- `UseHubUi()`

Optional diagnostics accessors:

- `IsAttachRetryPending()`
- `HasAttachRetryAttempted()`
- `LastAttemptFailureResult()`

Soft dependency behavior:

1. Startup attach attempt runs in `OnStartup()`.
2. If observer registration is available, `OnStartup()` auto-registers one hub-owned options-window-init observer for retry.
3. Exactly one retry is allowed after attach failure, delivered by either the hub-owned observer or explicit `OnOptionsWindowInit()`.
4. `OnOptionsWindowInit()` remains a legacy compatibility fallback for older hub builds that only expose `EMC_HUB_API_V1_MIN_SIZE`.
5. If registration fails after attach, helper returns fallback state (`UseHubUi() == false`).

Minimal wiring:

```cpp
emc::ModHubClient::Config config;
config.table_registration = ModHubConsumerAdapter_GetTableRegistration();
client.SetConfig(config);
```

`ModHubClient::Config` defaults the expected SDK stamp to `EMC_HUB_API_VERSION_1` +
`EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE`; override only when you intentionally target a different contract.

SDK stamp warning behavior:

- At startup attach, `ModHubClient` compares the expected stamp (`expected_sdk_api_version`, `expected_sdk_min_api_size`) against runtime `api_version` / `out_api_size`.
- Drift emits a warning and continues using fallback-compatible behavior where possible.

## Custom Int Row Buttons (Phase 20)

New V2 surfaces:

- `EMC_IntSettingDefV2`
- `EMC_HubApiV1::register_int_setting_v2`
- `MOD_HUB_CLIENT_SETTING_KIND_INT_V2`

Behavior:

1. V1 integer rows stay unchanged and keep the multiplier-based `-10/-5/-/+ /+5/+10` profile.
2. V2 integer rows can disable slots with `0`.
3. V2 integer rows apply exact deltas such as `-3` or `+7`.
4. Button captions are derived automatically from the configured deltas.

Validation rules:

1. Non-zero deltas must be positive.
2. Non-zero deltas must be multiples of `step`.
3. Decrement deltas must be strictly descending when read left-to-right, ignoring `0` slots.
4. Increment deltas must be strictly ascending when read left-to-right, ignoring `0` slots.
5. Duplicate non-zero deltas on the same side are rejected.

Fallback behavior:

- If runtime `out_api_size` is smaller than `EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE`, `MOD_HUB_CLIENT_SETTING_KIND_INT_V2` rows fail with `EMC_ERR_API_SIZE_MISMATCH`.
- The helper does not silently downgrade V2 rows to the legacy V1 layout.

Minimal V2 example:

```cpp
const EMC_IntSettingDefV2 kCountSettingV2 = {
    "count",
    "Count",
    "Custom int row",
    &g_state,
    0,
    20,
    1,
    { 3, 0, 1 },
    { 1, 0, 7 },
    &GetCount,
    &SetCount
};

const emc::ModHubClientSettingRowV1 kRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2, &kCountSettingV2 }
};
```

Migration note:

- `EMC_IntSettingDefV1` remains valid and unchanged for legacy rows.
- Use V2 only when you need exact deltas or a reduced button set.

## Select and Text Rows (Phase 25)

New surfaces:

- `EMC_SelectOptionV1`
- `EMC_SelectSettingDefV1`
- `EMC_TextSettingDefV1`
- `EMC_HubApiV1::register_select_setting`
- `EMC_HubApiV1::register_text_setting`
- `MOD_HUB_CLIENT_SETTING_KIND_SELECT`
- `MOD_HUB_CLIENT_SETTING_KIND_TEXT`

Behavior:

1. Select rows model fixed enum-style choices with stable integer values and labels.
2. Text rows model bounded single-line strings with an explicit `max_length`.
3. Helper registration fails deterministically when a runtime host is older than the required API-size gate.

Validation rules:

1. Select rows require at least one option.
2. Select option values must be unique.
3. Select option labels must be non-empty.
4. Text rows require `max_length > 0`.
5. Text rows reject `max_length` above the current host cap (`256`).

Fallback behavior:

- `MOD_HUB_CLIENT_SETTING_KIND_SELECT` requires `EMC_HUB_API_V1_SELECT_SETTING_MIN_SIZE`.
- `MOD_HUB_CLIENT_SETTING_KIND_TEXT` requires `EMC_HUB_API_V1_TEXT_SETTING_MIN_SIZE`.
- Older hosts fail with `EMC_ERR_API_SIZE_MISMATCH`; the helper does not silently remap these rows to another kind.

Minimal example:

```cpp
const EMC_SelectOptionV1 kPaletteOptions[] = {
    { 0, "Default" },
    { 1, "Warm" },
    { 2, "Cool" }
};

const EMC_SelectSettingDefV1 kPaletteSetting = {
    "palette",
    "Palette",
    "Choose a preset palette",
    &g_state,
    kPaletteOptions,
    3u,
    &GetPalette,
    &SetPalette
};

const EMC_TextSettingDefV1 kTitleSetting = {
    "title",
    "Title",
    "Single-line title",
    &g_state,
    64u,
    &GetTitle,
    &SetTitle
};

const emc::ModHubClientSettingRowV1 kRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT, &kPaletteSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT, &kTitleSetting }
};
```

## Color Rows (Phase 26)

New surfaces:

- `EMC_ColorPresetV1`
- `EMC_ColorSettingDefV1`
- `EMC_HubApiV1::register_color_setting`
- `MOD_HUB_CLIENT_SETTING_KIND_COLOR`
- `EMC_COLOR_PREVIEW_KIND_SWATCH`
- `EMC_COLOR_PREVIEW_KIND_TEXT`

Behavior:

1. Color rows store one canonical RGB value: uppercase `#RRGGBB`.
2. The current UI slice is palette-backed. Consumers can omit `presets` to use the core-owned default palette, or supply an override palette per row.
3. `preview_kind` chooses whether the row preview renders as a swatch chip or sample text.
4. Named colors are not part of the contract; the row value is always hex.
5. Helper registration fails deterministically when a runtime host is older than the required API-size gate.

Validation rules:

1. `preview_kind` must be `EMC_COLOR_PREVIEW_KIND_SWATCH` or `EMC_COLOR_PREVIEW_KIND_TEXT`.
2. Preset values are normalized to canonical uppercase `#RRGGBB` during registration.
3. Duplicate presets after normalization are rejected.
4. Preset labels must be non-empty when provided.
5. The row callback contract is RGB-only; alpha and named-color parsing are not part of the Mod Hub color row surface.

Fallback behavior:

- `MOD_HUB_CLIENT_SETTING_KIND_COLOR` requires `EMC_HUB_API_V1_COLOR_SETTING_MIN_SIZE`.
- Older hosts fail with `EMC_ERR_API_SIZE_MISMATCH`; the helper does not silently remap color rows to text or another row kind.

Minimal example:

```cpp
const EMC_ColorPresetV1 kRelationColorPresets[] = {
    { "#FF3333", "Enemy" },
    { "#DEE85A", "Ally" },
    { "#40FF40", "Squad" }
};

const EMC_ColorSettingDefV1 kRelationColorSetting = {
    "enemy_color_hex",
    "Enemy color",
    "Relation color used for enemy markers and tint",
    &g_state,
    EMC_COLOR_PREVIEW_KIND_TEXT,
    kRelationColorPresets,
    3u,
    &GetEnemyColor,
    &SetEnemyColor
};

const emc::ModHubClientSettingRowV1 kRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR, &kRelationColorSetting }
};
```

## Runtime Log Semantics (Hub Events)

Consumers should expect these event formats in RE_Kenshi logs when the corresponding code path is active. The alias deprecation event is debug-only and appears only when debug logging is enabled:

```text
event=hub_commit_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_commit_summary attempted=<n> succeeded=<n> failed=<n> skipped=<0|1> reason=<text_or_none>
event=hub_ui_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_registration_warning namespace=<id> mod=<id> setting=<id> field=<name> message=<text>
event=hub_registration_rejected api=<name> reason=<text> result=<code> message=<text>
event=hub_setting_registration_conflict namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_action_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_action_refresh_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_commit_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>
event=hub_get_api_alias_deprecated alias=<symbol> canonical=EMC_ModHub_GetApi removal_target=<release>
```

## Phase 9 Scaffold Command

Generate starter adapter files:

```bash
./scripts/init-mod-template.sh --with-hub
```

```powershell
./scripts/init-mod-template.ps1 -WithHub
```

Generated files:

- `src/mod_hub_consumer_adapter.h`
- `src/mod_hub_consumer_adapter.cpp`

Optional richer preset:

```bash
./scripts/init-mod-template.sh --with-hub --with-hub-single-tu-sample
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -WithHubSingleTuSample
```

Additional generated file:

- `samples/mod_hub_consumer_single_tu.cpp`

Optional typed row skeleton generation:

```bash
./scripts/init-mod-template.sh --with-hub \
  --hub-bool-setting show_overlay \
  --hub-bool-setting auto_save \
  --hub-keybind-setting toggle_overlay \
  --hub-int-setting max_markers \
  --hub-float-setting search_radius \
  --hub-action-row refresh_cache
```

```powershell
./scripts/init-mod-template.ps1 -WithHub `
  -HubBoolSetting "show_overlay", "auto_save" `
  -HubKeybindSetting "toggle_overlay" `
  -HubIntSetting "max_markers" `
  -HubFloatSetting "search_radius" `
  -HubActionRow "refresh_cache"
```

Bool-only generation remains valid. The extra flags simply replace the default
keybind/int/float/action examples with named scaffold rows when you want them.

For larger lists, use a small manifest instead of repeating flags:

```json
{
  "bool_settings": ["show_overlay", "auto_save"],
  "keybind_settings": ["toggle_overlay"],
  "int_settings": ["max_markers"],
  "float_settings": ["search_radius"],
  "action_rows": ["refresh_cache"]
}
```

```bash
./scripts/init-mod-template.sh --with-hub --hub-settings-manifest ./hub-settings.json
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -HubSettingsManifest .\hub-settings.json
```

This replaces the per-kind example rows you requested with generated state
fields, callbacks, setting definitions, and row entries. Generated setters now
use `ValidateBoolValue`, `ValidateValueInRange`, and
`ApplyUpdateWithRollback(...)` from `emc/mod_hub_consumer_helpers.h`, while a
local `PersistExampleModState(...)` stub keeps the persistence boundary visible
and consumer-owned.

Migration note:

- Existing scaffold output remains valid.
- Keep `src/mod_hub_consumer_adapter.*` as the default integration path.
- Use `samples/mod_hub_consumer_single_tu.cpp` as a public-assets-only reference sample when you want a single-file starting point.
- Do not add per-mod options-init hook RVAs to new scaffold output; current hubs already retry through the observer path from `OnStartup()`.
- Do not add any new consumer-local Kenshi hook RVAs; the remaining core literals are tracked in `docs/addresses/*.md` (including `docs/addresses/kenshi_1_0_65_x64.md`) and guarded by `./scripts/phase17_address_ssot_guard_test.ps1`.
- Wall-B-Gone Phase 6 fallback matrix harness is consumer-owned in `../Wall-B-Gone/scripts/phase6_wall_b_gone_fallback_test.ps1`; the Mod-Core `scripts/phase6_wall_b_gone_fallback_test.ps1` entrypoint delegates to that harness when present.

## SDK Sync Command

Per-consumer sync command (pull + validate only):

```powershell
./scripts/sync-mod-hub-sdk.ps1
```

```bash
./scripts/sync-mod-hub-sdk.sh
```

Optional validation-only mode:

```powershell
./scripts/sync-mod-hub-sdk.ps1 -SkipPull
```

```bash
./scripts/sync-mod-hub-sdk.sh --skip-pull
```

Behavior constraints:

- Pulls the SDK submodule when `-SkipPull` is not used.
- Validates required SDK contract assets and key ABI symbols.
- Does not edit changelog or release-note files; changelog updates remain manual.
- Sync command does not edit changelog entries automatically.

## Minimal Consumer Sample (Phase 11)

Packaged SDK sample assets:

- `samples/minimal/mod_hub_consumer_adapter.h`
- `samples/minimal/mod_hub_consumer_adapter.cpp`
- `samples/single-tu/mod_hub_consumer_single_tu.cpp`

Header snippet:

<!-- PHASE11_SAMPLE_HEADER_BEGIN -->
```cpp
#ifndef MOD_HUB_CONSUMER_ADAPTER_H
#define MOD_HUB_CONSUMER_ADAPTER_H

#include "emc/mod_hub_client.h"

// Integration wiring:
// 1) Call ModHubConsumerAdapter_OnStartup() from startPlugin.
// 2) Call ModHubConsumerAdapter_OnOptionsWindowInit() only when you need legacy compatibility with older hub builds that do not expose observer registration.
// 3) Guard local tab creation with ModHubConsumerAdapter_ShouldCreateLocalTab().

void ModHubConsumerAdapter_OnStartup();
void ModHubConsumerAdapter_OnOptionsWindowInit();

bool ModHubConsumerAdapter_UseHubUi();
bool ModHubConsumerAdapter_ShouldCreateLocalTab();
bool ModHubConsumerAdapter_IsAttachRetryPending();
bool ModHubConsumerAdapter_HasAttachRetryAttempted();
EMC_Result ModHubConsumerAdapter_LastAttachFailureResult();

const emc::ModHubClientTableRegistrationV1* ModHubConsumerAdapter_GetTableRegistration();

#endif
```
<!-- PHASE11_SAMPLE_HEADER_END -->

Source snippet:

<!-- PHASE11_SAMPLE_SOURCE_BEGIN -->
```cpp
#include "mod_hub_consumer_adapter.h"
#include "emc/mod_hub_consumer_helpers.h"

namespace
{
struct ExampleModState
{
    int32_t enabled;
    EMC_KeybindValueV1 hotkey;
    int32_t count;
    float radius;
};

ExampleModState g_state = {
    1,
    { 42, 0u },
    10,
    2.5f};
emc::ModHubClient g_client;
bool g_client_configured = false;

bool PersistExampleModState(const ExampleModState& next_state)
{
    (void)next_state;
    // TODO: Persist next_state to your config store and return false on failure.
    return true;
}

template <typename UpdateFn>
EMC_Result ApplyExampleModStateUpdate(
    void* user_data,
    char* err_buf,
    uint32_t err_buf_size,
    UpdateFn update)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_state");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleModState* state = static_cast<ExampleModState*>(user_data);
    const ExampleModState previous = *state;
    ExampleModState updated = previous;
    update(updated);

    return emc::consumer::ApplyUpdateWithRollback(
        previous,
        updated,
        err_buf,
        err_buf_size,
        [state](const ExampleModState& snapshot) {
            *state = snapshot;
        },
        &PersistExampleModState);
}

EMC_Result __cdecl GetEnabled(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetBoolFieldValue(user_data, out_value, &ExampleModState::enabled);
}

EMC_Result __cdecl SetEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result boolValidation = emc::consumer::ValidateBoolValue(value, err_buf, err_buf_size);
    if (boolValidation != EMC_OK)
    {
        return boolValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.enabled = value != 0 ? 1 : 0;
        });
}

EMC_Result __cdecl GetHotkey(void* user_data, EMC_KeybindValueV1* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::hotkey);
}

EMC_Result __cdecl SetHotkey(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
{
    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.hotkey = value;
        });
}

EMC_Result __cdecl GetCount(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::count);
}

EMC_Result __cdecl SetCount(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result rangeValidation = emc::consumer::ValidateValueInRange<int32_t>(
        value,
        0,
        100,
        err_buf,
        err_buf_size);
    if (rangeValidation != EMC_OK)
    {
        return rangeValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.count = value;
        });
}

EMC_Result __cdecl GetRadius(void* user_data, float* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::radius);
}

EMC_Result __cdecl SetRadius(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result rangeValidation = emc::consumer::ValidateValueInRange<float>(
        value,
        0.0f,
        10.0f,
        err_buf,
        err_buf_size);
    if (rangeValidation != EMC_OK)
    {
        return rangeValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.radius = value;
        });
}

EMC_Result __cdecl RefreshNow(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    return emc::consumer::ActionNoopSuccess(user_data, err_buf, err_buf_size);
}

const EMC_ModDescriptorV1 kModDescriptor = {
    "example.mod_hub",
    "Example Mod Hub",
    "example_consumer",
    "Example Consumer",
    &g_state };

const EMC_BoolSettingDefV1 kBoolSettingEnabled = {
    "enabled",
    "Enabled",
    "Generated bool setting for Enabled.",
    &g_state,
    &GetEnabled,
    &SetEnabled };

const EMC_KeybindSettingDefV1 kKeybindSetting = {
    "hotkey",
    "Hotkey",
    "Primary feature hotkey",
    &g_state,
    &GetHotkey,
    &SetHotkey };

const EMC_IntSettingDefV1 kIntSetting = {
    "count",
    "Count",
    "Example integer setting",
    &g_state,
    0,
    100,
    5,
    &GetCount,
    &SetCount };

const EMC_FloatSettingDefV1 kFloatSetting = {
    "radius",
    "Radius",
    "Example float setting",
    &g_state,
    0.0f,
    10.0f,
    0.5f,
    EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT,
    &GetRadius,
    &SetRadius };

const EMC_ActionRowDefV1 kActionRow = {
    "refresh_now",
    "Refresh now",
    "Re-sync values from runtime state",
    &g_state,
    EMC_ACTION_FORCE_REFRESH,
    &RefreshNow };

const emc::ModHubClientSettingRowV1 kRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kBoolSettingEnabled },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kKeybindSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kIntSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT, &kFloatSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kActionRow }
};

const emc::ModHubClientTableRegistrationV1 kRegistration = {
    &kModDescriptor,
    kRows,
    (uint32_t)(sizeof(kRows) / sizeof(kRows[0])) };

void EnsureClientConfigured()
{
    if (g_client_configured)
    {
        return;
    }

    emc::ModHubClient::Config config;
    config.table_registration = &kRegistration;
    g_client.SetConfig(config);
    g_client_configured = true;
}
}

void ModHubConsumerAdapter_OnStartup()
{
    EnsureClientConfigured();
    g_client.OnStartup();
}

void ModHubConsumerAdapter_OnOptionsWindowInit()
{
    EnsureClientConfigured();
    g_client.OnOptionsWindowInit();
}

bool ModHubConsumerAdapter_UseHubUi()
{
    EnsureClientConfigured();
    return g_client.UseHubUi();
}

bool ModHubConsumerAdapter_ShouldCreateLocalTab()
{
    return !ModHubConsumerAdapter_UseHubUi();
}

bool ModHubConsumerAdapter_IsAttachRetryPending()
{
    EnsureClientConfigured();
    return g_client.IsAttachRetryPending();
}

bool ModHubConsumerAdapter_HasAttachRetryAttempted()
{
    EnsureClientConfigured();
    return g_client.HasAttachRetryAttempted();
}

EMC_Result ModHubConsumerAdapter_LastAttachFailureResult()
{
    EnsureClientConfigured();
    return g_client.LastAttemptFailureResult();
}

const emc::ModHubClientTableRegistrationV1* ModHubConsumerAdapter_GetTableRegistration()
{
    return &kRegistration;
}
```
<!-- PHASE11_SAMPLE_SOURCE_END -->

## Phase 10 Versioned SDK Package

Build standalone SDK asset:

```powershell
./scripts/package-sdk.ps1
```

Build mod zip + SDK zip together:

```powershell
./scripts/build-and-package.ps1
```

Run the Debug reliability smoke matrix through the standard build wrapper:

```powershell
./scripts/build-and-package.ps1 -Configuration Debug -SkipSdkPackage -RunReliabilitySmoke [-SmokeKenshiPath <path-to-Kenshi>]
```

Use `-SmokeKenshiPath` when Kenshi runtime DLLs are not already on `PATH`.

Default SDK zip:

- `dist/Emkejs-Mod-Core-SDK-<VERSION>.zip`

## Validation Scripts

Phase 8:

```powershell
./scripts/phase8_mod_hub_client_table_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll>
```

Phase 9:

```powershell
./scripts/phase9_init_mod_template_scaffold_test.ps1
```

Phase 10:

```powershell
./scripts/phase10_sdk_packaging_test.ps1
```

Phase 11:

```powershell
./scripts/phase11_sdk_docs_test.ps1
```

Phase 5:

```powershell
./scripts/phase5_numeric_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

This harness validates numeric snap/clamp behavior and pending-text normalization semantics.

Phase 20:

```powershell
./scripts/phase20_int_button_layout_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

This harness validates V2 int-row registration, sparse/custom button layouts, exact-delta behavior, and deterministic rejection of invalid layouts.

Phase 25:

```powershell
./scripts/phase25_select_text_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

This harness validates select/text registration, pending-state updates, bounded text rejection, and select/text commit resync.

Phase 26:

```powershell
./scripts/phase26_color_row_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

This harness validates color-row registration, default/override palettes, duplicate normalized preset rejection, pending-color normalization, palette expansion state, and save-time color commit resync.

Phase 13:

```powershell
./scripts/phase13_export_contract_stability_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Use `-KenshiPath` when Kenshi runtime DLLs are not already on `PATH`.
Canonical + compatibility export validation works with Release or Debug DLLs; helper default-lookup and alias-warning-count assertions run only when Debug test exports are present.

Phase 14:

```powershell
./scripts/phase14_options_init_observer_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

Phase 15:

```powershell
./scripts/phase15_scaffold_single_tu_test.ps1
```

Phase 16:

```powershell
./scripts/phase16_hub_attach_reliability_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>] [-RepeatCount 2]
```

Use `-KenshiPath` when Kenshi runtime DLLs are not already on `PATH`.
Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

Phase 17:

```powershell
./scripts/phase17_address_ssot_guard_test.ps1
```

Phase 18:

```powershell
./scripts/phase18_dummy_consumer_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.

This smoke harness validates registration count and one bool/int commit path, plus bad-value rejection behavior.

The repo uses local automation for this guard path, so run the Phase 17 guard
script directly or through the recommended local git hooks.
