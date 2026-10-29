#define KLOC_DOMAIN "kenshi_rotate"
#include <Localization.h>
#include "Translate.h"

namespace {
    const char* const kEnglish[TR_COUNT] = {
    #define X(n, s) s,
        TR_KEYS(X)
    #undef X
    };
}
const char* Tr(TrKey key) {
    return key >= 0 && key < TR_COUNT ? Tr(kEnglish[key]) : "";
}
void DetectLanguage() { KLoc::EnsureLoaded(); }
