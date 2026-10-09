# Emkejs Mod Core Hub v1 (Revised KISS Plan)

## Summary
Build `Emkejs-Mod-Core` into a reusable settings hub with:
1. A stable C ABI SDK for third-party mods.
2. One tab per namespace.
3. Clear per-mod collapsible sections.
4. Search at the top of each namespace tab (per-tab scope, v1 decision).
5. V1 setting rows: `bool`, `keybind`, `int`, `float`, `action row`.
6. Wall-B-Gone migrated as first real consumer with soft dependency and one retry attach.

## Scope And Non-Goals
1. In scope: API export, registry, hub UI, filtering, per-mod grouping, Wall-B-Gone migration.
2. In scope: soft dependency behavior and deterministic fallback.
3. Out of scope: enum setting rows, global cross-tab search, dynamic unload workflows, external hot-reload for out-of-band config changes while options window is already open, and hub-level keybind conflict detection against game/other mods.

## Canonical Requirement Update (Traceability)
1. Requirement #5 is explicitly interpreted as per-namespace-tab search in v1.
2. Global search across all tabs is deferred to v2.

## ABI Appendix (SSOT)
This section is the single source of truth for the public ABI contract.

### 1) Version Handshake And Struct Size
1. Export:
`extern "C" __declspec(dllexport) EMC_Result __cdecl EMC_ModHub_GetApi(uint32_t requested_version, uint32_t caller_api_size, const EMC_HubApiV1** out_api, uint32_t* out_api_size);`
2. `EMC_HubApiV1` leading fields are:
`uint32_t api_version;`
`uint32_t api_size;`
full table shape is defined in ABI Appendix Section 8.
3. Caller passes `requested_version` and `caller_api_size` to support forward-compatible growth.
4. In v1 SDK, `EMC_HUB_API_V1_MIN_SIZE` is explicitly defined as `sizeof(EMC_HubApiV1)`.
5. Hub returns `out_api_size` and fails with explicit version/size mismatch errors when incompatible.
6. Version compatibility rule:
if `requested_version != 1`, return `EMC_ERR_UNSUPPORTED_VERSION`.
7. API size compatibility rule:
if `caller_api_size < EMC_HUB_API_V1_MIN_SIZE`, return `EMC_ERR_API_SIZE_MISMATCH`.
8. On success, hub returns v1 table pointer and sets `out_api_size = sizeof(EMC_HubApiV1)`.
9. Out-parameter contract:
`out_api` and `out_api_size` are required and must be non-null.
10. If either required out pointer is null, return `EMC_ERR_INVALID_ARGUMENT` and do not dereference missing pointers.
11. On non-argument failure paths, hub sets `*out_api = nullptr` and `*out_api_size = 0` before returning an error.

### 2) Public Result Codes
`EMC_Result` is `int32_t` with fixed values:
1. `EMC_OK = 0`
2. `EMC_ERR_INVALID_ARGUMENT = 1`
3. `EMC_ERR_UNSUPPORTED_VERSION = 2`
4. `EMC_ERR_API_SIZE_MISMATCH = 3`
5. `EMC_ERR_CONFLICT = 4`
6. `EMC_ERR_NOT_FOUND = 5`
7. `EMC_ERR_CALLBACK_FAILED = 6`
8. `EMC_ERR_INTERNAL = 7`
9. Usage note:
`EMC_ERR_CALLBACK_FAILED` is valid for callback return paths; hub export entrypoints (`EMC_ModHub_GetApi`, `register_*`) do not return it in v1.

### 3) Callback Signatures And Error Propagation
1. All callbacks return `EMC_Result`.
2. Callback typedefs (v1, fixed-width types only):
```c
typedef EMC_Result (__cdecl *EMC_GetBoolCallback)(void* user_data, int32_t* out_value);
typedef EMC_Result (__cdecl *EMC_SetBoolCallback)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result (__cdecl *EMC_GetKeybindCallback)(void* user_data, EMC_KeybindValueV1* out_value);
typedef EMC_Result (__cdecl *EMC_SetKeybindCallback)(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result (__cdecl *EMC_GetIntCallback)(void* user_data, int32_t* out_value);
typedef EMC_Result (__cdecl *EMC_SetIntCallback)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result (__cdecl *EMC_GetFloatCallback)(void* user_data, float* out_value);
typedef EMC_Result (__cdecl *EMC_SetFloatCallback)(void* user_data, float value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result (__cdecl *EMC_ActionRowCallback)(void* user_data, char* err_buf, uint32_t err_buf_size);
```
3. `bool` callback values use `0` or `1`; other values are invalid.
4. `get_*` callbacks write to out parameter and return `EMC_OK` on success.
5. `set_*` and action callbacks accept caller-provided error buffer (`char* err_buf`, `uint32_t err_buf_size`) and return non-zero on failure.
6. Callback nullability contract:
`get_*` out-value pointers are required and must be non-null; otherwise callback returns `EMC_ERR_INVALID_ARGUMENT`.
7. Error-buffer contract:
`err_buf` may be null only when `err_buf_size == 0`; callbacks must not write when buffer is null or size is zero.
8. `mod_user_data` is not passed into setting callbacks automatically; callbacks receive only the setting `user_data` pointer.
9. Hub logs callback failure with mod/setting context and callback-provided error text when available.
10. On `set_*` failure during save: hub keeps pending UI value dirty, does not overwrite with old value, and continues committing remaining settings.
11. Commit is not transactional in v1; successful commits are not rolled back if a later setting fails.

