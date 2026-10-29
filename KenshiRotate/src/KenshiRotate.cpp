#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

#include <Debug.h>
#include <PluginDiagnostics.h>
#include <core/Functions.h>

#pragma warning(push)
#pragma warning(disable: 4091) // '__declspec(dllimport)' ignored (BaseLayout.h)
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Item.h>
#include <kenshi/Inventory.h>
#include <kenshi/InputHandler.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/gui/InventoryGUI.h>

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_InputManager.h>

#pragma warning(pop)

#include <string>
#include <vector>

#include <kenshi/GameData.h>
#include <kenshi/gui/OptionsWindow.h>

#include "Settings.h"
#include "Translate.h"
#include "RotationState.h"
#include "TextureRotation.h"
#include "MouseInventoryAccess.h"
#include "CursorState.h"


void (*InventoryGUI_update_orig)(InventoryGUI*) = NULL;

// Accessor-only subclass: member-function pointers supply protected-method
// addresses to AddHook / GetRealAddress. No instances exist.
class ItemAccess : public Item
{
public:
	using Item::_DESTRUCTOR;
};

class InventoryGUIAccess : public InventoryGUI
{
public:
	using InventoryGUI::getMouseItem;
	using InventoryGUI::refreshAllSections;
	using InventoryGUI::refreshSection;
	using InventoryGUI::placeItemFromMouse;
	using InventoryGUI::sectionMouseButtonReleased;
};

static Item* CallGetMouseItem(InventoryGUI* gui)
{
	return static_cast<InventoryGUIAccess*>(gui)->getMouseItem();
}

// chargesProgress not touched — InventoryIcon::update() auto-sizes it per frame.
static void RefreshIcon(InventoryIcon* icon)
{
	if (!icon || !icon->item)
		return;

	Item* item = icon->item;
	int cellW = InventoryIcon::getItemPosition(1, 0).left;
	int cellH = InventoryIcon::getItemPosition(0, 1).top;
	int expectedW = item->itemWidth * cellW;
	int expectedH = item->itemHeight * cellH;

	MyGUI::types::TSize<int> sz = icon->getSize();
	if (sz.width != expectedW || sz.height != expectedH)
	{
		MyGUI::Widget* w = icon->getWidget();
		if (w) w->setSize(expectedW, expectedH);
		if (icon->image) icon->image->setSize(expectedW, expectedH);
		if (icon->quantityText) icon->quantityText->setSize(expectedW, expectedH);
	}

	if (RotationState::IsRotated(item))
		TextureRotation::ApplyRotatedTexture(icon);
	else
		TextureRotation::RestoreOriginalTexture(icon);
}

// No-op if the icon isn't found (e.g. cursor-held, not yet created).
static void RefreshIconForItem(InventoryGUI* gui, Item* item)
{
	if (!gui || !item)
		return;

	for (auto mapIt = gui->inventorySections.begin();
		mapIt != gui->inventorySections.end(); ++mapIt)
	{
		InventorySectionGUI* sectionGUI = mapIt->second;
		if (!sectionGUI)
			continue;

		for (size_t i = 0; i < sectionGUI->itemsIcons.size(); ++i)
		{
			InventoryIcon* icon = sectionGUI->itemsIcons[i];
			if (icon && icon->item == item)
			{
				RefreshIcon(icon);
				return;
			}
		}
	}
}

// Like InventorySection::existsItemInFootprint, but skips `self` so the rotated
// fit can be tested while the item still occupies its cells. Also bounded by the
// content grid, which _addItem throws on.
static bool IsAreaFree(InventorySection* section, Item* self, int x, int y, int w, int h)
{
	int maxW = section->width < (int)section->content.nRows ? section->width : (int)section->content.nRows;
	int maxH = section->height < (int)section->content.nCols ? section->height : (int)section->content.nCols;
	if (x < 0 || y < 0 || x + w > maxW || y + h > maxH)
		return false;

	for (size_t i = 0; i < section->items.size(); ++i)
	{
		const InventorySection::SectionItem& si = section->items[i];
		if (si.item == self)
			continue;
		if (x < si.x + si.w && si.x < x + w && y < si.y + si.h && si.y < y + h)
			return false;
	}
	return true;
}

