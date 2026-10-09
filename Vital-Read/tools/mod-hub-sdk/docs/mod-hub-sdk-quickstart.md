# Mod Hub SDK Quick Start (Mod Authors)

Fast path for integrating a mod with `Emkejs-Mod-Core` Mod Hub using the public SDK/helper.

Preferred path for new consumers:

1. Generate `src/mod_hub_consumer_adapter.*` with `--with-hub`.
2. Keep the adapter files as the production path.
3. Use `samples/mod_hub_consumer_single_tu.cpp` as a reference only when you want a single-file example.
4. Replace only the example row IDs you need and implement `PersistExampleModState(...)` as your local persistence seam.

## 1) Scaffold the adapter

From your mod repo root (where `scripts/init-mod-template.*` is available):

```bash
./scripts/init-mod-template.sh --with-hub
```

```powershell
./scripts/init-mod-template.ps1 -WithHub
```

This generates:

- `src/mod_hub_consumer_adapter.h`
- `src/mod_hub_consumer_adapter.cpp`

Optional reference preset:

```bash
./scripts/init-mod-template.sh --with-hub --with-hub-single-tu-sample
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -WithHubSingleTuSample
```

That also generates:

- `samples/mod_hub_consumer_single_tu.cpp`

Use the `samples/mod_hub_consumer_single_tu.cpp` file as a single-file reference only; keep the adapter files as the default integration path.

If you want scaffolded rows instead of hand-editing the default examples, you
can request specific row IDs directly:

```bash
./scripts/init-mod-template.sh --with-hub \
  --hub-bool-setting show_overlay \
  --hub-bool-setting auto_save \
  --hub-keybind-setting toggle_overlay \
  --hub-int-setting max_markers \
  --hub-float-setting search_radius \
  --hub-action-row refresh_cache \
  --hub-select-setting palette \
  --hub-text-setting title \
  --hub-color-setting accent_color
```

```powershell
./scripts/init-mod-template.ps1 -WithHub `
  -HubBoolSetting "show_overlay", "auto_save" `
  -HubKeybindSetting "toggle_overlay" `
  -HubIntSetting "max_markers" `
  -HubFloatSetting "search_radius" `
  -HubActionRow "refresh_cache" `
  -HubSelectSetting "palette" `
  -HubTextSetting "title" `
  -HubColorSetting "accent_color"
```

Bool-only generation still works; the new flags just let you replace the
default keybind/int/float/action examples with named skeletons too, and add
hand-editable select/text/color examples when you need richer rows.

If you already know several row IDs, put them in `hub-settings.json`:

```json
{
  "bool_settings": ["show_overlay", "auto_save"],
  "keybind_settings": ["toggle_overlay"],
  "int_settings": ["max_markers"],
  "float_settings": ["search_radius"],
  "action_rows": ["refresh_cache"],
  "select_settings": ["palette"],
  "text_settings": ["title"],
  "color_settings": ["accent_color"]
}
```

```bash
./scripts/init-mod-template.sh --with-hub --hub-settings-manifest ./hub-settings.json
```

```powershell
./scripts/init-mod-template.ps1 -WithHub -HubSettingsManifest .\hub-settings.json
```

Generated setters now use `ValidateBoolValue`, `ValidateValueInRange`, and
`ApplyUpdateWithRollback` from `emc/mod_hub_consumer_helpers.h`, plus a local
`PersistExampleModState(...)` stub that marks the consumer-owned persistence
seam explicitly.
Most mods only need to replace the example row IDs and implement that
`PersistExampleModState(...)` seam; the helper-backed callback structure can
stay otherwise unchanged.
String-backed text rows can also use `GetStringFieldValue` and
`NormalizeTextValue` from the same helper header instead of hand-writing trim
and bounds checks.

## 2) Set your namespace and mod IDs

Use stable identity values for your mod:

- `namespace_id`: shared bucket for related mods (for example `myteam.qol`)
- `mod_id`: unique within the namespace (for example `faster_looting`)
- `setting_id`: unique within the mod (for example `show_overlay`)

ID format rule for `namespace_id`, `mod_id`, and `setting_id`:

- allowed characters: lowercase `a-z`, digits `0-9`, `_`, `.`, `-`
- disallowed examples: camelCase IDs such as `showSearchEntryCount`
- recommended style: snake_case for `mod_id` / `setting_id`, dotted namespace for `namespace_id`

You can set these during scaffold:

```powershell
./scripts/init-mod-template.ps1 -WithHub `
  -HubNamespaceId "myteam.qol" `
  -HubNamespaceDisplayName "My Team QoL" `
  -HubModId "faster_looting" `
  -HubModDisplayName "Faster Looting"
```

```bash
./scripts/init-mod-template.sh --with-hub \
  --hub-namespace-id "myteam.qol" \
  --hub-namespace-display-name "My Team QoL" \
  --hub-mod-id "faster_looting" \
  --hub-mod-display-name "Faster Looting"
```

Or edit the generated `EMC_ModDescriptorV1` in `src/mod_hub_consumer_adapter.cpp`.

## 3) Register settings rows in one table

In the generated adapter, keep one static table using:

- `emc::ModHubClientSettingRowV1`
- `emc::ModHubClientTableRegistrationV1`

Supported row kinds:

