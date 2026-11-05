// movement_config.cpp - defaults and INI rows for the movement module.
#include "movement/movement_config.h"
#include "base/config_rows.h"
#include "base/ini_text.h"
#include "movement/formation_follow_policy.h"
#include "movement/movement_trace_policy.h"
#include "planner/planner_config.h"
#include "pathfind/pathfind_config.h"
#include "pathfind/astar_hier_policy.h"
#include "fixes/fixes_config.h"
#include <cstddef>
#include <string.h>

static bool ParseIslandEdgeRing(const std::string& val, ConfigLogFn log)
{
	bool b;
	bool isObserve = _stricmp(val.c_str(), "observe") == 0;
	bool isOff = _stricmp(val.c_str(), "off") == 0;
	bool isOn = _stricmp(val.c_str(), "on") == 0;
	if (isObserve || isOff || isOn) {
		b = isOn;
		log("Config: islandEdgeRing=" + val + " is a retired value; read as " + (b ? "true" : "false"));
		movement::g_movementCfg.islandEdgeRingEnabled = b;
		return true;
	}
	if (ParseBool(val, &b)) { movement::g_movementCfg.islandEdgeRingEnabled = b; return true; }
	return false;
}

static bool ParseK7PostDeathHold(const std::string& val, ConfigLogFn log)
{
	(void)log;
	bool b = true, third = false;
	if (ParseBoolOr(val, "observe", &b, &third)) {
		movement::g_movementCfg.cfg_k7PostDeathHold = third ? K7_HOLD_OBSERVE : (b ? K7_HOLD_ON : K7_HOLD_OFF);
		return true;
	}
	return false;
}

// formationFollow: off or probe; anything else is refused.
static bool ParseFormationFollow(const std::string& val, ConfigLogFn log)
{
	(void)log;
	if (_stricmp(val.c_str(), "off") == 0)
		movement::g_movementCfg.cfg_formationFollow = FF_OFF;
	else if (_stricmp(val.c_str(), "probe") == 0)
		movement::g_movementCfg.cfg_formationFollow = FF_PROBE;
	else
		return false;
	return true;
}

// movementTrace: off or on; anything else is refused.
static bool ParseMovementTrace(const std::string& val, ConfigLogFn log)
{
	(void)log;
	if (_stricmp(val.c_str(), "off") == 0)
		movement::g_movementCfg.cfg_movementTrace = TRACE_OFF;
	else if (_stricmp(val.c_str(), "on") == 0)
		movement::g_movementCfg.cfg_movementTrace = TRACE_ON;
	else
		return false;
	return true;
}

static const ConfigChoice kIslandEdgeRingChoices[] =
{
	{ "false", 0, "false" }, { "true", 1, "true" }
};

static const ConfigChoice kK7HoldChoices[] =
{
	{ "false", K7_HOLD_OFF, "Off" }, { "observe", K7_HOLD_OBSERVE, "Observe", true }, { "true", K7_HOLD_ON, "On" }
};

namespace movement {

const MovementConfig kMovementDefaults =
{
	true, // groupCohesionEnabled
	true, // islandFixEnabled
	2, // cfg_islandFarSpan
	false, // islandEdgeRingEnabled
	true, // playerCharRegistryEnabled
	true, // islandDeletedReissueEnabled
	K7_HOLD_ON, // cfg_k7PostDeathHold
	true, // k7DestReadyGateEnabled
	true, // k7ArrivalTriggerEnabled
	FF_OFF, // cfg_formationFollow
	true, // formationGatherPaceEnabled
	TRACE_OFF, // cfg_movementTrace
	true, // routingEnabled
};

MovementConfig g_movementCfg = kMovementDefaults;
} // namespace movement

namespace movement_config_detail {
union MovementConfigPodCheck { movement::MovementConfig s; };
} // namespace movement_config_detail
using namespace movement_config_detail;

