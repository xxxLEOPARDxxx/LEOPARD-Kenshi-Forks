#pragma once
namespace keo_jobs {
// Synchronous only. Caller supplies live native objects on their owning AI/UI
// phase; no Character, Tasker, Item or inventory pointer survives these calls.
bool SteelStockEligible(void* character,void* task);
void SteelStockRun(void* task,void* charBody);
bool SteelGatherTarget(void* building);
bool SteelOwnedCounter(void* building);
}
