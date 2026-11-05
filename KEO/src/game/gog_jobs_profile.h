#pragma once
#include <stdint.h>
#include <stddef.h>
namespace JobsProfile {
enum Key { FACTORY,REGISTER_DATA,GET_DATA,CAPTION,CONTEXT_SHOW,ADD_REQUIREMENT,SET_PERMAJOB,TASK_KEY,SET_LOCATION,DEFAULT_SCORE,FACTORY_DISPATCH,EMPTY_VTABLE,MOVE_VTABLE,BLANK_VTABLE,KEY_COUNT };
typedef bool (*Reader)(void*,uintptr_t,void*,size_t);
// Caller has already verified the selected executable with NativeProfile.
// Startup-only; all physical code bytes and complete class-specific vtable entries are checked.
uintptr_t Rva(bool gog,Key key);
// Includes the RTTI locator: Empty=7, Move=8, Blank=6.
unsigned TableCount(Key key);
bool Validate(bool gog,uintptr_t base,Reader reader,void*ctx);
// Caption/context may already hold our admitted detours at install time.
bool ValidateForInstall(bool gog,uintptr_t base,Reader reader,void*ctx);
}
