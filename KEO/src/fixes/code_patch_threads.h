#pragma once
#include <windows.h>
namespace code_patch_threads {
// POD for __try/__finally. Begin never allocates through the process heap;
// caller MUST End on success and failure before logging/allocating anything.
struct Set{HANDLE handles[512];DWORD ids[512];unsigned count;void*nativeResume;void*nativeTerminate;};
// Prepare an append-only emergency handle before exclusion. It is used only
// if Windows refuses every resume attempt; normal logging may hold heap locks.
bool Prepare(const char*logPath);
bool Begin(Set*set);
bool End(Set*set);
// Completes release. An unrecoverable OS resume failure is logged without CRT
// locks and terminates the current process instead of returning frozen threads.
bool Finish(Set*set);
}
