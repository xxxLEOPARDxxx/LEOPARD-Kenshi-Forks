#pragma once
#include <Windows.h>
#include <stdint.h>
#include <cstring>
#include <cstdlib>
#include <float.h>
#include <cctype>
namespace TauntExpansion {
struct Config {
    bool enabled, debug, logCandidates, logOnlyWhenTauntPresent, logOnlyChangedSelection;
    unsigned minimumLogIntervalMs, maximumCandidatesPerEntry;
    float tauntMultiplier, retargetMultiplier;
    Config() : enabled(true), debug(false), logCandidates(false),
        logOnlyWhenTauntPresent(true), logOnlyChangedSelection(true),
        minimumLogIntervalMs(500), maximumCandidatesPerEntry(32),
        tauntMultiplier(5.0f), retargetMultiplier(2.0f) {}
};
inline float EffectiveBonus(float vanilla, const Config& c, bool oldTarget) {
    return c.enabled ? vanilla*c.tauntMultiplier*(oldTarget ? c.retargetMultiplier : 1.0f) : vanilla;
}
inline float ParseNumber(const char* text, float def, float lo, float hi) {
    char* end = NULL;
    double n = strtod(text, &end);
    if (end == text || !_finite(n)) return def;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end) return def;
    return n < lo ? lo : n > hi ? hi : (float)n;
}
// Diagnostic identity values are never dereferenced. Direct mapping keeps both
// memory and lookup time bounded across character destruction and save/load.
struct ObservationCache {
    struct Entry { uintptr_t actor, selected; ULONGLONG time; bool occupied; } entries[256];
    ObservationCache() { Clear(); }
    void Clear() { memset(entries, 0, sizeof(entries)); }
    size_t Capacity() const { return 256; }
    bool ShouldLog(uintptr_t actor, uintptr_t selected, ULONGLONG now, unsigned interval, bool changedOnly) {
        Entry& e = entries[((actor >> 4) ^ actor) & 255];
        if (e.occupied && e.actor == actor) {
            const ULONGLONG age = now-e.time;
            if (age < interval) return false;
            // Old identities expire even if an allocator reuses the same address.
            if (changedOnly && age < 30000 && e.selected == selected) return false;
        }
        e.actor=actor; e.selected=selected; e.time=now; e.occupied=true;
        return true;
    }
};
}