// Row-major first fit — the order InventorySection::addItem scans in.
static bool FindFreeSlot(InventorySection* section, Item* self, int w, int h, int& outX, int& outY)
{
	for (int y = 0; y + h <= section->height; ++y)
	{
		for (int x = 0; x + w <= section->width; ++x)
		{
			if (IsAreaFree(section, self, x, y, w, h))
			{
				outX = x;
				outY = y;
				return true;
			}
		}
	}
	return false;
}

enum RotateResult { ROTATE_OK, ROTATE_NO_SPACE, ROTATE_LOCKED };

// Silent (no player messages) — caller handles error UX and preconditions.
// All validation happens before the section is touched: InventorySection::addItem
// is gated (enabled / very-limited slot), so a remove + addItem cycle can orphan
// the item. The removeItem + _addItem pair used here cannot fail.
static RotateResult RotateInSection(Item* item, InventoryGUI* gui, bool targetRotated)
{
	if (!item || !gui)
		return ROTATE_NO_SPACE;

	std::string sectionName = item->inventorySection;
	Inventory* inv = gui->_NV_getInventory();
	InventorySection* section = inv ? inv->getSection(sectionName) : NULL;
	if (!section)
		return ROTATE_NO_SPACE;

	if (RotationState::IsRotated(item) == targetRotated)
		return ROTATE_OK;

	int origX = -1, origY = -1;
	for (size_t i = 0; i < section->items.size(); ++i)
	{
		if (section->items[i].item == item)
		{
			origX = section->items[i].x;
			origY = section->items[i].y;
			break;
		}
	}
	if (origX < 0)
		return ROTATE_NO_SPACE;

	// Respect sections the game won't place this item into (animal inventories
	// are locked with a NULL very-limited slot).
	if (!section->enabled || !section->isLimitedSlotCompatible(item))
		return ROTATE_LOCKED;

	int newX, newY;
	if (!FindFreeSlot(section, item, item->itemHeight, item->itemWidth, newX, newY))
		return ROTATE_NO_SPACE;

	section->removeItem(item);
	if (!RotationState::ApplyRotationState(item, targetRotated))
	{
		section->_NV__addItem(item, origX, origY);
		return ROTATE_NO_SPACE;
	}
	section->_NV__addItem(item, newX, newY);

	auto mapIt = gui->inventorySections.find(sectionName);
	if (mapIt != gui->inventorySections.end())
	{
		mapIt->second->refreshIcons(section);
		RefreshIconForItem(gui, item);
	}
	return ROTATE_OK;
}

static void TryRotateItem(Item* item, InventoryGUI* sourceGUI)
{
    if (PluginDiagnostics::Enabled()) {
        char note[160]; sprintf_s(note, "KenshiRotate: inventory rotate equipped=%d size=%dx%d hooks=%d", item->isEquipped ? 1 : 0, item->itemWidth, item->itemHeight, RotationState::HooksReady() ? 1 : 0);
        DebugLog(note);
    }
	if (item->isEquipped)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_EQUIPPED), false);
		return;
	}
	if (item->itemWidth == item->itemHeight)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_SQUARE), false);
		return;
	}
	if (!RotationState::HooksReady())
	{
		ou->showPlayerAMessage(Tr(TR_ERR_HOOKS_FAILED), false);
		return;
	}

	RotateResult r = RotateInSection(item, sourceGUI, !RotationState::IsRotated(item));
    if (PluginDiagnostics::Enabled()) {
        char note[80]; sprintf_s(note, "KenshiRotate: rotation result=%d", (int)r); DebugLog(note);
    }
	if (r == ROTATE_LOCKED)
		ou->showPlayerAMessage(Tr(TR_ERR_LOCKED), false);
	else if (r == ROTATE_NO_SPACE)
		ou->showPlayerAMessage(Tr(TR_ERR_NO_SPACE), false);
}