### 4) ABI Type Rules
1. C ABI only (`extern "C"`), `__cdecl` calling convention.
2. Fixed-width integer/float types only across boundary.
3. No STL, no C++ references, no `bool`, and no compiler-dependent enums across boundary.
4. Public enums in headers are represented as `int32_t` constants.
5. v1 SDK ABI target is MSVC x64 builds only.
6. All hub API calls and consumer callbacks must run on the main thread only; re-entrancy is unsupported.

### 5) Packing, Alignment, And Compile-Time Guards
1. Public SDK structs must not be wrapped in packing pragmas.
2. SDK header defines compile-time ABI guards for every public struct:
`static_assert(sizeof(...))` and `static_assert(offsetof(..., field) == ...)` for all public fields.
3. Build configurations that violate these ABI guards are unsupported for v1.
4. Header publishes `EMC_HUB_API_V1_MIN_SIZE` for runtime handshake checks.
5. SDK uses a dedicated `ABI_GUARDS` block per public struct so guard coverage is explicit and reviewable.
6. SDK consumers must compile with MSVC x64 and default struct alignment; `/Zp` packing overrides are unsupported.

### 6) String, Encoding, And Search Rules
1. Display strings (`namespace_display_name`, `mod_display_name`, labels, descriptions, error text) allow UTF-8.
2. ID fields remain restricted to `^[a-z0-9_.-]{1,64}$`.
3. Search behavior: ASCII case-insensitive matching for ASCII codepoints; non-ASCII bytes are matched literally (no locale folding in v1).

### 7) Pointer Lifetime Rules
1. Hub deep-copies all descriptor strings during registration.
2. Hub stores `mod_user_data` and per-setting `user_data` as raw opaque pointers.
3. Caller owns those pointers and must keep pointed data valid for mod/plugin lifetime (or until future unregister support exists).

### 8) Public ABI Struct Shapes (v1)
1. `EMC_HubApiV1` function table includes all callable entrypoints:
```c
typedef struct EMC_HubApiV1 {
  uint32_t api_version;
  uint32_t api_size;
  EMC_Result (__cdecl *register_mod)(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle);
  EMC_Result (__cdecl *register_bool_setting)(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def);
  EMC_Result (__cdecl *register_keybind_setting)(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);
  EMC_Result (__cdecl *register_int_setting)(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);
  EMC_Result (__cdecl *register_float_setting)(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);
  EMC_Result (__cdecl *register_action_row)(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);
} EMC_HubApiV1;
```
2. Common identity scope:
`namespace_id` is globally unique in the hub process;
`mod_id` is unique within `namespace_id`;
`setting_id` is unique within `mod_id`.
3. `EMC_ModDescriptorV1` fields:
`const char* namespace_id;`
`const char* namespace_display_name;`
`const char* mod_id;`
`const char* mod_display_name;`
`void* mod_user_data;`
4. `EMC_BoolSettingDefV1` fields:
`const char* setting_id;`
`const char* label;`
`const char* description;`
`void* user_data;`
`EMC_GetBoolCallback get_value;`
`EMC_SetBoolCallback set_value;`
5. `EMC_KeybindValueV1` fields:
`int32_t keycode;`
`uint32_t modifiers;`
6. `EMC_KeybindSettingDefV1` fields:
`const char* setting_id;`
`const char* label;`
`const char* description;`
`void* user_data;`
`EMC_GetKeybindCallback get_value;`
`EMC_SetKeybindCallback set_value;`
7. `EMC_IntSettingDefV1` fields:
`const char* setting_id;`
`const char* label;`
`const char* description;`
`void* user_data;`
`int32_t min_value;`
`int32_t max_value;`
`int32_t step;`
`EMC_GetIntCallback get_value;`
`EMC_SetIntCallback set_value;`
8. `EMC_FloatSettingDefV1` fields:
`const char* setting_id;`
`const char* label;`
`const char* description;`
`void* user_data;`
`float min_value;`
`float max_value;`
`float step;`
`uint32_t display_decimals;`
`EMC_GetFloatCallback get_value;`
`EMC_SetFloatCallback set_value;`
9. `EMC_ActionRowDefV1` fields:
`const char* setting_id;`
`const char* label;`
`const char* description;`
`void* user_data;`
`uint32_t action_flags;`
`EMC_ActionRowCallback on_action;`
10. Descriptor validation constraints:
`int32_t step >= 1`, `float step > 0`, `min_value <= max_value`, and `display_decimals` must be in `0..3`.

### 9) Descriptor Equality (Exact Match) SSOT
1. String comparisons use byte-equal UTF-8 (`strcmp` style).
2. Pointer comparisons use pointer equality (same address): callback pointers, `user_data`, `mod_user_data`.
3. Integer fields compare by exact value; float fields compare by exact IEEE-754 bit pattern in memory.
Near-equal float metadata values with different bit patterns are treated as non-exact and may produce warning-only re-registration drift logs.
4. `EMC_ModDescriptorV1` exact match fields:
`namespace_id`, `namespace_display_name`, `mod_id`, `mod_display_name`, `mod_user_data`.
5. `EMC_BoolSettingDefV1` exact match fields:
`setting_id`, `label`, `description`, `user_data`, `get_value`, `set_value`.
6. `EMC_KeybindSettingDefV1` exact match fields:
`setting_id`, `label`, `description`, `user_data`, `get_value`, `set_value`.
7. `EMC_IntSettingDefV1` exact match fields:
`setting_id`, `label`, `description`, `user_data`, `min_value`, `max_value`, `step`, `get_value`, `set_value`.
8. `EMC_FloatSettingDefV1` exact match fields:
`setting_id`, `label`, `description`, `user_data`, `min_value`, `max_value`, `step`, `display_decimals`, `get_value`, `set_value`.
9. `EMC_ActionRowDefV1` exact match fields:
`setting_id`, `label`, `description`, `user_data`, `action_flags`, `on_action`.
10. Registration outcome mapping:
exact match => `EMC_OK` without warning;
identity match with non-identity differences => `EMC_OK` with warning and keep first canonical descriptor values;
identity mismatch => `EMC_ERR_CONFLICT`.

