# Existing-profile loading review and CPU pose spike, 2026-11-17

No game runs, live logs, WPR, captures or process-memory collection. Use only retained profiles, static engine files and isolated executable fixtures. Preserve seven foreign Steel files, personal INI/modlist; working release2 remains unpublished.

The retained ETW trace contains only4.7 seconds near the end of an80-second recording and is mostly rendering, so it cannot rank complete save loading. Old slow resource-completion samples show mesh callbacks dominate selected spikes, with substantial body creation and pose deformation. Counts are nested inclusive elapsed and snapshots predate later optimizations. Animation locking and field keys are not established dominant costs.

1. Reaggregate retained evidence with explicit selection/overlap limits; identify already addressed costs and unavailable whole-save CPU attribution.
2. Build a pure pose arithmetic oracle against actual native Ogre. Test worker MXCSR, exact operand order, sparse indices, disjoint ranges and bounds. Compare coarse native/serial/parallel paths at realistic and large sizes before choosing a threshold. Reject an accelerator that does not outperform native work.
3. Keep each native pose-call boundary: main thread owns native buffer/lock/lifetime; workers touch only owned scratch and immutable maps until synchronous join. No deferred pose queue across callbacks. Finite-weight/masked-FP/layout/caller/private-mesh gates; native fallback before publication, no replay after any native write begins.
4. Only if the spike establishes a useful safe path: integrate live setting/EN-RU-ZH, explicit worker stop, native guard/ownership/exception tests, independent review, builds/host/catalog/import/unwind checks, closed-game local install and scoped commit/push.

Across-pose deferral was rejected: GameData lookup may allocate and expose undeformed geometry through reentrant callbacks; proving all observers absent is disproportionate. No such deferred behavior is implemented.

## Outcome

The native arithmetic spike passed its limited oracle, but sparse/clustered workloads regressed and five ordinary static body meshes had zero eligible poses. Step4 was deliberately NOT performed. All tentative tracked production/settings integration was restored to226af2d; experimental sources are isolated under leopard/pose_cpu_spike. Existing ETW does not retain the save CPU span. See heavy-loading-review-20261117.md; the performance objective remains open.
