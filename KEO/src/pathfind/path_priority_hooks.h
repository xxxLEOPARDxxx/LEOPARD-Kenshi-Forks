#pragma once
// Startup-only preparation/finalization around the ordinary install sequence.
// The capability stays false until the internal profile and all three original
// trampolines are ready. No hot uninstall or late-write fallback.
void PathPriorityPrepare(bool wanted);
void PathPriorityFinalize(bool requestHook,bool submitHook,bool enqueueHook,bool originals);
// Any original caller thread, before native enqueue publishes the pointer.
void PathPriorityBeforeEnqueue(void* queue,void** item,const void* nativeReturn);
