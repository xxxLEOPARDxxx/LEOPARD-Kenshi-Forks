# Emkejs Mod Core Mod Hub Custom Int Row Buttons Plan

## Summary

Add a backward-compatible Mod Hub extension so consumer mods can control which integer-row step buttons are rendered and what exact value delta each button applies.

Primary outcomes:
1. Consumers can hide unwanted integer step buttons on a per-setting basis.
2. Consumers can provide exact per-button integer deltas instead of relying on the current hardcoded `-10/-5/-/+ /+5/+10` pattern.
3. Existing v1 integer consumers keep current behavior unchanged.
4. The SDK, registry, and hub UI remain versioned and backward compatible.
5. Validation covers both legacy v1 rows and new custom-layout rows.

## Background Problems (Observed)

1. The Mod Hub UI currently hardcodes six integer buttons in `src/hub_menu_bridge.cpp`: `-10`, `-5`, `-`, `+`, `+5`, `+10`.
2. Consumers cannot remove buttons that are meaningless for small bounded ranges such as `0..9`.
3. Current button actions are multiplier-based around `step`, not exact consumer-specified deltas.
4. The public `EMC_IntSettingDefV1` ABI exposes only `min`, `max`, and `step`, so there is no consumer-visible layout control today.
5. The `emc::ModHubClient` table registration path also has no way to distinguish a legacy int row from a richer int-row definition.

## Scope

In scope:
1. Public SDK/API extension for custom integer button layouts.
2. Hub registry/storage changes for the new metadata.
3. Hub UI changes to render data-driven integer buttons.
4. SDK helper/table-registration support for the new int-row variant.
5. Docs and script coverage for legacy and custom rows.

Out of scope:
1. New enum/select row types.
2. Float-row custom button layouts.
3. Arbitrary consumer-provided button captions in this phase.
4. Generic per-row widget templating or layout scripting.

## Design Constraints

1. Preserve existing `EMC_IntSettingDefV1` behavior and ABI exactly.
2. Do not break existing consumer binaries or existing hub registration paths.
3. Keep row layout visually stable; do not introduce unbounded button counts.
4. Custom button deltas must be easy to validate and reason about.
5. Button clicks must remain O(1) and must not add per-frame work.

## Mandatory Decisions (Freeze Before Implementation)

1. API growth model:
add a new versioned integer-setting descriptor plus a new size-gated registration function, rather than mutating `EMC_IntSettingDefV1`.
2. Button layout model:
support up to `3` decrement slots and `3` increment slots per integer row; `0` disables a slot.
3. Delta semantics:
custom button values are exact value deltas, not `step` multipliers.
4. Validation rule:
every custom button delta must be positive, unique within its side, strictly ordered, and a multiple of the setting `step`.
5. Caption policy:
button captions are derived automatically from the delta (`-7`, `+3`, etc.); no custom label strings in this phase.
6. Legacy fallback:
all existing v1 integer rows continue using the current hardcoded button profile and current multiplier semantics.

## Proposed Public API Shape

### New descriptor

Add a new fixed-size descriptor, for example:

```cpp
typedef struct EMC_IntSettingDefV2
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    int32_t dec_button_deltas[3];
    int32_t inc_button_deltas[3];
    EMC_GetIntCallback get_value;
    EMC_SetIntCallback set_value;
} EMC_IntSettingDefV2;
```

Semantics:
1. `dec_button_deltas[]` contains positive magnitudes such as `10, 5, 1`; the hub renders them as negative buttons.
2. `inc_button_deltas[]` contains positive magnitudes such as `1, 5, 10`; the hub renders them as positive buttons.
3. `0` disables a slot.
4. Validation rejects values that are not multiples of `step`, are duplicated on the same side, or are out of order.

### New hub API function

Append a new function pointer to `EMC_HubApiV1`, size-gated like the options-init observer additions:

```cpp
EMC_Result(__cdecl* register_int_setting_v2)(EMC_ModHandle mod, const EMC_IntSettingDefV2* def);
```

Rationale:
1. Keeps existing `register_int_setting` untouched.
2. Allows feature detection through `out_api_size`.
3. Avoids ambiguous overloading of the current registration path.

### New ModHubClient row kind

Extend `emc::ModHubClientSettingKind` with a new kind, for example:

```cpp
MOD_HUB_CLIENT_SETTING_KIND_INT_V2
```

Rationale:
1. `ModHubClientTableRegistrationV1` currently dispatches by row kind only.
2. A new kind is the smallest safe way to route V2 defs without guessing pointer types.

## Internal Hub Design

## Registry

