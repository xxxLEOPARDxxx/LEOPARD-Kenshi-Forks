#ifndef KEO_NM_AUTO_REBUILD_POLICY_H
#define KEO_NM_AUTO_REBUILD_POLICY_H

enum { NM_AUTO_CELLS = 4096, NM_AUTO_SCAN_BUDGET = 64 };
struct NmAutoState
{
    unsigned char requested[NM_AUTO_CELLS];
    int cursor;
    double nextScan, nextRequest;
};
void NmAutoReset(NmAutoState* s);
bool NmAutoBeginScan(NmAutoState* s, double now, bool enabled, bool worldReady);
int NmAutoNextCell(NmAutoState* s);
bool NmAutoRequested(const NmAutoState* s, int cell);
void NmAutoAccepted(NmAutoState* s, int cell, double now);
bool NmAutoDemand(float camera, float player);

#endif
