#define KLOC_DOMAIN "kenshi_rotate"
#include <PluginSettings.h>
#include <PluginDiagnostics.h>
#include <Localization.h>
#include <ModConfigMenu.h>
#include <HoldKey.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <mygui/MyGUI_InputManager.h>
#include "Settings.h"

namespace
{
    bool g_enabled = true;
    BindType g_types[2] = { BIND_MIDDLE_MOUSE, BIND_NONE };
    MCM_Key g_keys[2] = { { -1, 0 }, { -1, 0 } };
    void Load()
    {
        const std::string p = PluginSettings::Path();
        g_enabled = PluginSettings::Bool("KenshiRotate", "Enabled", true, p);
        PluginDiagnostics::Configure("KenshiRotate", p);
        PluginDiagnostics::Write(std::string("KenshiRotate: settings enabled=") + (g_enabled ? "1" : "0"));
        for (int i = 0; i < 2; ++i)
        {
            const std::string n = i ? "2" : "1";
            const std::string mode = PluginSettings::Text("KenshiRotate", ("Mode" + n).c_str(), i ? "NONE" : "MOUSE", p);
            g_types[i] = mode == "MOUSE" ? BIND_MIDDLE_MOUSE : mode == "KEYBOARD" ? BIND_KEYBOARD : BIND_NONE;
            g_keys[i].keycode = GetPrivateProfileIntA("KenshiRotate", ("OisKey" + n).c_str(), -1, p.c_str());
            g_keys[i].modifiers = GetPrivateProfileIntA("KenshiRotate", ("Modifiers" + n).c_str(), 0, p.c_str()) & 7;
        }
    }
    int GetKey(void* ud, MCM_Key* out) { *out = g_keys[(size_t)ud]; return 0; }
    int SetKey(void* ud, MCM_Key value, char*, unsigned)
    {
        const int i = (int)(size_t)ud;
        if (value.keycode < -1 || value.keycode > 0xED) return 1;
        const std::string n = i ? "2" : "1", p = PluginSettings::Path();
        char number[24];
        sprintf_s(number, "%d", value.keycode);
        WritePrivateProfileStringA("KenshiRotate", ("OisKey" + n).c_str(), number, p.c_str());
        sprintf_s(number, "%u", value.modifiers & 7);
        WritePrivateProfileStringA("KenshiRotate", ("Modifiers" + n).c_str(), number, p.c_str());
        WritePrivateProfileStringA("KenshiRotate", ("Mode" + n).c_str(), value.keycode < 0 ? "NONE" : "KEYBOARD", p.c_str());
        Load();
        return 0;
    }
    bool Down(int i)
    {
        if (g_types[i] == BIND_MIDDLE_MOUSE) return HoldKey::Down(VK_MBUTTON);
        if (g_types[i] != BIND_KEYBOARD || g_keys[i].keycode < 0 || key == NULL || key->keyboard == NULL) return false;
        const unsigned m = g_keys[i].modifiers;
        return key->keyboard->isKeyDown((OIS::KeyCode)g_keys[i].keycode) &&
            (!(m & MCM_KEY_CTRL) || HoldKey::Down(VK_CONTROL)) &&
            (!(m & MCM_KEY_SHIFT) || HoldKey::Down(VK_SHIFT)) &&
            (!(m & MCM_KEY_ALT) || HoldKey::Down(VK_MENU));
    }
}
namespace RotateSettings
{
    void Init() { Load(); }
    bool TriggerDown()
    {
        if (!g_enabled || (!Down(0) && !Down(1))) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        if (pid != GetCurrentProcessId()) return false;
        MyGUI::InputManager* const input = MyGUI::InputManager::getInstancePtr();
        if (input != NULL && input->getKeyFocusWidget() != NULL) return false;
        return true;
    }
    bool IsCapturing() { return false; }
    void InjectModsTabUI() {}
    void ProcessCapture() {}
    BindType GetBindType() { return g_types[0]; }
    BindType GetBindType2() { return g_types[1]; }
    OIS::KeyCode GetKeyCode() { return (OIS::KeyCode)g_keys[0].keycode; }
    OIS::KeyCode GetKeyCode2() { return (OIS::KeyCode)g_keys[1].keycode; }
}
extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string p = PluginSettings::Path();
    api->beginMod(api, "KenshiRotate", "Rotate Items", p.c_str(), Load);
    if (api->version >= 2) api->info(api, Tr("Rotate inventory items with middle mouse or a keyboard shortcut. Existing rotations stay save-compatible."));
    api->toggle(api, "KenshiRotate", "Enabled", Tr("Enable rotation input"), Tr("Disables new rotations only. Existing rotations keep their appearance and save correctly."), 1, 0);
    const char* modes[] = { "MOUSE", "KEYBOARD", "NONE" };
    const char* labels[] = { Tr("Middle mouse"), Tr("Keyboard"), Tr("Disabled") };
    api->choice(api, "KenshiRotate", "Mode1", Tr("Primary input"), Tr("Choose middle mouse, a keyboard shortcut, or disable this input."), "MOUSE", modes, labels, 3, 0);
    if (api->version >= 3) api->hotkeyFn(api, Tr("Primary keyboard shortcut"), Tr("Assigning a key switches the primary input to keyboard."), GetKey, SetKey, (void*)0, 0);
    api->choice(api, "KenshiRotate", "Mode2", Tr("Secondary input"), Tr("An optional second way to rotate an item."), "NONE", modes, labels, 3, 0);
    if (api->version >= 3) api->hotkeyFn(api, Tr("Secondary keyboard shortcut"), Tr("Assigning a key switches the secondary input to keyboard."), GetKey, SetKey, (void*)1, 0);
    api->toggle(api, "KenshiRotate", "Debug", Tr("Detailed log"), Tr("Details in RE_Kenshi_log.txt: input settings, rotation attempts and results."), 0, 0);
}
