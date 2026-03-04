#pragma once

#include "vs_runtime_state.h"

namespace vs_probe
{

enum ProbeRenderDirective
{
    PROBE_RENDER_NONE = 0,
    PROBE_RENDER_HIDE_ALL = 1,
    PROBE_RENDER_TICK = 2
};

ProbeRenderDirective TickKoProbe(RuntimeStateView& state, const char* pluginName);

} // namespace vs_probe
