#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/InputHandler.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Building.h>
#include <kenshi/Kenshi.h>
#include <kenshi/TitleScreen.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>
#include <ois/OISKeyboard.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_Widget.h>
#include <string>
#include <sstream>
#include <Windows.h>
#include <fstream>

// Track last failed dismantle to prevent rapid retries causing game corruption
static DWORD g_lastFailedDismantleTime = 0;
static const DWORD DISMANTLE_COOLDOWN_MS = 1000;

// --- Hotkey state (edge detect) ---
static bool g_prevXDown = false;

// Runtime toggle persisted to JSON and exposed in Plugins menu.
static bool g_modEnabled = true;
static std::string g_settingsPath;
static const char* kWallBGoneTabName = "Wall-B-Gone";
static const char* kWallBGonePanelName = "wall_b_gone_options";
static const int kWallBGonePanelLineId = 0x574247;

class ToolTip;
class ForgottenGUI;
class DatapanelGUI;
class DataPanelLine;
class DataPanelLine_CheckBox;
class DataPanelLine_KeyConfig;

class OptionsWindow : public GUIWindow, public wraps::BaseLayout
{
public:
    char _0xd0;
    lektor<std::string> _0xd8;
    int _0xf0;
    DataPanelLine_KeyConfig* _0xf8;
    DatapanelGUI* datapanel;
    MyGUI::TabControl* optionsTab;
    bool _0x110;
    ToolTip* tooltip;
    bool _0x120;
};

class DatapanelGUI : public GUIWindow
{
public:
    virtual ~DatapanelGUI() {}
    virtual void vfunc0x68() {}
    virtual void vfunc0x70() {}
    virtual void vfunc0x78() {}
    virtual void vfunc0x80() {}
    virtual void vfunc0x88() {}
    virtual void vfunc0x90() {}
    virtual void vfunc0x98() {}
    virtual void vfunc0xa0() {}
    virtual void vfunc0xa8() {}
    virtual void vfunc0xb0() {}
    virtual void vfunc0xb8() {}
    virtual void vfunc0xc0(int) {}
    virtual void vfunc0xc8() {}
    virtual void vfunc0xd0() {}
    virtual void vfunc0xd8() {}
    virtual void vfunc0xe0(float) {}
    virtual void vfunc0xe8() {}
    virtual void vfunc0xf0() {}
};

class DataPanelLine : public Ogre::GeneralAllocatedObject
{
public:
    virtual ~DataPanelLine() {}
    virtual void setVisible(bool) {}
    virtual void setEnabled(bool) {}
    virtual void chenge() {}
    virtual void input(float&) {}
    virtual void setTooltip(const std::string&, ToolTip*) {}
};

class DataPanelLine_CheckBox : public DataPanelLine
{
public:
    bool* valuePtr;
};

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef DataPanelLine* (*FnCreateHeaderLine)(DatapanelGUI*, const std::string&, const std::string&, int, bool, bool);
typedef DataPanelLine_CheckBox* (*FnCreateCheckboxLine)(DatapanelGUI*, const std::string&, bool&, int);
typedef void (*FnOptionsInit)(OptionsWindow*);
typedef void (*FnOptionsSave)(OptionsWindow*);

static FnCreateDatapanel g_fnCreateDatapanel = 0;
static FnCreateHeaderLine g_fnCreateHeaderLine = 0;
static FnCreateCheckboxLine g_fnCreateCheckboxLine = 0;
static FnOptionsInit g_fnOptionsInit = 0;
static FnOptionsSave g_fnOptionsSave = 0;
static FnOptionsInit g_fnOptionsInitOrig = 0;
static FnOptionsSave g_fnOptionsSaveOrig = 0;
static ForgottenGUI* g_ptrKenshiGUI = 0;

static bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    if (platform == 1)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC950);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FDE20);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FE210);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003ECA90);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FE9F0);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FEDE0);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021337B0);
            return true;
        }
    }
    else if (platform == 0)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFD40);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC570);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FD780);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FDB70);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC430);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FE390);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FE780);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021326E0);
            return true;
        }
    }

    return false;
}

static bool ReadEnabledFromFile(const std::string& configPath, bool* foundValue)
{
    std::ifstream in(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        return true;
    }

    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const size_t keyPos = body.find("\"enabled\"");
    if (keyPos == std::string::npos)
    {
        return true;
    }

    const size_t truePos = body.find("true", keyPos);
    const size_t falsePos = body.find("false", keyPos);

    *foundValue = true;
    if (falsePos != std::string::npos && (truePos == std::string::npos || falsePos < truePos))
    {
        return false;
    }

    return true;
}

static bool SaveEnabledToFile(const std::string& configPath, bool enabled)
{
    std::ofstream out(configPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return false;
    }

    out << "{\n  \"enabled\": " << (enabled ? "true" : "false") << "\n}\n";
    return true;
}

static bool LoadEnabledFromConfig()
{
    if (g_settingsPath.empty())
    {
        return true;
    }

    bool foundValue = false;
    const bool enabled = ReadEnabledFromFile(g_settingsPath, &foundValue);
    if (foundValue)
    {
        return enabled;
    }

    return true;
}

static bool SaveEnabledToConfig()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Wall-B-Gone: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveEnabledToFile(g_settingsPath, g_modEnabled))
    {
        ErrorLog("Wall-B-Gone: failed to save mod-config.json");
        return false;
    }

    return true;
}