- `MOD_HUB_CLIENT_SETTING_KIND_BOOL`
- `MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND`
- `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_INT`
- `MOD_HUB_CLIENT_SETTING_KIND_INT_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_FLOAT`
- `MOD_HUB_CLIENT_SETTING_KIND_SELECT`
- `MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_TEXT`
- `MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2`
- `MOD_HUB_CLIENT_SETTING_KIND_COLOR`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION`
- `MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2`

Use your existing get/set callbacks in row definitions; the helper performs deterministic registration and commit ordering.
Generated callback wrappers delegate to the shared consumer helper header to reduce repetitive state/update boilerplate.
The scaffold can now generate `EMC_SelectSettingDefV1`, `EMC_TextSettingDefV1`, and `EMC_ColorSettingDefV1` skeletons too. Treat those generated examples as starting points and replace the sample options, text-length contract, validation, presets, and preview mode with your real setting semantics after generation.
Color rows use canonical uppercase `#RRGGBB` values, skip named-color parsing, and can choose `EMC_COLOR_PREVIEW_KIND_SWATCH` or `EMC_COLOR_PREVIEW_KIND_TEXT` per row.
Semantic hover hints are hand-wired today: scaffold output does not emit `hover_hint` fields or V2 hover-row kinds automatically.
Keybind rows preserve the active Ctrl/Shift/Alt state when capture commits a non-modifier key. Read and write `EMC_KeybindValueV1.modifiers` with `EMC_KEYBIND_MODIFIER_CTRL_MASK`, `EMC_KEYBIND_MODIFIER_SHIFT_MASK`, `EMC_KEYBIND_MODIFIER_ALT_MASK`, and `EMC_KEYBIND_MODIFIER_SUPPORTED_MASK`; modifier keys alone are ignored during capture, and Backspace clears the binding.

If you need fewer integer step buttons or exact deltas, switch that row to `EMC_IntSettingDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_INT_V2`.

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
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2, "count", &kCountSettingV2, 0, 0 }
};
```

V2 rows require a host API size of at least `EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE`; otherwise registration fails deterministically instead of silently downgrading to V1.

If you want consumer-owned hover hints on a primary semantic control, switch eligible rows to the new V2 hover surfaces instead of changing the row footer description.

Eligible V2 hover rows:

- `EMC_BoolSettingDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2`
- `EMC_KeybindSettingDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2`
- `EMC_SelectSettingDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2`
- `EMC_TextSettingDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2`
- `EMC_ActionRowDefV2` + `MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2`

API-size gates:

- `EMC_HUB_API_V1_BOOL_SETTING_V2_MIN_SIZE`
- `EMC_HUB_API_V1_KEYBIND_SETTING_V2_MIN_SIZE`
- `EMC_HUB_API_V1_SELECT_SETTING_V2_MIN_SIZE`
- `EMC_HUB_API_V1_TEXT_SETTING_V2_MIN_SIZE`
- `EMC_HUB_API_V1_ACTION_ROW_V2_MIN_SIZE`

The existing `description` field still renders below the row. `hover_hint` is optional and additive.

If one row should hide or disable based on another bool row, register an `EMC_BoolConditionRuleDefV1` after both settings are registered.

- `target_setting_id`: the dependent row
- `controller_setting_id`: the bool row that drives the condition
- `effect`: `EMC_BOOL_CONDITION_EFFECT_HIDE` or `EMC_BOOL_CONDITION_EFFECT_DISABLE`
- `expected_bool_value`: `0` or `1`

Use `emc::RegisterBoolConditionRuleV1(...)` and gate on `EMC_HUB_API_V1_BOOL_CONDITION_RULE_MIN_SIZE` when you need legacy-host compatibility.

## 4) Wire lifecycle calls

Call into the adapter from your plugin lifecycle:

- Startup: `ModHubConsumerAdapter_OnStartup()`
- Options window init (legacy hub compatibility only): `ModHubConsumerAdapter_OnOptionsWindowInit()`
- Local UI fallback gate: `ModHubConsumerAdapter_ShouldCreateLocalTab()` (or `!ModHubConsumerAdapter_UseHubUi()`)

Current hub builds register the retry observer from `ModHubConsumerAdapter_OnStartup()`, so no per-mod options-init RVA hook is required unless you need compatibility with older hubs.

If hub attach/registration fails, helper fallback keeps your local UI path active.

## 5) Build and verify

Recommended checks:

```powershell
./scripts/phase9_init_mod_template_scaffold_test.ps1
./scripts/phase15_scaffold_single_tu_test.ps1
./scripts/phase11_sdk_docs_test.ps1
```

Wall-B-Gone fallback matrix ownership note:

- Canonical Phase 6 harness now lives in the consumer repo: `../Wall-B-Gone/scripts/phase6_wall_b_gone_fallback_test.ps1`.
- `Emkejs-Mod-Core/scripts/phase6_wall_b_gone_fallback_test.ps1` is a delegating wrapper for convenience; it runs the consumer harness when the Wall-B-Gone repo/script and `Wall-B-Gone.dll` are available.

If those phase scripts are not part of your mod repo, validate by building your plugin and confirming options UI behavior in game (hub path when attached, local fallback when hub is unavailable).

SDK sync workflow (no changelog mutation):

```powershell
./scripts/sync-mod-hub-sdk.ps1
```

```bash
./scripts/sync-mod-hub-sdk.sh
```

This command is intentionally scoped to pull + validate only. Changelog/release-note edits are manual.

For full API reference and complete sample code, see [docs/mod-hub-sdk.md](mod-hub-sdk.md).