bool (*placeItemFromMouse_orig)(InventoryGUI*, const std::string,
	const MyGUI::types::TPoint<int>&) = NULL;

// On a drop-swap, setupCursorItem re-fires _CONSTRUCTOR mid-call with the
// swapped item, so CursorState is already re-populated. Only Clear() when
// the cursor item is unchanged (plain drop).
bool placeItemFromMouse_hook(InventoryGUI* thisptr, const std::string sectionName,
	const MyGUI::types::TPoint<int>& mousePos)
{
	Item* prevCursorItem = CursorState::GetItem();
	bool result = placeItemFromMouse_orig(thisptr, sectionName, mousePos);
	if (result && CursorState::GetItem() == prevCursorItem)
		CursorState::Clear();
	return result;
}

// Kenshi's RClickAutoTrade returns TradeResult (hidden-return-ptr ABI, unhookable),
// so we hook the mouse-release dispatcher and set this flag during right-click dispatch.
// canStackWith/addQuantity consult it to let quick-transfer merge across rotation.
static bool g_inQuickTransfer = false;

static bool g_lastTriggerState = false;

// Hook failures — surfaced once on first inventory open. Required hooks
// additionally gate rotation (RotationState::HooksReady).
static std::vector<const char*> g_failedHookNames;
static bool g_hookFailureWarned = false;

void (*sectionMouseButtonReleased_orig)(InventoryGUI*, MyGUI::Widget*, int, int, MyGUI::MouseButton) = NULL;

void sectionMouseButtonReleased_hook(InventoryGUI* thisptr, MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
	bool wasRight = (id == MyGUI::MouseButton::Right);
	bool prev = g_inQuickTransfer;
	if (wasRight) g_inQuickTransfer = true;
	sectionMouseButtonReleased_orig(thisptr, sender, left, top, id);
	if (wasRight) g_inQuickTransfer = prev;
}

bool (*canStackWith_orig)(InventoryItemBase*, InventoryItemBase*) = NULL;

bool canStackWith_hook(InventoryItemBase* thisptr, InventoryItemBase* other)
{
	if (!thisptr || !other)
		return false;

	if (!canStackWith_orig(thisptr, other))
		return false;

	// Right-click quick-transfer: destination stack's rotation wins.
	if (g_inQuickTransfer)
		return true;

	bool thisRotated = RotationState::IsRotated(static_cast<Item*>(thisptr));
	bool otherRotated = RotationState::IsRotated(static_cast<Item*>(other));
	if (thisRotated != otherRotated)
		return false;

	return true;
}

// Defense-in-depth behind canStackWith: placeItemFromMouse's swap/fallback
// path calls addQuantity even when canStackWith returned false.
void (*addQuantity_orig)(InventoryItemBase*, int*, Item*, InventorySection*) = NULL;

void addQuantity_hook(InventoryItemBase* thisptr, int* amount, Item* addedItem, InventorySection* section)
{
	if (addedItem && !g_inQuickTransfer)
	{
		bool thisRotated = RotationState::IsRotated(static_cast<Item*>(thisptr));
		bool addedRotated = RotationState::IsRotated(addedItem);
		if (thisRotated != addedRotated)
			return;
	}
	addQuantity_orig(thisptr, amount, addedItem, section);
}

// Every stack split (InventorySection::addItem over max stack, the GUI's
// takeCertainAmountFrom, trade, drop) copies from the template, so the copy
// comes back unrotated — while the caller placed it using the source's
// rotated footprint (grid overrun -> Array2d::BoundsViolation crash).
Item* (*copyItem_orig)(RootObjectFactory*, Item*) = NULL;

Item* copyItem_hook(RootObjectFactory* thisptr, Item* from)
{
	Item* copy = copyItem_orig(thisptr, from);
	if (copy && from && RotationState::IsRotated(from))
		RotationState::ApplyRotationState(copy, true);
	return copy;
}

