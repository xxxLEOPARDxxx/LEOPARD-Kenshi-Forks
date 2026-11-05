#include "zone/readiness/zone_readiness_contract.h"
#include "zone/zone_ledger_core.h"

ZoneReadinessAnswer ZoneReadinessContractDecide(int cellClass, bool globalGameOwnedBypass)
{
 // A drained add list is not proof that outdoor AND building meshes are ready.
 // Keep the engine's answer for every caller and ownership class, including
 // unknown/invalid classes. The bucket inputs remain diagnostic only.
 (void)cellClass;
 (void)globalGameOwnedBypass;
 return ZONE_READY_ANSWER_ORIGINAL;
}

ZoneReadinessBucket ZoneReadinessContractBucket(int cellClass, bool globalGameOwnedBypass)
{
	if (globalGameOwnedBypass)
		return ZONE_READY_BUCKET_GLOBAL;

	switch (cellClass)
	{
	case ZONE_CLASS_PRIVATE: return ZONE_READY_BUCKET_PRIVATE;
	case ZONE_CLASS_ADOPTED: return ZONE_READY_BUCKET_ADOPTED;
	default:                 return ZONE_READY_BUCKET_NONE;
	}
}
