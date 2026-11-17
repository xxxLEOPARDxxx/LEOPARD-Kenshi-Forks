#include "navmesh/cache/nm_auto_rebuild_policy.h"
#include <string.h>
#include <float.h>
void NmAutoReset(NmAutoState* s) { memset(s, 0, sizeof(*s)); }
bool NmAutoBeginScan(NmAutoState* s, double now, bool enabled, bool worldReady)
{
    if (!enabled || !worldReady || !_finite(now) || now < s->nextScan || now < s->nextRequest)
        return false;
    s->nextScan = now + 0.25;
    return true;
}
int NmAutoNextCell(NmAutoState* s)
{
    const int cell = s->cursor;
    s->cursor = (cell + 1) % NM_AUTO_CELLS;
    return cell;
}
bool NmAutoRequested(const NmAutoState* s, int cell)
{
    return cell < 0 || cell >= NM_AUTO_CELLS || s->requested[cell] != 0;
}
void NmAutoAccepted(NmAutoState* s, int cell, double now)
{
    if (cell < 0 || cell >= NM_AUTO_CELLS || !_finite(now)) return;
    s->requested[cell] = 1;
    s->nextRequest = now + 5.0;
}
bool NmAutoDemand(float camera, float player)
{
    return (_finite(camera) && camera > 0) || (_finite(player) && player > 0);
}
