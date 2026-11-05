#pragma once
// Per-cell ledger input for readiness diagnostics (ZONEHAND_STEP >= 2).
// It no longer grants a readiness bypass: every class preserves the native
// answer. Below step 2 the bridge returns NONE and legacy hook rules remain.
// Live ZM+8 is sampled separately for the diagnostic GLOBAL bucket, avoiding
// a frame-old ledger mirror during a synchronous load. No native query here.

#include "base/config.h"

// Bounds-checks (gx, gy) itself before touching the ledger: a coordinate
// outside 0..63 answers unknown (ZONE_CLASS_NONE), the same as a cell the
// ledger has never seen, rather than reading through ZoneCellIndex's clamp
// into some other cell's class.
int ZoneReadinessBridgeClassOf(int gx, int gy);
