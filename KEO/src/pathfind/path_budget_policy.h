#pragma once
#include "pathfind/astar_cost_policy.h"

// One synchronous native fallback invocation, on its original calling thread.
// The scope owns these scalars; no request pointer survives its return/unwind.
struct PathBudgetRequest
{
	void* request;
	int priority;
	bool nativeNpc, consumed;
};
struct PathBudgetDecision
{
	void* request;
	int playerByReq;
	bool expand;
};
struct PathBudgetFrame
{
	PathBudgetRequest current;
	PathBudgetRequest* previous;
};
// POD frame so the native caller can restore through __finally for both C++
// and native unwinds under /EHsc, without allocating or swallowing a fault.
void PathBudgetEnter(PathBudgetFrame* frame, PathBudgetRequest*& slot, void* request, int priority, bool nativeNpc);
void PathBudgetLeave(const PathBudgetFrame* frame, PathBudgetRequest*& slot);

// Call with the caller classification made WITHOUT a request tag. Only the
// audited character site returns CHARACTER_UNKNOWN; other sites cannot borrow
// or consume this request. Each current invocation can be consumed only once.
bool PathBudgetTake(PathBudgetRequest* current, AstarCallerClass caller, PathBudgetDecision* out);
// Exact original KEO four-times sizes, no allocations or traversal changes.
// false/null is byte-for-byte pass-through of native inputs.
bool PathBudgetApply(void* input, bool expand);
