// The config module contract: ownership, defaults, bounds and load clamps.
#include "base/config_table.h"
#include "base/config_values.h"
#include "gui/settings_factory.h"
#include "render/render_config.h"
#include <cstring>
#include <set>
#include <sstream>
#include "check.h"

bool ApplyRenderConfig(const RenderConfig&) { return true; }
bool BenchWindowInForeground() { return false; }
int BenchLoadedZoneCount() { return 0; }

namespace config_modules_units_detail {
struct Owner { const char* key; const char* module; };
struct Expected { const char* name; const char* title; size_t size; const void* defaults; };
}
using namespace config_modules_units_detail;

static const Owner kOwners[] =
{
	{ "deferral", "zone" },
	{ "priorityBoost", "navmesh" },
	{ "preload", "zone" },
	{ "resourceRamCache", "zone" }, { "resourceRamGiB", "zone" },
	{ "resourceReadMBps", "zone" }, { "resourcePrefetch", "zone" },
	{ "resourceGpuWarm", "zone" }, { "resourceStartupTextures", "zone" },
	{ "resourceGeometryWarm", "zone" }, { "resourceTextureRetention", "zone" },
	{ "resourceGpuGiB", "zone" }, { "resourceWarmHud", "zone" },
	{ "movementAware", "zone" },
	{ "caching", "navmesh" },
	{ "playerCharRegistry", "zone" },
	{ "reprioFast", "navmesh" },
	{ "routeTier", "navmesh" },
	{ "sectionStamp", "fixes" },
	{ "navMeshUpdateGuard", "fixes" },
	{ "destroyListDiag", "fixes" },
	{ "destroyListDefer", "fixes" },
	{ "saveLoadUnload", "zone" },
	{ "escapePauseGuard", "zone" },
	{ "townGuard", "zone" },
	{ "zoneRetention", "zone" },
	{ "islandReadinessRule", "zone" },
	{ "readinessOverrides", "zone" },
	{ "zoneLifeUnload", "zone" },
	{ "navmeshVanillaPruning", "navmesh" },
	{ "navmeshNeighbourSeeds", "navmesh" },
	{ "navmeshBuildLockNarrow", "navmesh" },
	{ "navmeshStallThrottle", "navmesh" },
	{ "unstitchGuard", "fixes" },
	{ "stitchSourceLines", "fixes" },
	{ "graphVisitorGuard", "fixes" },
	{ "graphExpandGuard", "fixes" },
	{ "graphPositionGuard", "fixes" },
	{ "meshFaceGuard", "fixes" },
	{ "createInstanceGuard", "fixes" },
	{ "hullDoublePushGuard", "fixes" },
	{ "hullSameSkip", "fixes" },
	{ "sceneForkSkip", "fixes" },
	{ "instEmptySkip", "fixes" },
	{ "d3dStateSkip", "fixes" },
	{ "stitchByteGuard", "fixes" },
	{ "navmeshAdjExclusion", "navmesh" },
	{ "navMeshLife", "fixes" },
	{ "unstitchProbe", "fixes" },
	{ "sectionKeyProbe", "fixes" },
	{ "settingsPanel", "gui" },
	{ "navmeshMissHash", "navmesh" },
	{ "navmeshMissSplit", "navmesh" },
	{ "navmeshMissSplitBg", "navmesh" },
	{ "zoneCycleStats", "zone" },
	{ "zoneWedgeGuard", "zone" },
	{ "physPurecallRecord", "fixes" },
	{ "physQueryGuard", "fixes" },
	{ "corpsePin", "fixes" },
	{ "relationsSelfFind", "fixes" },
	{ "nestValidationGuard", "fixes" },
	{ "onScreenStagger", "fixes" },
	{ "pausedOffscreenSkip", "fixes" },
	{ "blackboardEmpty", "fixes" },
	{ "animationDirtyReuse", "fixes" },
	{ "animationStateLookup", "fixes" },
	{ "skeletonReserve", "fixes" },
	{ "meshWeights", "fixes" },
	{ "modelCloneReserve", "fixes" },
	{ "materialReserve", "fixes" },
	{ "equipmentFieldLookup", "fixes" },
	{ "morphPoseReuse", "fixes" },
	{ "saveReadBuffer", "fixes" },
	{ "saveFieldKeyReuse", "fixes" },
	{ "saveInstanceLookup", "fixes" },
	{ "saveStringTransfer", "fixes" },
	{ "graphHeuristicGuard", "fixes" },
	{ "ogreJoinSpinUs", "fixes" },
	{ "zoneGeometryMode", "zone" },
	{ "camFocus", "zone" },
	{ "preloadKeepAliveSeconds", "zone" },
	{ "navmeshWorkerCount", "navmesh" },
	{ "navmeshGenConcurrency", "navmesh" },
	{ "navmeshDiskCacheMaxMB", "navmesh" },
	{ "camLogInterval", "zone" },
	{ "camFocusMaxDist", "zone" },
	{ "camFocusHardMult", "zone" },
	{ "camFocusHysteresis", "zone" },
	{ "reprioritizeInterval", "navmesh" },
	{ "zoneLifeRetainRadius", "zone" },
	{ "zoneLifeSquadRadius", "zone" },
	{ "zoneRetentionMaxHeld", "zone" },
	{ "zoneLifeIdleSeconds", "zone" },
	{ "townClaimFix", "fixes" },
	{ "throwOutFix", "fixes" },
	{ "throwOutHoldMinutes", "fixes" },
	{ "wallSpliceFix", "navmesh" },
	{ "spliceSeamFull", "navmesh" },
	{ "autoNavmeshRepair", "navmesh" },
	{ "backpackFirstDefault", "inventory" },
	{ "backpackFoodScore", "inventory" },
	{ "backpackDialogueFunction", "inventory" },
	{ "operatorFillBeforeDeliver", "inventory" },
	{ "operatorHoldUntil", "inventory" },
	{ "operatorFillPercent", "inventory" },
	{ "backpackFixes", "inventory" },
	{ "carriedWeight", "inventory" },
	{ "gatherOutputJob", "inventory" },
	{ "stockCounterJob", "inventory" },
	{ "waypointRouteJob", "inventory" },
};
static const Expected kExpected[] =
{
	{ "render", "Render and particles", sizeof(RenderConfig), &kRenderDefaults },
	{ "zone", "Zone loading", sizeof(zone::ZoneConfig), &zone::kZoneDefaults },
	{ "navmesh", "Navmesh", sizeof(navmesh::NavMeshConfig), &navmesh::kNavMeshDefaults },
	{ "fixes", "Crash guards and probes", sizeof(fixes::FixesConfig), &fixes::kFixesDefaults },
	{ "inventory", "Inventory and jobs", sizeof(keo_inventory::InventoryConfig), &keo_inventory::kInventoryDefaults },
	{ "gui", "Settings panel", sizeof(keo_gui::GuiConfig), &keo_gui::kGuiDefaults },
	{ "core", "Benchmark and retired keys", 0, NULL }
};

