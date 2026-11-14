# Private: remaining completion attribution, 2026-11-14

Status: diagnostic source only until explicitly recorded installed/runtime.
The accepted resource cache/source2ad60b7 is unchanged. This is not a claimed
fix of residual stutters. No scheduling, spawn, readiness, batching, threading,
cache settings, MCM options, localization or public release changes.

## Reason for this narrow follow-up

Accepted runtime:688 meshes/2276 retained-reference hits, owner says much
smoother. Last busy10sec window had resource update2calls>20ms while direct
factory had none>2ms. Existing cumulative maximum91.904ms included initial
load and cannot describe one later stutter or one NPC.
updateMT includes mesh callbacks, manual texture expiry, upload and assignment.
It already batches20mesh completions but no equivalent bounded texture group.
Reducing work blindly risks its incomplete isLoading-ready-list contract.

## Added attribution only

* TextureLoadData::getTexture Steam44ACD0: RCX=request,RDX=caller output,
  RAX=original result. Original called once, no output construction/destruction.
* AppearanceBase::buildBody538590 and createEntityCallback536590: RCX=self,
  RDX=indirect by-value16-byte ResourcePtr temporary,R8=data. Raw pointer
  forwarding adds no C++ ownership; original callee destroys its original
  temporary, including native unwind. Caller arguments/results/exceptions kept.
* Array448BC0 already belongs to the existing GPU bridge. Time that wrapper
  using Begin/End token; do not add a conflicting native detour. Exception
  skips End and propagates through the existing bridge's cleanup.
* All new native detours require exact decodedSteam PE timestamp65D604D7,
  image232C000 and32prefix bytes. GOG remains refused for optional diagnostics;
  no attempt to guess addresses. The actual KenshiLib six-byte stub indices
  and paired Steam RVA table confirm all3sites. See native test.

Cached Debug off forwards with no QPC, INI, stats mutation or thread checks.
Off-main forwards without clocks. Enabled completed samples use two clock reads
and fixed numeric state; no resource lookup/retention or queue creation.
Array Begin returns-1 for disabled,0 is a valid timestamp. The existing wrapper
records completion only after original returned. No exception is intercepted.

Each stage now prints windowMaxUs alongside cumulative maxUs; window maximum
resets at the existing10second summary. For native resource updates>=10ms,
a fixed16-entry ring stores completion time, total duration and child-time
differences. Overflow replaces oldest and increments a saturating counter.
At most16 details flush with that summary, never in the native hot call.
Child timings remain inclusive/nested and are explicitly labeled overlapping;
do not sum them or claim an exclusive residual. Generic entities are not
NPC-only. Snapshots retain only numbers, no native resource pointers.

## Verification and limitations

* Production cpp included into VC10 fixtures:64 controls each/Od and/O2.
  Preserved42previous controls; new args/results/offmain/disabled, C++/SEH
  propagation, array0token, window reset, exact child deltas/ring bound.
* Nontrivial owned16-byte VC10 callback fixture verifies one caller copy and
  one native destructor on normal return and C++ unwind across raw wrapper.
  This is a compiler ABI surrogate, not a real SceneManager-created character.
* Actual native PE/32bytes and SDK export->Steam table, one array hook:8checks.
*4compiled expected-RED production copies:wrong2Dreturn,lostappearancedata,
  refusalofzeroarraytoken,wrongringcapacity. Production files never mutated.
* Real WARP/TLS/COM/byte-adoption fixture passed; added timing once and native
  throw propagation without a completed sample or stale TLS. Warmup adoption
  still delivers the same native COM object and changedbytes fallback.
* Full363source DEV/PROD/O2GL/LTCG links;183Klib0.5.0imports,31actualOgre and
  65MyGUI imports,CRT100only. Build verification in local stage.
* Independent review found no Critical/Important issue and independently
  reran64/Od and64/O2. No new gameplay/FPS/long-run memory claim.

Next runtime should be a normal save-load and a single route after startup
warmup, pause after a residual stutter. New log can identify a costly texture
finish/appearance callback or reveal an unmeasured wait/bookkeeping component.
Only then choose an optimization; do not portray diagnostics as acceleration.
