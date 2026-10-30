#pragma once
#include <vector>
// Adapter owns all native access. Tokens are identities, never consumed item
// pointers. Native reinsert may consume the source: the driver never reads it.
namespace StackMerge
{
struct Outcome { bool changed, fault; Outcome() : changed(false), fault(false) {} };
template<class Adapter> Outcome Consolidate(Adapter& native)
{
    Outcome out;
    std::vector<typename Adapter::Token> tokens;
    native.Snapshot(tokens);
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (!native.HasMergeTarget(tokens[i])) continue;
        const int result = native.Reinsert(tokens[i]);
        if (result != 0) out.changed = true;
        if (result < 0) { out.fault = true; break; }
    }
    return out;
}
}
