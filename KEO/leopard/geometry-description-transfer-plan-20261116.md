# Geometry description transfer

Goal: remove three redundant deep copies of the collected buffer description
when the existing geometry cache admits a model. No native decode, model/gear
construction, buffers, scheduling, budgets, settings or thread policy changes.

The actual production-inclusion/native Ogre fixture confirms the old Promote
allocates three additional 512-buffer arrays and retains a different array.
Fail-first test exits CHECK 1 after clean native teardown. The already collected
description owns the needed list; its two callers stop using it after success.

Implementation: insert an empty entry with the correct touch before victim-index
insertion, establish the single native MeshPtr reference, account buffer refs
from the still-owned input list, then swap its vector into the admitted entry.
Reset consumed source byte counters. All refusal paths leave the input intact;
allocation exceptions retain existing Clear-all bookkeeping recovery. No throwing
operation follows the final ownership transfer. Partial failure before transfer
must never lose the original list or leave an admitted half-entry after Tick.

Tasks: fail-first native/actual source test; implement one production function;
extend success/refusal/retry/allocation-failure controls and compiled regression
controls; independently review; native Od/O2, host suite, full DEV/PROD/import
gates; preserve foreign Steel and personal INI; commit/push/install local only.
Do not claim measured save-load or game FPS improvement. No new gameplay runs.

Separate investigation: scoped mapped-pose batching was rejected for production.
Native unlock is non-atomic on copy-back errors; handled SEH, reentrancy/renderer
exclusion and second-exception cleanup are not established. No such hook or
batching change belongs to this correction.
