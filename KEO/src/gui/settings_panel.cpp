#include "game/native_profile.h"
#include "base/log_admission.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gui/settings_panel.h"
#include "gui/settings_rows.h"
#include "gui/requested_settings.h"
#include "gui/mcm_bridge.h"
#include "gui/bench_buttons.h"
#include "render/render_config.h"
#include "render/render_keys.h"
#include "bench/bench_group.h"
#include "bench/bench_lever_ab.h"
#include "bench/bench_runner.h"
#include "bench/bench_sweep.h"
#include "base/config.h"
#include "game/game.h"
#include "base/core.h"
#include "zone/zone_config.h"
#include "base/keo_tr.h"   // LEOPARD fork: the tab in the game's language
#include <stddef.h>
#include <string.h>
#include "base/klib_include.h"
#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_Delegate.h>
#include "base/klib_include_end.h"

static_assert(offsetof(OptionsWindow, tabs) == 0x108, "OptionsWindow::tabs");
static_assert(offsetof(OptionsWindow, tooltip) == 0x118, "OptionsWindow::tooltip");
static_assert(offsetof(DatapanelGUI, currentCategory) == 0xB0, "DatapanelGUI::currentCategory");
static_assert(offsetof(DatapanelGUI, basicSpacing) == 0x130, "DatapanelGUI::basicSpacing");
static_assert(offsetof(DataPanelLine, callback) == 0x20, "DataPanelLine::callback");
static_assert(offsetof(DataPanelLine_Button, button) == 0x140, "DataPanelLine_Button::button");
static_assert(offsetof(DataPanelLine_DropBox, listBox) == 0x120, "DataPanelLine_DropBox::listBox");

#ifdef KEO_DEBUG
static const bool DEV_BUILD = true;
#else
static const bool DEV_BUILD = false;
#endif

typedef void (*OptionsFn_t)(OptionsWindow* win);

static OptionsFn_t     s_origCreate = NULL;
static OptionsFn_t     s_origSave   = NULL;
static bool            s_active     = false;   // both hooks installed
static bool            s_staged     = false;   // a tab was built since the last close
static SettingsStaging s_staging;              // what the rows write
static SettingsStaging s_saved;                // what KEO.ini holds
static const char*     s_token      = "off(gate)";   // until InstallSettingsPanel runs
static bool            s_buttonsOk  = false;   // the button calls' addresses match KenshiLib's
static bool            s_groupsOk   = false;   // a checkbox line's toggle invokes its callback (checked at install)

namespace settings_panel_detail {
// A greyed row's line and the checkbox line whose staged value enables it; rebuilt on every
// create and dropped at every close, as the Benchmark buttons are.
struct GatedLine
{
	DataPanelLine* member;
	DataPanelLine* head;
	const bool*    value;
};
} // namespace settings_panel_detail
using namespace settings_panel_detail;

static const int MAX_GATED = 8;
static GatedLine s_gated[MAX_GATED];
static int       s_gatedCount = 0;
static const unsigned char kCheckboxToggleCallback[18] =
	{ 0x48,0x8B,0x4B,0x20,0x48,0x85,0xC9,0x74,0x09,0x48,0x8B,0x01,0x48,0x8B,0xD3,0xFF,0x50,0x10 };

// The Benchmark section's button lines of the tab being shown, rebuilt on
// every create. hide() destroys the lines, so a pointer here is only ever
// compared with the line a press hands in, or used during that press.
struct ButtonLine
{
	DataPanelLine_Button* line;
	int                   id;
};
static const int  MAX_BUTTONS = 2 * BENCH_SLOT_COUNT + 1;
static ButtonLine s_buttons[MAX_BUTTONS];
static int        s_buttonCount = 0;

static const float BUTTON_WIDTH = 0.4f;

static const int PREFERRED_CATEGORY = 48;

