# GPU constant-map rebind invalidation - 2026-11-15

Stage-only source change in `src/render/gpu_params.cpp`. No gameplay, profiling,
new capture, installed DLL, INI change, source-checkout edit or publication by
this task. The existing127ms attachment callback is not attributed to this work.
No measured FPS or load-time improvement is claimed.

Frozen production SHA256:
`6005d103700837c0463b3ca4a7739038a35011bb569296fb0554089760b5ce86`.

## Proven redundant work and conservative fix

GpuParamCache keys are `(GpuNamedConstants*, name-object*)`, not parameters-object
identity. Previously every nonnull OLD map passed through4096 cache slots before
any `_setNamedConstants` assignment, even when another params still owned that
unchanged map. Off-owner replacement requested a full owner-cache flush too.
Assignment from an initially null old map already had a cheap no-op; that case
must not be described as a4096-slot scan.

Actual Ogre SHA256:
`7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6`.
Exported GpuProgramParameters::_setNamedConstants atE89F0 first assigns the
SharedPtr atparams+128 viaD9CA0, then grows only the params' float/double/int/uint
value buffers at58/78/98/B8. It never edits OLD map nodes. D9CA0 increments the
incoming reference, swaps pointer/control-block fields, then releases the old
reference; equal control blocks are a native no-op.

The fix skips OLD-map invalidation for different-map assignment **only** when
the OLD control block is the admitted native default map deleter. This preserves
old definitions for other native owners; actual final deletion remains observed
before freeing map nodes. Same-pointer rebinds still invalidate, preserving the
existing external in-place-edit/rebind convention. SDK/custom/unknown control
blocks retain the old invalidation and off-owner flush behavior.

This qualification matters: the real SDK-created GpuNamedConstantsPtr fixture
frees maps without enteringDAC30. A simple different-pointer-only optimization
failed the SDK last-reference regression and was not frozen for integration.
The final implementation does not assume every possible destructor/deleter uses
the same native code path.

## Lifetime and ABI contract

Native program mapping factoryDA2A0 creates a control block with vtable684050.
Its destructorDABB0 loads the map atcontrol+28 and callsDAC30 withflags1; the
native DELETE_T destructorDA970 also callsDAC30(flags0), but unrecognized control
blocks conservatively retain invalidation rather than widening the whitelist.

The fast case requires Ogre timestamp5CA5F929, image size9C9000, vtable684050
pointing toDABB0, exact32-byte DABB0 prefix and exact19-byte map-load/flags/call
sequence atDABD7. The latter ends with the relative call toDAC30. Metadata is
cached before installing/publishing the setter hook. Missing/mismatched metadata
disables this optimization; it does not disable the existing conservative cache.

The hot wrapper borrows raw pointers only: incoming SharedPtr's pointer is at0,
params' named SharedPtr is at128 and its control pointer at130. It adds no
SharedPtr construction/destruction, allocation, worker, native hook, counter,
lock, logging, module lookup or world traversal. Existing setter/deletion hooks,
their ABI guards, original arguments and exactly-once native call are preserved.
GPU upload's dependency on the deletion counter is unchanged.

The old comment about GpuProgram::setManualNamedConstants keeping the same map
address was incorrect for this binary. ActualD8410 calls
createNamedParameterMappingStructures(true) D8290, allocating a fresh48-byte
named map before copying definitions. HighLevelGpuProgram::unloadHighLevel
at10EEB1 calls that same fresh-map routine. Old params retain their previous map
until their own references disappear. The new comment describes the separate
supported compatibility case: an external caller edits and rebinds the SAME map.

## Native fixture and verification

New files: `leopard/test_gpu_params_rebind.cpp` and
`leopard/test_gpu_params_rebind.py`. Run the Python script from the stage.
Outputs are under`build/gpu-params-rebind`.

- Fail first: with the original production wrapper, actual native params/map
  construction and lookup passed, then the shared-old-map cache entry was lost
  on different-map rebind. The fixture failed at that exact cache assertion.
- Separate compatibility RED: the initially proposed ungated optimization left
  an SDK-created map's cache entry after its actual final free. It failed the
  safe DropMap-only assertion without dereferencing freed map nodes.
- Final **38 actual-native controls PASS in both VC10 /Od and /O2**. The runner
  includes the complete production wrapper bodies, stripping includes only, and
  links the real production GpuParamCache implementation.
- Actual native constructors, named-map factory, parameter setters, constant
  lookup, SharedPtr control blocks and destructors run without a renderer/GPU.
  The fixture temporarily detoursDAC30 only inside its own standalone process,
  using its verified reloc-free15-byte prefix and a trampoline, then restores
  that process's DLL mapping before exit. Installed files and the game are never
  patched. Native last-reference frees therefore exercise the real production
  delete wrapper before map-node destruction, not a simulated refcount.
- Coverage: shared OLD map survives native different-map rebind; new lookup uses
  the new map; borrowed input has no transient extra reference; same-map rebind
  and in-place edit invalidate; owner and worker final frees invalidate/flush;
  worker replacement of a live native map avoids global flush; unknown image
  and missing metadata fall back; SDK owner/worker control blocks retain their
  existing invalidation despite bypassingDAC30. Actual manual-named setter gives
  a fresh map while an old params' definitions remain valid until final release.
- Native null/empty mapping inputs are outside the internal API's nonempty-map
  requirement. Controlled SEH tests compare direct native calls with the wrapper:
  both retain identical access-violation/partial-assignment behavior and exactly
  one forwarded call. A separate C++ thrown-value control also stays unchanged.
  Actual float/double/int/uint buffer growth is verified after native assignment.
- **Six independently compiled /O2 mutants are expected RED**: unconditional
  OLD-map flush, omitted same-map invalidation, accepting unknown control blocks,
  omitted final-delete invalidation, omitted off-owner flush and duplicate setter
  forwarding. Every mutant compiles; compile failures are not accepted as RED.
  The off-owner no-op mutation includes a real semicolon, preserving valid C++.

The fixture and pinned-image assertions verify semantics/lifetime and the removed
bounded scan. They do not establish how often the affected replacements occur in
the owner's game or their share of NPC material construction time.
