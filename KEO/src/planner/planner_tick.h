// planner_tick.h - The route planner's main-thread half: the order capture, the tick (validation,
// drops, re-plans, the preload feed, the loaded-set snapshot) and the reports. Every entry returns
// at once while the planner is not armed.
#pragma once
#include <stdint.h>

namespace planner {

const int PLANNER_SKIP_FORMATION = -1;

// Main thread, from hook_addOrderSelected's task-29 branch before the original: one plan per
// selected character chars[0..n) toward location, whatever the order's two flags carry; a repeat
// of the character's plan inside its first second is counted and skipped. formationForms is whether the
// order hook's formation precondition holds (cohesion on, the scatter patch in, more than one character),
// without which no run-together merge runs. Returns the index in chars of the member the formation group
// leads with, or PLANNER_SKIP_FORMATION when the requested route merge cannot be made.
// mergeOrder receives the accepted record number, or 0.
int PlannerNoteOrder(const uintptr_t* chars, int n, const float* location, void* destIndoors,
                     bool shift, bool addDontClear, bool formationForms, int* mergeOrder);
// Discard the character's plan. A player order also invalidates a recent formation decision.
// A Follow issued by an existing formation only replaces the member's individual plan.
void PlannerDrop(uintptr_t character, bool cancelFormationDecision = true);
// Main thread, every frame after IslandTick (never during a save load).
void PlannerTick(void* zoneMgr, double now);
// Main thread: in on, whether the character's legged plan replaces the straight-line ahead enqueue.
bool PlannerRouteReplacesAhead(uintptr_t character);
// Main thread, from the store's start step once the plan store is armed: allocates the search
// scratch and snapshots the tick's settings; false on an allocation failure.
bool PlannerTickArm();
// Main thread, planner_report.cpp: the capped per-order line and the heartbeat.
void PlannerReportPlan(const char* line);
void PlannerReportTick(double now);
// Main thread, the session build only (empty otherwise): the goal section's borders by opposite
// section, classified both ways as the cross resolution classifies them; capped per session.
void PlannerReportBorders(int order, int goalDir);

} // namespace planner
