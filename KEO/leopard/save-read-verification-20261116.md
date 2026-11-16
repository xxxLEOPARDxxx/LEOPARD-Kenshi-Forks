# Native save input batching - local KEO 2, 2026-11-16

No game run, replay, live log/process memory read, capture, profiler or runtime
collector. User's existing save files and settings were not edited. This change
reduces CRT refills while native parsing reads fields; it does not reduce parser
method calls, decode game records or parallelize restoration. Real save-load
time, FPS and long-run memory gains remain unmeasured.

## Native seam and behavior

Pinned Steam EXE SHA256504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e.
GameDataContainer::load6C0800 opens its private wide-path ifstream at6C0927 through
thunk38B95->6C8330, returning6C092C before the first parser read. Args: binary20,
protection40, most-derived1. Native constructor sets filebuf at+10, codecvt+80,
FILE pointer+98. VC10 sizeofifstream110/filebufA0. Native filebuf vtable168EF18
has setbufslot60 ->13A20->1323C0, whose native code calls MSVCR100 setvbuf and
reinitializes buffer pointers. Eight production byte proofs plus the relocated
slot check refuse changed/native-foreign builds rather than signature-scanning.

After the original returns exactly self, only the main thread, enabled live key,
exact caller/arguments and `.save`/`.platoon`/`.zone` extension may prepare it.
Native filebuf identity is checked before the typed view. Refuse nonnull codecvt
(setbuf resets conversion state), failed/used/foreign streams, nonempty FILE
buffer pointers/count, non-read flags and negative descriptor. Existing native
handle is borrowed using MSVCR100 _get_osfhandle. GetFileSizeEx does not move the
cursor or take ownership. Below128KiB or failed query: keep native buffering.
Otherwise request64KiB through native pubsetbuf(NULL,n). No pointer/buffer lease
or handle survives this helper; CRT owns/frees its allocation at native close.

Native constructor forwards once outside optional catches, preserving its return
and exceptions. A failed buffer request logs an error once and retains normal
reading. This is not a strong unchanged-state guarantee: CRT setvbuf may remove
buffering before allocation fails. The fresh-empty FILE gate excludes freeing
an already-owned data buffer with stale streambuf pointers. Fixture failure
injection unbuffers a fresh native stream then returns NULL, or throws bad_alloc;
it is not a real allocator-failure experiment.

Cost: one extra constructor detour on this supported function; eligibility checks
and one metadata request on eligible main-thread save opens. At most64KiB in the
native file stream, released at close. Tiny files pay the metadata check but do
not receive a large buffer.128KiB is a conservative cutoff, not a measured
optimum. No new scan, persistent cache, worker, queue or per-frame call.

## Verification

- Baseline actual VC10 stream fixture compiled and failed capacity CHECK: after
  one byte, ordinary buffering had4095 available. Native64KiB request yielded
 65535. The public stream API, not a hand-written parser, performs all reads.
- Current production-wrapper fixture:400124 checks each /Od and /O2, including
  per-byte assertions over two200003-byte reads. Patterned bytes match exactly;
  observed refills50 versus4. Native seek, buffer-boundary reads, EOF/truncation,
  gcount, close, failed opens, main/config/caller/arg/result/filebuf guards,
  codecvt XOR refusal, empty/tiny files, metadata failure, negative descriptor,
  constructor exceptions and preparation failure/logging are exercised.
- Fixture constructor is a VC10 public ifstream constructor with recorded args;
  it does not execute the game's constructor or install an actual game detour.
  Admission/hook failure tests use a private fake image and fake hook installer.
-22 offline native contracts validate the pinned image, all eight byte proofs,
  caller/thunks/native vtable and actual read/setvbuf imports.10 separately
  compiled production mutations must return ordinary CHECK exit1; compilation
  errors/crashes are rejected. One initially surviving control exposed overlapping
  caller/vtable gates in the fixture; a direct Prepare identity check was added,
  and all10 now fail through the intended runtime checks.
-127 host suites pass. New Performance key/default/live changes and Debug-last
  ordering are checked; INI template, expected module ownership and golden rows
  updated consistently.3 compiled MO catalogs each141 keys roundtrip; Russian
  strings pass ASCII-dash checks. No restart flag for this live setting.
- Full VC10 /O2 /GL /LTCG DEV/PROD builds cover370 CPP files. All installed-engine
  exports resolve;183 KenshiLib/37 Ogre/65 MyGUI imports, startPlugin export, CRT100.
  Import audit explicitly confirms native setvbuf and plugin _get_osfhandle use
  the SAME MSVCR100.dll descriptor table. KenshiLib0.5.0 compatibility gate passes.
- Independent source/fixture/contract/control review found no remaining code
  blocker, subject to the same-CRT check above; that check subsequently passed.

Evidence: stage `build/save-read-*`, `save-read-binaries.json`, source plan/tests.
No new release, ZIP, version bump, publication or ALL_CHANGELOG entry.

## Equipment and remaining CPU work

The existing native material audit was rechecked: getByName841291 and cache-hit
branch8412A4->84293C bypass clone8413B9. Another generic mutable-material cache
would duplicate this mechanism and risk mixing per-item tint/appearance state.
Native Entity skeleton initialization, sharing, scene/physics callbacks and
restoration globals remain native. The earlier41ms attachment callback contained
no morph work and can include texture/material/shader/state waits; it is not proof
of one specific CPU decoder bottleneck. No speculative worker transfer or shared
mutable equipment instance was added. Existing accepted material preparation,
texture/model retention and body-model changes remain present. Actual equipment
assembly and CPU save decoding remain open work; this input change does not claim
to have completed those broader optimizations.
