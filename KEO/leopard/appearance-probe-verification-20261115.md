# Private KEO appearance attribution, 2026-11-15

This is a diagnostic follow-up, not a threading optimization. Preserves the
accepted menu-fast warmup and RAM/geometry consumer fixes. No INI, MCM, locale,
native batching/readiness, resource ownership or worker behavior changes.

Latest user run: runtime-menu-startup-first immutable snapshot. Menu first
complete63.569s vs209.878s before;7379processed,7295loaded,84skipped,limitnotreached.
Actual startupFastReadBytes14554737079. Final705retainedmodels/2914referencehits.
No controlled timing/FPS comparison, no physicalGPUresidency inference.

Stock native morph reuse already exists. Creating a missing body variant clones
Mesh/submeshes/poses, blends into mutable hardwarebuffers, switches Entity and
transfers materials. Whole native functions are not established worker APIs.
Six timing seams now distinguish lookup,create,switch,animation,attachmentbuild,
and softwareposeblend. An already measured latecallback21.410ms contains
20.228msattachment; another21.462mscallback had only37usattachment. Measure
both paths rather than attributing every callback to morph math or squad AI.

Each new seam is pinned to exactSteam image/RVA/full32prefix. Ogreposeblend
also checks actualnamedexport. GOG leaves current native fallback. Refused site
logs an error and does not replace unknown code. Native owned16MeshPtr is raw
forwarded, never copied/destroyed by our wrapper. Exact replacement pointer
returned; inputEntity may already be destroyed, so never inspected aftercall.

Storage: six staticStats (48bytes each) plus6*8*16=768extra bytes in existing
16recordring, no resource/pointer retention, no percall heap/INI/log/mutex/world
walk. DebugOFF shortcircuits before mainthread query/QPC; nonowner forwards
without QPC/counters. Enabledtimings are inclusive elapsed, not CPU time.
Globalstage counters may include operations outside completion; slowrecords
use deltas within the actual updateMT call. Nested times must not be summed.
Fixed instrumentation is not proof of zero FPS overhead or absence of leaks.

Verification:
* Added failing production-inclusion fixture before implementation (missing
  seams/ChildTimes), then110controlsPASS each VC10/Od and/O2. Exact sixarg
  float/stackbool,staticXMM0,borrowedpointer,return,debugOFF/nonowner,C++/SEH,
  owned16copy/destruction/normalunwind,ringchildren/reportcadence covered.
* Sixsites12actualnativeimage/prefix/export/tablechecksPASS; earlier10checksPASS.
* Fournewcompiledproductionmutants fail as expected (loststackfloat,swapped
  posemaps,lostreplacementresult,wrongslowchild), earlier4mutantsalsoPASS.
* Independent reviewer reran110controls/Od+/O2 and12nativechecks. No Critical
  or Important findings. Tests use stand-ins and do not prove livehookadmission
  or performance. Descriptor extraction used customPEparser;pefile's8192export
  cap omits some real symbols in these large libraries.
* Full363sourceDEV/PROD/O2GL/LTCG builds pass.183Klibimports0.5.0,31actualOgre,
  65MyGUI,CRT100only. Binary hashes recorded separately in buildverification.

Runtime acceptance pending for these six new sites. Preserve priorfdc97105
acceptedfastwarm DLL and personalsettings. No release bump/publication/ALL_CL.
