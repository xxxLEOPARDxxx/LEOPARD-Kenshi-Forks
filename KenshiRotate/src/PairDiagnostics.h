#pragma once
#include "PairTracePolicy.h"
class InventoryItemBase;
extern PairTrace::Context s_pairPlaceContext;
void PairTraceRememberUIThread();
bool PairTraceOnUIThread();
void RecordStackPair(InventoryItemBase*,InventoryItemBase*,bool nativeResult,bool finalResult,void* caller);
