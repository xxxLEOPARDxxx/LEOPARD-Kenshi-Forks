# Carried weight provenance and scope

KenshiQOL by Steel. Thanks to Steel for the original feature and bundled source release.
Original mod: https://steamcommunity.com/sharedfiles/filedetails/?id=3772744830
Original source: bundled src/KenshiQOL/KenshiQOL.cpp, lines176-227 and1343-1369.
Author source-release comment: https://steamcommunity.com/sharedfiles/filedetails/comments/3772744830
No separate source repository has been verified.
Audited source SHA256: 955b340eb25b0f66c1e08479ec2e8d4636a39f622962582d9bff8e6bed27363e
Original src/LICENSE and KEO LICENSE are GPLv3. This is a modified integration.

Preserved: volume/age/Height/Frame body scaling, hunger and robot factor; native Inventory weighing and worn-backpack reductions. Native body extra is observed from a paired unmodified Inventory call inside native CharacterCarry; no body constant is guessed. Root Inventory may return signed correction only when final CharacterCarry is finite/nonnegative. Carried-child queries add their own held native body exactly once.

Changed: fixed query-local64-inventory/4096-item/8-level budget; duplicate inventories counted once; cycles/budget/nonfinite failures return original whole-query weight. No temporary item collections, retained actor pointers, world scans or hot module lookups. TLS restores on C++ and SEH unwind. Equipped containers are not added twice. Animation and appearance intermediates are NULL-guarded; absent data uses native-size defaults.

Default OFF, startup-only. Existing non-live ConfigKey sets MCM restart flag. Loaded KenshiQOL.dll refuses installation/admission regardless of its toggle. Admission stays false until first Main tick; module check at most1Hz thereafter, permanent refusal once observed. Covers normal startup order; arbitrary late injection between checks is not promised. Unknown SDK/native contracts refuse. Full carry44-byte contract includes runtime float32 addition and pinned inventory jump thunk; actual Steam/GOG EXEs are tested by test_native_contract.py. No runtime body value is inferred.

Focused controls: test_policy.py, test_adapter.py (actual HookWeight/ReadNativeBody extraction), test_admission.py (actual Install/Tick extraction), test_native_contract.py (actual EXEs). Compact receipt is build/carried-weight-tests/native-receipt.json. Static/host controls are not in-game FPS/memory/UI/save-load proof. No release or install by this task. The parent authorized a narrow local leopard commit after independent review; no upstream push.