static void TryRotateCursorItem()
{
	Item* item = CursorState::GetItem();
	InventoryIcon* icon = CursorState::GetIcon();

	if (!item || !icon)
		return;

	// Validate icon still references our tracked item (catches stale state)
	if (icon->item != item)
	{
		CursorState::Clear();
		return;
	}

	if (item->itemWidth == item->itemHeight)
	{
		ou->showPlayerAMessage(Tr(TR_ERR_SQUARE), false);
		return;
	}
	if (!RotationState::HooksReady())
	{
		ou->showPlayerAMessage(Tr(TR_ERR_HOOKS_FAILED), false);
		return;
	}

    PluginDiagnostics::Write("KenshiRotate: rotate cursor-held item");
	bool wasRotated = RotationState::IsRotated(item);
	RotationState::ApplyRotationState(item, !wasRotated);

	MyGUI::types::TSize<int> sz = icon->getSize();
	int newW = sz.height;
	int newH = sz.width;

	// Rewrite the stored grab offset so the item rotates around the mouse cursor
	// (game stores itemTopLeft - mousePos; rotation changes the mouse's relative position).
	int grabX, grabY;
	if (MouseInventoryAccess::GetGrabOffset(grabX, grabY))
	{
		int newGrabX, newGrabY;
		if (!wasRotated)
		{
			// CW rotation: rel(relX,relY) -> (oldH-1-relY, relX)
			// grab = -rel, so newGrabX = -(oldH-1+grabY), newGrabY = grabX
			newGrabX = -sz.height + 1 - grabY;
			newGrabY = grabX;
		}
		else
		{
			// CCW (un-rotate): rel(relX,relY) -> (relY, oldW-1-relX)
			// newGrabX = grabY, newGrabY = -(oldW-1+grabX)
			newGrabX = grabY;
			newGrabY = -sz.width + 1 - grabX;
		}
		MouseInventoryAccess::SetGrabOffset(newGrabX, newGrabY);
	}

	MyGUI::Widget* w = icon->getWidget();
	if (w) w->setSize(newW, newH);
	if (icon->image) icon->image->setSize(newW, newH);
	if (icon->quantityText) icon->quantityText->setSize(newW, newH);

	// Charge bar: update() does NOT run for cursor-held icons, so size it manually.
	if (icon->chargesProgress && item->originalFullChargeAmount > 0)
	{
		MyGUI::types::TSize<int> cpSize = icon->chargesProgress->getSize();
		int barW = (int)((newW - 2) * item->chargesLeft / item->originalFullChargeAmount);
		icon->chargesProgress->setSize(barW, cpSize.height);
	}

	if (MouseInventoryAccess::GetShadowWidget())
		MouseInventoryAccess::GetShadowWidget()->setSize(newW, newH);

	if (!wasRotated)
		TextureRotation::ApplyRotatedTexture(icon);
	else
		TextureRotation::RestoreOriginalTexture(icon);
}

