#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/InputHandler.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Building.h>
#include <kenshi/util/hand.h>
#include <ois/OISKeyboard.h>
#include <string>
#include <sstream>
#include <Windows.h> // For SEH exception handling

// Track last failed dismantle to prevent rapid retries causing game corruption
static DWORD g_lastFailedDismantleTime = 0;
static const DWORD DISMANTLE_COOLDOWN_MS = 1000; // 1 second cooldown after failure

// Helper function to check internal buildings safely (SEH wrapper)
// Returns true if there are internal buildings or if an error occurs
bool CheckInternalBuildingsSafely(Building* b)
{
    bool hasInternal = false;
    __try
    {
        // Simple integer check, unlikely to crash but good to be safe
        if (b->getNumInternalBuildings() > 0)
        {
            hasInternal = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED in CheckInternalBuildingsSafely");
        hasInternal = true; // Assume unsafe
    }
    return hasInternal;
}

// Helper function that actually performs the unsafe check with C++ objects
bool UnsafeCheckMounted(Building* b)
{
    lektor<Building*> mountedBuildings;
    int numMounted = b->getMountedBuildings(&mountedBuildings);
    return (numMounted > 0);
}

// Helper function to check mounted buildings safely (SEH wrapper)
// Returns true if there are mounted buildings or if an error occurs
bool CheckMountedBuildingsSafely(Building* b)
{
    bool hasMounted = false;
    __try
    {
        // Call the function that uses C++ objects (lektor)
        if (UnsafeCheckMounted(b))
        {
            hasMounted = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED in CheckMountedBuildingsSafely");
        hasMounted = true; // Assume unsafe
    }
    return hasMounted;
}

// Non-SEH helper to perform the actual dismantle
// SIMPLIFIED: Just drop materials and destroy, avoiding the complex dismantle state machine
bool PerformDismantleLogic(Building* b, const hand& sel)
{
    // Drop the materials (returns them to inventory)
    b->dropMats();

    // Destroy the building
    ou->dynamicDestroyBuilding(sel);
    return true;
}

// SEH wrapper to safely dismantle a wall
// Returns true if successful, false if it crashed
bool SafelyDismantleWall(Building* b, const hand& sel)
{
    bool success = false;
    __try
    {
        success = PerformDismantleLogic(b, sel);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED during dismantle! Wall may be connected to problematic structures.");
        success = false;
    }
    return success;
}

// --- Hotkey state (edge detect) ---
static bool g_prevXDown = false;

static void HandleHotkeyX()
{
    // InputHandler is a global in Globals.h named "key"
    if (!key || !ou || !ou->player)
        return;

    // Access keyboard directly using OIS key codes
    // KC_X = 0x2D from OISKeyboard.h
    bool xDown = false;
    if (key->keyboard) {
        xDown = key->keyboard->isKeyDown(OIS::KC_X);
    }
    
    const bool pressedThisFrame = (xDown && !g_prevXDown);
    g_prevXDown = xDown;

    if (!pressedThisFrame)
        return;

    // selectedObject is a "hand" (per PlayerInterface.h)
    const hand& sel = ou->player->selectedObject;

    // First, try a building (a wall is a building)
    Building* b = sel.getBuilding();
    if (b)
    {
        // Building.h has virtual isAWall() -> WallBuilding*
        if (b->isAWall())
        {
            // Check if building can be dismantled
            if (b->canDismantle())
            {
                bool hasAttachedBuildings = false;
                
                // 1. Check internal buildings
                if (CheckInternalBuildingsSafely(b))
                {
                    hasAttachedBuildings = true;
                }

                // 2. Check mounted buildings if internal check passed
                if (!hasAttachedBuildings)
                {
                    if (CheckMountedBuildingsSafely(b))
                    {
                        hasAttachedBuildings = true;
                    }
                }
                
                if (hasAttachedBuildings)
                {
                    return;
                }
                
                // Check cooldown after failed dismantle
                DWORD currentTime = GetTickCount();
                if (g_lastFailedDismantleTime != 0)
                {
                    DWORD timeSinceLastFailure = currentTime - g_lastFailedDismantleTime;
                    if (timeSinceLastFailure < DISMANTLE_COOLDOWN_MS)
                    {
                        DebugLog("Hotkey X: Dismantle cooldown active - wait before trying again");
                        return;
                    }
                }
                
                __try
                {
                    // Check if wall is complete before dismantling
                    Building::ConstructionState* buildState = b->getBuildState();
                    
                    if (buildState && !buildState->isComplete)
                    {
                    }
                    else
                    {
                        // Use safe wrapper for dismantle
                        bool dismantleResult = SafelyDismantleWall(b, sel);
                        
                        if (!dismantleResult)
                        {
                            DebugLog("Hotkey X: Dismantle failed - wall may be connected to problematic structures");
                            // Set cooldown timer to prevent rapid retries that corrupt game state
                            g_lastFailedDismantleTime = GetTickCount();
                        }
                        else
                        {
                            // Success - clear cooldown
                            g_lastFailedDismantleTime = 0;
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    DebugLog("Hotkey X: CRASH AVERTED in outer dismantle wrapper!");
                    // Set cooldown timer to prevent rapid retries that corrupt game state
                    g_lastFailedDismantleTime = GetTickCount();
                }
            }
            else
            {
                DebugLog("Hotkey X: Wall cannot be dismantled");
            }
        }

        return;
    }

    // Not a building. Check whether any RootObject is selected
    RootObject* ro = sel.getRootObject();
    if (ro)
    {
        return;
    }
}

// --- Hook: PlayerInterface::updateUT (runs every frame) ---
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    // Call original
    PlayerInterface_updateUT_orig(thisptr);

    // Then run our hotkey logic
    HandleHotkeyX();
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Wall-B-Gone: startPlugin()");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig
    ))
    {
        ErrorLog("Wall-B-Gone: Could not hook PlayerInterface::updateUT");
        return;
    }

    DebugLog("Wall-B-Gone: Hooked PlayerInterface::updateUT (Hotkey X logger enabled)");
}
