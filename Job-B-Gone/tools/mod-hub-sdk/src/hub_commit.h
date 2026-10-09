#ifndef EMC_HUB_COMMIT_H
#define EMC_HUB_COMMIT_H

#include "emc/mod_hub_api.h"

enum HubCommitSkipReason
{
    HUB_COMMIT_SKIP_REASON_NONE = 0,
    HUB_COMMIT_SKIP_REASON_KEYBIND_CAPTURE_ACTIVE = 1
};

struct HubCommitSummary
{
    uint32_t attempted;
    uint32_t succeeded;
    uint32_t failed;
    uint32_t skipped;
    int32_t skip_reason;
};

void HubCommit_RunOptionsSave();
void HubCommit_GetLastSummary(HubCommitSummary* out_summary);

#endif
