#pragma once

#include <stdint.h>

namespace edge_triangles {
// The caller proves that source has room for its preceding B100 header and
// that the three output pointers refer to the already-sized native storage.
bool TryDecode(const unsigned char* source, const unsigned char* end,
               unsigned char* triangles, unsigned char* normals, unsigned char* scratch,
               uint32_t count, uint32_t groups);
}

void InstallEdgeTriangles(int*, int*);
