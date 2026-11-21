#include "fixes/world/edge_triangles.h"
#include <cstring>
#include <emmintrin.h>

namespace edge_triangles {

static bool Separate(uintptr_t left, size_t leftSize, uintptr_t right, size_t rightSize)
{
    return left <= right ? right - left >= leftSize : left - right >= rightSize;
}

bool TryDecode(const unsigned char* source, const unsigned char* end,
               unsigned char* triangles, unsigned char* normals, unsigned char* scratch,
               uint32_t count, uint32_t groups)
{
    if (count < 128 || count > 262144 || groups > 4096 || !triangles || !normals || !scratch)
        return false;
    const uintptr_t pos = reinterpret_cast<uintptr_t>(source);
    const uintptr_t limit = reinterpret_cast<uintptr_t>(end);
    const size_t wireBytes = 48 * static_cast<size_t>(count);
    if (pos < 18 || limit < pos || limit - pos < wireBytes)
        return false;
    const unsigned char* const chunk = source - 18;
    uint32_t chunkBytes, wireTriangles, wireGroups;
    std::memcpy(&chunkBytes, chunk + 2, 4);
    std::memcpy(&wireTriangles, chunk + 10, 4);
    std::memcpy(&wireGroups, chunk + 14, 4);
    if (chunk[0] != 0 || chunk[1] != 0xb1 || chunk[8] != 0 || chunk[9] > 1 ||
        wireTriangles != count || wireGroups != groups ||
        chunkBytes < 18 + wireBytes + 22 * groups || chunkBytes - 18 > limit - pos)
        return false;
    const uintptr_t tri = reinterpret_cast<uintptr_t>(triangles);
    const uintptr_t norm = reinterpret_cast<uintptr_t>(normals);
    const uintptr_t temp = reinterpret_cast<uintptr_t>(scratch);
    const size_t triBytes = 64 * static_cast<size_t>(count);
    const size_t normalBytes = 16 * static_cast<size_t>(count);
    const uintptr_t chunkStart = pos - 18;
    const uintptr_t addressLimit = static_cast<uintptr_t>(-1);
    if (tri > addressLimit - triBytes || norm > addressLimit - normalBytes || temp > addressLimit - 12 ||
        !Separate(tri, triBytes, norm, normalBytes) ||
        !Separate(tri, triBytes, chunkStart, chunkBytes) ||
        !Separate(norm, normalBytes, chunkStart, chunkBytes) ||
        !Separate(temp, 12, chunkStart, chunkBytes) ||
        !Separate(temp, 12, tri, triBytes) || !Separate(temp, 12, norm, normalBytes))
        return false;
    const __m128i zero = _mm_setzero_si128();
    for (uint32_t i = 0; i < count; ++i) {
        const __m128i first = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
        const __m128i second = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + 16));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(triangles), _mm_unpacklo_epi32(first, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(triangles + 16), _mm_unpackhi_epi32(first, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(triangles + 32), _mm_unpacklo_epi32(second, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(triangles + 48), _mm_unpackhi_epi32(second, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(normals), _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + 32)));
        source += 48;
        triangles += 64;
        normals += 16;
    }
    std::memcpy(scratch, source - 28, 12);
    return true;
}

} // namespace edge_triangles
