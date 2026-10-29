#pragma once
#define TR_KEYS(X) \
    X(ERR_EQUIPPED, "Cannot rotate: item is equipped") \
    X(ERR_SQUARE, "Cannot rotate: item is square") \
    X(ERR_HOOKS_FAILED, "Cannot rotate: save/load hooks failed") \
    X(ERR_NO_SPACE, "Cannot rotate: not enough space") \
    X(ERR_LOCKED, "Cannot rotate: this inventory doesn't accept items") \
    X(WARN_HOOKS_FAILED, "KenshiRotate: some hooks failed to install (see log); features may not work correctly")
enum TrKey {
#define X(n, s) TR_##n,
    TR_KEYS(X)
#undef X
    TR_COUNT
};
const char* Tr(TrKey key);
void DetectLanguage();
