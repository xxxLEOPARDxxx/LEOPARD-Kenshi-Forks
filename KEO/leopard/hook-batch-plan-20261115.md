# Hook batching and cache/load follow-up

Goal: repair the module-ID wrapper defect locally in KEO and reduce startup thread-freeze cycles without touching KenshiLib.dll or the user's settings. Existing-source/offline/native fixtures only; no game reproduction or collector.

1. Add a fixed-capacity coordinator that collects explicitly independent hooks, queues with ONE caller module ID, applies once and publishes completion only afterwards. All originals survive queue/apply failure; no retry through AddHook after touching the queue.
2. Admit the internal SDK exports only after existing SDK SHA verification plus loaded entry/RVA/prefix validation. Unsupported admission uses the old immediate path BEFORE any queuing.
3. Opt in far visibility, paused skip, faction self-relation and corpse position hooks. They have initialized independent native inputs and own originals, no worker capability/mid-body patch dependencies. Their success/error logging becomes completion-based. Other coupled hooks remain immediate.
4. Actual-production coordinator/install fixtures: success, every creation/queue/apply failure, each partial enable subset, reentrance/callback publication, original pointer lifetime, unsupported SDK, no retry, native args/results/exceptions. Native exports/signatures and caller-ID mismatch checked against pinned installed SDK/source. Full DEV/PROD/import and host regression checks follow.
5. Inspect current cache and saved loading evidence for another proven load-time cost; do not replace native NPC creation with unproven worker/GPU calls or remove file identity checks. Explain RAM source-byte versus prepared GPU resource caches, with CPU skeleton/body/physics costs separate.
6. Review, sync only owned files while protecting foreign Steel edits, commit/push and install with closed-game check. Preserve settings, retain backup, clean own intermediates and update memory. No publication/version bump or measured FPS/startup claim.
