#pragma once
// Supported builds (ZONEHAND_STEP >= 2) preserve native readiness for every
// caller and cell, including NONE/game-owned and invalid classes. A zero
// pending-section count does not prove outdoor and building navmesh readiness.
// The historical early-ready override is retired; workers, caching, preload
// and queue prioritization are independent. Steps below 2 keep their legacy
// hook behavior and are outside this correction.
// Pure policy: no game calls, locks, allocation or side effects.

enum ZoneReadinessAnswer
{
 ZONE_READY_ANSWER_ORIGINAL = 0,
 ZONE_READY_ANSWER_TODAY_BYPASS = 1 // Retired. Numeric value retained for compatibility; never returned.
};

// Inputs are retained for source compatibility and classification diagnostics.
// Every value of cellClass and globalGameOwnedBypass returns ORIGINAL.
ZoneReadinessAnswer ZoneReadinessContractDecide(int cellClass, bool globalGameOwnedBypass);

// Diagnostic classification only; does not grant readiness. GLOBAL reports
// live ZM+8 (justLoadedAGame) and masks the per-cell bucket as before.
enum ZoneReadinessBucket
{
 ZONE_READY_BUCKET_NONE = 0,
 ZONE_READY_BUCKET_PRIVATE,
 ZONE_READY_BUCKET_ADOPTED,
 ZONE_READY_BUCKET_GLOBAL
};
ZoneReadinessBucket ZoneReadinessContractBucket(int cellClass, bool globalGameOwnedBypass);
