#include "game/native_profile.h"
#include "base/log_admission.h"
// backpack_window.cpp - The backpack-first tick box in a worn backpack's window. A post-hook on
// BackpackInventoryLayout::setupSections, which builds each backpack window once per instance,
// adds a tick box beside the window's arrange button when the window belongs to a player
// character wearing that backpack. The box shows the character's table entry, else the INI
// default, and a click flips the entry. The box carries its owner's hand key in a user string,
// never a pointer, and is destroyed with the window's layout. Everything here runs on the main
// thread, the table's only writer, and takes no lock.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "inventory/backpack_window.h"
#include "inventory/backpack_first.h"
#include "inventory/backpack_policy.h"
#include "inventory/backpack_table.h"
#include "inventory/backpack_reader.h"
#include "inventory/backpack_sidecar.h"
#include "inventory/inventory_config.h"
#include "inventory/operator_trips.h"
#include "inventory/operator_policy.h"
#include "game/game.h"
#include "game/klib_member_contract.h"
#include "game/hand_key.h"
#include "plugin/hook_manifest.h"
#include "base/core.h"
#include "base/keo_tr.h"
#include <windows.h>
#include <algorithm>
#include <string>
#include <cstdio>
#include <string.h>
#include "base/klib_include.h"
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Delegate.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_LayerManager.h>
#include <mygui/MyGUI_ScrollBar.h>
#include <mygui/MyGUI_TextBox.h>
#include "base/klib_include_end.h"

namespace backpack_window_detail {
typedef void (__fastcall *setupSections_t)(void* layout, void* gui, void* sections, void* inventory);
typedef int  (__fastcall *getDataType_t)(void* object);
typedef void* (__fastcall *getFaction_t)(void* object);
} // namespace backpack_window_detail
using namespace backpack_window_detail;

