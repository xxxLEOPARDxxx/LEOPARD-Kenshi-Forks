from pathlib import Path
import os
import re
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "Source/BetterLooting.cpp").read_text(encoding="utf-8-sig")

def function(name):
    start = source.index(name + "(")
    while source.find(";", start) < source.find("{", start):
        start = source.index(name + "(", start + len(name) + 1)
    start = source.rfind("\nstatic ", 0, start) + 1
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

assert not re.search(r"static\s+(?:InventoryGUI|Character)\*\s+g_(?:player\w*Window|radialLoot\w+)", source)
target = re.search(r"struct BetterLootingRadiusTarget\s*\{([^}]*)\}", source).group(0)
assert "hand owner;" in target and "*" not in target
assert "QueueSmartLoot" not in source
assert source.count("== KenshiLib::SUCCESS;") >= 2
assert source.index("updateGuard(processing)") < source.index("g_origInventoryGUIUpdate(self);", source.index("} updateGuard(processing);"))
code = r"""
#include <cstdio>
#include <cstring>
#include <vector>
#include <stdint.h>

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
enum itemType { CHARACTER, BUILDING };
struct RootObject;
struct Character;
struct Building;
struct hand {
    RootObject* object;
    unsigned serial;
    hand() : object(NULL), serial(0) {}
    hand(RootObject* value);
    Character* getCharacter() const;
    Building* getBuilding() const;
    void setNull() { object = NULL; serial = 0; }
};
struct Inventory { int items; int reads; Inventory() : items(1), reads(0) {}
    int getNumItems() { ++reads; return items; }
};
struct ContainerItem {
    Inventory* inventory;
    ContainerItem() : inventory(NULL) {}
    Inventory* getInventory() { return inventory; }
};
struct RootObject {
    unsigned serial; bool loaded; bool eligible; bool inRadius; itemType type; Inventory* inventory;
    RootObject(itemType value) : serial(1), loaded(true), eligible(true), inRadius(true), type(value), inventory(NULL) {}
    Inventory* getInventory() { return inventory; }
};
struct Character : RootObject {
    bool player; ContainerItem* backpack;
    Character() : RootObject(CHARACTER), player(true), backpack(NULL) {}
    bool isPlayerCharacter() { return player; }
    ContainerItem* hasABackpackOn() { return backpack; }
};
struct Building : RootObject { Building() : RootObject(BUILDING) {} };
hand::hand(RootObject* value) : object(value), serial(value ? value->serial : 0) {}
Character* hand::getCharacter() const {
    return object && object->loaded && object->serial == serial && object->type == CHARACTER
        ? static_cast<Character*>(object) : NULL;
}
Building* hand::getBuilding() const {
    return object && object->loaded && object->serial == serial && object->type == BUILDING
        ? static_cast<Building*>(object) : NULL;
}
struct InventoryGUI {
    Character* actor; Inventory* inventory; bool visible;
    InventoryGUI(Character* value, Inventory* inv) : actor(value), inventory(inv), visible(true) {}
    bool isVisible() { return visible; }
    Character* getCallbackCharacter() { return actor; }
    Inventory* getInventory() { return inventory; }
};
struct GUI {
    Character* actor; InventoryGUI* window;
    GUI() : actor(NULL), window(NULL) {}
    InventoryGUI* getInventoryWindow(const hand& owner) {
        return owner.getCharacter() == actor ? window : NULL;
    }
} nativeGui;
GUI* gui = &nativeGui;
struct Rules { bool preferBackpack; Rules() : preferBackpack(true) {} } g_betterLootingRules;
void BlTrace(const char*) {}
"""
a = source.index("struct BetterLootingDestinations")
b = source.index("struct BetterLootingItemLocation", a)
code += source[a:b] + "\n" + target + ";\n"
code += r"""
static std::vector<BetterLootingRadiusTarget> g_radialLootQueue;
static std::size_t g_radialLootIndex = 0;
static hand g_radialLootOwner;
static unsigned g_inventoryLifecycleRevision = 0;
static uintptr_t g_radialLootWindowIdentity = 0;
static int g_radialLootWaitFrames = 0;
static bool g_radialLootActive = false;
static bool g_betterLootingTransferFault = false;
static int transfers = 0;
static Inventory* usedFirst = NULL;
static Inventory* usedSecond = NULL;
static bool IsBetterLootingCharacterTarget(Character* value) { return value && value->eligible; }
static bool IsBetterLootingBuildingTarget(Building* value) { return value && value->eligible; }
static bool IsBetterLootingInsideConfiguredRadius(RootObject* value, itemType, Character*) { return value->inRadius; }
static void EvaluateTargetInventory(Inventory*, const BetterLootingDestinations& dest) {
    ++transfers; usedFirst = dest.first; usedSecond = dest.second;
}
"""
code += "\n" + function("FinishBetterLootingRadiusRun")
code += "\n" + function("CancelBetterLootingForWindow")
code += "\n" + function("ProcessBetterLootingRadiusRun")
code += r"""
static void Arm(Character& actor, InventoryGUI& window, RootObject& victim) {
    g_radialLootQueue.clear();
    BetterLootingRadiusTarget target; target.owner = hand(&victim); target.type = victim.type; target.distanceSquared = 1;
    g_radialLootQueue.push_back(target); g_radialLootIndex = 0; g_radialLootWaitFrames = 0;
    g_radialLootOwner = hand(&actor); g_radialLootWindowIdentity = reinterpret_cast<uintptr_t>(&window);
    nativeGui.actor = &actor; nativeGui.window = &window; g_radialLootActive = true; transfers = 0;
}
int main() {
    Inventory main, bag, replacement, loot;
    Character actor, corpse, other; actor.inventory = &main; corpse.inventory = &loot;
    ContainerItem backpack; backpack.inventory = &bag; actor.backpack = &backpack;
    InventoryGUI window(&actor, &main), otherWindow(&other, &main), reopened(&actor, &main);
    BetterLootingDestinations dest = GetBetterLootingDestinations(&actor);
    CHECK(dest.first == &bag && dest.second == &main);
    g_betterLootingRules.preferBackpack = false;
    dest = GetBetterLootingDestinations(&actor); CHECK(dest.first == &main && dest.second == &bag);
    g_betterLootingRules.preferBackpack = true;
    actor.backpack = NULL; dest = GetBetterLootingDestinations(&actor);
    CHECK(dest.first == &main && dest.second == NULL);
    actor.backpack = &backpack;

    Arm(actor, window, corpse);
    ProcessBetterLootingRadiusRun(&otherWindow);
    CHECK(transfers == 0 && g_radialLootIndex == 0 && g_radialLootActive);
    ProcessBetterLootingRadiusRun(&window);
    CHECK(transfers == 1 && usedFirst == &bag && usedSecond == &main && !g_radialLootActive);

    Arm(actor, window, corpse);
    backpack.inventory = &replacement;
    ProcessBetterLootingRadiusRun(&window);
    CHECK(transfers == 1 && usedFirst == &replacement);

    Arm(actor, window, corpse); actor.backpack = NULL;
    ProcessBetterLootingRadiusRun(&window);
    CHECK(transfers == 1 && usedFirst == &main && usedSecond == NULL);

    Arm(actor, window, corpse); corpse.loaded = false;
    ProcessBetterLootingRadiusRun(&window);
    CHECK(transfers == 0);
    corpse.loaded = true;
    Arm(actor, window, corpse); ++corpse.serial;
    ProcessBetterLootingRadiusRun(&window);
    CHECK(transfers == 0);

    Arm(actor, window, corpse); corpse.eligible = false;
    ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0);
    corpse.eligible = true;
    Arm(actor, window, corpse); corpse.inRadius = false;
    ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0);
    corpse.inRadius = true;

    Arm(actor, window, corpse); window.visible = false;
    ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0 && !g_radialLootActive);
    window.visible = true;
    Arm(actor, window, corpse); nativeGui.window = NULL;
    ProcessBetterLootingRadiusRun(&otherWindow);
    CHECK(transfers == 0 && !g_radialLootActive);
    Arm(actor, window, corpse); nativeGui.window = &reopened;
    ProcessBetterLootingRadiusRun(&reopened);
    CHECK(transfers == 0 && !g_radialLootActive);

    Arm(actor, window, corpse); CancelBetterLootingForWindow(&otherWindow);
    CHECK(g_radialLootActive);
    CancelBetterLootingForWindow(&window); CHECK(!g_radialLootActive);
    nativeGui.window = &window; ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0);

    Arm(actor, window, corpse); actor.loaded = false;
    ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0 && !g_radialLootActive);
    actor.loaded = true;
    Arm(actor, window, corpse); ++actor.serial;
    ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 0 && !g_radialLootActive);

    Building chest; chest.inventory = &loot;
    Arm(actor, window, chest); ProcessBetterLootingRadiusRun(&window); CHECK(transfers == 1);
    puts("PASS: production destination/queue/lifecycle functions; backpack preference, replacement/removal, wrong actor, unloaded/reused/revived/out-of-range targets, close/reopen, unload and building targets");
    return 0;
}
"""
base = root / "tests/_generated"
base.mkdir(exist_ok=True)
fixture = base / "inventory_lifetime_test.cpp"
fixture.write_text(code, encoding="utf-8")
vc = Path(r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC")
sdk = Path(r"C:\Program Files\Microsoft SDKs\Windows\v7.1")
env = os.environ.copy()
env["INCLUDE"] = f'{vc / "include"};{sdk / "Include"}'
env["LIB"] = f'{vc / "lib/amd64"};{sdk / "Lib/x64"}'
env["PATH"] = str(vc / "bin/amd64") + ";" + env["PATH"]
command = [str(vc / "bin/amd64/cl.exe"), "/nologo", "/EHsc", "/MD", "/O2", str(fixture),
           "/Fo" + str(base / "inventory_lifetime_test.obj"), "/Fe" + str(base / "inventory_lifetime_test.exe")]
result = subprocess.run(command, env=env, capture_output=True, text=True)
if result.returncode:
    print(result.stdout, result.stderr)
    raise SystemExit(result.returncode)
raise SystemExit(subprocess.run([str(base / "inventory_lifetime_test.exe")]).returncode)

