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
    DebugLog("Hotkey X: Dropping materials");
    
    // Drop the materials (returns them to inventory)
    b->dropMats();
    
    DebugLog("Hotkey X: Materials dropped, destroying building");
    
    // Destroy the building
    ou->dynamicDestroyBuilding(sel);
    
    DebugLog("Hotkey X: Building destroyed successfully");
    return true;
}

// SEH wrapper to safely dismantle a wall
// Returns true if successful, false if it crashed
bool SafelyDismantleWall(Building* b, const hand& sel)
{
    DebugLog("Hotkey X: Entered SafelyDismantleWall");
    bool success = false;
    __try
    {
        DebugLog("Hotkey X: Inside __try block, calling PerformDismantleLogic");
        success = PerformDismantleLogic(b, sel);
        DebugLog("Hotkey X: PerformDismantleLogic returned");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED during dismantle! Wall may be connected to problematic structures.");
        success = false;
    }
    DebugLog("Hotkey X: Exiting SafelyDismantleWall");
    return success;
}

// --- Hotkey state (edge detect) ---
static bool g_prevXDown = false;

static void HandleHotkeyX()
{
    // InputHandler je globál v Globals.h ako "key"
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

    DebugLog("Hotkey X pressed");

    // selectedObject je "hand" (vieme z PlayerInterface.h)
    const hand& sel = ou->player->selectedObject;

    // Najprv skúsime building (wall je building)
    Building* b = sel.getBuilding();
    if (b)
    {
        // Building.h má virtual isAWall() -> WallBuilding*
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
                    DebugLog("Hotkey X: Cannot dismantle - has internal buildings");
                }

                // 2. Check mounted buildings if internal check passed
                if (!hasAttachedBuildings)
                {
                    if (CheckMountedBuildingsSafely(b))
                    {
                        hasAttachedBuildings = true;
                        DebugLog("Hotkey X: Cannot dismantle - has mounted buildings (e.g. harpoons)");
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
                
                DebugLog("Hotkey X: Dismantling wall (returning materials)");
                
                __try
                {
                    // Check if wall is complete before dismantling
                    Building::ConstructionState* buildState = b->getBuildState();
                    DebugLog("Hotkey X: getBuildState() returned");
                    
                    if (buildState && !buildState->isComplete)
                    {
                        DebugLog("Hotkey X: Wall is not complete, cannot dismantle");
                    }
                    else
                    {
                        DebugLog("Hotkey X: Wall is complete, starting dismantle");
                        DebugLog("Hotkey X: About to call SafelyDismantleWall");
                        
                        // Use safe wrapper for dismantle
                        bool dismantleResult = SafelyDismantleWall(b, sel);
                        DebugLog("Hotkey X: SafelyDismantleWall returned");
                        
                        if (!dismantleResult)
                        {
                            DebugLog("Hotkey X: Dismantle failed - wall may be connected to problematic structures");
                            // Set cooldown timer to prevent rapid retries that corrupt game state
                            g_lastFailedDismantleTime = GetTickCount();
                            DebugLog("Hotkey X: Cooldown activated - wait 1 second before next dismantle");
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
                    DebugLog("Hotkey X: Cooldown activated - wait 1 second before next dismantle");
                }
            }
            else
            {
                DebugLog("Hotkey X: Wall cannot be dismantled");
            }
        }
        else
        {
            DebugLog("Hotkey X: selected object is NOT wall (building)");
        }

        return;
    }

    // Nie je to building. Skúsime či vôbec niečo je vybraté (RootObject)
    RootObject* ro = sel.getRootObject();
    if (ro)
    {
        DebugLog("Hotkey X: selected object is NOT wall (non-building)");
        return;
    }

    // Nič nie je vybraté
    DebugLog("Hotkey X: NO selection");
}

// --- Hook: PlayerInterface::updateUT (runs every frame) ---
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    // zavolaj originál
    PlayerInterface_updateUT_orig(thisptr);

    // potom naša hotkey logika
    HandleHotkeyX();
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("HelloWorld: startPlugin()");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig
    ))
    {
        ErrorLog("HelloWorld: Could not hook PlayerInterface::updateUT");
        return;
    }

    DebugLog("HelloWorld: Hooked PlayerInterface::updateUT (Hotkey X logger enabled)");
}