// A category no other tab's panel uses.
static int PickCategory(MyGUI::TabControl* tabs, size_t count)
{
	int maxCat = 0;
	bool clash = false;
	for (size_t i = 0; i < count; ++i)
	{
		DatapanelGUI** p = tabs->getItemDataAt<DatapanelGUI*>(i, false);
		if (!p || !*p)
			continue;
		int cat = (*p)->currentCategory;
		if (cat == PREFERRED_CATEGORY)
			clash = true;
		if (cat > maxCat)
			maxCat = cat;
	}
	return clash ? maxCat + 1 : PREFERRED_CATEGORY;
}

// Each Run button reads Stop while its slot's run is active; the sweep
// button shows the sweep's progress.
static void UpdateRunCaptions(int activeSlot)
{
	for (int i = 0; i < s_buttonCount; ++i)
	{
		int id = s_buttons[i].id;
		MyGUI::Button* b = s_buttons[i].line->button;
		if (!b)
			continue;
		if (id == BENCH_BUTTON_SWEEP)
			b->setCaption(SweepCaption(BenchSweepLegNumber(), BenchSweepLegTotal()));
		else if (id >= BENCH_BUTTON_RUN && id < BENCH_BUTTON_RUN + BENCH_SLOT_COUNT)
			b->setCaption(id - BENCH_BUTTON_RUN == activeSlot ? "Stop" : "Run");
	}
}

// The line's callback (DataPanelLine_Button::pressCallback), from MyGUI's
// click dispatch on the main thread; nothing may throw back into the game.
static void OnButtonPress(DataPanelLine* line)
{
	try
	{
		for (int i = 0; i < s_buttonCount; ++i)
		{
			if (s_buttons[i].line != line)
				continue;
			int id = s_buttons[i].id;
			int slot = id % 100;
			int speed = id != BENCH_BUTTON_SWEEP && slot >= 0 && slot < BENCH_SLOT_COUNT ? s_staging.benchSpeed[slot] : 1;
			UpdateRunCaptions(BenchButtonPressed(id, speed));
			return;
		}
	}
	catch (...)
	{
		LogError("Settings panel: a Benchmark button failed");
	}
}

// LEOPARD fork (16.10.2026): the game's setEnabled(false) greys the whole line, and a disabled
// MyGUI widget gets no mouse focus - so a greyed row showed no tooltip at all. The line stays
// greyed, then every widget but its control (checkbox, drop box) is enabled again: the control
// still cannot be changed, the label shows the tooltip (which says what the row needs).
static void SetGatedLineEnabled(DataPanelLine* line, bool on)
{
	line->setEnabled(on);
	if (on)
		return;
	MyGUI::Widget* control = NULL;
	MyGUI::Widget* control2 = NULL;
	if (line->classType == DataPanelLine::DPL_CHECK)
		control = static_cast<DataPanelLine_CheckBox*>(line)->button;
	else if (line->classType == DataPanelLine::DPL_DROPBOX)
	{
		control = static_cast<DataPanelLine_DropBox*>(line)->listBox;
		control2 = static_cast<DataPanelLine_DropBox*>(line)->goBox;
	}
	else
		return;   // another kind of row: leave it as the game greys it
	for (size_t i = 0; i < line->widgets.size(); ++i)
	{
		MyGUI::Widget* const w = line->widgets[i];
		if (w && w != control && w != control2)
			w->setEnabled(true);
	}
	if (line->w1 && line->w1 != control)
		line->w1->setEnabled(true);
}

// A grouped checkbox's callback (DataPanelLine_CheckBox::notifyToggleCheck, after it wrote the
// staged value), from MyGUI's click dispatch on the main thread: its members follow its value.
static void OnGroupToggle(DataPanelLine* line)
{
	try
	{
		for (int i = 0; i < s_gatedCount; ++i)
		{
			if (s_gated[i].head == line)
				SetGatedLineEnabled(s_gated[i].member, *s_gated[i].value);
		}
	}
	catch (...)
	{
		LogError("Settings panel: a grouped row failed to follow its checkbox");
	}
}

