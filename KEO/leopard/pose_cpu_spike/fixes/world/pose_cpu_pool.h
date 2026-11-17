#pragma once
#include "fixes/world/pose_cpu_kernel.h"
namespace pose_cpu_pool {
// Synchronous, CPU-only; clears every borrowed job before returning.
bool Execute(pose_cpu_kernel::Work*work);
bool Prepare();
void SignalStop();
void Stop();
}