void InventoryGUI_update_hook(InventoryGUI* thisptr)
{
	InventoryGUI_update_orig(thisptr);

	// Child GUIs (backpack, container, loot): skip input processing.
	// The owner GUI handles input for all linked inventories.
	if (thisptr->ownerInventory != NULL)
		return;

	if (!g_hookFailureWarned && !g_failedHookNames.empty())
	{
		ou->showPlayerAMessage(Tr(TR_WARN_HOOKS_FAILED), false);
		g_hookFailureWarned = true;
	}

	// Lazy one-time discovery; both are cheap after the first successful find.
	MouseInventoryAccess::FindShadowWidget();
	MouseInventoryAccess::FindMouseInventory();

	// Detect stale cursor state: dropping an item to the game world bypasses
	// placeItemFromMouse, leaving CursorState with dangling pointers.
	if (CursorState::IsHolding() && MouseInventoryAccess::IsReady() && !MouseInventoryAccess::HasHeldItem())
		CursorState::Clear();

	if (!CursorState::IsHolding())
	{
		CursorState::ClearHoverPos();
		Item* hoverItem = CallGetMouseItem(thisptr);
		InventoryGUI* hoverGUI = thisptr;
		if (!hoverItem && thisptr->childInventory)
		{
			hoverItem = CallGetMouseItem(thisptr->childInventory);
			hoverGUI = thisptr->childInventory;
		}
		if (hoverItem && RotationState::IsRotated(hoverItem))
		{
			std::string secName = hoverItem->inventorySection;
			auto mapIt = hoverGUI->inventorySections.find(secName);
			if (mapIt != hoverGUI->inventorySections.end())
			{
				InventorySectionGUI* secGUI = mapIt->second;
				CursorState::StoreHoverPos(secGUI->getItemAbsolutePosition(
					hoverItem->inventoryPos.x, hoverItem->inventoryPos.y));
			}
		}
	}

	// Skip rotation during key capture (settings rebind in progress)
	if (RotateSettings::IsCapturing())
		return;

	bool triggerDown = RotateSettings::TriggerDown();

	// Rising-edge detection, but only consume the edge when rotation
	// actually happens. _NV_update fires for every standalone InventoryGUI
	// (character body + any open container like a weapon stand), so if the
	// character's call consumed the edge when the mouse was over a container,
	// the container's later call would never see the rising edge.
	if (triggerDown && !g_lastTriggerState)
	{
		bool rotated = false;
		if (CursorState::IsHolding())
		{
			// Holding an item blocks grid rotation.
			if (CursorState::GetIcon() != NULL)
			{
				TryRotateCursorItem();
				rotated = true;
			}
		}
		else
		{
			Item* mouseItem = CallGetMouseItem(thisptr);
			InventoryGUI* sourceGUI = thisptr;
			if (!mouseItem && thisptr->childInventory)
			{
				mouseItem = CallGetMouseItem(thisptr->childInventory);
				sourceGUI = thisptr->childInventory;
			}

			if (mouseItem)
			{
				TryRotateItem(mouseItem, sourceGUI);
				rotated = true;
			}
		}

		if (rotated)
			g_lastTriggerState = true;
	}
	else if (!triggerDown)
	{
		g_lastTriggerState = false;
	}
}

// State follows object lifetimes, including stack merges and world drops.
// Item persistence, copying and stacking still use the original hook paths.
static void (*Item_destroy_orig)(Item*) = NULL;
static void Item_destroy_hook(Item* item)
{
	RotationState::EraseKey(RotationState::GetHandleKey(item));
	if (CursorState::GetItem() == item) CursorState::Clear();
	Item_destroy_orig(item);
}

static void (*InventoryIcon_destroy_orig)(InventoryIcon*) = NULL;
static void InventoryIcon_destroy_hook(InventoryIcon* icon)
{
	if (CursorState::GetIcon() == icon) CursorState::Clear();
	InventoryIcon_destroy_orig(icon);
}

InventoryIcon* (*InventoryIcon_ctor_orig)(InventoryIcon*, Item*,
	const MyGUI::types::TPoint<int>&, MyGUI::Widget*) = NULL;

