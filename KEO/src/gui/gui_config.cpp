// gui_config.cpp - defaults and INI rows for the gui module.
#include "gui/gui_config.h"
#include "base/config_rows.h"
#include "base/ini_text.h"
#include <cstddef>
#include <string.h>

namespace keo_gui {

const GuiConfig kGuiDefaults =
{
	true, // settingsPanelEnabled
	false, // debugLogging
};

GuiConfig g_guiCfg = kGuiDefaults;
} // namespace keo_gui

namespace gui_config_detail {
union GuiConfigPodCheck { keo_gui::GuiConfig s; };
} // namespace gui_config_detail
using namespace gui_config_detail;

namespace keo_gui {

const ConfigKey g_guiConfigKeys[] =
{
	CFG_OBOOL("settingsPanel", GuiConfig, settingsPanelEnabled,         DOC, DEVROW,
	  "KEO settings tab",
	  "This tab in the game's Options window."),
	CFG_OBOOL_LIVE("Debug", GuiConfig, debugLogging, DOC, SHOW,
	  "Detailed log", "Write detailed diagnostics to KEO.log. Errors are always logged."),
	{ NULL, CK_BOOL, 0, 0, 0.0f, 0.0f, false, NULL, NULL, false, 0.0f, 0, NULL, INT_MIN, false, false, false, NULL, NULL, NULL, NULL, 0 }
};

} // namespace keo_gui
