// Bounded diagnostic admission only. Main-thread caller; no native queries or allocations.
#ifndef KEO_ZONE_LIFECYCLE_DIAG_H
#define KEO_ZONE_LIFECYCLE_DIAG_H
struct ZoneLifecycleDiagBudget
{
    struct Key { int kind, x, y; double at; bool used; } keys[32];
    unsigned epoch, emitted, next;
    double windowAt;
    bool initialized;
    ZoneLifecycleDiagBudget() : epoch(0), emitted(0), next(0), windowAt(0), initialized(false)
    { for (int i=0;i<32;++i) keys[i].used=false; }
    bool Admit(unsigned worldEpoch, int kind, int x, int y, double now)
    {
        if (!(now >= 0.0 && now < 1.0e100)) return false;
        if (!initialized || epoch != worldEpoch || now < windowAt)
        {
            initialized=true; epoch=worldEpoch; emitted=next=0; windowAt=now;
            for (int i=0;i<32;++i) keys[i].used=false;
        }
        if (now-windowAt >= 60.0) { windowAt=now; emitted=0; }
        if (emitted >= 64) return false;
        int found=-1;
        for (int i=0;i<32;++i)
            if (keys[i].used && keys[i].kind==kind && keys[i].x==x && keys[i].y==y)
            { if (now-keys[i].at < 5.0) return false; found=i; break; }
        if (found<0) { found=(int)next; next=(next+1)%32; }
        keys[found].used=true; keys[found].kind=kind; keys[found].x=x; keys[found].y=y; keys[found].at=now;
        ++emitted;
        return true;
    }
};
#endif
