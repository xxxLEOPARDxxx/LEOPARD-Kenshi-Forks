#pragma once
#include <stdint.h>
namespace keo_jobs {
struct StockTransferSnapshot {uintptr_t section,asset;uint64_t epoch;int x,y,quantity;double sourceMeasure,destMeasure,originalMeasure,unit;bool charged;};
// Native merges preserve charges, so movedUnits may be fractional. A fault stops
// the stock job; quarantineItem is DIAGNOSTIC ONLY, retained by this module.
struct StockTransferResult {double movedUnits;bool fault;void*quarantineItem;uint64_t epoch;};
struct StockTransferOps {
 void*ctx;bool(*snapshot)(void*,void*,void*,void*,StockTransferSnapshot*);
 bool(*valid)(void*,const StockTransferSnapshot&);void*(*detach)(void*,void*,void*,int);
 int(*offer)(void*,void*,void*); // 1 complete,0 native partial/refusal,-1 fault
 bool(*restore)(void*,void*,void*,const StockTransferSnapshot&);
 bool(*measure)(void*,void*,void*,const StockTransferSnapshot&,double*,double*);
};
StockTransferResult TransferStockItemWithOps(const StockTransferOps&,void*,void*,void*);
bool PrepareStockTransfer(); // startup, before enabling any stock callback
bool CanTransferStockItem(void*sourceInventory,void*destInventory,void*item);
StockTransferResult TransferStockItem(void*sourceInventory,void*destInventory,void*item);
// World boundary only. Old-world native objects are never dereferenced/deleted.
void ResetStockTransfer();
}
