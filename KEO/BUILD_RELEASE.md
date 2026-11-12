# Building the maintenance release

The production DLL for KEO v2 was built from code commit `e68e2a6927c1ebfb90e36a8736c60f92f9cbf3b9`.
Its SHA-256 is `9f5392092d45494e05bde1473975fdda4504c837f2a4692040353cd10ff171f4`.

The KEO v1 DLL was built from `7609b76e125ab0709a804325fc7e631064875eeb`
with SHA-256 `ea56647bb504a5287ee2589b815d9b1ac1cef6e760d6921048420908ede86751`.

Use Windows x64, Visual C++ 2010 x64 and Windows SDK 7.1. Set `KENSHILIB`
to the KenshiLib development tree containing `Include/kenshi/GameWorld.h`
and `Libraries/KenshiLib/KenshiLib.lib`; set `BOOST_ROOT` to Boost 1.60.0.
The original build scripts describe the required KenshiLib 0.5.1 header layout;
the admitted GOG runtime profile uses KenshiLib 0.5.0. Development headers and
the runtime DLL are separate requirements. Do not replace a runtime library
merely to match the development-header label.

With compiler/SDK INCLUDE, LIB and PATH initialized, run:

```bat
set BUILD_MP=6
build_step4_noenv.bat
```

The default zone-handoff stage is 3. Output is
`build/KEO_step4_prod/KEO.dll` (production) and
`build/KEO_step4_dev/KEO.dll` (diagnostic development build).
Use the production DLL for distribution. `tools/build` and `tools/kenshilib`
contain the actual build and dependency checks. External SDKs, game executable
files and proprietary game libraries are not redistributed in this archive.

The `leopard` folder retains focused host checks and native-contract tests.
Tests that inspect Steam/GOG executables require the user's own admitted files;
they read those files and do not execute or distribute them. Native profile
checks do not replace game testing on either store.

Original KEO source: https://github.com/jacobericson/KEO
KenshiQOL by Steel distributes its source with the original mod:
https://steamcommunity.com/sharedfiles/filedetails/?id=3772744830
See LICENSE and the Steel provenance files. The source archive contains the
modification sources and build scripts; README_UPSTREAM.md preserves the
original author's documentation separately from the current release guide.