InventoryIcon* InventoryIcon_ctor_hook(InventoryIcon* thisptr, Item* item,
	const MyGUI::types::TPoint<int>& position, MyGUI::Widget* parent)
{
	InventoryIcon* result = InventoryIcon_ctor_orig(thisptr, item, position, parent);

	// Track cursor icons — makeIconForItem creates them with parent==NULL
	if (parent == NULL && item != NULL)
	{
		CursorState::Set(item, thisptr);

		// Fix pickup offset for rotated items.
		// setupCursorItem clamps the grab offset to template dimensions.
		// Recompute the correct offset from the cached pre-pickup grid position.
		MyGUI::IntPoint hoverPos;
		if (RotationState::IsRotated(item) && CursorState::TryGetHoverPos(hoverPos) && MouseInventoryAccess::IsReady())
		{
			MyGUI::InputManager* inputMgr = MyGUI::InputManager::getInstancePtr();
			if (inputMgr)
			{
				const MyGUI::IntPoint& mousePos = inputMgr->getMousePosition();
				int correctGrabX = hoverPos.left - mousePos.left;
				int correctGrabY = hoverPos.top - mousePos.top;

				// Apply same half-cell clamping as setupCursorItem but with instance dims
				int cellW = InventoryIcon::getItemPosition(1, 0).left;
				int cellH = InventoryIcon::getItemPosition(0, 1).top;
				int halfCellW = cellW / 2;
				int halfCellH = cellH / 2;
				int instancePixelW = item->itemWidth * cellW;
				int instancePixelH = item->itemHeight * cellH;
				int minX = -(instancePixelW - halfCellW);
				int maxX = -halfCellW;
				int minY = -(instancePixelH - halfCellH);
				int maxY = -halfCellH;

				if (correctGrabX < minX) correctGrabX = minX;
				if (correctGrabX > maxX) correctGrabX = maxX;
				if (correctGrabY < minY) correctGrabY = minY;
				if (correctGrabY > maxY) correctGrabY = maxY;

				MouseInventoryAccess::SetGrabOffset(correctGrabX, correctGrabY);
			}
		}
		CursorState::ClearHoverPos();
	}

	if (item && RotationState::IsRotated(item))
	{
		MyGUI::types::TSize<int> origSize = thisptr->getSize();

		int newW = origSize.height;
		int newH = origSize.width;

		MyGUI::Widget* widget = thisptr->getWidget();
		if (widget) widget->setSize(newW, newH);
		if (thisptr->image) thisptr->image->setSize(newW, newH);
		if (thisptr->quantityText) thisptr->quantityText->setSize(newW, newH);
		if (thisptr->chargesProgress && item->originalFullChargeAmount > 0)
		{
			MyGUI::types::TSize<int> cpSize = thisptr->chargesProgress->getSize();
			int barW = (int)((newW - 2) * item->chargesLeft / item->originalFullChargeAmount);
			thisptr->chargesProgress->setSize(barW, cpSize.height);
		}

		TextureRotation::ApplyRotatedTexture(thisptr);
	}

	return result;
}

bool (*getBestPositionSlot_orig)(InventorySectionGUI*,
	const MyGUI::types::TPoint<int>&, InventorySection*, Item*,
	MyGUI::types::TPoint<int>&) = NULL;

bool getBestPositionSlot_hook(InventorySectionGUI* thisptr,
	const MyGUI::types::TPoint<int>& position, InventorySection* section,
	Item* item, MyGUI::types::TPoint<int>& slot)
{
	if (item && RotationState::IsRotated(item))
	{
		// Original clamps using GameData (template) dimensions; for rotated items
		// we need instance dimensions. getPositionSlot does raw pixel-to-grid
		// conversion without item clamping, then we clamp against the instance.
		MyGUI::types::TPoint<int> rawSlot = thisptr->getPositionSlot(position, section, true);

		int maxX = section->width - item->itemWidth;
		int maxY = section->height - item->itemHeight;
		if (maxX < 0) maxX = 0;
		if (maxY < 0) maxY = 0;

		slot.left = rawSlot.left;
		slot.top = rawSlot.top;
		if (slot.left > maxX) slot.left = maxX;
		if (slot.top > maxY) slot.top = maxY;
		if (slot.left < 0) slot.left = 0;
		if (slot.top < 0) slot.top = 0;

		return section->canItemGoHere(item, slot.left, slot.top);
	}

	return getBestPositionSlot_orig(thisptr, position, section, item, slot);
}

GameData* (*Item_serialiseInv_orig)(Item*, GameDataContainer*, GameData*) = NULL;

GameData* Item_serialiseInv_hook(Item* thisptr, GameDataContainer* container, GameData* refList)
{
	GameData* state = Item_serialiseInv_orig(thisptr, container, refList);

	if (state && RotationState::IsRotated(thisptr))
		state->add("rotated", true, std::string(), false);

	return state;
}

void (*Item_loadInv_orig)(Item*, GameDataContainer*, GameData*) = NULL;