namespace keo_inventory {

static const size_t OFF_LAYOUT_MAIN_WIDGET     = 0x8;
static const size_t OFF_GUI_CALLBACK_OBJECT    = 0x50;
static const size_t OFF_CONTAINER_OWNER        = 0x230;
static const size_t OFF_FACTION_IS_PLAYER      = KLIB_OFF_Faction_isPlayer;
static const size_t VT_ROOT_GET_DATA_TYPE      = 0x20;
static const size_t VT_ROOT_GET_FACTION        = 0x58;
static const int    DATA_TYPE_CHARACTER        = 1;
static const int    DATA_TYPE_CONTAINER        = 46;
static const char*  const BOX_NAME_SUFFIX      = "keo_backpackFirst";
static const char*  const BOX_KEY              = "keo_key";
static const char*  const FILL_ROOT            = "keo_fill_root";
static const char*  const ARRANGE_SUFFIX       = "ArrangeButton";

// Instructions whose bytes encode the three offsets above: the layout's main widget, the window's
// callback object, and the backpack's callback owner.
static const unsigned char kMainWidgetCheck[8] = { 0x48,0x8B,0x4F,0x08,0x48,0x83,0xC1,0x08 };
static const unsigned char kOwnerReadCheck[4]  = { 0x4C,0x8B,0x5B,0x50 };
static const unsigned char kOwnerWriteCheck[7] = { 0x4C,0x89,0x9F,0x30,0x02,0x00,0x00 };

static setupSections_t orig_setupSections = NULL;
static volatile LONG s_boxes = 0, s_skipped = 0, s_noArrange = 0, s_clicks = 0, s_clickFailed = 0;

// The object's RootObjectBase::getDataType, or -1 for NULL.
static int DataTypeOf(void* object)
{
	if (!object)
		return -1;
	getDataType_t fn = *(getDataType_t*)(*(const uintptr_t*)object + VT_ROOT_GET_DATA_TYPE);
	return fn(object);
}

// The character's faction is the player's: its isPlayer pointer is set.
static bool IsPlayerFaction(void* character)
{
	getFaction_t fn = *(getFaction_t*)(*(const uintptr_t*)character + VT_ROOT_GET_FACTION);
	void* faction = fn(character);
	return faction && *(void* const*)((const char*)faction + OFF_FACTION_IS_PLAYER) != NULL;
}

// Depth first under parent, at most levels deep: the first widget whose name ends with suffix.
static MyGUI::Widget* FindBySuffix(MyGUI::Widget* parent, const char* suffix, int levels)
{
	if (!parent || levels <= 0)
		return NULL;
	const size_t n = strlen(suffix);
	const size_t count = parent->getChildCount();
	for (size_t i = 0; i < count; ++i)
	{
		MyGUI::Widget* child = parent->getChildAt(i);
		if (!child)
			continue;
		const std::string& name = child->getName();
		if (name.size() >= n && name.compare(name.size() - n, n, suffix) == 0)
			return child;
		MyGUI::Widget* found = FindBySuffix(child, suffix, levels - 1);
		if (found)
			return found;
	}
	return NULL;
}

// Main thread, from MyGUI's click dispatch. Flips the owner's choice and the box; nothing is
// thrown back into the game. The box's key is followed to its character's live hand first, and
// the table re-keyed, so the write updates the entry the character's pickups read.
static bool GetWearerKey(MyGUI::Widget* widget, game::HandKey* key, bool requireLive = true)
{
	const std::string& text = widget->getUserString(BOX_KEY);
	if (!HandKeyParse(text.c_str(), (int)text.size(), key))
		return false;
	game::HandKey live;
	const bool resolved = BackpackResolveKey(*key, &live);
	if (!resolved && requireLive)
		return false;
	if (resolved && !game::HandKeyEqual(live, *key))
	{
		char buf[64] = { 0 };
		HandKeyFormat(live, buf, (int)sizeof(buf));
		widget->setUserString(BOX_KEY, buf);
		*key = live;
	}
	BackpackRekeyNow(false);
	return true;
}

static void OnBackpackFirstClick(MyGUI::Widget* sender)
{
	try
	{
		MyGUI::Button* box = sender ? sender->castType<MyGUI::Button>(false) : NULL;
		game::HandKey key;
		if (!box || !GetWearerKey(box, &key, false))
		{
			InterlockedIncrement(&s_clickFailed);
			return;
		}
		int cur = BackpackFirstGet(key);
		bool on = cur < 0 ? g_inventoryCfg.backpackFirstDefault : cur != 0;
		if (!BackpackFirstSet(key, on ? 0 : 1))
		{
			InterlockedIncrement(&s_clickFailed);
			return;
		}
		box->setStateSelected(!on);
		InterlockedIncrement(&s_clicks);
	}
	catch (...)
	{
		InterlockedIncrement(&s_clickFailed);
	}
}

static const char* kFillSliderSuffix = "slider";
static const char* kFillTextSuffix = "value";
static const char* kFillApplySuffix = "apply";
static const char* kFillGlobalSuffix = "global";

static void ShowPersonalFillIndicator(MyGUI::Button* box, bool personal)
{
	const std::string name = box->getName() + "_personal";
	MyGUI::Widget* indicator = box->findWidget(name);
	if (!indicator && personal)
	{
		indicator = box->createWidget<MyGUI::Widget>("WhiteSkin",
		    MyGUI::IntCoord(4, box->getHeight() - 5, box->getWidth() - 8, 3),
		    MyGUI::Align::HStretch | MyGUI::Align::Bottom, name);
		indicator->setNeedMouseFocus(false);
		indicator->setColour(MyGUI::Colour(0.25f, 1.0f, 0.1f));
	}
	box->setColour(personal ? MyGUI::Colour(0.35f, 1.0f, 0.25f) : MyGUI::Colour::White);
	if (indicator)
		indicator->setVisible(personal);
}

static void UpdateFillValue(MyGUI::Widget* panel, size_t index)
{
	MyGUI::Widget* widget = panel->findWidget(panel->getName() + kFillTextSuffix);
	MyGUI::TextBox* label = widget ? widget->castType<MyGUI::TextBox>(false) : NULL;
	if (!label)
		return;
	char value[8];
	sprintf_s(value, "%u%%", (unsigned)(index + 1) * 5);
	label->setCaption(value);
}

static void OnFillSlider(MyGUI::ScrollBar* slider, size_t index)
{
	try { UpdateFillValue(slider->getParent(), index); }
	catch (...) { InterlockedIncrement(&s_clickFailed); }
}

static void OnFillChoice(MyGUI::Widget* sender)
{
	try
	{
		MyGUI::Widget* panel = sender->getParent();
		MyGUI::Button* box = panel->getParent()->castType<MyGUI::Button>(false);
		MyGUI::Widget* widget = panel->findWidget(panel->getName() + kFillSliderSuffix);
		MyGUI::ScrollBar* slider = widget ? widget->castType<MyGUI::ScrollBar>(false) : NULL;
		const bool inherit = sender->getName() == panel->getName() + kFillGlobalSuffix;
		game::HandKey key;
		const int percent = slider && !inherit ? (int)(slider->getScrollPosition() + 1) * 5 : 0;
		if (!box || !slider || !GetWearerKey(box, &key) || !BackpackOperatorFillSet(key, percent))
		{
			InterlockedIncrement(&s_clickFailed);
			return;
		}
		panel->setVisible(false);
		ShowPersonalFillIndicator(box, percent != 0);
	}
	catch (...) { InterlockedIncrement(&s_clickFailed); }
}

static MyGUI::Widget* CreateFillPopup(MyGUI::Button* box, const std::string& name, const MyGUI::IntCoord& coord)
{
	// Use the same layer as the game's drop-down lists. The checkbox still owns
	// this popup, so closing the backpack also retires its controls and delegates.
	MyGUI::Widget* panel = box->createWidget<MyGUI::Widget>(
	    MyGUI::WidgetStyle::Popup, "Kenshi_GenericTextBoxSkin", coord, MyGUI::Align::Default, "Popup", name);
	try
	{
		panel->setVisible(false);
		const int width = coord.width;
		const bool narrow = width < 240;
		const MyGUI::Align align = MyGUI::Align::Default;
		MyGUI::TextBox* title = panel->createWidget<MyGUI::TextBox>("Kenshi_TextboxPaintedText",
		    MyGUI::IntCoord(8, 6, width - 16, 24), align, name + "title");
		title->setCaption(KeoTr("Personal operator fill"));
		title->setTextAlign(MyGUI::Align::Center);
		title->setNeedMouseFocus(false);
		MyGUI::ScrollBar* slider = panel->createWidget<MyGUI::ScrollBar>("Kenshi_ScrollBar",
		    MyGUI::IntCoord(6, 32, width - 64, 34), align, name + kFillSliderSuffix);
		slider->setScrollRange(20);
		slider->setScrollPage(1);
		slider->setScrollViewPage(1);
		slider->setMoveToClick(true);
		slider->eventScrollChangePosition += MyGUI::newDelegate(&OnFillSlider);
		MyGUI::TextBox* value = panel->createWidget<MyGUI::TextBox>("Kenshi_TextboxPaintedText",
		    MyGUI::IntCoord(width - 58, 36, 50, 24), align, name + kFillTextSuffix);
		value->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
		value->setNeedMouseFocus(false);
		const int middle = width / 2;
		MyGUI::Button* apply = panel->createWidget<MyGUI::Button>("Kenshi_Button1",
		    MyGUI::IntCoord(8, 74, narrow ? width - 16 : middle - 12, 28), align, name + kFillApplySuffix);
		MyGUI::Button* inherit = panel->createWidget<MyGUI::Button>("Kenshi_Button1",
		    MyGUI::IntCoord(narrow ? 8 : middle + 4, narrow ? 106 : 74,
		                   narrow ? width - 16 : width - middle - 12, 28), align, name + kFillGlobalSuffix);
		apply->setCaption(KeoTr("Apply"));
		inherit->setCaption(KeoTr("Use global limit"));
		apply->eventMouseButtonClick += MyGUI::newDelegate(&OnFillChoice);
		inherit->eventMouseButtonClick += MyGUI::newDelegate(&OnFillChoice);
	}
	catch (...)
	{
		MyGUI::Gui::getInstance().destroyWidget(panel);
		throw;
	}
	return panel;
}

static void OnBackpackRightClick(MyGUI::Widget* sender, int, int, MyGUI::MouseButton button)
{
	if (button != MyGUI::MouseButton::Right || !g_inventoryCfg.backpackFixesEnabled || !OperatorFillInstalled())
		return;
	try
	{
		MyGUI::Button* box = sender->castType<MyGUI::Button>(false);
		if (!box)
			return;
		MyGUI::Widget* root = box->getParent();
		while (root && !root->isUserString(FILL_ROOT))
			root = root->getParent();
		if (!root || root->getWidth() < 160 || root->getHeight() < 120)
			return;
		const std::string name = box->getName() + "_fill";
		MyGUI::Widget* panel = box->findWidget(name);
		if (panel && panel->getVisible())
		{
			panel->setVisible(false);
			return;
		}
		game::HandKey key;
		if (!GetWearerKey(box, &key))
			return;
		const int width = root->getWidth() > 292 ? 280 : root->getWidth() - 12;
		const int height = width < 240 ? 144 : 112;
		// A Popup is owned by the checkbox but laid out in screen coordinates.
		const int left = (std::max)(4, root->getAbsoluteLeft() + (root->getWidth() - width) / 2);
		const int top = (std::max)(4, box->getAbsoluteTop() - height - 8);
		const MyGUI::IntCoord coord(left, top, width, height);
		if (panel && panel->getWidth() != width)
		{
			MyGUI::Gui::getInstance().destroyWidget(panel);
			panel = NULL;
		}
		if (!panel)
			panel = CreateFillPopup(box, name, coord);
		else
			panel->setCoord(coord);
		MyGUI::Widget* widget = panel->findWidget(name + kFillSliderSuffix);
		MyGUI::ScrollBar* slider = widget ? widget->castType<MyGUI::ScrollBar>(false) : NULL;
		if (!slider)
			return;
		const int personal = BackpackOperatorFillGet(key);
		ShowPersonalFillIndicator(box, personal != 0);
		const int percent = personal ? personal : g_inventoryCfg.operatorFillPercent;
		const size_t index = OperatorFillPercentValid(percent) ? (size_t)percent / 5 - 1 : 19;
		slider->setScrollPosition(index);
		UpdateFillValue(panel, index);
		panel->setVisible(true);
		MyGUI::LayerManager::getInstance().upLayerItem(panel);
	}
	catch (...) { InterlockedIncrement(&s_clickFailed); }
}

// Main thread, once per backpack window, after setupSections built its sections and buttons.
// Every type is read before a type-specific read; the widget pointers live only for this call.
static void AddBackpackFirstBox(void* layout, void* gui)
{
	void* container = gui ? *(void**)((char*)gui + OFF_GUI_CALLBACK_OBJECT) : NULL;
	int containerType = DataTypeOf(container);
	void* owner = containerType == DATA_TYPE_CONTAINER ? *(void**)((char*)container + OFF_CONTAINER_OWNER) : NULL;
	int ownerType = DataTypeOf(owner);
	bool isAnimal = false, isPlayer = false, wears = false;
	if (ownerType == DATA_TYPE_CHARACTER)
	{
		isAnimal = CharacterIsAnimal(owner);
		isPlayer = IsPlayerFaction(owner);
		wears = WornBackpack(owner) == container;
	}
	if (!BackpackBoxWanted(containerType, ownerType, isAnimal, isPlayer, wears))
	{
		InterlockedIncrement(&s_skipped);
		return;
	}

	// A squad move since the last re-key tick is followed before the entry is read.
	BackpackRekeyNow(false);

	MyGUI::Widget* root = layout ? *(MyGUI::Widget**)((char*)layout + OFF_LAYOUT_MAIN_WIDGET) : NULL;
	MyGUI::Widget* arrange = FindBySuffix(root, ARRANGE_SUFFIX, 4);
	MyGUI::Widget* parent = arrange ? arrange->getParent() : NULL;
	if (!parent)
	{
		InterlockedIncrement(&s_noArrange);
		return;
	}
	// The layout prefixes every widget name; the box takes the arrange button's prefix.
	const std::string& arrangeName = arrange->getName();
	const std::string prefix = arrangeName.substr(0, arrangeName.size() - strlen(ARRANGE_SUFFIX));
	if (root->findWidget(prefix + BOX_NAME_SUFFIX))
		return;

	const MyGUI::IntCoord& c = arrange->getCoord();
	int side = c.height;
	MyGUI::Button* box = parent->createWidget<MyGUI::Button>("Kenshi_TickBoxSkin", MyGUI::IntCoord(c.left + c.width + 4, c.top, side, side), MyGUI::Align::Right | MyGUI::Align::Bottom, prefix + BOX_NAME_SUFFIX);

	game::HandKey key = game::HandKeyOfObject(owner);
	char buf[64] = { 0 };
	HandKeyFormat(key, buf, (int)sizeof(buf));
	box->setUserString(BOX_KEY, buf);

	int cur = BackpackFirstGet(key);
	box->setStateSelected(cur < 0 ? g_inventoryCfg.backpackFirstDefault : cur != 0);
	if (BackpackOperatorFillGet(key))
		ShowPersonalFillIndicator(box, true);

	box->eventMouseButtonClick += MyGUI::newDelegate(&OnBackpackFirstClick);
	root->setUserString(FILL_ROOT, "1");
	box->eventMouseButtonReleased += MyGUI::newDelegate(&OnBackpackRightClick);
	InterlockedIncrement(&s_boxes);
}

// Main thread, once per backpack window, after the window's own sections and buttons exist.
static void __fastcall hook_setupSections(void* layout, void* gui, void* sections, void* inventory)
{
	orig_setupSections(layout, gui, sections, inventory);
	if (!g_inventoryCfg.backpackFixesEnabled) return;
	try
	{
		AddBackpackFirstBox(layout, gui);
	}
	catch (...)
	{
		InterlockedIncrement(&s_skipped);
	}
}

void InstallBackpackWindow(int* installed, int*)
{
	if (!HookRowWanted(HOOK_BACKPACK_LAYOUT_SETUP_SECTIONS)) return;
	// A box would write a table no pickup reads.
	if (!BackpackFirstInstalled()) { LogError("BackpackWindow: not installed (backpackFirst)"); return; }
	const char* why = NULL;
	if (!NativeProfileMatches(RVA_LAYOUT_MAIN_WIDGET_CHECK,GameAddr(RVA_LAYOUT_MAIN_WIDGET_CHECK),kMainWidgetCheck,sizeof(kMainWidgetCheck)))
		why = "mainWidget";
	else if (!NativeProfileMatches(RVA_BACKPACK_OWNER_READ_CHECK,GameAddr(RVA_BACKPACK_OWNER_READ_CHECK),kOwnerReadCheck,sizeof(kOwnerReadCheck)))
		why = "ownerRead";
	else if (!NativeProfileMatches(RVA_BACKPACK_OWNER_WRITE_CHECK,GameAddr(RVA_BACKPACK_OWNER_WRITE_CHECK),kOwnerWriteCheck,sizeof(kOwnerWriteCheck)))
		why = "ownerWrite";
	if (!why)
		why = HookInstall(HOOK_BACKPACK_LAYOUT_SETUP_SECTIONS, hook_setupSections, &orig_setupSections, installed, true);
	if (!why)
		KEO_DETAIL("BackpackWindow: installed");
	else
		LogError(std::string("BackpackWindow: not installed (") + why + ")");
}

void BackpackWindowCounters(long out[5])
{
	out[0] = InterlockedCompareExchange(&s_boxes, 0, 0);
	out[1] = InterlockedCompareExchange(&s_skipped, 0, 0);
	out[2] = InterlockedCompareExchange(&s_noArrange, 0, 0);
	out[3] = InterlockedCompareExchange(&s_clicks, 0, 0);
	out[4] = InterlockedCompareExchange(&s_clickFailed, 0, 0);
}

} // namespace keo_inventory
