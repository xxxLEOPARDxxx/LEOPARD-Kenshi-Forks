#pragma once
#include <Windows.h>
namespace TauntExpansion {
// A single private code-cave constant is shared by all target-selection calls.
// Every call must hold the same recursive lock, including vanilla/disabled calls.
// Save/restore composes with recursion; finally preserves it on SEH/C++ unwinding.
// The transaction and cave remain alive for the DLL/process lifetime.
struct BonusTransaction {
    CRITICAL_SECTION lock;
    float* address;
    typedef bool (*Callback)(void*);
    void Initialise(float* value) { InitializeCriticalSection(&lock); address=value; }
    void Dispose() { DeleteCriticalSection(&lock); address=NULL; }
    bool Invoke(float bonus, Callback callback, void* context) {
        EnterCriticalSection(&lock);
        bool result=false;
        __try { result=InvokeLocked(bonus,callback,context); }
        __finally { LeaveCriticalSection(&lock); }
        return result;
    }
    // Caller already holds lock when atomically reading the config/bonus.
    bool InvokeLocked(float bonus, Callback callback, void* context) {
        const float previous=address ? *address : 0.0f;
        if (address) *address=bonus;
        bool result=false;
        __try { result=callback(context); }
        __finally {
            if (address) *address=previous;
        }
        return result;
    }
};
}