## Public API Surface (v1)
SDK header location: `include/emc/mod_hub_api.h`

1. Handle type:
`typedef struct EMC_ModHandle_t* EMC_ModHandle;`
2. v1 uses a mod handle only; setting identity is IDs-only (`setting_id`) and does not depend on registration order.
3. Registration functions:
`register_mod`, `register_bool_setting`, `register_keybind_setting`, `register_int_setting`, `register_float_setting`, `register_action_row`
4. Formal function signatures:
`EMC_Result (__cdecl *register_mod)(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle);`
`EMC_Result (__cdecl *register_bool_setting)(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def);`
`EMC_Result (__cdecl *register_keybind_setting)(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);`
`EMC_Result (__cdecl *register_int_setting)(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);`
`EMC_Result (__cdecl *register_float_setting)(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);`
`EMC_Result (__cdecl *register_action_row)(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);`
5. `register_mod` out-handle contract:
on `EMC_OK`, `*out_handle` is set to canonical mod handle;
on any non-`EMC_OK` result, hub sets `*out_handle = nullptr`.
6. Registration argument contract:
`register_mod` requires non-null `desc` and `out_handle`;
all setting registration functions require non-null `mod` and non-null `def`;
null argument violations return `EMC_ERR_INVALID_ARGUMENT`.
All required callback pointers in descriptor structs (`get_value`, `set_value`, `on_action`) must be non-null; null callback pointers return `EMC_ERR_INVALID_ARGUMENT`.
7. Namespace lifecycle:
namespaces are implicit in v1; first successful `register_mod(namespace_id=...)` creates namespace identity and its tab model.
8. Empty namespace behavior:
namespace tab is created only after first successful mod registration in that namespace; failed/conflicting registrations must not create empty tabs.
9. Removed from v1:
`unregister_mod` (YAGNI)
10. Action row callback uses the `EMC_ActionRowCallback` typedef defined in ABI Appendix Section 3.
11. Keybind value model:
SDK uses a stable keybind value struct from v1:
`typedef struct EMC_KeybindValueV1 { int32_t keycode; uint32_t modifiers; } EMC_KeybindValueV1;`
`keycode` uses OIS KeyCode numeric values; `modifiers` is reserved in v1 UI but kept in ABI for forward compatibility.
Hub roundtrips `modifiers` unchanged when pending value contains them; v1 capture UI writes `modifiers = 0`.
12. Unbound key constant:
`EMC_KEY_UNBOUND = -1`
13. Unbound behavior:
UI displays `Unbound`; clear action sets pending keybind to `{ keycode = EMC_KEY_UNBOUND, modifiers = 0 }`; `set_keybind` receives that value on save.
14. Setting IDs and scope:
`namespace_id` unique globally, `mod_id` unique inside namespace, `setting_id` unique inside mod.
Display names are UI-only and are not identity fields; `namespace_display_name` and `mod_display_name` are not required to be unique.
15. API targeting rule:
all setting-targeting operations use `(EMC_ModHandle, setting_id)`; setting order/index must never be used as identity.
16. Lookup semantics:
if `setting_id` is not found within the provided `EMC_ModHandle` scope, return `EMC_ERR_NOT_FOUND`.
17. Mod handle validity rules:
on successful `register_mod`, handle is non-null and owned by this hub instance for the session lifetime;
unknown/invalid/garbage handles passed to hub API return `EMC_ERR_INVALID_ARGUMENT`.
18. Registration timing rule:
if options window is currently open, registration calls are rejected with `EMC_ERR_INVALID_ARGUMENT` and warning log; consumers must register before first window-open.
19. Action row ID requirement:
`EMC_ActionRowDefV1` includes `setting_id` unique within mod scope; this ID exists for idempotent registration and row identity, not value targeting.
20. Action row flags:
`EMC_ActionRowDefV1` includes `uint32_t action_flags`; v1 defines `EMC_ACTION_FORCE_REFRESH = 1u << 0`.
21. Force-refresh action behavior:
if `EMC_ACTION_FORCE_REFRESH` is set and callback succeeds, hub clears dirty state for value-bearing rows in that mod and then performs immediate resync.
Actions that intentionally mutate other value-bearing settings in the same mod must set `EMC_ACTION_FORCE_REFRESH`.
22. Float display decimals default:
SDK defines `EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT = 3`; consumers should pass this constant explicitly in `EMC_FloatSettingDefV1.display_decimals` when default formatting is desired.
23. Setting registration return semantics:
all `register_*_setting` calls are idempotent when identity fields match;
display/callback/user_data differences return `EMC_OK` with warning and keep canonical values;
numeric metadata differences (`min`, `max`, `step`, `display_decimals`) return `EMC_OK`, log warning, and keep first canonical metadata values;
identical descriptor re-registration returns `EMC_OK` without creating duplicates;
identity mismatch (same `setting_id` with different setting kind) returns `EMC_ERR_CONFLICT`.
Exact match vs warning-only behavior is defined in ABI Appendix Section 9 (item 10) and is the single SSOT.

