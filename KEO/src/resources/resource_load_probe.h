#pragma once
// Optional, fail-closed loading diagnostics. Call initialization/report on main.
void ResourceLoadProbeInit();
void ResourceLoadProbeReport();
// Existing array bridge already owns its native hook. Time it without a second
// detour; -1 disables measurement, zero is a valid clock value. No RAII/native
// resource ownership is introduced by this token.
__int64 ResourceLoadProbeArrayBegin();
void ResourceLoadProbeArrayEnd(__int64 begin);