namespace movement {

const ConfigKey g_movementConfigKeys[] =
{
	CFG_OBOOL("groupCohesion", MovementConfig, groupCohesionEnabled,         NDOC, SHOW,
	  "Keep squads together",
	  "Keeps a squad given one move order walking together instead of scattering."),
	CFG_OBOOL("islandFix", MovementConfig, islandFixEnabled,             NDOC, DEVROW,
	  "Island routing overlay",
	  "Lets the mod's island overlay answer the engine's island checks."),
	CFG_OINT("islandFarSpan", MovementConfig, cfg_islandFarSpan, 0.0f, 8.0f, INT_MIN, DOC, DEVROW,
	  "Far order island span in cells",
	  "Treats two cells this many or more apart as different islands, so a far move order walks leg by"
	  " leg instead of one path that can end short. 0 keeps the game's answer."),
	CFG_OCUSTOM_CHOICES("islandEdgeRing", MovementConfig, islandEdgeRingEnabled, ParseIslandEdgeRing, DOC, DEVROW,
	  "Edge route leg ring",
	  "Narrows the island list the engine's edge-route leg finder sees to the cells around the"
	  " character, so a leg is at most about two cells. false only counts what it would remove.", kIslandEdgeRingChoices),
	CFG_OBOOL("playerCharRegistry", MovementConfig, playerCharRegistryEnabled,    DOC, DEVROW,
	  "Watch every player character",
	  "Watches every player-faction character, not only those with a move order, so the zone each"
	  " stands in keeps its place in the navmesh queue and in what loads first."),
	CFG_OBOOL("islandDeletedReissue", MovementConfig, islandDeletedReissueEnabled,  NDOC, DEVROW,
	  "Re-issue deleted move orders",
	  "Re-issues a player move order the engine deleted, never one the player cancelled."),
	CFG_OCUSTOM_CHOICES("k7PostDeathHold", MovementConfig, cfg_k7PostDeathHold, ParseK7PostDeathHold, DOC, SHOW,
	  "Resume move orders after combat",
	  "Holds a player move order that ended as combat took over, and re-issues it once the fight ends,"
	  " up to 60 seconds. Observe (DEV) only logs what would be held.", kK7HoldChoices),
	CFG_OBOOL("k7DestReadyGate", MovementConfig, k7DestReadyGateEnabled,       DOC, DEVROW,
	  "Wait for the destination's navmesh",
	  "Before re-issuing a deleted order, waits up to 15 seconds for the destination cell's navmesh to"
	  " be in the world."),
	CFG_OBOOL("k7ArrivalTrigger", MovementConfig, k7ArrivalTriggerEnabled,      DOC, DEVROW,
	  "Re-issue on navmesh arrival",
	  "When an order stops far short while its destination's navmesh is missing, re-issues it as soon"
	  " as that navmesh arrives. Off only logs when it would have sent."),
	CFG_OROW("formationFollow", CK_CUSTOM, MovementConfig, cfg_formationFollow, 1.0f, 0.0f, INT_MIN, false, NDOC,
	         ParseFormationFollow, NULL, NULL, false, 0.0f, 0, NULL, 0, true),
	CFG_OBOOL("formationGatherPace", MovementConfig, formationGatherPaceEnabled, DOC, SHOW,
	  "Arrive together when gathering",
	  "When a squad set to run together gathers before it sets off, the members closer to the"
	  " gathering point walk slower so everyone arrives at about the same time."),
	CFG_OROW("movementTrace", CK_CUSTOM, MovementConfig, cfg_movementTrace, 1.0f, 0.0f, INT_MIN, false, NDOC,
	         ParseMovementTrace, NULL, NULL, false, 0.0f, 0, NULL, 0, true),
	CFG_OBOOL("keoRouting", MovementConfig, routingEnabled, NDOC, SHOW,
	  "KEO pathfinding",
	  "KEO's changes to how characters find and follow routes: the route planner for long orders,"
	  " squad cohesion, re-issued move orders and the rest. Off: characters path as in the vanilla game;"
	  " zone loading, navmesh and the stability fixes stay on."),
	{ NULL, CK_BOOL, 0, 0, 0.0f, 0.0f, false, NULL, NULL, false, 0.0f, 0, NULL, INT_MIN, false, false, false, NULL, NULL, NULL, NULL, 0 }
};

} // namespace movement

// LEOPARD fork: keoRouting=false. The same clears CheckBuildGate makes when no hook may
// install (islandFix, groupCohesion, islandFarSpan), plus the planner, the hierarchical
// player search, cluster connectivity bypass/repath tiers, cluster cross cost and
// deleted-order re-issue. pathfindDiag is an installer switch, not just logging:
// its hooks also change queue tiers and A* budgets. Keep those hooks out rather
// than leaving the default pathfindDiag=true behind the master OFF switch.
void movement::ApplyRoutingSwitch(ConfigLogFn log)
{
	if (movement::g_movementCfg.routingEnabled)
		return;
	movement::g_movementCfg.groupCohesionEnabled = false;
	movement::g_movementCfg.islandFixEnabled = false;
	movement::g_movementCfg.cfg_islandFarSpan = 0;
	movement::g_movementCfg.islandEdgeRingEnabled = false;
	movement::g_movementCfg.islandDeletedReissueEnabled = false;
	movement::g_movementCfg.cfg_k7PostDeathHold = K7_HOLD_OFF;
	movement::g_movementCfg.k7DestReadyGateEnabled = false;
	movement::g_movementCfg.k7ArrivalTriggerEnabled = false;
	movement::g_movementCfg.formationGatherPaceEnabled = false;
	planner::g_plannerCfg.mode = planner::PLANNER_OFF;
	pathfind::g_pathfindCfg.pathfindDiagEnabled = false;
	pathfind::g_pathfindCfg.clusterGraphBypassMode = CGB_ORIGINAL;
	pathfind::g_pathfindCfg.playerRepathTierEnabled = false;
	pathfind::g_pathfindCfg.playerHierarchicalMode = AHIER_OFF;
	fixes::g_fixesCfg.clusterCrossCostOn = 0;
	if (log)
		log("Config: keoRouting=false -- route planner, pathfinding hooks, connectivity bypass, repath tiers,"
		    " hierarchical search, cluster cross cost, island overlay, deleted-order re-issue, squad cohesion"
		    " and gather pace are off; zone loading, navmesh and independent guard settings are unchanged");
}