void Item_loadInv_hook(Item* thisptr, GameDataContainer* container, GameData* state)
{
	Item_loadInv_orig(thisptr, container, state);

	if (!state)
		return;

	auto it = state->bdata.find("rotated");
	if (it != state->bdata.end() && it->second)
	{
		// Game resets dims to template on load.
		RotationState::SwapItemDimensions(thisptr);
		RotationState::SetTracked(thisptr, true);
	}
	else
	{
		// Stale handle from a previous session (handles are unique within a
		// session, so this only fires after a full reload when reassigned).
		RotationState::EraseKey(RotationState::GetHandleKey(thisptr));
	}
}

// Options tab widgets are destroyed when the menu closes, so re-inject every
// frame. InjectModsTabUI is idempotent.
void (*OptionsWindow_update_orig)(OptionsWindow*) = NULL;

void OptionsWindow_update_hook(OptionsWindow* thisptr)
{
	OptionsWindow_update_orig(thisptr);
	RotateSettings::InjectModsTabUI();
	RotateSettings::ProcessCapture();
}

__declspec(dllexport) void startPlugin()
{
	PluginDiagnostics::Write("[KenshiRotate] Starting plugin...");

	DetectLanguage();
	RotateSettings::Init();

	// okFlag is non-NULL only for the required hooks (RotationState::HooksReady).
	struct HookEntry
	{
		const char* name;
		void* realAddr;
		void* detour;
		void** origStore;
		bool* okFlag;
	};

	#define HOOK_ENTRY(label, targetFn, hookFn, origPtr, okFlagPtr) \
		{ label, \
		  reinterpret_cast<void*>(KenshiLib::GetRealAddress(&targetFn)), \
		  reinterpret_cast<void*>(hookFn), \
		  reinterpret_cast<void**>(&origPtr), \
		  okFlagPtr }

	HookEntry hooks[] =
	{
		HOOK_ENTRY("Item::_DESTRUCTOR",                      ItemAccess::_DESTRUCTOR,                        Item_destroy_hook,                  Item_destroy_orig,                  NULL),
		HOOK_ENTRY("InventoryIcon::_DESTRUCTOR",             InventoryIcon::_DESTRUCTOR,                      InventoryIcon_destroy_hook,         InventoryIcon_destroy_orig,         NULL),
		HOOK_ENTRY("InventoryGUI::_NV_update",                InventoryGUI::_NV_update,                       InventoryGUI_update_hook,          InventoryGUI_update_orig,          NULL),
		HOOK_ENTRY("InventoryIcon::_CONSTRUCTOR",             InventoryIcon::_CONSTRUCTOR,                    InventoryIcon_ctor_hook,           InventoryIcon_ctor_orig,           NULL),
		HOOK_ENTRY("InventorySectionGUI::getBestPositionSlot", InventorySectionGUI::getBestPositionSlot,      getBestPositionSlot_hook,          getBestPositionSlot_orig,          NULL),
		HOOK_ENTRY("Item::serialiseInInventory",              Item::_NV_serialiseInInventory,                 Item_serialiseInv_hook,            Item_serialiseInv_orig,            &RotationState::hookSaveOK),
		HOOK_ENTRY("Item::loadFromSerialiseInInventory",      Item::_NV_loadFromSerialiseInInventory,         Item_loadInv_hook,                 Item_loadInv_orig,                 &RotationState::hookLoadOK),
		HOOK_ENTRY("InventoryGUI::placeItemFromMouse",        InventoryGUIAccess::placeItemFromMouse,         placeItemFromMouse_hook,           placeItemFromMouse_orig,           NULL),
		HOOK_ENTRY("InventoryItemBase::canStackWith",         InventoryItemBase::canStackWith,                canStackWith_hook,                 canStackWith_orig,                 NULL),
		HOOK_ENTRY("RootObjectFactory::copyItem",             RootObjectFactory::copyItem,                    copyItem_hook,                     copyItem_orig,                     &RotationState::hookCopyOK),
		HOOK_ENTRY("InventoryItemBase::addQuantity",          InventoryItemBase::addQuantity,                 addQuantity_hook,                  addQuantity_orig,                  NULL),
		HOOK_ENTRY("InventoryGUI::sectionMouseButtonReleased", InventoryGUIAccess::sectionMouseButtonReleased, sectionMouseButtonReleased_hook,  sectionMouseButtonReleased_orig,   NULL)
	};

	#undef HOOK_ENTRY

	const size_t hookCount = sizeof(hooks) / sizeof(hooks[0]);
	for (size_t i = 0; i < hookCount; ++i)
	{
		const HookEntry& h = hooks[i];
		if (KenshiLib::SUCCESS != KenshiLib::AddHook(h.realAddr, h.detour, h.origStore))
		{
			ErrorLog((std::string("[KenshiRotate] Failed to hook ") + h.name).c_str());
			g_failedHookNames.push_back(h.name);
		}
		else
		{
			PluginDiagnostics::Write((std::string("[KenshiRotate] Hooked ") + h.name + " OK").c_str());
			if (h.okFlag)
				*h.okFlag = true;
		}
	}

	// TryRotateItem / TryRotateCursorItem / KenshiRotate_CanRotate each refuse
	// with TR_ERR_HOOKS_FAILED if any required hook failed; startup only logs.
	if (!RotationState::HooksReady())
		ErrorLog("[KenshiRotate] WARNING: required hooks (save/load/copyItem) incomplete — rotation will be disabled");

	PluginDiagnostics::Write("[KenshiRotate] Plugin started successfully");
}

