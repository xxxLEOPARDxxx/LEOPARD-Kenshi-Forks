#pragma once
#include <stddef.h>
#include <stdint.h>
// Current synchronous native submit only; no pointer survives the wrapper.
struct PathPriorityContext
{
    void* request;
    void* queue;
    const void* enqueueReturn;
    int tier, nativePriority;
    bool repath, admitted, consumed;
};
struct PathPriorityFrame { PathPriorityContext current; PathPriorityContext* previous; };
struct PathPriorityDecision { bool repath; };
void PathPriorityEnter(PathPriorityFrame* frame, PathPriorityContext*& slot, void* request,
    void* inputQueue, const void* enqueueReturn, int tier, bool repath, bool highPriority, bool admitted);
void PathPriorityLeave(const PathPriorityFrame* frame, PathPriorityContext*& slot);
// Metadata admission precedes all request reads. Call immediately before native
// enqueue; a true result has already written the scalar tier exactly once.
bool PathPriorityApply(PathPriorityContext* context, void* queue, void** item,
    const void* enqueueReturn, PathPriorityDecision* decision);
bool PathPriorityCapability(bool wanted, bool profile, bool requestHook, bool submitHook,
    bool enqueueHook, bool originals);
// Fixed Steam1.0.65 byte contract, verified BEFORE any entry is patched.
typedef bool (*PathPriorityReadBytes)(uintptr_t rva, unsigned char* out, size_t count, void* user);
typedef const unsigned char* (*PathPriorityExpectedBytes)(uintptr_t rva,size_t count);
bool PathPriorityProfile(PathPriorityReadBytes reader, void* user, PathPriorityExpectedBytes expected=NULL);
const uintptr_t PATH_PRIORITY_ENQUEUE_RETURN = 0x3AAF60;
