# Saved-object state-list CPU transfer

Goal: remove the final per-string copy loop and destination array growth when
the native save parser transfers its temporary state list into a fresh object.
No new gameplay runs, profiles, logs or process reads. Equipment is separate.

Architecture: retain the six-byte FF15 Quaternion assignment call6CABC7 and
atomically retarget only its RIP displacement through a read-only nearby pointer
slot. A fixed POD/kernel-handle thread exclusion covers code publication and
cache flush; an atomic data store alone is not an instruction synchronization
claim. Original return6CABCD, native assignment once and exceptions are preserved.
After successful assignment, admit exact outer parser return6C21C7 from the
native frame. Source arg7 is at returnSlot+A8, outer return at+70, target list
is quaternion destination+44. Transfer bounded nonoverlapping native arrays to
a pristine destination; retain no more capacity than native10/doubling growth.
Preserve allocator/vtable prefixes and cookies. Source.count0 then reaches the
native epilog without callbacks/allocations/throwing operations. No TLS or
provisional ownership/rollback is needed in the loading path. Installation has
SEH-safe rollback/flush/protection cleanup before resuming threads; any possibly
published pointer slot and unresolved suspension handles retain persistent owners.

This supersedes rejected early publication at map insertion: an allocation
callback could reenter/throw between that point and state transfer. The final
metadata assignment boundary eliminates that interval.

- [x] Fail-first x64 assembly fixture with seven native arguments, real caller
      returns/frame layout, original field assignment and copied-state loop.
- [x] Implement pure bounded triple transfer plus exact callsite/normal C++ wrapper.
- [x] Test actual emitted indirect callsite, normal/cross-frame exception unwind,
      unchanged fields/returns, source/dest ownership, fallback, allocation fault,
      overlap, cookie/capacity and startup install failures.
- [x] Verify pinned full native function/caller/destructor/array-growth/import
      proofs; independent review. Keep original and any published slot on failures.
- [x] Existing saveStringTransfer live switch, three updated tooltips, README;
      full host/build/import checks, closed-game installation preserving settings.
- [x] Scoped commit/push, local release2 only; no publication or FPS claim.
