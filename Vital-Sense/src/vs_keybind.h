#pragma once

#include "vs_types.h"

#include <string>

namespace vs_keybind
{

extern const int32_t kKeyCodeUnbound;
extern const int32_t kDefaultHighlightKeyCode;

bool TryParseKeyCode(const std::string& rawValue, int32_t* keyCodeOut);
bool ValidatePrimaryKeyCode(int32_t keyCode, std::string* reasonOut);
std::string KeyCodeToConfigString(int32_t keyCode);
std::string FormatKeybind(const PluginConfig& config);
bool IsHighlightGateOpen(const PluginConfig& config);

} // namespace vs_keybind