// Public C API for cross-mod integration (e.g. StackSort). Consumers load via
// GetModuleHandle + GetProcAddress; void* avoids header dependencies.
extern "C"
{

__declspec(dllexport) int KenshiRotate_ApiVersion()
{
	return 1;
}

__declspec(dllexport) int KenshiRotate_IsRotated(void* item)
{
	if (!item)
		return 0;
	return RotationState::IsRotated(static_cast<Item*>(item)) ? 1 : 0;
}

__declspec(dllexport) int KenshiRotate_CanRotate(void* item)
{
	if (!item)
		return 0;
	Item* it = static_cast<Item*>(item);
	if (it->isEquipped)
		return 0;
	if (it->itemWidth == it->itemHeight)
		return 0;
	if (!RotationState::HooksReady())
		return 0;
	return 1;
}

// inventoryGUI != NULL: atomic rotate with layout validation, reverts on fit failure.
// inventoryGUI == NULL: state-only swap (batch callers handle placement + UI).
// Cursor-held items not handled — inventorySection is empty on cursor.
__declspec(dllexport) int KenshiRotate_SetRotated(void* item, int rotated, void* inventoryGUI)
{
	if (!item)
		return 0;

	Item* it = static_cast<Item*>(item);
	if (it->isEquipped)
		return 0;
	if (it->itemWidth == it->itemHeight)
		return 0;
	if (!RotationState::HooksReady())
		return 0;

	bool target = (rotated != 0);
	if (RotationState::IsRotated(it) == target)
		return 1;

	if (inventoryGUI)
		return RotateInSection(it, static_cast<InventoryGUI*>(inventoryGUI), target) == ROTATE_OK ? 1 : 0;
	return RotationState::ApplyRotationState(it, target) ? 1 : 0;
}

__declspec(dllexport) void KenshiRotate_RefreshVisuals(void* inventoryGUI)
{
	if (!inventoryGUI)
		return;

	InventoryGUI* gui = static_cast<InventoryGUI*>(inventoryGUI);
	for (auto mapIt = gui->inventorySections.begin();
		mapIt != gui->inventorySections.end(); ++mapIt)
	{
		InventorySectionGUI* sectionGUI = mapIt->second;
		if (!sectionGUI)
			continue;

		for (size_t i = 0; i < sectionGUI->itemsIcons.size(); ++i)
			RefreshIcon(sectionGUI->itemsIcons[i]);
	}
}

} // extern "C"
