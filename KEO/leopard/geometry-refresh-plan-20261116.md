# Allocation-free unchanged geometry refresh

Continue NPC/model/equipment/save work using existing evidence only. No new game
run, capture, process-memory/log read or profile. Working release2, no publication.

Native Entity builder already reserves exact SubEntity storage. Replacing its
constructor/copy/exception path for a56B temporary-map saving is rejected; no
native Entity rewrite or extra reservation hook is justified by this inspection.

Confirmed own cost: geometry Tick creates a new Description twice each250ms,
allocating its buffer vector even if the cached model has not changed. Replace
this validation-only collection with a local, allocation-free matching visitor.
Share all mesh/vertex/index/LOD traversal and metadata bounds with the collector.
Preserve null/system skips, first-encounter dedup order, duplicate skip before
metadata reads, size/shadow validation,512-buffer and8GiB/64-binding/64-submesh/
2048-index-LOD limits. Search only previously matched prefix for duplicates.

On exact match retain the entry unchanged. On mismatch call ordinary Describe
before the existing Release/Promote path. New observations still collect normally.
No persistent scratch, pointers, queues, threads, hooks, settings or budget edits.

First add actual-native stable512-buffer fixture with allocationFailAfter0:
baseline refresh clears its cache on the first description allocation; optimized
refresh must keep identical storage/accounting/pins without attempted descriptor
allocation. Then exercise reorder/new/missing/duplicate buffers, size/shadow/
eligibility changes, limits and failing fallback. Strict source-copy controls,
existing native ownership/accounting suites, independent review, full builds and
imports. Preserve settings/3MO/7foreign files, scoped commit/push, closed-game
local DLL installation with backup. No measured loading/FPS benefit assertion.
