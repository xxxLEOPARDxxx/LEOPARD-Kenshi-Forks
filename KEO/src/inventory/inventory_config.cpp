// inventory_config.cpp - defaults and INI rows for the inventory module.
#include "inventory/inventory_config.h"
#include "base/config_rows.h"
#include "base/ini_text.h"
#include "inventory/operator_policy.h"
#include <cstddef>
#include <string.h>

namespace keo_inventory {

const InventoryConfig kInventoryDefaults =
{
	true, // backpackFirstDefault
	true, // backpackFoodScoreEnabled
	true, // backpackDialogueFunctionEnabled
	true, // operatorFillBeforeDeliverEnabled
	OPERATOR_HOLD_HEAVY, // operatorHoldUntil
	true, // backpackFixesEnabled
	false, // carriedWeightEnabled: optional candidate, requires restart
	false, // gatherOutputJobEnabled: live, independent
	false, // stockCounterJobEnabled: live, independent
	false, // waypointRouteJobEnabled: live, independent
};

InventoryConfig g_inventoryCfg = kInventoryDefaults;
} // namespace keo_inventory

namespace inventory_config_detail {
union InventoryConfigPodCheck { keo_inventory::InventoryConfig s; };
} // namespace inventory_config_detail
using namespace inventory_config_detail;

namespace keo_inventory {

static const ConfigChoice kOperatorHoldChoices[] =
{
	{ "weightless", OPERATOR_HOLD_WEIGHTLESS, "Weightless" }, { "lightweight", OPERATOR_HOLD_LIGHTWEIGHT, "Lightweight" },
	{ "moderate", OPERATOR_HOLD_MODERATE, "Moderate" }, { "heavy", OPERATOR_HOLD_HEAVY, "Heavy" },
	{ "overloaded", OPERATOR_HOLD_OVERLOADED, "Overloaded" }
};

// operatorHoldUntil: one of kOperatorHoldChoices' words, any case; anything else is refused and the
// key keeps its value. Main thread, at load.
static bool ParseOperatorHoldUntil(const std::string& val, ConfigLogFn log)
{
	(void)log;
	for (int i = 0; i < CFG_COUNT(kOperatorHoldChoices); ++i)
	{
		if (_stricmp(val.c_str(), kOperatorHoldChoices[i].ini) == 0)
		{
			g_inventoryCfg.operatorHoldUntil = kOperatorHoldChoices[i].value;
			return true;
		}
	}
	return false;
}

const ConfigKey g_inventoryConfigKeys[] =
{
	CFG_OBOOL_LIVE("backpackFixes", InventoryConfig, backpackFixesEnabled,                  DOC, SHOW,
	  "Backpack fixes",
	  "Picked-up items go into the worn backpack first (each backpack's window can turn that off for its wearer), food and dialogue checks look in the backpack, and machine operators fill up before delivering."),
	CFG_OBOOL("backpackFirstDefault", InventoryConfig, backpackFirstDefault,                 DOC, DEVROW,
	  "Backpack first",
	  "Picked-up items go into the worn backpack first, unless the character's backpack window says otherwise."),
	CFG_OBOOL("backpackFoodScore", InventoryConfig, backpackFoodScoreEnabled,                DOC, DEVROW,
	  "Food in the backpack counts",
	  "A character carrying food in its worn backpack does not go looking for food on the ground."),
	CFG_OBOOL("backpackDialogueFunction", InventoryConfig, backpackDialogueFunctionEnabled,  DOC, DEVROW,
	  "Dialogue checks the backpack",
	  "A dialogue line that needs an item of a kind also finds it in the worn backpack."),
	CFG_OBOOL("operatorFillBeforeDeliver", InventoryConfig, operatorFillBeforeDeliverEnabled, DOC, DEVROW,
	  "Operators fill up before delivering",
	  "A machine operator collects until its inventory and backpack are full or its load passes the weight tier set below, then delivers; not while hungry."),
	CFG_OCUSTOM_CHOICES_LIVE("operatorHoldUntil", InventoryConfig, operatorHoldUntil, ParseOperatorHoldUntil, DOC, SHOW,
	  "Operators fill up to",
	  "A machine operator keeps collecting until its load passes this tier, the word on its character panel, then delivers. Overloaded: weight never sends it.",
	  kOperatorHoldChoices),
	CFG_OBOOL("carriedWeight", InventoryConfig, carriedWeightEnabled, DOC, SHOW,
	  "Carried body and container weight",
	  "Include contents of carried containers and people, and scale body weight by race, size and hunger. Keeps native backpack weight reductions. Off by default; disabled if KenshiQOL.dll is loaded, regardless of its settings. Requires restart."),
	CFG_OBOOL_LIVE("gatherOutputJob", InventoryConfig, gatherOutputJobEnabled, DOC, SHOW,
	  "Gather Output jobs",
	  "Collect machine output using native collection rules. Turning OFF pauses these jobs and blocks new assignments without deleting queued job rows."),
	CFG_OBOOL_LIVE("stockCounterJob", InventoryConfig, stockCounterJobEnabled, DOC, SHOW,
	  "Stock Counter jobs",
	  "Put carried items into your own shop counters. Uses products from your Gather Output jobs, or native resources when none exist. This is physical stocking, not a sale: no money changes hands. Turning OFF keeps queued job rows."),
	CFG_OBOOL_LIVE("waypointRouteJob", InventoryConfig, waypointRouteJobEnabled, DOC, SHOW,
	  "Cyclic waypoint jobs",
	  "Use SHIFT + ground clicks to queue a cyclic route. Waypoints remain native job rows and are saved with the game. Turning OFF pauses the route and blocks new waypoints without deleting queued rows."),
	{ NULL, CK_BOOL, 0, 0, 0.0f, 0.0f, false, NULL, NULL, false, 0.0f, 0, NULL, INT_MIN, false, false, false, NULL, NULL, NULL, NULL, 0 }
};

} // namespace keo_inventory