static void Fail(const char* key, const std::string& reason)
{
	++CheckFailureCounter();
	printf("config_modules_units: %s: %s\n", key, reason.c_str());
}

static void DiscardLog(const std::string&) {}

// Retired navigation preferences are accepted only as ignored legacy keys.
static void CheckRemovedRouteKeys()
{
 const char* keys[] = {"pathfindDiag","npcWaitDiag","gatePassDiag","pathCostLines","clusterGraphBypass","playerRepathTier","playerHierarchical","playerHierOnCap","groupCohesion","islandFix","islandFarSpan","islandEdgeRing","islandDeletedReissue","k7PostDeathHold","k7DestReadyGate","k7ArrivalTrigger","formationFollow","formationGatherPace","movementTrace","keoRouting","plannerMode","plannerLegSpan","plannerBaseBuild","plannerAheadTiles","plannerWaitSeconds","plannerWaterCost","plannerWaterEngine","plannerAcidCost","plannerPreArrivalMs","plannerLegAim","plannerMergeBias","plannerMergeDetour","pathExtractGuard","clusterCrossCost" };
 const zone::ZoneConfig zoneBefore = zone::g_zoneCfg;
 const navmesh::NavMeshConfig navBefore = navmesh::g_navmeshCfg;
 ConfigLoadState state;
 for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
  Check(FindConfigKey(keys[i], NULL) == NULL, "removed route key has no active writer");
  ConfigApplyLine(keys[i], "true", int(i + 1), &state, &DiscardLog);
  ConfigApplyLine(keys[i], "on", int(i + 1), &state, &DiscardLog);
 }
 Check(state.overrides == 0 && state.unrecognised == 0 && state.retired == 34,
       "all removed route keys are ignored once, never applied");
 Check(!memcmp(&zoneBefore, &zone::g_zoneCfg, sizeof(zoneBefore)) &&
       !memcmp(&navBefore, &navmesh::g_navmeshCfg, sizeof(navBefore)),
       "legacy route keys cannot change retained streaming settings");
}

