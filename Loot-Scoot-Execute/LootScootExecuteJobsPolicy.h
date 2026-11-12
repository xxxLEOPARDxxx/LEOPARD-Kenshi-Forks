#pragma once
#include <stdint.h>
#include <stddef.h>

namespace lse_jobs
{
enum
{
    OWN_DEFEATS = 294,
    ALL_ENEMIES = 295,
    ACTOR_CAP = 50,
    LEDGER_CAP = 1024
};
struct Identity
{
    uint32_t word[5];
};
inline bool Same(const Identity& identity, const Identity& otherIdentity)
{
    for (unsigned probeIndex = 0; probeIndex < 5; ++probeIndex)
    {
        if (identity.word[probeIndex] != otherIdentity.word[probeIndex])
        {
            return false;
        }
    }
    return true;
}
inline bool Custom(int type)
{
    return type == OWN_DEFEATS || type == ALL_ENEMIES;
}
inline bool TargetAllowed(int mode, bool executable, bool enemy, bool credited)
{
    return executable && (mode == OWN_DEFEATS ? credited : mode == ALL_ENEMIES && enemy);
}
// A fixed table with a fixed probe budget. Overflow loses permission; it can
// never transfer it to a different serial/container or an unrelated actor.
struct Defeat
{
    bool used, pending, confirmed;
    Identity victim, actor;
    uint32_t at;
    Defeat() : used(false), pending(false), confirmed(false), at(0)
    {
    }
};
class Ledger
{
    Defeat rows[LEDGER_CAP];
    unsigned usedCount;
    unsigned Hash(const Identity& identity) const
    {
        uint32_t hash = 2166136261u;
        for (unsigned probeIndex = 0; probeIndex < 5; ++probeIndex)
        {
            hash = (hash ^ identity.word[probeIndex]) * 16777619u;
        }
        return hash & (LEDGER_CAP - 1);
    }

  public:
    Ledger() : usedCount(0)
    {
    }
    unsigned Count() const
    {
        return usedCount;
    }
    void Clear()
    {
        for (unsigned probeIndex = 0; probeIndex < LEDGER_CAP; ++probeIndex)
        {
            rows[probeIndex] = Defeat();
        }
        usedCount = 0;
    }
    Defeat* Find(const Identity& identity)
    {
        const unsigned start = Hash(identity);
        for (unsigned probeIndex = 0; probeIndex < 16; ++probeIndex)
        {
            Defeat& record = rows[(start + probeIndex) & (LEDGER_CAP - 1)];
            if (record.used && Same(record.victim, identity))
            {
                return &record;
            }
        }
        return 0;
    }
    void Forget(const Identity& identity)
    {
        Defeat* record = Find(identity);
        if (record)
        {
            *record = Defeat();
            --usedCount;
        }
    }
    void Record(const Identity& victim, const Identity& actor, bool pending, uint32_t now)
    {
        Defeat* record = Find(victim);
        if (!record)
        {
            const unsigned start = Hash(victim);
            for (unsigned probeIndex = 0; probeIndex < 16 && !record; ++probeIndex)
            {
                if (!rows[(start + probeIndex) & (LEDGER_CAP - 1)].used)
                {
                    record = &rows[(start + probeIndex) & (LEDGER_CAP - 1)];
                }
            }
            if (!record)
            {
                record = &rows[start];
            }
        }
        if (!record->used)
        {
            ++usedCount;
        }
        *record = Defeat();
        record->used = true;
        record->victim = victim;
        record->actor = actor;
        record->pending = pending;
        record->confirmed = !pending;
        record->at = now;
    }
    void Medical(const Identity& victim, bool down, bool dead, uint32_t now)
    {
        Defeat* record = Find(victim);
        if (!record)
        {
            return;
        }
        if (dead || (!down && record->confirmed) || (record->pending && uint32_t(now - record->at) >= 2000u))
        {
            Forget(victim);
            return;
        }
        if (down && record->pending)
        {
            record->pending = false;
            record->confirmed = true;
        }
    }
    void Hit(const Identity& victim, const Identity& actor, bool wasDown, bool down, bool dead, uint32_t now)
    {
        if (dead || (wasDown && !down))
        {
            Forget(victim);
            return;
        }
        if (!wasDown && down)
        {
            Record(victim, actor, false, now);
        }
    }
    bool Owns(const Identity& victim, const Identity& actor)
    {
        const Defeat* record = Find(victim);
        return record && record->confirmed && Same(record->actor, actor);
    }
};
}