## SRP / SSOT Boundaries
1. Hub responsibility: registration, UI rendering, search/filtering, staging, generic range validation, deterministic save orchestration (implemented in `src/hub_commit.cpp`).
2. Consumer mod responsibility: authoritative values, persistence format, domain validation, and callback implementation.
3. SSOT for values: consumer `get_*` callbacks are canonical source; hub does not hold authoritative defaults.
4. Hub never writes consumer config files directly.

## Deterministic Registration And Conflict Rules
1. First registration establishes canonical display text for namespace/mod/setting labels and descriptions.
2. Re-registration with non-identity differences (display text, callback pointers, `mod_user_data`/`user_data`) is non-fatal: log warning and keep first canonical values.
3. `register_mod` is idempotent when identity fields match and returns `EMC_OK` with same handle.
4. Re-registering same `setting_id` with identity-field mismatches returns `EMC_ERR_CONFLICT`.
5. Identity fields are:
`namespace_id + mod_id` for mods, and `setting_id + setting kind` for settings within a mod.
6. Numeric metadata is not identity in v1:
re-registration with numeric metadata differences logs warning and keeps first canonical metadata values.
7. Equality rules used by idempotent registration are defined in ABI Appendix Section 9.
8. Mod-level conflicts are not expected in v1 under current identity rules (`namespace_id + mod_id`); later `register_mod` calls resolve to exact match or warning-only non-identity drift.

## UI Model
1. One namespace tab per `namespace_id`.
2. Top search box inside each namespace tab.
3. Search match fields: mod display name, setting label, setting description.
4. Search UX note:
search semantics are defined only in ABI Appendix Section 6 (SSOT).
empty-result state explicitly says `No matches in this tab. Try checking other tabs?` to make per-tab scope clear.
5. One collapsible section per mod inside the namespace tab.
6. Section collapse state is session-only in v1.
7. `bool`, `keybind`, `int`, and `float` values are staged in UI and committed on options save.
8. `action row` callbacks execute immediately on click.
9. Action row/save interaction:
action rows do not auto-save and never trigger commit; they may change underlying values and those changes are reflected by action refresh rules.
10. `int`/`float` controls provide both stepper buttons and text entry.
11. Numeric SSOT behavior:
stepper changes always clamp to `[min,max]`;
text entry parse failures are non-fatal and remain pending with inline error state;
parsed text values snap to nearest valid step and clamp to `[min,max]` before commit;
midpoint ties in step snapping round toward zero;
float step/snap comparisons use epsilon `1e-6f` to avoid jitter from floating-point precision noise;
float display uses fixed `display_decimals` from descriptor while stored value remains float.
12. On `set_*` failure, the affected row shows inline error text (from `err_buf` when provided) until a later successful apply clears it.
13. Numeric descriptor validation:
validation constraints are defined in ABI Appendix Section 8 and enforced by registration.
14. Keybind capture UX (v1):
clicking `Bind...` enters capture mode;
the next key press sets pending keybind `{ keycode = <pressed>, modifiers = 0 }`;
`Esc` cancels capture;
`Backspace` clears to `{ keycode = EMC_KEY_UNBOUND, modifiers = 0 }`.

## Initial Canonical Sync On Window Open
1. When the options window opens, hub calls `get_*` for every registered value-bearing setting (`bool`, `keybind`, `int`, `float`) before first render.
2. This initial pass establishes the canonical baseline used by dirty tracking.
3. If a row `get_*` fails during initial sync, hub shows that row in inline error state (`Unavailable`), keeps it non-dirty, and continues syncing other rows.
4. Initial-sync failures are logged as `hub_ui_get_failure`.

## After Action Row Success
1. After a successful action row callback, hub immediately refreshes that mod section by calling `get_*` for all value-bearing settings (`bool`, `keybind`, `int`, `float`) in the same mod.
2. Default behavior (`EMC_ACTION_FORCE_REFRESH` not set): refresh updates rows that are not dirty; dirty rows keep pending edits.
3. Force-refresh behavior (`EMC_ACTION_FORCE_REFRESH` set): hub clears dirty state for value-bearing rows in that mod before refresh so reset-like actions fully resync visible values.
4. Refresh is best-effort; if a `get_*` call fails, hub logs and skips that row without aborting the refresh pass.

## After Action Row Failure
1. If action callback returns non-`EMC_OK`, the action row shows inline error text from `err_buf` when available, otherwise a generic failure message.
2. Failure UI is inline only (no modal) and does not trigger commit.
3. Existing dirty edits are preserved.

