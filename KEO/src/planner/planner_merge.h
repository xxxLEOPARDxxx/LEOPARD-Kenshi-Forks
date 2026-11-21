// planner_merge.h - What the formation reads of a merged run-together order: its gather point and the
// members the merge left alone, and the re-plan once the group has gathered. Main thread only; every
// entry returns at once unless the planner is on.
#pragma once
#include <stdint.h>

namespace planner {

// Reads the accepted merge named by this formation's order, with the leader first.
bool PlannerMergeGather(int order, const uintptr_t* chars, int n, float gather[3],
                        unsigned char* alone, float* gatherDist);
// From the formation's gather completion, before the travel send: each of chars[0..n) is re-planned
// from where it stands, the hold cleared: from its plan while that holds the gather's hold, else from
// the order of the group's merge record when the engine's own halt dropped the plan while it gathered
// (PlanMergeResumeFrom). One memo for the call, so a group standing on one node costs one search.
void PlannerResumeFromGather(int order, const uintptr_t* chars, int n, double now);

} // namespace planner
