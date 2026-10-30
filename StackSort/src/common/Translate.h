#pragma once

#include <istream>
#include <string>

// X() args must be plain identifiers (MSVC 2010 preprocessor quirk with `##`).
#define TR_KEYS(X)                                                                                                     \
    X(SORT_BUTTON, "Sort [{n}]")                                                                                       \
    X(SORT_BUTTON_COMPACT, "Sort. [{n}]")                                                                              \
    X(SORT_TOOLTIP, "Sort items. Left-click cycles arrangements; right-click restores item positions.")                \
    X(DIM_H, "H")                                                                                                      \
    X(DIM_W, "W")                                                                                                      \
    X(BACK_ARROW, "<")                                                                                                 \
    X(FORWARD_ARROW, ">")                                                                                              \
    X(WARN_HOOKS_FAILED,                                                                                               \
      "StackSort: some hooks failed to install (see RE_Kenshi log); sorting may not work correctly")                   \
    X(WARN_CONFIG, "StackSort: {n} problem(s) in StackSort.cfg were ignored (see RE_Kenshi log)")

enum TrKey
{
#define X(n, s) TR_##n,
    TR_KEYS(X)
#undef X
    TR_COUNT
};

// Shared Localization.h uses the game locale and English fallback.
const char* Tr(TrKey key);