## Save Commit Algorithm (Deterministic)
1. Commit order is deterministic:
namespace first-accepted registration order -> mod first-accepted registration order -> setting first-accepted registration order.
2. Dirty tracking definition:
`dirty` means a row has a pending user-edited value that differs from last successful canonical sync.
3. For each dirty staged setting:
call `set_*`.
4. If `set_*` succeeds:
immediately call `get_*` and sync staged/display value to canonical callback value.
5. If post-commit `get_*` fails:
log `hub_commit_get_failure`, treat the successfully-sent dirty value as new canonical for that row, clear dirty flag, keep displaying the sent value, and continue.
6. If `set_*` fails:
keep pending dirty value, record error, continue with remaining settings.
7. Save summary is logged with counts: attempted/succeeded/failed.
8. Registration order affects only UI/commit ordering; identity and lookup are ID-based.
9. Commit order is stable for the session: later registrations append to namespace/mod/setting sequences and do not reorder already-accepted entries.
10. While options window is open, dirty staged UI values take precedence on save; later internal runtime value changes to those same settings may be overwritten by commit.

## Save Hook Integration Point
1. Hub commit runs in `OptionsWindowSaveHook`.
2. Hook ordering: call original game save handler first, then run hub commit.
3. Hub commit failures do not block the game options save flow.
4. Hub setting persistence is independent of Kenshi options persistence; hub commits apply to mod-owned config/state via callbacks.
5. Rationale: never risk blocking base game options save; hub apply is best-effort and isolated.
6. Capture-mode safety guard:
hub must not call any `set_*` while a keybind row is in active capture mode; save-triggered hub commit is skipped and the user must save again after capture exits.
7. When commit is skipped due to capture mode, hub still emits `hub_commit_summary` with `attempted=0 succeeded=0 failed=0 skipped=1 reason=keybind_capture_active`.

## Action Row Re-Entrancy Rules
1. Action row callbacks execute on the main thread only.
2. Action row callbacks must not call hub registration APIs or otherwise mutate the hub registry.
3. Action row callbacks may update their own mod state/config and return failure text via `err_buf`.
4. Callback recursion guard:
callbacks must not trigger hub commit directly or indirectly (no re-entrant commit path from `get_*`, `set_*`, or action callbacks).
5. Action row clicks are allowed during keybind capture mode and execute immediately; this does not bypass the save-commit capture guard.

## Callback Performance Budget
1. All hub callbacks run on the main thread and must stay fast.
2. Callbacks must avoid blocking IO and heavy work (`set_*` and action callbacks especially).
3. Any expensive persistence or recomputation should be deferred to mod-controlled background/work queues outside callback paths.