// The game's own Options tabs draw a heading as "[title]" in this colour,
// after a full line of space.
static const char* const HEADING_COLOUR = "#afa68b";
static const char* const NOTE_COLOUR    = "#a0a0a0";

// The game's slider lines end their value box, and its checkbox lines their
// box, at this fraction of the panel's width; the value box starts this far
// before it.
static const float VALUE_COLUMN_RIGHT = 0.98f;
static const float VALUE_COLUMN_WIDTH = 0.16f;

// A drop box line places its box at a fixed left offset; this moves it into
// the value column, keeping its row and its width.
static void AlignToValueColumn(MyGUI::Widget* box)
{
	if (!box || !box->getParent())
		return;
	int panelWidth = box->getParent()->getWidth();
	box->setPosition((int)(panelWidth * (VALUE_COLUMN_RIGHT - VALUE_COLUMN_WIDTH)), box->getTop());
}

static void AddRows(DatapanelGUI* panel, const std::vector<SettingsRow>& rows, int cat, ToolTip* tooltip)
{
	const std::string buttonSkin = "Kenshi_Button2";
	std::vector<DataPanelLine*> lines(rows.size(), (DataPanelLine*)NULL);
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const SettingsRow& r = rows[i];
		switch (r.kind)
		{
		case SR_HEADER:
			// A note already brings its own space.
			if (i && rows[i - 1].kind != SR_NOTE)
				panel->addSpace(cat, 1.0f);
			panel->setLine(std::string(HEADING_COLOUR) + "[" + KeoTr(r.label.c_str()) + "]", std::string(), cat, false, true);
			break;
		case SR_TEXT:
			panel->setLine(r.label, std::string(), cat, false, true);
			break;
		case SR_NOTE:
			panel->addSpace(cat, 1.0f);
			panel->setLine(NOTE_COLOUR + r.label, std::string(), cat, false, true);
			break;
		case SR_BUTTON:
		{
			// NULL when a line with this key exists and is not a button.
			DataPanelLine_Button* b = panel->setLineTextButton(r.label, r.caption, cat, BUTTON_WIDTH, buttonSkin);
			if (!b || s_buttonCount >= MAX_BUTTONS)
				break;
			b->setToolTip(r.tooltip, tooltip);
			b->callback = MyGUI::newDelegate(&OnButtonPress);
			s_buttons[s_buttonCount].line = b;
			s_buttons[s_buttonCount].id = r.buttonId;
			++s_buttonCount;
			break;
		}
		case SR_CHECKBOX:
		{
			DataPanelLine_CheckBox* c = panel->setLineCheckbox(r.label, r.boolPtr, cat);
			c->setToolTip(r.tooltip, tooltip);
			lines[i] = c;
			break;
		}
		case SR_SLIDER:
		{
			DataPanelLine_SliderEditable* s = panel->setLineSliderEditable(r.label, cat, true, r.lo, r.hi, r.floatPtr);
			s->setPrecision(r.stepExp);
			s->setToolTip(r.tooltip, tooltip);
			lines[i] = s;
			break;
		}
		case SR_DROPBOX:
		{
			DataPanelLine_DropBox* d = panel->setLineDropBox(r.label, cat, r.intPtr, false, VALUE_COLUMN_WIDTH);
			AlignToValueColumn(d->listBox);
			for (size_t c = 0; c < r.choices.size(); ++c)
				d->addAValue(r.choices[c].first, r.choices[c].second);
			d->refresh();
			d->setToolTip(r.tooltip, tooltip);
			lines[i] = d;
			break;
		}
		}
	}
	if (!s_groupsOk)
		return;
	int refused = 0;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const int head = rows[i].enabledByRow;
		if (head < 0 || (size_t)head >= rows.size() || !lines[i] || !lines[head] || !rows[head].boolPtr)
			continue;
		if (s_gatedCount >= MAX_GATED)
		{
			++refused;
			continue;
		}
		SetGatedLineEnabled(lines[i], *rows[head].boolPtr);
		GatedLine g = { lines[i], lines[head], rows[head].boolPtr };
		s_gated[s_gatedCount++] = g;
		lines[head]->callback = MyGUI::newDelegate(&OnGroupToggle);
	}
	if (refused)
	{
		std::ostringstream ss;
		ss << "Settings panel: " << refused << " grouped row(s) past the " << MAX_GATED
		   << "-row gated list stay enabled";
		KEO_DETAIL(ss.str());
	}
}

