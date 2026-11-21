#pragma once
#include <string>

struct MCM_Api;

namespace BetterLootingPresetPanel
{
    void Register(MCM_Api* api, const std::string& ini, void (__cdecl *reload)());
    void UpdateStatus();
}
