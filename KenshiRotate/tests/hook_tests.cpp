#include <cstdio>
#include <string>
#include <stdexcept>
#include "PairTracePolicy.h"

namespace MyGUI
{
    namespace types { template<class T> struct TPoint { T left, top; }; }
    struct Widget {};
    struct MouseButton
    {
        enum Enum { Left, Right };
        MouseButton(Enum value) : value(value) {}
        bool operator==(Enum other) const { return value == other; }
        Enum value;
    };
}
struct InventoryItemBase
{
    bool rotated;
    InventoryItemBase() : rotated(false) {}
};
struct Item : InventoryItemBase {};
struct InventoryGUI {};
struct InventorySection {};

static Item target, source;
static bool debug = false, ui = true, nativeCompatible = true, placeThrows = false;
static int nativeCalls = 0, rotationReads = 0, logs = 0, scopedCalls = 0, quantityCalls = 0;
static bool recordedNative = false, recordedResult = false, placedPairAccepted = false;

namespace PluginDiagnostics { static bool Enabled() { return debug; } }
namespace RotationState { static bool IsRotated(Item* item) { ++rotationReads; return item->rotated; } }
namespace CursorState
{
    static Item* held = NULL;
    static Item* GetItem() { return held; }
    static void Clear() { held = NULL; }
}

static void RecordStackPair(InventoryItemBase*, InventoryItemBase*, bool native, bool result, void*)
{
    if (!ui) return;
    ++logs;
    recordedNative = native;
    recordedResult = result;
}
static void* ReturnSite() { return NULL; }
#define _ReturnAddress ReturnSite
static PairTrace::Context s_pairPlaceContext;
static bool PairTraceOnUIThread() { return debug && ui; }
static void (*sectionMouseButtonReleased_orig)(InventoryGUI*, MyGUI::Widget*, int, int, MyGUI::MouseButton);
static bool (*placeItemFromMouse_orig)(InventoryGUI*, const std::string, const MyGUI::types::TPoint<int>&);

#include "transfer_policy_under_test.h"
#include "stack_hooks_under_test.h"
#include "place_hook_under_test.h"

static bool NativePair(InventoryItemBase*, InventoryItemBase*)
{
    ++nativeCalls;
    return nativeCompatible;
}

static void NativeQuantity(InventoryItemBase*, int* amount, Item*, InventorySection*)
{
    ++quantityCalls;
    *amount = 0;
}

static bool NativePlace(InventoryGUI* gui, const std::string, const MyGUI::types::TPoint<int>&)
{
    if (s_pairPlaceContext.gui == gui) ++scopedCalls;
    placedPairAccepted = canStackWith_hook(&target, &source);
    int remaining = 1;
    // The native drag fallback calls addQuantity even after a compatibility rejection.
    addQuantity_hook(&target, &remaining, &source, NULL);
    if (placeThrows) throw std::runtime_error("place");
    return true;
}

static void NativeRelease(InventoryGUI* gui, MyGUI::Widget*, int, int, MyGUI::MouseButton)
{
    const MyGUI::types::TPoint<int> pos = {0, 0};
    placeItemFromMouse_hook(gui, "bag", pos);
}

static void Reset()
{
    debug = placeThrows = false;
    ui = nativeCompatible = g_manualPlacementHookOK = true;
    nativeCalls = rotationReads = logs = scopedCalls = quantityCalls = 0;
    g_inQuickTransfer = g_inManualPlacement = false;
    target.rotated = false;
    source.rotated = true;
    CursorState::held = &source;
}

static int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL hook %d: %s\n", __LINE__, #x); } } while (0)

int main()
{
    canStackWith_orig = NativePair;
    addQuantity_orig = NativeQuantity;
    placeItemFromMouse_orig = NativePlace;
    sectionMouseButtonReleased_orig = NativeRelease;
    InventoryGUI gui;
    const MyGUI::types::TPoint<int> pos = {0, 0};

    Reset();
    int remaining = 1;
    CHECK(canStackWith_hook(&target, &source));
    addQuantity_hook(&target, &remaining, &source, NULL);
    CHECK(nativeCalls == 1 && rotationReads == 0 && quantityCalls == 1 && remaining == 0);
    CHECK(!target.rotated && source.rotated);

    Reset();
    debug = true;
    nativeCompatible = false;
    CHECK(!canStackWith_hook(&target, &source));
    CHECK(nativeCalls == 1 && rotationReads == 0 && logs == 1 && !recordedNative && !recordedResult);
    CHECK(!canStackWith_hook(NULL, &source));
    CHECK(nativeCalls == 1 && logs == 1);

    Reset();
    CHECK(placeItemFromMouse_hook(&gui, "bag", pos));
    CHECK(!placedPairAccepted && quantityCalls == 0 && rotationReads == 4);
    CHECK(scopedCalls == 0 && !g_inManualPlacement && CursorState::held == NULL);

    Reset();
    debug = true;
    source.rotated = false;
    CHECK(placeItemFromMouse_hook(&gui, "bag", pos));
    CHECK(placedPairAccepted && quantityCalls == 1 && scopedCalls == 1);
    CHECK(logs == 1 && recordedNative && recordedResult);
    CHECK(!g_inManualPlacement && s_pairPlaceContext.gui == NULL);

    Reset();
    debug = true;
    ui = false;
    CHECK(placeItemFromMouse_hook(&gui, "bag", pos));
    CHECK(!placedPairAccepted && quantityCalls == 0 && logs == 0);

    Reset();
    sectionMouseButtonReleased_hook(&gui, NULL, 0, 0, MyGUI::MouseButton::Right);
    CHECK(placedPairAccepted && quantityCalls == 1 && rotationReads == 0);
    CHECK(!g_inManualPlacement && !g_inQuickTransfer);

    Reset();
    sectionMouseButtonReleased_hook(&gui, NULL, 0, 0, MyGUI::MouseButton::Left);
    CHECK(!placedPairAccepted && quantityCalls == 0);

    Reset();
    debug = placeThrows = true;
    try { sectionMouseButtonReleased_hook(&gui, NULL, 0, 0, MyGUI::MouseButton::Right); }
    catch (const std::runtime_error&) {}
    CHECK(scopedCalls == 1 && s_pairPlaceContext.gui == NULL);
    CHECK(!g_inManualPlacement && !g_inQuickTransfer);

    Reset();
    g_manualPlacementHookOK = false;
    CHECK(!canStackWith_hook(&target, &source));
    addQuantity_hook(&target, &remaining, &source, NULL);
    CHECK(quantityCalls == 0 && rotationReads == 4);

    printf("%d extracted production hook checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
