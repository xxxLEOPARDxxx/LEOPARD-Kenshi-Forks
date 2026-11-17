#include "navmesh/cache/nm_auto_rebuild_policy.h"
#include "check.h"
#include <float.h>
#include <limits>

int main()
{
    NmAutoState s;
    NmAutoReset(&s);
    Check(!NmAutoBeginScan(&s, 0, false, true), "off never scans");
    Check(!NmAutoBeginScan(&s, 0, true, false), "loading never scans");
    Check(NmAutoBeginScan(&s, 0, true, true), "ready first scan begins immediately");
    Check(!NmAutoBeginScan(&s, 0.249, true, true), "cadence caps scans");
    Check(NmAutoBeginScan(&s, 0.25, true, true), "quarter-second scan");
    for (int cell = 0; cell < NM_AUTO_CELLS; ++cell)
    {
        Check(NmAutoNextCell(&s) == cell, "every cell visited once per sweep");
        Check(!NmAutoRequested(&s, cell), "not prematurely admitted");
    }
    Check(NmAutoNextCell(&s) == 0, "cursor wraps");
    NmAutoAccepted(&s, 17, 1.0);
    Check(NmAutoRequested(&s, 17) && !NmAutoRequested(&s, 18), "admission is cell-specific");
    Check(!NmAutoBeginScan(&s, 5.999, true, true), "accepted request spaces work five seconds");
    Check(!NmAutoBeginScan(&s, 6, false, true), "live disable blocks work");
    Check(NmAutoRequested(&s, 17), "disable preserves once-per-world history");
    Check(NmAutoBeginScan(&s, 6, true, true), "live enable resumes");
    Check(NmAutoRequested(&s, -1) && NmAutoRequested(&s, NM_AUTO_CELLS), "off-grid rejected");
    NmAutoAccepted(&s, -1, 999);
    Check(NmAutoBeginScan(&s, 6.25, true, true), "invalid admission cannot delay all cells");
    NmAutoReset(&s);
    Check(!NmAutoRequested(&s, 17) && NmAutoNextCell(&s) == 0, "new world clears history and cursor");
    Check(NmAutoBeginScan(&s, 0, true, true), "new world clears clocks");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Check(NmAutoDemand(1, 0) && NmAutoDemand(0, 1), "camera or player demand admitted");
    Check(!NmAutoDemand(0, -1) && !NmAutoDemand(-1, 0), "expired and negative leases excluded");
    Check(!NmAutoDemand(nan, inf) && !NmAutoDemand(inf, nan), "nonfinite leases excluded");
    Check(NmAutoDemand(nan, 1) && NmAutoDemand(1, inf), "one valid live lease suffices");
    Check(NM_AUTO_SCAN_BUDGET == 64, "fixed bounded scan budget");
    return CheckExit("nm_auto_rebuild_units");
}
