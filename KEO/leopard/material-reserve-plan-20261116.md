# Scoped material copy storage preparation

Continue working release2 NPC/equipment/save loading from existing source and
pinned native files. No game launch, capture, new profile or gameplay data.

Native Pass copy constructor211C90 initializes the texture-pointer vector at118
before calling Pass assignment212020 at211E4B (return211E50). Assignment retains
vector capacity and pushes independently copied TextureUnitState objects using
1.5x capacity growth. Existing source8/16/64 paths grow6/8/12 times respectively.
Independent VC10/native spike confirmed Pass210/vector32/offset118 and matching
allocator behavior through native clear/destruction. SDK TUS inline parent/name
getters have different layout: do not use these fields in production/tests.

Design: one admitted Pass assignment detour; prepare only exact ctor-return
211E50, main owner, live opt-in, nonnull distinct source/destination and EMPTY
destination pointer vector, source2..64, insufficient capacity. Use exact native
Ogre vector reserve. Native ctor/assignment/texture objects/settings, shader
work, file loading, refcounts and exceptions stay native. Allocation failure
falls back once to original. No shared materials, saved pointers, queues or
worker threads. Other callers forward untouched; preserve SDK original on hook
failure, require valid original before readiness. Unknown builds refuse.

Verification sequence:
1. Actual native copyCtor fixture with source8: baseline must fail a zero-growth
   expectation cleanly, proving original repeated pointer-vector growth.
2. Production wrapper/prepare/admission plus actual native clone/copy tests:
   counts0/1/2/3/8/16/64/65, output/independentTUS/nativeparent equality,
   source stability, OFF/nonowner/wrongcaller/partialinstall/refusedlayout,
   reserve failure/native exception, nonempty/existing-capacity/self cases.
3. Strict production-copy controls, pinned ABI/caller/clearing/growth contracts,
   independent review. No throughput/FPS/whole-save speed claim.
4. Own live/defaultON MCM option, EN/RU/ZH, DebuglastOFF/no restart flag, host
   config/metadata checks, full DEV/PROD builds/imports. Preserve7foreignSteel
   paths, personal INI/mods.cfg. Scoped commit/push and closed-game local install.
