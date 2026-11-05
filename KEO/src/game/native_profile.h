#pragma once
#include <stdint.h>
#include <stddef.h>
// Startup-only main-thread writer; immutable after successful publication.
// Steam remains the original path. GOG requires both pinned EXE and SDK files.
bool NativeProfilePrepareGog(uintptr_t base, void (*log)(const char*));
bool NativeProfileIsGog();
uintptr_t NativeProfileAddress(uintptr_t base, uintptr_t steamRva);
uintptr_t NativeProfileCanonical(uintptr_t actualRva);
const unsigned char* NativeProfileBytes(uintptr_t steamRva,const unsigned char* steamBytes,size_t count);

bool NativeProfileMatches(uintptr_t key,const void*actual,const unsigned char*steam,size_t count);
