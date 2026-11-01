# Container Highlight restore lifetime audit, 2026-11-01

The Steam report describes an intermittent freeze relieved while holding Alt or
alt-tabbing, reportedly after interior/container model rebuilding. The supplied
Claude explanation is a hypothesis, not an observed stack trace or a reproduced
game session. No Kenshi process was returned by `Get-Process -Name kenshi*` at
the start of this audit or at the later verification check. No game was started,
focused, loaded, saved, or modified by this task.

## What had already been fixed

Repository history contains real earlier fixes:

- `5563633`: Asur's save-load cooldown, disabled crash-prone overlay, focus and
  shader-parameter safety, position/bounds checks. This is a separate set of
  failures; it did not validate cached model lifetime during material restore.
- `e322b72`: remove owned material clones from MaterialManager and install D3D
  tracing hooks only on request.
- `7ba4339`: avoid unchanged tint/style work and repeated `lektor` allocation.
- `adc8b08`: detailed localization logging follows this plugin's setting.

The pre-change `ClearTintBinding` still dereferenced a cached raw Entity pointer,
iterated the saved original-material count, restored unconditionally, and cleared
the binding only after all native calls. Its `catch (...)` catches C++ exceptions,
but the production build uses VC10 `/EHsc`, which does not catch access violations.
An escaping native fault therefore leaves the same binding available for the next
frame. This remaining retry/lifetime defect is reproduced below.

## The reported out-of-range explanation needs correction

Installed `OgreMain_x64.dll` SHA256:
`7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6`.

Its exported non-const `Ogre::Entity::getSubEntity(size_t)` is RVA `0xC09C0`.
Disassembly shows an index/current-count comparison at `0xC0A0E`, conditional
branch at `0xC0A11`, and an exception-construction/throw path ending at `0xC0AD5`.
It does not simply read past the subentity vector on an ordinary too-large index.
The old C++ catch already contains that ordinary Ogre exception. The count getter
at `0xC0CC0` directly reads Entity's current vector bounds.

Consequently a rebuilt *live* smaller mesh alone does not prove the reported
world-loop abort. A stale/invalid native object or fault during material work is
the distinct remaining way to escape the old catch. No evidence here proves that
ClickToTeleport caught Blitz's exception, that it interrupted a particular game
world update, or the quoted real-world repetition rate.

Reproduce the native inspection with VS2022 dumpbin `/exports`, then
`/disasm /range:0x1800c09c0,0x1800c0b10` against the installed Ogre DLL. VC10's
dumpbin cannot disassemble here because its `msdis170.dll` is absent.

## Narrow candidate change

- Resolve the building hand again and collect its current model graph once while
  clearing a nonempty entry. Never dereference a cached entity absent from it.
- Guard that actual collector boundary too. A native/C++ collection failure drops
  partial results and retires the whole entry without native restoration.
  Optional per-call failure flags propagate errors from the actual recursive,
  physical, root-getter and movable-type helpers, including their existing inner
  exception handlers. Existing non-strict diagnostic callers keep default behavior.
  Strict partial-result rejection applies to cleanup collection. Normal active
  SyncTint collection still uses the existing tolerant default behavior.
- Reuse SyncTint's existing current entity collection for rebinding and removal.
- Bound restoration by current part count and both saved material arrays.
- Restore only if the current material is exactly this binding's owned clone.
  A replacement mesh or another plugin's material is preserved.
- Contain native faults with small SEH guards that have no owning C++ locals;
  allow ordinary C++ exceptions to reach the surrounding C++ catch.
- Retire the cache and release its clones even when native restoration fails.
  Emit one always-visible error per failed binding, not per subsequent frame.
- Stop touching an entity after its first material restoration failure. Apply the
  same current-membership, clone-ownership and exception rules to batch cleanup.

Instanced-batch tinting and the experimental overlay remain disabled by their
existing production constants. This change does not enable either path.

## Verification

`python tests/test_restore_lifetime.py --baseline` extracts cleanup functions from
pre-change `adc8b08` (pinned so this remains reproducible after commit) and compiles
them with the same VC10 `/EHsc` behavior:
**48 checks, 30 failures, 107 escaping native faults**. The native seam models an
ordinary bad index as a C++ exception, as the installed Ogre binary does.

`python tests/test_restore_lifetime.py` extracts the actual candidate functions:
**48 checks, zero failures, zero escaping restoration/collection native faults,
29 owned clone releases**.
It covers ordinary restore, replacement materials, a smaller mesh, detached
models, failing count/material calls, C++ exceptions, mismatched saved arrays,
full ClearAllTint entry erasure, and 100 repeated inactive cleanup frames.
It extracts the real CollectTintTargetsForTarget, both actual recursive/physical
collectors, both root/physical getters, and both movable-type append helpers.
Seven fault locations include hand-resolution C++/SEH errors, a recursive fault
after partial collection, root/physical getter errors, a movable-type C++ error,
and an invalid physical vector access through the real inner SEH handler.
Batch-only entries, detached batches, foreign materials, and both exception types
also exercise the actual batch cleanup. The disabled batch collection branch is
enabled only in the offline fixture; the production constant stays false.
The hardware-fault fixture calls the failing binding repeatedly under an outer
SEH boundary; the baseline keeps escaping, the candidate retires it on first use.
Ogre seams are doubles; this is an offline lifetime regression, not live gameplay.

