#include "pathfind/path_budget_policy.h"
#include <cstring>

void PathBudgetEnter(PathBudgetFrame* frame, PathBudgetRequest*& slot, void* request, int priority, bool nativeNpc)
{
	frame->previous = slot;
	frame->current.request = request;
	frame->current.priority = priority;
	frame->current.nativeNpc = nativeNpc;
	frame->current.consumed = false;
	slot = &frame->current;
}
void PathBudgetLeave(const PathBudgetFrame* frame, PathBudgetRequest*& slot) { slot = frame->previous; }

bool PathBudgetTake(PathBudgetRequest* current, AstarCallerClass caller, PathBudgetDecision* out)
{
	if (!out) return false;
	out->request = NULL;
	out->playerByReq = -1;
	out->expand = false;
	if (caller != ASTAR_CALLER_CHARACTER_UNKNOWN || !current || current->consumed || !current->request) return false;
	current->consumed = true;
	out->request = current->request;
	out->playerByReq = current->priority >= 20 ? 1 : 0;
	// The native owner flag positively identifies a resolved player. Priority
	// remains the author's routing/diagnostic label, not the budget decision.
	out->expand = !current->nativeNpc;
	return true;
}

bool PathBudgetApply(void* input, bool expand)
{
	if (!input || !expand) return false;
	const int openBytes = 131072 * 4;
	const int stateBytes = 590336 * 4;
	unsigned char* p = static_cast<unsigned char*>(input);
	std::memcpy(p + 156, &openBytes, sizeof(openBytes));
	std::memcpy(p + 160, &stateBytes, sizeof(stateBytes));
	return true;
}