// The retain radius as an empty INI loads it, and as an explicit line inside its range loads it.
static void CheckZoneRetainRadius()
{
	const zone::ZoneConfig held = zone::g_zoneCfg;
	zone::g_zoneCfg = zone::kZoneDefaults;
	ConfigClampLoaded(&DiscardLog);
	Check(zone::g_zoneCfg.cfg_zoneLifeRetainRadius == 1, "zone: an empty INI loads zoneLifeRetainRadius 1");
	const char* const texts[] = { "2", "1", "4" };
	const int values[] = { 2, 1, 4 };
	for (int i = 0; i < 3; ++i)
	{
		zone::g_zoneCfg = zone::kZoneDefaults;
		zone::g_zoneCfg.cfg_zoneLifeRetainRadius = -7;
		ConfigLoadState st;
		ConfigApplyLine("zoneLifeRetainRadius", texts[i], 1, &st, &DiscardLog);
		ConfigClampLoaded(&DiscardLog);
		std::ostringstream ss;
		ss << "zone: zoneLifeRetainRadius=" << texts[i] << " loads " << values[i];
		Check(zone::g_zoneCfg.cfg_zoneLifeRetainRadius == values[i] && st.overrides == 1 && st.unrecognised == 0,
		      ss.str().c_str());
	}
	zone::g_zoneCfg = held;
}

// The squad radius and the held-zone cap: their defaults, the squad radius's
// 0 (vanilla) accepted and a negative value refused, the cap clamped to its
// range.
static void CheckZoneSquadRadiusAndCap()
{
	const zone::ZoneConfig held = zone::g_zoneCfg;
	zone::g_zoneCfg = zone::kZoneDefaults;
	ConfigClampLoaded(&DiscardLog);
	Check(zone::g_zoneCfg.cfg_zoneLifeSquadRadius == 1, "zone: an empty INI loads zoneLifeSquadRadius 1");
	Check(zone::g_zoneCfg.cfg_zoneRetentionMaxHeld == 45, "zone: an empty INI loads zoneRetentionMaxHeld 45");

	ConfigLoadState st;
	ConfigApplyLine("zoneLifeSquadRadius", "0", 1, &st, &DiscardLog);
	ConfigClampLoaded(&DiscardLog);
	Check(zone::g_zoneCfg.cfg_zoneLifeSquadRadius == 0 && st.overrides == 1, "zone: zoneLifeSquadRadius=0 loads 0");

	ConfigLoadState st2;
	ConfigApplyLine("zoneLifeSquadRadius", "-1", 1, &st2, &DiscardLog);
	Check(zone::g_zoneCfg.cfg_zoneLifeSquadRadius == 0 && st2.unrecognised == 1,
	      "zone: zoneLifeSquadRadius=-1 is refused and leaves the value");

	ConfigLoadState st3;
	ConfigApplyLine("zoneRetentionMaxHeld", "5", 1, &st3, &DiscardLog);
	ConfigClampLoaded(&DiscardLog);
	Check(zone::g_zoneCfg.cfg_zoneRetentionMaxHeld == 12, "zone: zoneRetentionMaxHeld=5 clamps to 12");
	zone::g_zoneCfg = held;
}