## Logging SSOT
1. Log destination is RE_Kenshi plugin log through existing `DebugLog`/`ErrorLog`.
2. Commit failure line format:
`event=hub_commit_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
3. Commit summary line format:
`event=hub_commit_summary attempted=<n> succeeded=<n> failed=<n> skipped=<0|1> reason=<text_or_none>`
4. Initial UI sync get failure line format:
`event=hub_ui_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
5. Registration warning line format:
`event=hub_registration_warning namespace=<id> mod=<id> setting=<id> field=<name> message=<text>`
6. Registration rejected line format (for unsupported timing/state):
`event=hub_registration_rejected api=<name> reason=<text> result=<code> message=<text>`
7. Setting registration conflict line format:
`event=hub_setting_registration_conflict namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
8. Action callback failure line format:
`event=hub_action_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
9. Action refresh get failure line format:
`event=hub_action_refresh_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
10. Post-commit get failure line format:
`event=hub_commit_get_failure namespace=<id> mod=<id> setting=<id> result=<code> message=<text>`
11. Registration warning `field` values are string names:
`namespace_display_name`, `mod_display_name`, `label`, `description`, `callback`, `user_data`, `mod_user_data`.

## Soft Dependency Contract For Consumer Mods
1. Consumer tries to attach to hub in `startPlugin`.
2. If attach fails, consumer sets `hub_attach_retry_pending = true`.
3. Consumer performs exactly one retry on first safe UI hook (`OptionsWindowInitHook`).
4. If retry still fails, consumer uses local settings UI fallback.
5. If attach succeeds but `register_mod` returns any non-`EMC_OK` result, consumer treats hub UI as unusable for this session: log once and use local settings UI fallback.
6. If attach and `register_mod` succeed, consumer suppresses local settings tab to avoid duplicate settings UIs.
7. Duplicate suppression mechanism:
consumer maintains a single runtime flag (`use_hub_ui`) set only after successful attach; local tab creation checks this flag every time and is skipped when true.

## Wall-B-Gone Migration (First Consumer)
1. Add hub client adapter in Wall-B-Gone startup path.
2. Register namespace and mod:
`namespace_id = emkej.qol`, `mod_id = wall_b_gone`
3. Register settings:
`enabled` (bool), `sleeping_bag_dismantle_enabled` (bool), `dismantle_hotkey` (keybind), existing numeric settings if any as int/float.
4. Register action row:
`reset_hotkey_default`
5. Keep legacy local tab when hub attach fails after one retry or when `register_mod` returns non-`EMC_OK`.

## Implementation Sequence
1. Phase 0: Feasibility gate for UI primitives.
Artifacts: temporary spike wiring in `src/hub_ui.cpp` and `src/hub_menu_bridge.cpp` guarded behind local feature toggles.
Done criteria: search input works in options panel, per-mod collapsibles are stable, int/float text+stepper widgets are stable, keybind capture isolates input from live game controls.
Validation commands/tests: run build/launch flow (`./run.sh`), load hub-only, open options, execute manual UI checklist for search/collapse/numeric/keybind capture, and during capture press movement keys (`W/A/S/D`) to verify character does not move.
Rollback: disable spike toggles and keep original options flow only; if any criterion fails, block v1 implementation pending engine-level investigation.
2. Phase 1: ABI scaffold and export handshake.
Artifacts: `include/emc/mod_hub_api.h` (all public types, callbacks, structs, ABI asserts), `src/hub_exports.cpp` (`EMC_ModHub_GetApi` + v1 table).
Done criteria: header compiles in a dummy consumer translation unit, ABI static assertions pass, `GetApi` enforces version/size rules and returns non-null v1 table on valid input.
Validation commands/tests: project build succeeds; run handshake scenarios from acceptance list (`requested_version`, size mismatch, null out params, success path).
Rollback: keep `EMC_ModHub_GetApi` but return `EMC_ERR_UNSUPPORTED_VERSION` and expose no active registration table until ABI issues are resolved.
3. Phase 2: Registry and deterministic registration semantics.
Artifacts: `src/hub_registry.cpp` and registry-facing portions of `src/hub_exports.cpp`.
Done criteria: deterministic idempotency/conflict behavior, ID-scope enforcement, descriptor copy/lifetime rules, warning/conflict logs, mod-handle creation/lookup complete.
Validation commands/tests: hub + dummy mod registration matrix (valid, duplicate, conflict, re-register variants, null arguments, null callbacks, invalid handles, register while options open) with expected `EMC_Result` and log events.
Rollback: gate registry attach path; on rollback, registration entrypoints return deterministic error and consumer falls back to local UI.
4. Phase 3: Core UI/commit path for bool+keybind.
Artifacts: `src/hub_ui.cpp`, `src/hub_commit.cpp`, `src/hub_menu_bridge.cpp`.
Done criteria: namespace tabs + mod collapsibles render, bool/keybind rows stage/edit correctly, initial window-open canonical sync works, deterministic save commit and capture-mode guard work.
Validation commands/tests: load hub + dummy mod, edit bool/keybind, save, verify commit order/logs, verify capture-mode skip summary, verify action row behavior during capture.
Rollback: runtime flag in menu bridge disables hub rendering/commit hooks and calls original game handlers only.
5. Phase 4: Search/filter and collapse interactions.
Artifacts: search/filter and section-state logic in `src/hub_ui.cpp`.
Done criteria: per-tab search filters by mod/label/description, empty-state text is correct, section collapse/expand behavior remains stable with filtering.
Validation commands/tests: manual search matrix (hits/misses/case/non-ASCII literal) and collapse-state checks while filtering and clearing query.
Rollback: disable search filter branch and render full unfiltered rows while keeping hub tabs/sections active.
6. Phase 5: Full numeric row support (int/float).
Artifacts: numeric row rendering/edit logic in `src/hub_ui.cpp`, numeric descriptor validation in `src/hub_registry.cpp`, numeric commit behavior in `src/hub_commit.cpp`.
Done criteria: int/float registration accepts valid descriptors, rejects invalid constraints, UI step/text behavior matches clamp/snap rules, save applies only dirty rows with correct error handling.
Validation commands/tests: numeric bounds/step/parse matrix, midpoint tie checks, commit success/failure paths, post-commit resync and `hub_commit_get_failure` paths; run `./scripts/phase5_numeric_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll>` against a test-exports-enabled build (Debug).
Rollback: disable numeric row registration/rendering path and keep bool/keybind/action functionality active.
7. Phase 6: Wall-B-Gone migration and fallback contract.
Artifacts: Wall-B-Gone consumer bridge/adapter code, attach+retry logic, fallback flag wiring (`use_hub_ui`) and duplicate-tab suppression.
Done criteria: attach success uses hub UI only, attach fail (including one retry) falls back deterministically, and `register_mod` non-`EMC_OK` also falls back deterministically.
Validation commands/tests: load matrix: hub only, hub+Wall-B-Gone, Wall-B-Gone without hub, hub present with forced `register_mod` non-`EMC_OK`; verify visible tabs and logs.
Rollback: force consumer local-tab path (`use_hub_ui = false`) and leave hub plugin unaffected.
8. Phase 7: Client helper library (`mod_hub_client`) core.
Artifacts: `mod_hub_client.h/.cpp` exposing deterministic consumer entrypoints (`OnStartup`, `OnOptionsWindowInit`, `UseHubUi`) that wrap attach + one retry + fallback + `use_hub_ui` state.
Done criteria: consumer mods can adopt helper entrypoints instead of re-implementing attach/retry/fallback logic; behavior matches soft dependency contract; helper header is the canonical integration surface.
Validation commands/tests: helper unit/harness checks for attach success, attach fail + one retry, retry fail fallback, and attach success + `register_mod` non-`EMC_OK` fallback.
Rollback: keep raw integration path (`EMC_ModHub_GetApi` + direct `register_*`) and disable helper adoption.
9. Phase 8: Single settings-table registration API.
Artifacts: helper-side settings descriptor table API supporting v1 row kinds (`bool`, `keybind`, `int`, `float`, `action`) with static table registration and one canonical descriptor schema in helper header.
Done criteria: consumer defines one static rows table and helper performs ordered `register_*` calls with deterministic error handling; table schema is documented once and reused by scaffold + docs.
Validation commands/tests: compile + run dummy consumer using table-only registration for all v1 row kinds; verify IDs, callbacks, and failure propagation.
Rollback: keep helper attach/fallback only; require direct per-setting `register_*` calls.
10. Phase 9: Consumer scaffold command (`init-mod-template --with-hub`).
Artifacts: updates to template/init scripts to generate a buildable consumer adapter skeleton with helper wiring, fallback flag handling, and sample callbacks.
Done criteria: scaffolded mod builds without manual wiring edits and defaults to duplicate-safe local-tab suppression when hub is active; scaffold output references only SDK/helper public assets and does not copy or include hub implementation internals (`src/hub_*`).
Validation commands/tests: generate fresh scaffold with `--with-hub`, build it, and run startup/options-init smoke tests for helper path + local fallback path.
Rollback: keep existing template command path and publish manual setup instructions.
11. Phase 10: Versioned SDK packaging and release asset.
Artifacts: versioned SDK output bundle (header + client helper + minimal sample) and packaging metadata tied to project version.
Done criteria: consumers can integrate from one versioned SDK release asset without copying hub implementation files from repository internals; package metadata explicitly maps SDK package version to supported Hub API version(s) for compatibility checks.
Validation commands/tests: consume packaged SDK in a clean dummy mod workspace and build successfully against exported interfaces only.
Rollback: ship header-only package and defer helper/scaffold assets.
12. Phase 11: Docs and SDK examples.
Artifacts: `docs/mod-hub-sdk.md`, updated `docs/plans/mod_hub_v1_plan.md`, minimal consumer sample snippets aligned with final header.
Done criteria: docs match shipped ABI/types/log semantics and sample registration code compiles against current header.
Validation commands/tests: run `./scripts/phase11_sdk_docs_test.ps1`; copy/paste sample into dummy consumer TU and compile; run quick doc-to-header consistency pass.
Rollback: docs-only revert; no runtime impact.

## Consumer SDK SSOT (Phase 7+)
1. Canonical consumer integration API is defined in `mod_hub_client.h`; scaffold and docs must reference this header as the single source of truth.
2. Canonical helper entrypoints are `OnStartup`, `OnOptionsWindowInit`, and `UseHubUi`; duplicate local-tab suppression logic must consume only `UseHubUi`.
3. Canonical settings-table descriptor schema is defined once in helper header and reused by scaffold templates and docs examples.
4. Consumer integrations must not include or copy hub implementation internals (`src/hub_*`); they consume SDK/header/helper assets only.
5. SDK package metadata must declare supported Hub API versions and be versioned with release artifacts.
6. Helper diagnostic accessors (`IsAttachRetryPending`, `HasAttachRetryAttempted`, `LastAttemptFailureResult`) are optional observability APIs; duplicate local-tab suppression remains driven by `UseHubUi` only.

## Minimal File Layout (KISS)
1. `Emkejs-Mod-Core.cpp` as thin entry/wiring.
2. `src/hub_menu_bridge.cpp` for options hooks and game function pointers.
3. `src/hub_registry.cpp` for registration, validation, canonical metadata, idempotency rules.
4. `src/hub_ui.cpp` for tab/row rendering, search, collapse, staging, and row-level error presentation.
5. `src/hub_commit.cpp` for save orchestration and deterministic commit execution.
6. `src/hub_exports.cpp` for API export and function table.
7. SDK header in `include/emc/mod_hub_api.h`.
8. `mod_hub_client` helper surface (`mod_hub_client.h/.cpp`) shipped as consumer-facing SDK asset.
9. Template/init script support for `init-mod-template --with-hub`.
10. Docs in `docs/plans/mod_hub_v1_plan.md` and `docs/mod-hub-sdk.md`.

## Testing And Acceptance Scenarios
1. API handshake accepts callers with `caller_api_size >= EMC_HUB_API_V1_MIN_SIZE` (v1 equals `sizeof(EMC_HubApiV1)`), returns `out_api_size = sizeof(EMC_HubApiV1)`, and rejects smaller sizes deterministically.
2. API handshake rejects unsupported `requested_version` with `EMC_ERR_UNSUPPORTED_VERSION`.
3. Returned `EMC_HubApiV1` contains non-null function pointers for all v1 register entrypoints.
4. `register_mod` sets `out_handle` on `EMC_OK` and clears it (`nullptr`) on non-`EMC_OK` results.
5. Null-argument handling and null required-callback handling return `EMC_ERR_INVALID_ARGUMENT` for all `register_*` entrypoints (`desc`/`def`/`mod`/`out_handle`/callback pointers as applicable).
6. Namespace/mod/setting ID scope enforcement matches ABI rules (`namespace_id` global, `mod_id` per-namespace, `setting_id` per-mod).
7. Duplicate and invalid IDs are rejected with correct `EMC_Result` codes.
8. Idempotent re-registration of identical mod/setting descriptors returns success.
9. Display/callback/user_data-only re-registration differences log warning and retain first canonical values.
10. After callback-pointer drift re-registration, hub still invokes the first canonical callback pointer.
11. Numeric metadata re-registration logs warning, keeps first canonical metadata, and does not return conflict.
12. Conflict re-registration with identity mismatch (same `setting_id`, different setting kind) returns `EMC_ERR_CONFLICT`.
13. `register_mod` non-`EMC_OK` on successful hub attach triggers deterministic consumer fallback to local UI.
14. Setting-level registration conflicts log `hub_setting_registration_conflict` format.
15. `(EMC_ModHandle, setting_id)` lookup miss returns `EMC_ERR_NOT_FOUND`; invalid/unknown handles return `EMC_ERR_INVALID_ARGUMENT`.
16. Namespace tab is not created when no mod registration succeeds for that namespace.
17. Namespace tab creation works for multiple namespaces with at least one successful mod each.
18. Multiple mods in one namespace render as separate collapsible sections.
19. Per-tab search filters by mod display name, setting label, and setting description; empty sections are hidden.
20. Per-tab empty-result state shows `No matches in this tab. Try checking other tabs?`.
21. On options-window open, hub calls `get_*` for every value-bearing row before first render.
22. Initial window-open `get_*` failure leaves row non-dirty in inline error state and logs `hub_ui_get_failure`.
23. Bool/keybind/int/float staging commits only dirty settings on options save.
24. Save commit order is deterministic and stable for the session.
25. New registrations append order and do not reorder already-accepted namespace/mod/setting sequences.
26. Per-setting `set_*` failure keeps pending dirty state and does not block other commits.
27. Post-commit `get_*` resync happens for successful settings.
28. Post-commit `get_*` failure logs `hub_commit_get_failure`, treats sent value as canonical, clears dirty, and continues commit processing.
29. Per-setting `set_*` failure renders row-level inline error text that clears after successful apply.
30. Save-triggered commit is skipped while any keybind row is in active capture mode; saving again after capture exits commits normally.
31. Capture-mode skipped commit still emits `hub_commit_summary` with `skipped=1` and reason.
32. Action row clicks execute during active keybind capture and do not bypass commit guard.
33. Keybind value roundtrip uses `EMC_KeybindValueV1` and preserves ABI fields (`keycode`, `modifiers`) even though v1 UI capture sets `modifiers = 0`.
34. Int and float stepper updates respect min/max/step.
35. Int and float text input parse behavior is stable, bounds-checked, safe on invalid input, and float snap/clamp math is stable under `1e-6f` epsilon handling.
36. Midpoint step-snap ties resolve toward zero deterministically.
37. Action row callback executes immediately, cannot re-enter registration or commit paths, and does not affect save pipeline.
38. Action row failure renders inline row-level error text and preserves dirty edits.
39. After action row success without force-refresh flag, same-mod non-dirty value-bearing rows resync through `get_*`; dirty rows remain pending.
40. Actions that mutate other settings are registered with `EMC_ACTION_FORCE_REFRESH`; after such action success, same-mod value-bearing rows are fully resynced (dirty flags cleared first).
41. UTF-8 labels/descriptions render correctly; ID restrictions remain enforced.
42. Phase 0 keybind capture primitive works without leaking captured input into live game controls (for example pressing `W/A/S/D` during capture does not move the character).
43. Wall-B-Gone hub attach success path removes duplicate local menu.
44. Wall-B-Gone attach fail + one retry + fallback path is deterministic and stable.
45. Wall-B-Gone attach success plus `register_mod` non-`EMC_OK` falls back to local tab deterministically.
46. Validated target game version for this release is `1.0.65`; menu initialization and save flows pass without crash on that version.
47. Callback invocations receive per-setting `user_data` only; `mod_user_data` is not auto-forwarded to `get_*`/`set_*`/action callbacks.
48. Registration calls made while options window is open are deterministically rejected with `EMC_ERR_INVALID_ARGUMENT` and `hub_registration_rejected` log entries.
49. `mod_hub_client` helper `OnStartup` and `OnOptionsWindowInit` paths preserve soft dependency attach/retry/fallback behavior and expose `use_hub_ui`-equivalent state for duplicate suppression checks.
50. Helper settings-table API registers one static descriptor table covering all v1 row kinds (`bool`, `keybind`, `int`, `float`, `action`) without per-row manual `register_*` boilerplate in consumer startup code.
51. `init-mod-template --with-hub` scaffold output compiles and includes working helper wiring + deterministic local fallback wiring by default while depending only on SDK/helper public assets (no `src/hub_*` integration copies).
52. Versioned SDK package artifact contains header + helper + minimal sample, declares supported Hub API version compatibility metadata, and is consumable from a clean workspace without copying internal hub implementation files.

## Consumer Author Pitfalls (v1)
1. See ABI Appendix Section 5 for packing/alignment requirements (`/Zp` unsupported).
2. See ABI Appendix Section 1 for API size handshake requirements.
3. See Deterministic Registration And Conflict Rules for canonical-first behavior on non-identity re-registration differences.
4. Callback/user_data changes on re-registration are warning-only and first canonical callbacks remain active; treat callback pointer drift as an integration bug.
5. For idempotent behavior, reuse module-scope/static callback functions and stable `user_data` addresses; avoid rebuilding callback/user_data pointers per registration attempt.
6. Numeric metadata re-registration is warning-only and canonical-first in v1; later registrations must not be used to change numeric constraints.
7. `mod_user_data` is registry metadata only; pass shared runtime context through each setting `user_data` if callbacks need it.
8. While options is open, dirty staged hub values win on save; internal runtime changes to the same setting may be overwritten when commit runs.

## Assumptions And Defaults
1. Single-threaded main-thread API usage.
2. Session-only collapse state and search query.
3. Per-tab search is final for v1.
4. Enum rows are deferred to v2; keybind ABI already includes `modifiers`, but v1 UI capture sets `modifiers = 0`.
5. Consumer mods remain responsible for persistence files and schema evolution.
6. Validation and acceptance matrix in this plan targets Kenshi `1.0.65` for v1 release scope.
7. All registrations are expected before the options window is first opened; if registration is attempted while options is open, hub rejects it with `EMC_ERR_INVALID_ARGUMENT` and logs `hub_registration_rejected`.
