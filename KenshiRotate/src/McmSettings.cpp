#define KLOC_DOMAIN "kenshi_rotate"
#include <PluginSettings.h>
#include <PluginDiagnostics.h>
#include <Localization.h>
#include <ModConfigMenu.h>
#include <HoldKey.h>
#include <kenshi/gui/OptionsWindow.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_EditBox.h>
#include "Settings.h"

namespace
{
    bool g_enabled = true;
    HoldKey::Key g_rotationKey;

    std::string RawSettingsPath()
    {
        char path[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameA(reinterpret_cast<HMODULE>(&__ImageBase), path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return std::string();
        const std::string file(path, n);
        const size_t dot = file.rfind('.');
        return dot == std::string::npos ? std::string() : file.substr(0, dot) + ".ini";
    }

    std::string LegacyBinding(const std::string& path)
    {
        for (int i = 0; i < 2; ++i)
        {
            const std::string suffix = i == 0 ? "1" : "2";
            const std::string mode = PluginSettings::Text("KenshiRotate", ("Mode" + suffix).c_str(),
                                                         i == 0 ? "MOUSE" : "NONE", path);
            if (!_stricmp(mode.c_str(), "MOUSE")) return "MMB";
            if (_stricmp(mode.c_str(), "KEYBOARD")) continue;
            const int scan = GetPrivateProfileIntA("KenshiRotate", ("OisKey" + suffix).c_str(), -1, path.c_str());
            if (scan <= 0 || scan > 0xED) continue;
            // OIS/DirectInput encodes extended scan codes with bit7. Win32
            // MapVirtualKey expects the E0 prefix instead.
            const UINT winScan = (scan & 0x80) ? (0xE000u | (scan & 0x7F)) : static_cast<UINT>(scan);
            HoldKey::Key key;
            key.vk = MapVirtualKeyA(winScan, MAPVK_VSC_TO_VK_EX);
            const unsigned mods = GetPrivateProfileIntA("KenshiRotate", ("Modifiers" + suffix).c_str(), 0, path.c_str());
            key.ctrl = (mods & MCM_KEY_CTRL) != 0;
            key.shift = (mods & MCM_KEY_SHIFT) != 0;
            key.alt = (mods & MCM_KEY_ALT) != 0;
            const std::string binding = HoldKey::Format(key);
            if (binding != "NONE") return binding;
        }
        return "NONE";
    }

    void Load()
    {
        const std::string raw = RawSettingsPath();
        const char* const missing = "__MCM_ROTATION_KEY_MISSING__";
        std::string binding = PluginSettings::Text("KenshiRotate", "RotationKey", missing, raw);
        const bool migrate = !raw.empty() && IniDefaults::Exists(raw) && binding == missing;
        if (migrate)
        {
            // Before Path/Prepare can append the new defaults, preserve the
            // first active old input. Retain every old INI key for recovery.
            binding = LegacyBinding(raw);
            if (!WritePrivateProfileStringA("KenshiRotate", "RotationKey", binding.c_str(), raw.c_str()))
                ErrorLog("KenshiRotate: cannot save migrated RotationKey");
        }
        const std::string path = PluginSettings::Path();
        if (!migrate) binding = PluginSettings::Text("KenshiRotate", "RotationKey", "MMB", path);
        g_rotationKey = HoldKey::Parse(binding);
        g_enabled = PluginSettings::Bool("KenshiRotate", "Enabled", true, path);
        PluginDiagnostics::Configure("KenshiRotate", path);
        PluginDiagnostics::Write(std::string("KenshiRotate: settings enabled=") + (g_enabled ? "1" : "0") +
                                 " RotationKey=" + binding);
    }
}

namespace RotateSettings
{
    void Init() { Load(); }
    bool TriggerDown()
    {
        // HoldKey NONE means always-held for visibility consumers. Rotation
        // explicitly treats it as disabled, so it never becomes an auto-repeat.
        if (!g_enabled || g_rotationKey.vk == 0 || !HoldKey::Held(g_rotationKey)) return false;
        OptionsWindow* const options = OptionsWindow::getSingleton();
        if (options != NULL && options->isVisible()) return false; // assigning a binding is not a rotation
        MyGUI::InputManager* const input = MyGUI::InputManager::getInstancePtr();
        // Inventory focus is valid; an editable field, including its skin
        // children, owns text input and suppresses rotation.
        for (MyGUI::Widget* focus = input ? input->getKeyFocusWidget() : NULL;
             focus != NULL; focus = focus->getParent())
        {
            MyGUI::EditBox* const edit = focus->castType<MyGUI::EditBox>(false);
            if (edit != NULL && !edit->getEditReadOnly()) return false;
        }
        return true;
    }
}

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string path = PluginSettings::Path();
    api->beginMod(api, "KenshiRotate", "Rotate Items", path.c_str(), Load);
    if (api->version >= 2) api->info(api, Tr("Rotate inventory items with one keyboard or mouse binding. Existing rotations stay save-compatible."));
    api->toggle(api, "KenshiRotate", "Enabled", Tr("Enable rotation input"), Tr("Disables new rotations only. Existing rotations keep their appearance and save correctly."), 1, 0);
    api->hotkey(api, "KenshiRotate", "RotationKey", Tr("Rotation binding"), Tr("Press one key or mouse button, with optional CTRL, SHIFT or ALT. NONE disables rotation input."), "MMB", MCM_ALLOW_MOUSE);
    api->toggle(api, "KenshiRotate", "Debug", Tr("Detailed log"), Tr("Details in RE_Kenshi_log.txt: input settings, rotation attempts and results."), 0, 0);
}
