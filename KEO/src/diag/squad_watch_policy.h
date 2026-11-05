#pragma once
#include <string.h>
// Main-thread-only diagnostic state. Never retains a game pointer or changes AI.
struct SquadWatchKey { unsigned v[5]; };
enum SquadWatchEvent { SQUAD_WATCH_SEEN, SQUAD_WATCH_FIRST, SQUAD_WATCH_ACTIVATED,
    SQUAD_WATCH_RETURNED, SQUAD_WATCH_FULL };
template<unsigned N> struct SquadWatchLedger {
    struct Slot { SquadWatchKey key; unsigned pass, flags; bool used; };
    Slot slots[N];
    unsigned epoch, serial, unique, active, unloaded;
    double nextAt;
    bool initialized, running, baseline;
    SquadWatchLedger() : epoch(0), serial(0), nextAt(0), initialized(false), running(false), baseline(true) {}
    bool Begin(bool enabled, unsigned world, double now) {
        if (!enabled) { initialized=false; running=false; return false; }
        if (!initialized || epoch!=world || now<nextAt-10.0) {
            memset(slots,0,sizeof(slots)); epoch=world; serial=0; nextAt=now;
            initialized=true; running=false;
        }
        if (running || now<nextAt) return false;
        if (++serial==0) { memset(slots,0,sizeof(slots)); serial=1; }
        baseline=serial==1; running=true; unique=active=unloaded=0; return true;
    }
    SquadWatchEvent Observe(const SquadWatchKey& k, unsigned flags) {
        unsigned h=2166136261u;
        for(unsigned i=0;i<5;++i) h=(h^k.v[i])*16777619u;
        // Collision saturation is explicit; never turn diagnostics into a long
        // hash-table walk on the game's frame thread.
        for(unsigned i=0;i<N && i<64;++i) {
            Slot& s=slots[(h+i)%N];
            if (!s.used || memcmp(&s.key,&k,sizeof(k))==0) {
                SquadWatchEvent e=SQUAD_WATCH_SEEN;
                if(!s.used) { s.used=true; s.key=k; s.pass=0; s.flags=0; e=SQUAD_WATCH_FIRST; }
                else if(s.pass!=serial && (flags&1) && !(s.flags&1)) e=SQUAD_WATCH_ACTIVATED;
                else if(s.pass+1<serial && (flags&1)) e=SQUAD_WATCH_RETURNED;
                if(s.pass!=serial) { s.pass=serial; s.flags=0; ++unique; }
                if((flags&1) && !(s.flags&1)) ++active;
                if((flags&2) && !(s.flags&2)) ++unloaded;
                s.flags|=flags; return e;
            }
        }
        return SQUAD_WATCH_FULL;
    }
    void End(double now) { running=false; nextAt=now+10.0; }
};
