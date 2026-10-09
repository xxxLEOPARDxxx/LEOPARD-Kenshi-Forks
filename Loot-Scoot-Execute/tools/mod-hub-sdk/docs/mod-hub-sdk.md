# Mod Hub SDK (v1)

Consumer-facing SDK reference for integrating with `Emkejs-Mod-Core` Mod Hub.

Need the shortest integration path first? Start with [Mod Hub SDK Quick Start](mod-hub-sdk-quickstart.md).

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
- Alias usage emits one deprecation warning event per process: `event=hub_get_api_alias_deprecated`.

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

Core value type:

- `EMC_KeybindValueV1 { int32_t keycode; uint32_t modifiers; }`

## Registration Model (Phase 8 SSOT)

`mod_hub_client.h` table schema:

- `emc::ModHubClientSettingKind`
- `emc::ModHubClientSettingRowV1`
- `emc::ModHubClientTableRegistrationV1`
- `emc::RegisterSettingsTableV1(...)`

Supported row kinds:

- `MOD_HUB_CLIENT_SETTING_KIND_BOOL`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND`
- `MOD_HUB_CLIENT_SETTING_KIND_INT`
- `MOD_HUB_CLIENT_SETTING_KIND_FLOAT`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION`

Deterministic helper behavior:

1. `register_mod` first.
2. Row registration in table order.
3. Fail-fast on first non-`EMC_OK`.

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

## Runtime Log Semantics (Hub Events)

Consumers should expect these event formats in RE_Kenshi logs:

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

Optional bool-setting skeleton generation:

```bash
./scripts/init-mod-template.sh --with-hub --hub-bool-setting show_overlay --hub-bool-setting auto_save
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -HubBoolSetting "show_overlay", "auto_save"
```

For larger lists, use a small manifest instead of repeating flags:

```json
{
  "bool_settings": ["show_overlay", "auto_save"]
}
```

```bash
./scripts/init-mod-template.sh --with-hub --hub-settings-manifest ./hub-settings.json
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -HubSettingsManifest .\hub-settings.json
```

This replaces the default single bool example with generated bool state fields,
get/set callbacks, setting definitions, and row entries. Generated setters also
include a small persistence/rollback TODO scaffold so the repetitive save-fail
pattern is harder to miss, now centralized in `emc/mod_hub_consumer_helpers.h`.

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

namespace
{
struct ExampleState
{
    int32_t enabled;
    EMC_KeybindValueV1 hotkey;
    int32_t count;
    float radius;
};

ExampleState g_state = { 1, { 42, 0u }, 10, 2.5f };
emc::ModHubClient g_client;
bool g_client_configured = false;

EMC_Result __cdecl GetEnabled(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    *out_value = state->enabled;
    return EMC_OK;
}

EMC_Result __cdecl SetEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    state->enabled = value != 0 ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl GetHotkey(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    *out_value = state->hotkey;
    return EMC_OK;
}

EMC_Result __cdecl SetHotkey(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    state->hotkey = value;
    return EMC_OK;
}

EMC_Result __cdecl GetCount(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    *out_value = state->count;
    return EMC_OK;
}

EMC_Result __cdecl SetCount(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    state->count = value;
    return EMC_OK;
}

EMC_Result __cdecl GetRadius(void* user_data, float* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    *out_value = state->radius;
    return EMC_OK;
}

EMC_Result __cdecl SetRadius(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ExampleState* state = static_cast<ExampleState*>(user_data);
    state->radius = value;
    return EMC_OK;
}

EMC_Result __cdecl RefreshNow(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)user_data;
    (void)err_buf;
    (void)err_buf_size;
    return EMC_OK;
}

const EMC_ModDescriptorV1 kModDescriptor = {
    "example.mod_hub",
    "Example Mod Hub",
    "example_consumer",
    "Example Consumer",
    &g_state
};

const EMC_BoolSettingDefV1 kBoolSetting = {
    "enabled",
    "Enabled",
    "Enable the feature",
    &g_state,
    &GetEnabled,
    &SetEnabled
};

const EMC_KeybindSettingDefV1 kKeybindSetting = {
    "hotkey",
    "Hotkey",
    "Primary feature hotkey",
    &g_state,
    &GetHotkey,
    &SetHotkey
};

const EMC_IntSettingDefV1 kIntSetting = {
    "count",
    "Count",
    "Example integer setting",
    &g_state,
    0,
    100,
    5,
    &GetCount,
    &SetCount
};

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
    &SetRadius
};

const EMC_ActionRowDefV1 kActionRow = {
    "refresh_now",
    "Refresh now",
    "Re-sync values from runtime state",
    &g_state,
    EMC_ACTION_FORCE_REFRESH,
    &RefreshNow
};

const emc::ModHubClientSettingRowV1 kRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kBoolSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kKeybindSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kIntSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT, &kFloatSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kActionRow }
};

const emc::ModHubClientTableRegistrationV1 kRegistration = {
    &kModDescriptor,
    kRows,
    (uint32_t)(sizeof(kRows) / sizeof(kRows[0]))
};

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

This harness validates numeric snap/clamp behavior and pending-text normalization semantics.

Phase 13:

```powershell
./scripts/phase13_export_contract_stability_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Use `-KenshiPath` when Kenshi runtime DLLs are not already on `PATH`.

Phase 14:

```powershell
./scripts/phase14_options_init_observer_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

Phase 15:

```powershell
./scripts/phase15_scaffold_single_tu_test.ps1
```

Phase 16:

```powershell
./scripts/phase16_hub_attach_reliability_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>] [-RepeatCount 2]
```

Use `-KenshiPath` when Kenshi runtime DLLs are not already on `PATH`.

Phase 17:

```powershell
./scripts/phase17_address_ssot_guard_test.ps1
```

Phase 18:

```powershell
./scripts/phase18_dummy_consumer_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]
```

This smoke harness validates registration count and one bool/int commit path, plus bad-value rejection behavior.

The repo uses local automation for this guard path, so run the Phase 17 guard
script directly or through the recommended local git hooks.
