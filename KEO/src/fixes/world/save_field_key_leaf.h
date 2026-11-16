#pragma once
#include <stddef.h>
namespace save_field_key_leaf {
// Authenticated first-key CALL sites only: RSI is the native const permission
// filter; RCX/RDX/R8 are fresh string/source/count. Leaf changes no stack/nonvol.
unsigned Build(unsigned char*out,size_t capacity,const volatile bool*enabled,void*original,bool provenFrame=false);
}
