#include "fixes/world/animation_dirty_reuse.h"
#include <stdint.h>
namespace animation_dirty_reuse {
static_assert(sizeof(unsigned)==4&&sizeof(void*)==8,"native mutex/dirty DWORD ABI");
bool TryOwned(void*parent,unsigned currentThread){
 if(!parent||((uintptr_t)parent&3)||!currentThread)return false;
 volatile unsigned*fields=(volatile unsigned*)parent;
 if(fields[1]!=currentThread||fields[0]==0)return false;
 // Native recursive acquire/notify/release has the same final state when this
 // thread already owns the mutex with nonzero recursion. Unsigned wrap is native.
 fields[6]=fields[6]+1;
 return true;
}
}
