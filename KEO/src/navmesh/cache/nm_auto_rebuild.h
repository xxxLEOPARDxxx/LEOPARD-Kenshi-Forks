#ifndef KEO_NM_AUTO_REBUILD_H
#define KEO_NM_AUTO_REBUILD_H
// Main-thread safe point only. No pointers retained between calls.
void NmAutoRebuildTick(void* zoneMgr, double now, bool saveLoading);
void NmAutoRebuildOnWorldReset();
#endif