// Real INI parsing must publish valid limits to both the UI and the caches;
// downstream cache clamps alone do not normalize the stored configuration.
static void CheckResourceLimits()
{
	struct ResourceLimit { const char* key; int zone::ZoneConfig::* field; int lo, hi, def; };
	const ResourceLimit limits[] =
	{
		{ "resourceRamGiB", &zone::ZoneConfig::cfg_resourceRamGiB, 1, 32, 1 },
		{ "resourceReadMBps", &zone::ZoneConfig::cfg_resourceReadMBps, 1, 128, 32 },
		{ "resourceGpuGiB", &zone::ZoneConfig::cfg_resourceGpuGiB, 1, 8, 1 }
	};
	const zone::ZoneConfig held = zone::g_zoneCfg;
	for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i)
	{
		const ResourceLimit& r = limits[i];
		const ConfigModule* mod = NULL;
		const ConfigKey* key = FindConfigKey(r.key, &mod);
		Check(key && mod && !strcmp(mod->name, "zone") && key->kind == CK_INT && key->live
		      && !key->devOnly && key->lo == r.lo && key->hi == r.hi, "resource budget metadata: owned, live and bounded");
		zone::g_zoneCfg = zone::kZoneDefaults;
		ConfigClampLoaded(&DiscardLog);
		Check(zone::g_zoneCfg.*(r.field) == r.def, "empty INI preserves the resource budget default");
		for (int side = 0; side < 2; ++side)
		{
			zone::g_zoneCfg = zone::kZoneDefaults;
			std::ostringstream text, name;
			text << (side ? r.hi + 1 : r.lo - 1);
			ConfigLoadState state;
			ConfigApplyLine(r.key, text.str(), 1, &state, &DiscardLog);
			ConfigClampLoaded(&DiscardLog);
			name << "resource INI load clamps " << r.key << (side ? " above maximum" : " below minimum");
			Check(zone::g_zoneCfg.*(r.field) == (side ? r.hi : r.lo) && state.overrides == 1 && state.unrecognised == 0,
			      name.str().c_str());
		}
	}
	zone::g_zoneCfg = held;
}

