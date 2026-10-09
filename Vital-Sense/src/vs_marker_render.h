#pragma once

#include "vs_runtime_state.h"

namespace vs_marker_render
{

bool EnsureProjectionUtility(RuntimeStateView& state);
void HideAllKoMarkerWidgets(RuntimeStateView& state, const char* pluginName);
void TickKoMarkerRender(RuntimeStateView& state, const char* pluginName);

} // namespace vs_marker_render
