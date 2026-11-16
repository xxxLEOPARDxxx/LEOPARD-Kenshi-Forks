# CPU save-string transfer, local working release2

## Change and boundary

This pass changes actual CPU string-copy work inside GameDataContainer parsing,
separately from the preceding save-read buffer. Pinned Steam helperA47B0 accepts
an owned by-value std::string, copies it to slot[count], increments count, then
frees/empties its argument. Exact parser callers return at6C1EBC/6C1F47/6C2070;
GameData::addSavedInstancedObject returns at6CAC21. The latter first constructs
a separate stack temporary from the original state list at6CAC0E.

On the main thread, with live/defaultON saveStringTransfer enabled, a long
argument (>15bytes) and fresh constructed destination (empty/capacity15) use one
typed VC10 string swap and one count increment. The argument becomes empty SSO;
native list cleanup retains ownership of the same heap buffer. No allocation,
free, raw whole-string memcpy, worker, cache, retained game pointer or world
scan is introduced. Short strings, stale/nonempty/retainedheap destinations,
growth, other callers and threads forward once to native, outside catches.
Source state lists and save format/field values remain unchanged.

Runtime admission checks exact image ID and247 bytes across the complete152-byte
helper, thunk, caller sequences and native delete thunk. Counts/capacity are
bounded; address overflow and argument overlap with the complete destination
allocation refuse optimization. Unknown/changed/hooked code refuses admission.
The release VC10 string40/list24 layout is compile-time checked.

## Ownership qualification

Initial design used two swaps plus a local old-string destructor. It was narrowed
to a fresh empty SSO destination and one swap to eliminate plugin-side freeing
of native allocations entirely and avoid changing native spare-buffer reuse.
Native deleteED5628 imports mfc100u ordinal1278; the installed system MFC ordinal
is a jump to MSVCR100.free, but this implementation does not depend on that
cross-module delete equivalence. Normal native cleanup remains responsible.

## Verification

- Fail-first faithful VC10 native-helper reference: long-state insertion reports
  one allocation and one free and fails the zero-extra-allocation assertion.
  Final production fixture reports zero allocations/frees on that insertion.
- **17985 checks each /Od and /O2**, using actual VC10 std::string behavior:
  source lengths0..259, SSO boundary15/16, stale destination contents, already
  reserved empty destination, source preservation, alias/capacity/caller/thread/
  live-setting/admission fallback, native exceptions, repeated slot reuse,
  allocation failure, synthetic proof refusal and partial/failed installation.
- **1000 real x64 by-value function-pointer calls** directly to the production
  hook: precisely the initial argument-copy allocation, transferred unchanged
  buffer, one eventual release by destination destruction, unchanged source.
  Ordinary pointer-driver tests separately verify empty argument state.
- **12 compiled production-copy negative controls** each compile successfully,
  then fail with ordinary CHECK exit1. Compilation failures/crashes are not
  accepted. Canonical source and fixture hashes remain unchanged.
- **22 pinned on-disk contracts** verify executable hash, all proof bytes,
  actual calls/thunks and consumed argument cleanup. No game execution.
- **127 host suites pass**, including config ownership, golden/template counts,
  MCM live OFF/ON/defaultON/noRestart and final Detailed log row.
- **371 units, DEV and PROD /O2 /GL /LTCG builds pass**. Full export/import audit:
 183KenshiLib/37Ogre/65MyGUI imports resolve, startPlugin present, CRT100 only;
 KenshiLib0.5.0 compatibility gate passes. Three MO catalogs contain143 keys,
 same EN/RU/ZH keys and placeholders; Russian game strings pass glyph checks.
- Independent final source/fixture/contract/config review by
 `/root/morph_upload_design_review`: no Critical/Important blocker; reviewer
 inspected evidence but did not rerun tests. Initial by-value ABI and allocator
 feedback was addressed before the final verification above.

Evidence is under `_codex_work/keo-resource-cache-20261114/build/save-string-*`
and `save-string-binaries.json`. This proves local ownership/copy-work behavior,
not live incidence, total save loading time, FPS or long-run leak freedom.

## Equipment audit in this pass

Rechecked actual material-name construction at840B80. Calls841078,84108F,
8410A6 and8410BD use rvalue append helpers, not repeated lvalue deep-copy
concatenation: thunk32D6C resolves toA4180, thunk39C1B toA44F0; each appends to
its left temporary, then moves that temporary into the returned string via
21A3. Replacing these with naive reserve+two-append concatenation would discard
existing native ownership transfer and can increase work. No such rewrite was
added. Existing getByName841291/cache-hit8412A4->84293C already bypasses material
cloning. Shared mutable materials would risk per-item/faction appearance state.

No new equipment assembly optimization is claimed by this pass. Existing copied
material storage, body/morph and resource changes remain intact. Native item
assembly, remaining parser fields/object construction and complete save loading
remain further work. No new gameplay run, log read, profile, capture or process
memory access was performed. General FPS work remains deferred per user.

Local candidate only: release2, no ZIP/version bump/publication/changelog entry.