// Live module rows either publish through their frame tick or load the stored field once per use.
static bool LiveOffsetRowAllowed(const char* module, const char* key)
{
	if (!strcmp(module, "navmesh")) return !strcmp(key, "autoNavmeshRepair");
	if (!strcmp(module, "gui")) return !strcmp(key, "Debug");
	if (!strcmp(module, "inventory"))
		return !strcmp(key, "operatorHoldUntil") || !strcmp(key, "operatorFillPercent") || !strcmp(key, "backpackFixes") || !strcmp(key, "gatherOutputJob") || !strcmp(key, "stockCounterJob") || !strcmp(key, "waypointRouteJob");
	if (!strcmp(module, "zone"))
		return !strcmp(key, "zoneLifeRetainRadius") || !strcmp(key, "zoneLifeSquadRadius")
		    || !strcmp(key, "zoneRetentionMaxHeld")
		    || !strcmp(key, "resourceRamCache") || !strcmp(key, "resourceRamGiB")
		    || !strcmp(key, "resourceReadMBps") || !strcmp(key, "resourcePrefetch")
		    || !strcmp(key, "resourceGpuWarm") || !strcmp(key, "resourceStartupTextures")
		    || !strcmp(key, "resourceGeometryWarm") || !strcmp(key, "resourceTextureRetention")
		    || !strcmp(key, "resourceGpuGiB") || !strcmp(key, "resourceWarmHud");
	if (!strcmp(module, "fixes"))
		return !strcmp(key, "relationsSelfFind")
		    || !strcmp(key, "hullSameSkip") || !strcmp(key, "onScreenStagger")
		    || !strcmp(key, "pausedOffscreenSkip") || !strcmp(key, "blackboardEmpty") || !strcmp(key, "animationStateLookup") || !strcmp(key, "animationDirtyReuse") || !strcmp(key, "skeletonReserve") || !strcmp(key, "meshWeights") || !strcmp(key, "modelCloneReserve") || !strcmp(key, "morphPoseReuse") || !strcmp(key, "materialReserve") || !strcmp(key, "equipmentFieldLookup") || !strcmp(key, "saveReadBuffer") || !strcmp(key, "saveInstanceLookup") || !strcmp(key, "saveFieldKeyReuse") || !strcmp(key, "saveStringTransfer")
		    || !strcmp(key, "ogreJoinSpinUs")
		    || !strcmp(key, "sceneForkSkip") || !strcmp(key, "instEmptySkip")
		    || !strcmp(key, "d3dStateSkip");
	return false;
}

// A live field LiveFieldStore can store: 1, 4 or 8 bytes at an offset aligned to its width.
static bool LiveRowStorable(const ConfigKey& k)
{
	return (k.size == 1 || k.size == 4 || k.size == 8) && k.offset % k.size == 0;
}

static ConfigKey StorableProbe(size_t offset, size_t size)
{
	ConfigKey k;
	memset(&k, 0, sizeof(k));
	k.name = "storableProbe";
	k.kind = CK_INT;
	k.offset = offset;
	k.size = size;
	k.live = true;
	return k;
}

static void CheckLiveRowStorable()
{
	Check(LiveRowStorable(StorableProbe(3, 1)) && LiveRowStorable(StorableProbe(4, 4)) && LiveRowStorable(StorableProbe(8, 8)),
	      "live rows: 1-, 4- and 8-byte aligned fields are storable");
	Check(!LiveRowStorable(StorableProbe(0, 2)), "live rows: a two-byte field is not storable");
	Check(!LiveRowStorable(StorableProbe(2, 4)) && !LiveRowStorable(StorableProbe(4, 8)),
	      "live rows: a misaligned field is not storable");
}