1. Extend the internal integer setting storage with:
   - `bool use_custom_int_buttons`
   - `int32_t dec_button_deltas[3]`
   - `int32_t inc_button_deltas[3]`
2. Legacy registration populates the current default profile:
   - decrement slots: `10, 5, 1`
   - increment slots: `1, 5, 10`
   - `use_custom_int_buttons = false`
3. V2 registration stores the consumer-provided layout and sets `use_custom_int_buttons = true`.
4. Drift handling for duplicate registration must treat button-layout metadata as part of the canonical numeric metadata set.

## Hub UI

1. Replace the hardcoded six-button integer block in `src/hub_menu_bridge.cpp` with a data-driven layout loop.
2. Render only the enabled decrement slots, then the value box, then the enabled increment slots.
3. Compute group width from the active button count instead of assuming six buttons.
4. Keep button captions derived from the stored delta values.
5. Keep text-entry behavior unchanged.

## Hub actions

1. Introduce a new exact-delta path, for example:
   - `HubUi_AdjustPendingIntDelta(...)`
2. Keep the current `HubUi_AdjustPendingIntStep(...)` for legacy rows, or reimplement it as a thin wrapper over the new exact-delta path.
3. Button clicks for V2 rows must apply the exact configured delta, clamp to `[min,max]`, and preserve step alignment by construction through registration-time validation.

## SDK / Consumer Integration

1. Add `EMC_IntSettingDefV2` and the new registration function to the public SDK header.
2. Extend `include/emc/mod_hub_client.h` and `src/mod_hub_client.cpp` to dispatch the new row kind when the host API exposes `register_int_setting_v2`.
3. Define fallback behavior clearly:
   - if the host does not expose V2 support, the client must fail that row registration deterministically and fall back to local config behavior rather than silently downgrading to a misleading legacy layout.
4. Update SDK docs with one minimal V2 example and one migration note from `EMC_IntSettingDefV1`.

## Validation Plan

### Automated

1. Add a dedicated numeric-layout test script, tentatively:
   - `scripts/phase19_int_button_layout_test.ps1`
2. Cover:
   - valid V2 registration with one button per side
   - valid V2 registration with sparse slots
   - rejection of zero/negative/out-of-order/duplicate deltas
   - rejection of deltas not divisible by `step`
   - legacy V1 rows still rendering and behaving exactly as before
   - V2 rows applying exact deltas rather than multiplier semantics
3. Extend the dummy consumer/test exports so UI/state assertions can distinguish V1 vs V2 row behavior.

### Manual

1. Register one legacy int row and confirm the current six-button UI remains unchanged.
2. Register one V2 row with only `-1` and `+1`.
3. Register one V2 row with `-3`, `-1`, `+1`, `+7`.
4. Confirm captions match configured values exactly.
5. Confirm clamping at min/max still works.
6. Confirm typed input still snaps to `step` exactly as before.

## Documentation Updates

1. Update `docs/mod-hub-sdk.md` with:
   - new V2 descriptor
   - new row kind
   - validation rules
   - fallback behavior
2. Update any scaffold/sample assets that demonstrate int rows.
3. Add a short migration note in the changelog/release notes when shipped.

## Phase Plan

1. Phase 19A: API and SDK extension
   - add `EMC_IntSettingDefV2`
   - add `register_int_setting_v2`
   - add `MOD_HUB_CLIENT_SETTING_KIND_INT_V2`
2. Phase 19B: Registry and validation
   - store custom button metadata
   - validate deltas and duplicate registration drift rules
3. Phase 19C: Hub UI rendering and exact-delta actions
   - replace hardcoded int buttons with data-driven rendering
   - add exact-delta adjustment path
4. Phase 19D: Docs and validation harness
   - SDK docs
   - dummy consumer/test updates
   - phase test script

## Acceptance Criteria

1. Existing V1 integer consumers behave identically with no source changes.
2. A consumer can register a V2 int row that shows only a subset of buttons.
3. A consumer can choose exact button deltas such as `-1` and `+7`.
4. The hub rejects invalid button layouts with deterministic registration errors.
5. Button captions reflect configured values exactly.
6. The UI no longer assumes all integer rows must show `-10/-5/-/+ /+5/+10`.

## Recommended First Implementation Slice

Implement the smallest end-to-end proof:
1. `EMC_IntSettingDefV2`
2. `register_int_setting_v2`
3. one V2 row kind in `ModHubClient`
4. one UI path that supports up to one decrement and one increment button
5. one phase test proving exact-delta behavior

Then expand from `1+1` buttons to the full `3+3` slot model once the ABI, registry, and action path are stable.
