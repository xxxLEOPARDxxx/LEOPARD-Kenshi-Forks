#pragma once
#include <stddef.h>
#include <stdint.h>

// Only GOG 1.0.65, pinned to the user-supplied RE_Kenshi image. No hook,
// native dereference, process callback, file I/O, heap or worker lives here.
// Callers must first select/verify this image; Steam behavior is separate.
namespace GogProfile {
struct AddressPair {uintptr_t from,to;};
typedef bool (*Reader)(void*user,uintptr_t actualRva,void*out,size_t count);
const char* ExecutableSha256();
bool MatchesImage(uint32_t timestamp,uint32_t imageSize,const char*sha256);
// Outputs become zero on refusal. Unknown GOG keys NEVER fall back to Steam.
bool Translate(uintptr_t steamRva,uintptr_t*gogRva);
bool Absolute(uintptr_t base,uintptr_t steamRva,uintptr_t*address);
bool CanonicalExact(uintptr_t gogRva,uintptr_t*steamRva);
// Expected native bytes are GOG-specific; count must fit the owned 32B window.
const unsigned char* ExpectedBytes(uintptr_t steamRva,size_t count);
bool VerifyWindow(Reader reader,void*user,uintptr_t steamRva,size_t count);
// Immutable inventory for startup validation and offline fixture checking.
size_t AddressCount();
const AddressPair* AddressAt(size_t index);
size_t WindowCount();
uintptr_t WindowKeyAt(size_t index);
}
