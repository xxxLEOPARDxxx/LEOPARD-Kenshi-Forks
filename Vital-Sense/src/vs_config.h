#pragma once

#include "vs_runtime_state.h"

namespace vs_config
{

bool LoadConfigState(RuntimeStateView& state, const char* pluginName);
bool SaveConfigState(const RuntimeStateView& state, const char* pluginName);

} // namespace vs_config
