# Phase 2: fixed diagnostic termination

Private source review and standalone host verification, 2026-11-15. No game run,
profile, diagnostic DLL, installation, settings change or performance claim.

## Confirmed defect and change

`src/base/fixed_log_buf.h` reserved one byte while appending, but `FlbDone`
applied the same append rule to the terminator. At `n == cap - 1`, it returned
an unterminated buffer. The deferred and main logging paths consume C strings;
a sufficiently long diagnostic could read beyond its local storage.

`FlbDone` now writes the reserved final byte when the content is saturated.
All append operations, existing short-line bytes and count behavior remain
unchanged. A full buffer keeps `n == cap - 1`; a shorter termination still uses
`FlbChar`, including its existing count increment. No heap allocation, native
call, synchronization, queue or new lifetime is introduced.

For external storage, a null buffer is supported only with zero capacity.
Zero capacity returns the supplied pointer untouched and does not promise a C
string. Capacity one yields an empty terminated string without advancing `n`.

## Verification

The production header is included directly by
`tools/tests/fixed_log_buf_units.cpp`. Its prior edge test explicitly required
the unsafe missing terminator. That expectation was replaced with independent
poisoned-storage and bounded-length checks before production was changed:
the old header compiled and produced **26 runtime assertion failures**.

Final VC10 x64 `/Od` and `/O2`, `/EHsc /W4`: **all checks passed**. Existing
numeric/format forms remain covered at fixed capacities 320/352/384/512/1024
and external capacities 16/64; fixed capacity two was added. New checks cover
external capacity zero, null zero-capacity storage, fixed/external capacity
one, exact `cap - 1` content, input of exactly `cap`, oversized input, short
termination/count preservation, caller-pointer identity and adjacent canaries.
Bounded scans verify the terminator lies inside each nonempty buffer.

The completed suite compiled with a separate build-only mutant header restoring
the old `FlbDone`: `/O2` produced **169 runtime failures** and exit 1. This is a
compiled negative control, not a compiler failure. The first fail-first result
has fewer failures because the legacy numeric-reference correction and exact
capacity cases were added afterward.

Frozen SHA256:

- Production `src/base/fixed_log_buf.h`:
  `55d6dae071482d3a76fccc7655c7ddc70c711f3025df86329ee3d9210250f9f9`
- Test `tools/tests/fixed_log_buf_units.cpp`:
  `013a57a3357ad3e7c80df92dbebdf59db66878e20b436af2e805d78268f37e9e`

Only the production header, its existing fixture and this private report were
changed for the fix. Independent aggregate review/build belongs to the parent.
