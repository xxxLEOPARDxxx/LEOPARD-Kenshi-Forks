#pragma once

#include "vs_runtime_state.h"

namespace vs_character_tint
{

void SyncKoCharacterTint(RuntimeStateView& state, const char* pluginName);
void ClearKoCharacterTint(RuntimeStateView& state, const char* pluginName);

} // namespace vs_character_tint
