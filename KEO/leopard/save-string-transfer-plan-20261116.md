# CPU save string transfer

Continue CPU parsing, separately from file buffering. No gameplay runs, new
profiles, logs or process-memory reads. Work from pinned disk binaries only.

Native helper A47B0 (thunk544B7) accepts an owned by-value VC10 string,
copies it into a constructed spare lektor slot, then destroys/empties the
argument. On exact parser callers6C1EBC/6C1F47/6C2070/6CAC21, transfer
ownership using typed std::string::swap instead of the second deep copy.
Main thread only, live/defaultON saveStringTransfer control. Preserve native
growth, all other callers, native exceptions and input source strings.

Admission: exact image ID and complete helper bytes, thunk, four caller
sequences; sizeof string40/lektor24, sane count/capacity and non-aliasing
argument. Only long (>15-byte) arguments and empty, capacity15 destination
slots qualify. One typed swap, no plugin allocation/free. Other destinations
retain native reuse. No raw string-object memcpy, global assign hook, worker, cache or
retained game reference. Native fallback exactly once with untouched argument.

- [x] Fail-first native-CRT fixture proves redundant allocation/copy.
- [x] Implement isolated transfer and all admission/fallback tests.
- [x] Verify native bytes, no cross-module allocation/free and negative controls.
- [x] Independent review; DEV/PROD build and host/config/localization checks.
- [x] Install only if game absent, preserve INI and foreign Steel files.
- [x] Scoped commit/push. Release2 remains a local candidate; no publication.

Equipment assembly needs a separate proved opportunity; this parser helper
must not be described as an equipment improvement without evidence.
