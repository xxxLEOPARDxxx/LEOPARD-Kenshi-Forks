# Optional Rotate API binding and mod-list order

The reported failure is explained by the previous source: `startPlugin` called
`StackSort_InitRotateAPI()` once, and that function returned immediately when
`GetModuleHandleA("KenshiRotate.dll")` returned null. Every later item/capability
query read the permanently false `s_krAvailable`. With Rotate loaded first the
same API resolved successfully. The production compile flag was already enabled.

The Rotate side needs no modification. Its exported `KenshiRotate_ApiVersion`
returns 1; `CanRotate` and `SetRotated` check the required save/load/copy hooks
through `RotationState::HooksReady()` on each call. `IsRotated` remains readable
for existing rotations. Rotate's MCM `Enabled` setting gates `TriggerDown()`
(the input switch), not these C API exports; that existing behavior is preserved.

The binder now retries only at explicit GUI-thread events: a new inventory
tracking context, or `FirstClickInit` before sorting. Startup remains an early
attempt. After success it returns immediately without looking up a DLL or
export again. Missing DLLs remain retryable; incompatible loaded modules log
one error per module identity and stay disabled. No load/hook is forced by the
consumer, and neither steady GUI updates nor item queries perform discovery.

On the single false-to-true binding transition, existing plain-data jobs are
aborted by opaque tracking keys, all cached result generations are invalidated,
and tracked GUIs re-snapshot from their own next live update. The current click
can use synchronous packing immediately. Undo captures and sorted-without-capture
markers survive this capability change; world-reset invalidation still clears
them. Background workers only see copied pack items and cancellation/generation
state. They never query the DLL or native items/GUI. Saved undo comparison
identities migrate from exact dimensions to `(min,max)` on this transition;
their captured exact width/height remain unchanged. This migration reads only
copied identities, never native item pointers.

`rotation_binding_tests.cpp` compiles the actual binder, inventory-open prefix
through the first snapshot, and click prefix through the pre-merge boundary.
It covers Stack-first, Rotate-first, absence, loading after a panel was opened,
StackSort enable after startup, hook readiness, the preserved input-only switch,
bad API version, missing required exports, optional refresh absence, cached
successful binding and single transition invalidation. `core_tests.cpp` runs
the actual cache, packer and RevertSection matching pass to verify stale-generation
rejection, functional wide-item undo across late binding, rejection of changed
orientation/reused identity, and existing-rotation dimensions without the API. Source-side lifecycle tests
are not a substitute for testing actual automatic rotation in both mod orders.

No KEO/body/backpack geometry, user configuration or Rotate source is changed.
