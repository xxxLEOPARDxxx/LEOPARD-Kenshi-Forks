# Archive opens: reject unsupported cache formats before building paths

RAM cache admission supports texture, .mesh and .skeleton files. Previously,
every native read-only archive open first built/canonicalized an absolute path,
even for .xml, .material, .program, .fontdef, .layout and .cfg files. Find and
Observe rejected those names afterwards without changing their statistics.

A shared pure extension predicate now precedes Path and is also used by absolute
path admission. Unsupported files forward directly to native loading, exactly
once. Full absolute-path, file-identity, memory and budget validation remains in
the store for eligible files. There is no path allocation, Windows path call or
cache lookup for unsupported names. No supported extension was removed/added.

The fixture extracts the actual production Path/Open functions and uses real
Ogre output SharedPtr objects. Fail-first rejected the old path work. 32 controls
pass /Od and /O2: supported formats and mixed case, unsupported names, exact
native output/arguments, native exceptions, disabled/write paths and isolated
cache lookup failure. This eliminates confirmed unnecessary work; no startup
speedup factor or game FPS result is inferred from these controls.
