# Private: native finalization timing probes, 2026-11-14

Diagnostics implementation only. No installed DLL, game input, process control,
native scheduling/readiness changes, release or public announcement in this task.

Authoritative binary analysis: `npc-finalization-seams-20261114.md`.

## Measured boundaries and native identity

The existing `factoryDrain` and `resourceFinish` probes and public
`ResourceLoadProbeInit` / `ResourceLoadProbeReport` APIs remain intact.
Three additional raw Windows x64 wrappers preserve original arguments, result,
native C++ exceptions and native SEH. Each invocation calls its original once.

* `factoryProcess`: `void* (void* factory, void* item)`, Steam RVA `580CE0`.
  It measures generic RootObjectFactory processing, including non-NPC objects.
  Prefix: `48 8b c4 55 56 57 41 54 41 55 41 56 41 57 48 8d a8 a8 fe ff ff 48 81 ec 20 02 00 00 48 c7 45 c0`.
* `setMeshData`: static `void (void* entity, const void* data)`, Steam RVA
  `447850`; there is no ResourceLoader this argument.
  Prefix: `48 8b c4 57 48 81 ec 80 00 00 00 48 c7 44 24 20 fe ff ff ff 48 89 58 18 48 89 70 20 48 8b 05 45`.
* `entityInitialise`: `void (void* entity, bool force)`, Ogre RVA `BFAC0`,
  named export `?_initialise@Entity@Ogre@@QEAAX_N@Z`.
  Prefix: `48 8b c4 55 41 54 41 55 41 56 41 57 48 8d 68 a1 48 81 ec e0 00 00 00 48 c7 45 97 fe ff ff ff 48`.

All three require the existing Steam path, plus EXE PE timestamp `65D604D7`
and image size `232C000`. New EXE seams additionally require every prefix byte.
Ogre admission additionally requires the actually loaded `OgreMain_x64.dll`,
AMD64 PE64 headers, timestamp `5CA5F929`, image size `9C9000`, exact 32-byte
prefix at `BFAC0` and the named export resolving to that exact address.
Prior detours or mismatches refuse the optional diagnostic and log an error;
there is no replacement fallback or speculative alternate address. The existing
GOG early refusal remains, leaving its native loading behavior untouched.

## Cost, ownership and attribution

Every disabled invocation immediately forwards after the cached detailed-log
gate. No QPC, thread check, formatting, allocation or counter mutation occurs.
An off-main-thread invocation forwards after the cached gate/thread check,
without QPC or statistics. The measured path uses two QPC reads and fixed POD
statistics only. Exceptions propagate before the second clock/sample and are
excluded from completed-call counters.

Three fixed-size Stats instances hold saturating count, inclusive elapsed total,
maximum, over-2ms and over-20ms counts. Existing thresholds remain strictly `>`.
No retained entities/data pointers, queues, events, heap storage or per-call
locks were added. Wrappers never read INI or invoke loading/world work.
The existing ten-second summary includes all five seams in one line. Counters
are cumulative completed-call elapsed wall time, including waits. Nested and
overlapping seam totals must not be summed. Neither factory nor entity counters
are NPC-only, and none demonstrates CPU utilization or a performance gain.

## Fresh verification

Command: `cmd /c leopard\test_resource_load_probe.cmd` from this stage root.

* Actual production cpp is included into the fixture; wrapper logic is not
  duplicated in tests. All original 17 controls remain.
* Initial RED: new wrapper/guard tests failed to compile because the three
  wrappers, their counters and admission helper were absent.
* Runtime negative control: temporarily returning NULL instead of the original
  factory result failed `factoryProcess exact arguments return and once` with
  exit 1. Production source was restored in a finally block.
* Fresh restored GREEN: 42 controls passed under VC10 `/Od` and `/O2`.
* Controls cover arguments, factory result, exactly-once native calls, independent
  completed accounting, true/false force, disabled/off-main paths without QPC,
  C++ and SEH exception propagation for all three wrappers, saturation and
  thresholds, PE/header/range refusal, byte 32 refusal, and summary cadence.
* The fixture maps the actual installed Ogre DLL with
  `DONT_RESOLVE_DLL_REFERENCES` (no DllMain/dependency execution), admits the pinned
  PE build and all 32 bytes, and checks its actual named export address.
* The native translation unit compiled against actual KenshiLib/Ogre/Boost
  headers in both DEV (`/DKEO_DEBUG`) and PROD configurations.

No full link, gameplay hook-installation, FPS, load-time, long-run memory or
leak claim is made by these focused checks. Root handles full-build integration
and any later installation separately after all stage changes are frozen.