Full build succeeds with the canonical build wrapper and `-FromVcxproj`.
Existing SDK enum warnings and the pre-existing `HasStorageFlagsSafe` C4715
warning remain; they are not introduced by this change.
`python tests/check_candidate_imports.py`: **270 named imports / 7 modules /
zero missing** against the installed game and system DLLs (including forwarders).
`git diff --check -- Container-Highlight` passes.

## Binary identity and delivery state

| Location | SHA256 |
| --- | --- |
| Installed DLL before/after this audit | `26d15d879f74b2d5737cb85cdf9ef36ee1bb2e2fb42bf0f70b8956d03cec52ae` |
| Previous build DLL at audit start | `26d15d879f74b2d5737cb85cdf9ef36ee1bb2e2fb42bf0f70b8956d03cec52ae` |
| Workshop content DLL / prepared Nexus v2 ZIP's DLL | `adf457f4c2b7ddaf149aa68cbbbb251ef4406b9351e3133747e8b2aa9054c884` |
| Candidate build, 331264 bytes | `ba4ce0f870584f8b293f189f394bc507de5ddc5e8864d5a66403ce5133cf97ef` |

Candidate: `D:/DEV/Kenshi/build/Container-Highlight/Container-Highlight.dll`.
Prepared ZIP CRC check passes; ZIP SHA256 is
`49d6262e40541b42670ba058aa39956ca2e27e2cba8a398942b59c33beb33c7a`.
The local archive and workshop content are not proof of the public Steam binary.
No public content was downloaded or changed during this audit.

The candidate is **not installed or published**. No release number was bumped.
No release archive was rebuilt. Git commit/push is coordinated by the parent task.

Parent review requested collector-boundary containment, stopping after first
native failure, and auditing batch cleanup. The first candidate failed the added
regression in nine expectations with five escaping native faults; the revised
candidate passes all 33 checks. The original candidate's hash was
`fc2d7d2b8191713a2d3971bfadf4a712e09d6456647524eba123dc7e45d48316`.
Independent review then found that the initial collector regression replaced
inner tolerant helpers rather than executing them. Extracting those actual
helpers reproduced a swallowed partial failure; per-call failure propagation now
passes the expanded 48-check suite. The intermediate 33-check build hash was
`2dcab57604fda3522ae47503e11a487a16e43dcde7672b8824e2931baf8f8919`.

## Performance and lifetime limits

No additional traversal is performed on unchanged active tint frames. Existing
current model lists are reused for active rebinding. The new traversal occurs only
when retiring nonempty entries (key release, removed targets, reset); the normal
target cap remains 32 and recursion depth 10. Temporary current-model vectors are
destroyed after cleanup. No new persistent cache, timer, queue, widget, or retained
native pointer is introduced. Inactive ClearAllTint calls do not collect models.
Owned clone removal remains the existing MaterialManager removal path.
The offline suite substitutes this release boundary with a counting double;
it does not inject corrupted owned MaterialPtr controlblocks or MaterialManager
state. ReleaseTintClone's existing C++ catch is not native-SEH containment. Such
broader heap/manager corruption could still escape during release; this change
does not claim universal recovery from all cleanup faults. The proven and tested
fault domain is stale model collection/restoration while owned materials remain
valid. A quarantine for corrupted owning pointers would need a separate bounded
ownership policy and could intentionally retain resources.

Pre-existing separate limit: save-load `DropAllTintWithoutTouchingOgre()` merely
clears tintEntries, intentionally avoiding Ogre during transition. Unlike normal
cleanup it does not remove named owned clones from MaterialManager. Such clones
can therefore remain manager-owned when a load interrupts active tint. This
candidate does not change that path; safe deferred cleanup would need a separate
bounded design and verification, rather than invoking Ogre during load.

Proposed separate cleanup plan (not implemented): register each clone's unique
name and resource handle when created, with a hard cap covering active plus pending
resources. On load only move those already-recorded identities to pending state;
do not query Ogre or retain old entity/scene pointers. After the cooldown, drain a
fixed small budget per safe render phase, looking up the current manager resource
by name and verifying its handle before removal. Missing/recreated resources are
retired without touching cached objects. Stop creating new clones at the registry
cap until cleanup makes room; never silently discard overflow ownership records.
Tests must cover repeated loads, manager replacement, repeated same names with
different handles, capacity saturation, and a native failure during draining.

This static/offline review does not prove live FPS, long-term memory behavior,
thread races, or that the Steam player's freeze is cured. Travel/interior rebuild,
rapid Alt release, and save/load should be checked in an actual controlled game
session before a public release. No UI skin behavior was changed.
