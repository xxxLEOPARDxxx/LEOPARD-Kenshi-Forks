# KEO RAM resource cache - implementation ledger (private)

Goal: reuse actual engine resource file data without activating zones/NPCs or doing graphics work on a worker.

Architecture: intercept the verified Ogre FileSystemArchive read-only open. Native loading stays authoritative on misses. A single low-priority IO worker caches successful DDS/mesh/skeleton opens. Hits return independent read-only Ogre streams backed by shared immutable bytes. This first stage improves reuse of already requested resources, not prediction for unvisited zones. No VRAM allocations in this stage.

Constraints: VC10 x64 /GL + LTCG; opt-in; 1-32 GiB RAM ceiling, not allocation target; retain system reserve; bounded entry/queue/file size; cancellation on disable; in-flight streams retain safe ownership and count toward budget; native fallback on failure/content change; no game object access on worker; no world scans/per-frame allocations. Settings live, debug summaries only with detailed logging. Preserve existing path diagnostic configuration and unrelated steel jobs edits. Three locales, ASCII hyphen in Russian game strings.

- [x] Test actual asynchronous store: immutable reads, file change, queue cap, disable/cancel, stream lifetime, budget/admission.
- [x] Implement store, verified archive boundary, lifecycle and settings.
- [x] Build DEV/PROD, validate native import/export/ABI and three locales.
- [x] Independent perf/lifetime review; fix findings.
- [x] Installed DEV DLL and three MO after fresh process check; personal config hashes unchanged. Gameplay A/B remains pending before performance claims or release.

Ruling: retain files requested by the engine first. No guessed zone-to-resource association and no extra zone activation. Prediction and GPU residency are separate later stages after a measurable first implementation.

