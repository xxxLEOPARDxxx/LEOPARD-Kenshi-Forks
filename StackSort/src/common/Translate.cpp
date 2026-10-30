#define KLOC_DOMAIN "stack_sort"
#include <Localization.h>
#include "Translate.h"
const char* Tr(TrKey key)
{
    static const char* const english[TR_COUNT] = {
#define X(n, s) s,
        TR_KEYS(X)
#undef X
    };
    return ::Tr(key >= 0 && key < TR_COUNT ? english[key] : "");
}