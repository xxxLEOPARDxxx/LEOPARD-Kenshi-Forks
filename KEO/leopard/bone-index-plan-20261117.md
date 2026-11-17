# Bone index table preparation, 2026-11-17

Working KEO release 2. Use only the retained complete load profile; no new
game runs, captures, game logs or process memory. Offline native fixtures
are verification, not proof of overall save-load speed.

Raw main TID16080: Mesh::buildIndexMap has151 inclusive/43 leaf samples;
35..40s110/32 out of4840 samples. Its repeated set insertion has76/71 in
that window and calls back to buildIndexMap1A5EE1. This is distinct from
normalization and assignment import, but at most2.27% of that old window.
Edge-list deserialization is larger (231 inclusive there), but batching its
stream/endian/partial-state handling is a separate, less certain change.
General allocator replacement and a borrowed-node insertion cache are rejected.

Replace the exported whole buildIndexMap method on the supported owner/ABI.
Use an8192-byte local bitset plus128-byte occupied-word hierarchy for the full uint16 bone domain, with bounded
input size. Collect before changing output arrays. Keep native output growth
1B0570 and its order: inverse map, then forward map. Reload size/data after
callbacks; zero only newly grown elements, preserve old holes and shrink
storage. Ascending keys retain exact ranks including65535/length65536.
Small/large, aliased or unsupported cases forward before output writes.
Enumerate occupied words/bits only; scanning the full sparse domain was
rejected after its retained-output microbenchmark regressed.
No catch-and-retry after growth, no worker/cache/retained source pointers.
Eliminating temporary set allocation intentionally removes those failure
points; output-allocation errors and partial state must retain native behavior.
Reuse the live meshWeights option, all3 localizations, Debug unchanged last/OFF.

- [x] Native-oracle regression fails on forwarding-only helper (RED).
- [x] Implement helper, admission contract and whole-method bridge.
- [x] Native oracle, growth errors/reentry, actual assets and full-wrapper benchmark.
- [x] DEV/PROD, relevant/full host checks, imports, translations and review.
- [x] Preserve settings/foreign edits, local install if closed, commit/push.

Do not ship a slower experiment or claim whole-save/FPS gain from a fixture.
