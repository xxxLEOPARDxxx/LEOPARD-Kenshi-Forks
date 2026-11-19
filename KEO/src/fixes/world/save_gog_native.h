#pragma once
#include <stdint.h>
#include <stddef.h>

namespace save_gog_native {

static const uintptr_t sectionManagerRva = 0x21314d0;

// The shared KEO startup gate has already verified the complete GOG EXE
// hash. Each hook then checks its own live, relocation-free code windows.
inline bool IsGogImage(unsigned stamp, unsigned size)
{
    return stamp == 0x65d60519 && size == 0x232a000;
}

struct Window {
    uintptr_t rva;
    unsigned size;
    uint64_t fingerprint;
};

template<size_t Count>
bool Match(const void* module, const Window (&windows)[Count])
{
    const unsigned char* base = static_cast<const unsigned char*>(module);
    for(size_t i = 0; i < Count; ++i) {
        const Window& window = windows[i];
        uint64_t hash = 14695981039346656037ULL;
        for(unsigned offset = 0; offset < window.size; ++offset)
            hash = (hash ^ base[window.rva + offset]) * 1099511628211ULL;
        if(hash != window.fingerprint)
            return false;
    }
    return true;
}

} // namespace save_gog_native
