#include "diag/squad_watch_policy.h"
#include <assert.h>
#include <stdio.h>
static SquadWatchKey Key(unsigned n) { SquadWatchKey k={{1,2,3,n,7}}; return k; }
int main() {
    static SquadWatchLedger<8> l;
    assert(!l.Begin(false,1,0));
    assert(l.Begin(true,1,0)); assert(l.baseline);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_FIRST);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_SEEN);
    l.End(0); assert(!l.Begin(true,1,9));
    assert(l.Begin(true,1,10)); assert(!l.baseline);
    assert(l.Observe(Key(1),2)==SQUAD_WATCH_SEEN);
    l.End(10); assert(l.Begin(true,1,20));
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_ACTIVATED);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_SEEN);
    for(unsigned i=2;i<=8;++i) assert(l.Observe(Key(i),1)==SQUAD_WATCH_FIRST);
    assert(l.Observe(Key(9),1)==SQUAD_WATCH_FULL);
    l.End(20); assert(l.Begin(true,2,21)); assert(l.baseline);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_FIRST);
    l.End(21); assert(!l.Begin(false,2,30));
    assert(l.Begin(true,2,31)); assert(l.baseline);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_FIRST);
    // No pointers, no eviction of still-useful identities, bounded storage.
    SquadWatchKey variant=Key(1); variant.v[4]++;
    assert(l.Observe(variant,1)==SQUAD_WATCH_FIRST);
    l.End(31); assert(l.Begin(true,2,41));
    assert(l.Observe(Key(1),2)==SQUAD_WATCH_SEEN);
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_SEEN); // both lists in same census
    l.End(41); assert(l.Begin(true,2,51));
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_SEEN);
    // Absence is not destruction; re-observation of the same stable identity
    // must not become a fresh spawn, even after a missed rolling census.
    l.End(51); assert(l.Begin(true,2,61)); l.End(61);
    assert(l.Begin(true,2,71));
    assert(l.Observe(Key(1),1)==SQUAD_WATCH_RETURNED);
    assert(l.Observe(Key(1),2)==SQUAD_WATCH_SEEN);
    assert(l.unique==1 && l.active==1 && l.unloaded==1);
    // A large burst remains bounded, and the same record is never counted twice.
    static SquadWatchLedger<8192> big;
    assert(big.Begin(true,4,0));
    unsigned first=0, saturated=0;
    for(unsigned i=0;i<20000;++i) {
        SquadWatchEvent e=big.Observe(Key(i),1);
        first+=e==SQUAD_WATCH_FIRST; saturated+=e==SQUAD_WATCH_FULL;
    }
    assert(first<=8192 && saturated>0 && big.active==first);
    assert(sizeof(big)<512*1024);
    puts("squad watch policy: PASS");
}
