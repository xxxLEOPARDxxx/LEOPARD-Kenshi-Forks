# KEO: split the remaining model-completion timings

Goal: determine which character-body operation is worth preparing on workers.
Latest immutable evidence: runtime-menu-startup-first. Menu warmup accepted,
first complete63.569s vs209.878s previously. Do not change cache behavior.

Native investigation confirms stock morph caching and render-owned mutable
buffers inside pose blending. Calling these whole functions from a worker is
not an established safe optimization. Measure first; no scheduling changes.

Implementation: extend existing resource_load_probe.cpp with six exact native
boundaries from build/appearance-seam-descriptors-20261115.json: morph lookup,
morph creation, replacement Entity, animation body install, attachment build,
and software pose blend. Use raw borrowed pointers; forward exact float/bool
stack arguments/results/owned16 temporary without extra ownership. Debug OFF
and non-owner calls forward without clocks. No retained native pointers,
per-call allocation/logging, queues, world walks or INI reads. Bounded counters
and existing16 slow-update records only; nested times must not be summed.

1. Add production-inclusion failing controls for exact ABI, C++/SEH propagation,
   disabled/non-owner behavior, result identity, owned16 normal/unwind and
   nested slow-record attribution. Validate exact installed native bytes and
   SDK export/table identities, not comments' RVAs.
2. Implement counters/wrappers and strict per-site native admission; refusal
   preserves stock behavior. Include slow-call sums in existing ring.
3. Run controls /Od+/O2, native contract checks and negative controls, full DEV
   and PROD builds/import validation. Independent review required.
4. Synchronize only owned files, preserve unrelated Steel changes; commit/push.
5. Prepare local test DLL. If game still open, installation waits for closure;
   ask only after binary and evidence are concrete. No release/version bump.

Initial apparent silence: native locations are seeded32entries/250ms, roots
must settle2s, then inventory is scanned on IO worker before first read. Last
run11.066s315roots,21.819s598roots+7379files and1.89GB startup read. These10s
samples do not establish exact first-read time. Existing HUD has resource-list
waiting/scanning phases. No new blind startup-delay or UI rewrite justified.