// The squad radius row's floor from the game's Fast zone hopping option as
// it reads now. Its checkbox writes the option when clicked, so the close
// sees a change made during the same visit to the Options window.
static ConfigFloor SquadRadiusFloor()
{
	if (!KeoMcmRuntimeReady()) { ConfigFloor empty = {NULL,0,NULL}; return empty; }
	return zone::ZoneSquadRadiusFloor(GameFastZoneHopping());
}

ConfigFloor KeoMcmRuntimeFloor() { return SquadRadiusFloor(); }

static void LogFloorRaised(const ConfigFloor& f, const char* when)
{
	if (!DetailedLogEnabled()) return;
	std::ostringstream ss;
	ss << "Settings panel: " << f.key << " below its floor, raised to " << f.floor << " " << when;
	KEO_DETAIL(ss.str());
}

// Inserts the tab before the last one (Mods, where RE_Kenshi puts its
// button). hide() casts every tab's item data to DatapanelGUI* and throws on
// anything else, then destroys the panel and removes the tab itself, so the
// tab carries its panel from the moment the panel exists and nothing here
// outlives this call.
static void BuildTab(OptionsWindow* win)
{
	MyGUI::TabControl* tabs = win->tabs;
	if (!tabs || !gui)
		return;
	size_t count = tabs->getItemCount();
	if (count == 0)
		return;

	if (!RequestedConfig().Copy(&s_saved)) { LogError("Settings panel: requested preferences unavailable"); return; }
	s_staging = s_saved;
	StageBenchSpeeds(&s_staging, g_benchSlots);
	s_buttonCount = 0;
	s_gatedCount = 0;

	SettingsBench bench;
	bench.available = s_buttonsOk && BenchAvailable();
	bench.reason = !s_buttonsOk ? "button calls" : BenchUnavailableReason();
	bench.slots = g_benchSlots;
	bench.activeSlot = BenchRunnerSlot();
	bench.sweepLeg = BenchSweepLegNumber();
	bench.sweepLegs = BenchSweepLegTotal();
	bench.runSec = BenchLeverRunSeconds(BenchLeversCombined());
	bench.runSecCombined = BenchLeverRunSeconds(true);
	bench.sweepLegCount = BenchSweepLegCount();

	MyGUI::TabItem* tab = NULL;
	bool owned = false;
	try
	{
		std::vector<SettingsRow> rows;
		BuildSettingsRows(&s_staging, DEV_BUILD, &bench, &rows);
		ConfigFloor floor = SquadRadiusFloor();
		if (ApplySettingsFloor(&s_staging, floor, &rows))
			LogFloorRaised(floor, "on the tab");
		int cat = PickCategory(tabs, count);
		tab = tabs->insertItemAt(count - 1, "KEO");
		DatapanelGUI* panel = gui->createDatapanel("keo_options", tab, true);
		if (!panel)
		{
			tabs->removeItem(tab);
			return;
		}
		tabs->setItemData(tab, (DatapanelGUI*)panel);
		owned = true;
		panel->changeCategory(cat);
		panel->setLineSpacing(25.0f);
		// Until its first control line, a panel puts each line in a fixed slot
		// a full line apart; packing from the start keeps the first heading as
		// close to its rows as the others.
		panel->basicSpacing = false;
		AddRows(panel, rows, cat, win->tooltip);
		tab->setVisible(false);
		s_staged = true;
	}
	catch (...)
	{
		try
		{
			if (owned)
				tab->setVisible(false);
			else if (tab)
				tabs->removeItem(tab);
		}
		catch (...) {}
		LogError("Settings panel: building the tab failed, left out");
	}
}