int main()
{
	Check(kConfigModuleCount == 7 && kConfigModuleCount <= CONFIG_MODULE_MAX, "seven modules within stage capacity");
	std::set<std::string> names;
	int moduleKeys = 0, activeCore = 0, retiredCore = 0, debug = 0;
	for (int m = 0; m < kConfigModuleCount; ++m)
	{
		const ConfigModule& mod = kConfigModules[m];
		if (m >= 7) { Fail(mod.name, "unexpected module"); continue; }
		const Expected& e = kExpected[m];
		if (strcmp(mod.name, e.name) || strcmp(mod.title, e.title)) Fail(mod.name, "module order or title");
		if (mod.state)
		{
			if (!mod.defaults) Fail(mod.name, "offset rows with no defaults");
			if (mod.stateSize != e.size || mod.stateSize > CONFIG_STATE_MAX) Fail(mod.name, "state size");
			if (mod.defaults != e.defaults) Fail(mod.name, "defaults object address");
			if (mod.defaults && memcmp(mod.state, mod.defaults, mod.stateSize)) Fail(mod.name, "instance differs from defaults");
		}
		else if (strcmp(mod.name, "core") || mod.defaults || mod.stateSize) Fail(mod.name, "core state must be null");
		std::set<size_t> offsets;
		int i = 0;
		for (; i < CONFIG_STAGE_MAX && mod.keys[i].name; ++i)
		{
			const ConfigKey& k = mod.keys[i];
			if (!names.insert(k.name).second) Fail(k.name, "duplicate key");
			if (strcmp(mod.name, "core") == 0) { if (k.retired) ++retiredCore; else ++activeCore; }
			else if (strcmp(mod.name, "render")) ++moduleKeys;
			if (k.debugOnlyReader)
			{
				++debug;
				if (strcmp(k.name, "unstitchProbe") && strcmp(k.name, "sectionKeyProbe")) Fail(k.name, "unexpected debugOnlyReader");
			}
			if (mod.state)
			{
				if (k.target) Fail(k.name, "target row in offset module");
				if (k.offset + k.size > mod.stateSize) Fail(k.name, "offset outside state");
				if (!offsets.insert(k.offset).second) Fail(k.name, "duplicate offset");
				size_t width = k.kind == CK_BOOL ? 1 : k.kind == CK_INT || k.kind == CK_FLOAT ? 4 : k.kind == CK_DOUBLE ? 8 : k.size;
				if (k.kind == CK_CUSTOM) width = sizeof(int);
				// Render scalar rows preserve the legacy zero size; their kind supplies the width.
				if (k.size != width && (strcmp(mod.name, "render") || k.size != 0)) Fail(k.name, "kind width");
				if (k.offset + width > mod.stateSize) Fail(k.name, "kind width outside state");
			}
			if (k.live && strcmp(mod.name, "render") && !LiveOffsetRowAllowed(mod.name, k.name)) Fail(k.name, "live row outside the render module and the allowed rows");
			if (k.live && strcmp(mod.name, "render") && !LiveRowStorable(k)) Fail(k.name, "live row width or alignment");
			if (!mod.state || !strcmp(mod.name, "render") || k.retired || k.lo > k.hi
			    || (k.kind != CK_INT && k.kind != CK_FLOAT && k.kind != CK_DOUBLE)) continue;
			std::vector<unsigned char> saved((unsigned char*)mod.state, (unsigned char*)mod.state + mod.stateSize);
			char* p = (char*)mod.state + k.offset;
			bool ok;
			if (k.kind == CK_INT) { *(int*)p = (int)k.hi + 1; ConfigClampLoaded(NULL); ok = *(int*)p == (int)k.hi; }
			else if (k.kind == CK_FLOAT) { *(float*)p = k.hi + 1.0f; ConfigClampLoaded(NULL); ok = *(float*)p == k.hi; }
			else { *(double*)p = (double)k.hi + 1.0; ConfigClampLoaded(NULL); ok = *(double*)p == (double)k.hi; }
			memcpy(mod.state, &saved[0], mod.stateSize);
			if (!ok) Fail(k.name, "not clamped at load");
		}
		if (i == CONFIG_STAGE_MAX) Fail(mod.name, "no table end within stage capacity");
	}
	Check(moduleKeys == 111 && activeCore == 2 && retiredCore == 57 && debug == 2, "module and core row counts");
	const char* const removed[] = { "npcFailMemo", "rqStructClear", "ogreWorkerPriority", "factionSelfGuard" };
	for (int i = 0; i < 4; ++i)
		Check(FindConfigKey(removed[i], NULL) == NULL, "removed switches have no config row");
	for (size_t i = 0; i < sizeof(kOwners) / sizeof(kOwners[0]); ++i)
	{
		const ConfigModule* mod = NULL;
		const ConfigKey* k = FindConfigKey(kOwners[i].key, &mod);
		if (!k) Fail(kOwners[i].key, std::string("expected in ") + kOwners[i].module + ", missing");
		else if (strcmp(mod->name, kOwners[i].module)) Fail(kOwners[i].key, std::string("expected in ") + kOwners[i].module + ", found in " + mod->name);
	}
	CheckRemovedRouteKeys();
	CheckZoneRetainRadius();
	CheckZoneSquadRadiusAndCap();
	CheckResourceLimits();
	CheckLiveRowStorable();
	return CheckExit("config_modules_units");
}
