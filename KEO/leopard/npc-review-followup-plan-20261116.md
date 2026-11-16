# NPC loading review follow-up

User requests continued NPC/model/equipment/save loading work from existing
evidence and a quality review; no new game runs, profiles or data collection.
Continue working release2; no package/publication/version increase.

Two confirmed targets, independent of unresolved native appearance/save cost:

1. The admitted body-pose cleanup checks membership quadratically even when the
   entire list exactly matches recorded borrowed pointers. Add an ordered full
   sequence check, clearing only after every element matches; retain existing
   compaction for mixed, shorter, longer and reordered lists. Actual Ogre fixture
   extracts the production function and counts only instrumented comparisons;
   first show current512-pose path exceeds512 comparisons, then fix and prove
   output/owned elements/source lifetime plus strict negative controls.
2. Geometry Init erases an SDK-published trampoline when hook enable fails.
   Preserve the original and require valid original for readiness. Extract actual
   Init body with controlled module identity/SDK results; reproduce failure, fix,
   verify successful/failed/null outcomes and foreign-thread/repeated init.

Native/core ownership review is separate from resource/save review. Final review
must inspect changes, existing native exception/mixed/nested cases, boundedness,
no additional persistent cache, hook behavior, build and import compatibility.
No native thread scheduling or body/equipment/world logic is rewritten.

Preserve seven unrelated Steel files and personal INI/locales/mods.cfg. Commit
only tested scoped changes and push origin. Install only after fresh game-closed
check, with backup and hashes. No request for another gameplay route.
