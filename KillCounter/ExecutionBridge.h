#pragma once
#include <stdint.h>
// Optional C ABI. The five words are type, container, containerSerial, index,
// serial. No Kenshi hand vtable, STL object or pointer crosses this interface.
#pragma pack(push, 4)
struct KillCounterExecutionV1
{
    uint32_t size, version;
    uint32_t actor[5], victim[5];
};
#pragma pack(pop)
// 0 rejected/disabled/full; 1 accepted (possibly queued); 2 duplicate.
// Call only after a successful execution with a confirmed dead victim.
#ifdef KILLCOUNTER_BRIDGE_BUILD
#define KILLCOUNTER_BRIDGE_API __declspec(dllexport)
#else
#define KILLCOUNTER_BRIDGE_API
#endif
extern "C" KILLCOUNTER_BRIDGE_API uint32_t __cdecl KillCounter_RecordExecutionV1(const KillCounterExecutionV1* request);