static void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig)
    {
        g_fnOptionsInitOrig(self);
    }

    if (!self || !self->optionsTab || !g_ptrKenshiGUI || !g_fnCreateDatapanel || !g_fnCreateCheckboxLine)
    {
        return;
    }

    if (self->optionsTab->findItemWith(kWallBGoneTabName))
    {
        return;
    }

    MyGUI::TabItem* pluginOptionTab = self->optionsTab->addItem(kWallBGoneTabName);
    if (!pluginOptionTab)
    {
        ErrorLog("Wall-B-Gone: failed to add Wall-B-Gone tab item");
        return;
    }

    DatapanelGUI* pluginOptionPanel = g_fnCreateDatapanel(g_ptrKenshiGUI, kWallBGonePanelName, pluginOptionTab, true);
    if (!pluginOptionPanel)
    {
        ErrorLog("Wall-B-Gone: failed to create wall_b_gone_options datapanel");
        return;
    }

    const int tabID = kWallBGonePanelLineId;
    pluginOptionPanel->vfunc0xc0(tabID);
    pluginOptionPanel->vfunc0xe0(25.0f);

    if (g_fnCreateHeaderLine)
    {
        g_fnCreateHeaderLine(pluginOptionPanel, "[Wall-B-Gone Settings]", "", tabID, false, true);
    }

    DataPanelLine_CheckBox* toggleLine = g_fnCreateCheckboxLine(pluginOptionPanel, "Enable Wall-B-Gone", g_modEnabled, tabID);
    if (toggleLine && self->tooltip)
    {
        toggleLine->setTooltip("Enable or disable Wall-B-Gone hotkey behavior.", self->tooltip);
    }

    pluginOptionTab->setVisible(false);
    self->optionsTab->setItemData(pluginOptionTab, pluginOptionPanel);
}

static void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig)
    {
        g_fnOptionsSaveOrig(self);
    }
    SaveEnabledToConfig();
}

// Helper function to check internal buildings safely (SEH wrapper)
// Returns true if there are internal buildings or if an error occurs
static bool CheckInternalBuildingsSafely(Building* b)
{
    bool hasInternal = false;
    __try
    {
        if (b->getNumInternalBuildings() > 0)
        {
            hasInternal = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED in CheckInternalBuildingsSafely");
        hasInternal = true;
    }
    return hasInternal;
}

static bool UnsafeCheckMounted(Building* b)
{
    lektor<Building*> mountedBuildings;
    int numMounted = b->getMountedBuildings(&mountedBuildings);
    return (numMounted > 0);
}

static bool CheckMountedBuildingsSafely(Building* b)
{
    bool hasMounted = false;
    __try
    {
        if (UnsafeCheckMounted(b))
        {
            hasMounted = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey X: CRASH AVERTED in CheckMountedBuildingsSafely");
        hasMounted = true;
    }
    return hasMounted;
}

static bool PerformDismantleLogic(Building* b, const hand& sel)
{
    b->dropMats();
    ou->dynamicDestroyBuilding(sel);
    return true;
}

static bool SafelyDismantleWall(Building* b, const hand& sel)
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

static void HandleHotkeyX()
{
    if (!g_modEnabled)
        return;

    if (!key || !ou || !ou->player)
        return;

    bool xDown = false;
    if (key->keyboard)
    {
        xDown = key->keyboard->isKeyDown(OIS::KC_X);
    }

    const bool pressedThisFrame = (xDown && !g_prevXDown);
    g_prevXDown = xDown;

    if (!pressedThisFrame)
        return;

    const hand& sel = ou->player->selectedObject;

    Building* b = sel.getBuilding();
    if (b)
    {
        if (b->isAWall())
        {
            if (b->canDismantle())
            {
                bool hasAttachedBuildings = false;

                if (CheckInternalBuildingsSafely(b))
                {
                    hasAttachedBuildings = true;
                }

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

                DWORD currentTime = GetTickCount();
                if (g_lastFailedDismantleTime != 0)
                {
                    DWORD timeSinceLastFailure = currentTime - g_lastFailedDismantleTime;
                    if (timeSinceLastFailure < DISMANTLE_COOLDOWN_MS)
                    {
                        return;
                    }
                }

                __try
                {
                    Building::ConstructionState* buildState = b->getBuildState();

                    if (buildState && !buildState->isComplete)
                    {
                    }
                    else
                    {
                        bool dismantleResult = SafelyDismantleWall(b, sel);

                        if (!dismantleResult)
                        {
                            DebugLog("Hotkey X: Dismantle failed - wall may be connected to problematic structures");
                            g_lastFailedDismantleTime = GetTickCount();
                        }
                        else
                        {
                            g_lastFailedDismantleTime = 0;
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    DebugLog("Hotkey X: CRASH AVERTED in outer dismantle wrapper!");
                    g_lastFailedDismantleTime = GetTickCount();
                }
            }
            else
            {
            }
        }

        return;
    }

    RootObject* ro = sel.getRootObject();
    if (ro)
    {
        return;
    }
}

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    HandleHotkeyX();
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Wall-B-Gone: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || (version != "1.0.65" && version != "1.0.68"))
    {
        ErrorLog("Wall-B-Gone: unsupported Kenshi version/platform");
        return;
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPluginMenuFunctions(platform, version, baseAddr))
    {
        ErrorLog("Wall-B-Gone: failed to initialize plugin menu function pointers");
        return;
    }

    g_modEnabled = LoadEnabledFromConfig();
    SaveEnabledToConfig();

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
    {
        ErrorLog("Wall-B-Gone: Could not hook options init");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsSave, OptionsWindowSaveHook, &g_fnOptionsSaveOrig))
    {
        ErrorLog("Wall-B-Gone: Could not hook options save");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Wall-B-Gone: Could not hook PlayerInterface::updateUT");
        return;
    }

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            std::string fullPath(dllPath);
            size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string myDirectory = fullPath.substr(0, sep);
                g_settingsPath = myDirectory + "\\mod-config.json";
            }
        }
    }
    return TRUE;
}