static void CommitStaging()
{
	s_staged = false;
	char error[256] = {0};
	KeoMcmCommitStaging(s_staging, s_saved, error, sizeof(error));
}

static void hook_OptionsCreate(OptionsWindow* win)
{
	s_staged = false;
	s_buttonCount = s_gatedCount = 0;
	s_origCreate(win);
	if (s_active && (DEV_BUILD || !KeoMcmAvailable()))
		BuildTab(win);
}

// Runs once per close, from hide(), before the panels are destroyed.
static void hook_OptionsSaveOptions(OptionsWindow* win)
{
	s_origSave(win);
	s_buttonCount = 0;
	s_gatedCount = 0;
	if (s_active && s_staged)
		CommitStaging();
}

// KenshiLib's address for the function must be the one the prologue row checks.
static bool HookSite(size_t rva, void* klibAddr, void* detour, void** orig, const char* name)
{
	void* ours = GameAddr(rva);
	if (klibAddr != ours)
	{
		LogError(std::string("Settings panel: ") + name + " address differs from KenshiLib's");
		return false;
	}
	if (!VerifyPrologueByRva(rva))
		return false;
	if (KenshiLib::AddHook(ours, detour, orig) != KenshiLib::SUCCESS)
	{
		LogError(std::string("Settings panel: ") + name + " hook failed");
		return false;
	}
	return true;
}

void InstallSettingsPanel()
{
	if (!keo_gui::g_guiCfg.settingsPanelEnabled)
	{
		s_token = "off(ini)";
		return;
	}
	if (!RequestedConfig().Copy(&s_saved))
	{
		s_token = "off(config)";
		LogError("Settings panel: requested preferences unavailable");
		return;
	}

	// The create hook does nothing until both are in, so a tab is never
	// built whose changes could not be applied.
	bool ok = HookSite(RVA_OPTIONS_CREATE, (void*)KlibRealAddress(&OptionsWindow::create),
	                   (void*)hook_OptionsCreate, (void**)&s_origCreate, "OptionsWindow::create")
	       && HookSite(RVA_OPTIONS_SAVE_OPTIONS, (void*)KlibRealAddress(&OptionsWindow::saveOptions),
	                   (void*)hook_OptionsSaveOptions, (void**)&s_origSave, "OptionsWindow::saveOptions");
	s_active = ok;
	s_token = ok ? "ok" : "off(hook)";
	if (!ok)
		KEO_DETAIL("Settings panel: off, the Options window keeps its own tabs only");

	// The Benchmark buttons (DEV only) call setLineTextButton and rely on
	// pressCallback invoking the line's callback; a mismatch leaves only their
	// section out.
	s_buttonsOk = (void*)KlibRealAddress(&DatapanelGUI::setLineTextButton) == GameAddr(RVA_DP_SET_LINE_TEXT_BUTTON)
	           && (void*)KlibRealAddress(&DataPanelLine_Button::pressCallback) == GameAddr(RVA_DP_BUTTON_PRESS);
	if (ok && !s_buttonsOk && DEV_BUILD)
		LogError("Bench: button call addresses differ from KenshiLib's, Benchmark section left out");

	// Grouped rows grey while their checkbox is off; that relies on the checkbox's toggle invoking
	// the line's callback after it writes the value. A mismatch leaves every row enabled.
	s_groupsOk = NativeProfileMatches(RVA_DP_CHECKBOX_TOGGLE_CALLBACK,GameAddr(RVA_DP_CHECKBOX_TOGGLE_CALLBACK),kCheckboxToggleCallback,sizeof(kCheckboxToggleCallback));
	if (ok && !s_groupsOk)
		LogError("Settings panel: the checkbox toggle differs; grouped rows stay enabled");
}

const char* SettingsPanelToken()
{
	if (!keo_gui::g_guiCfg.settingsPanelEnabled)
		return "off(ini)";
	return s_token;
}
