#pragma once
#include <cstring>
#include <cstddef>
namespace TauntExpansion {
// Include the entire RIP-relative instruction in the bounds check, not only
// its opcode prefix. Ambiguous patterns must leave original game code intact.
inline const unsigned char* FindTauntInstruction(const unsigned char* code, size_t size) {
    static const unsigned char signature[]={
        0x80,0xB8,0x2A,0x01,0,0,0,0x74,0x10,0xF3,0x0F,0x58,0x3D
    };
    const unsigned char* match=NULL;
    for (size_t i=0;i+17<=size;++i) {
        if (!memcmp(code+i,signature,sizeof(signature))) {
            if (match) return NULL;
            match=code+i+9;
        }
    }
    return match;
}
}
