#pragma once

#include <set>
#include <kenshi/util/hand.h>

class Item;

class RotationState
{
public:
	static bool IsRotated(Item* item);
	static void SetTracked(Item* item, bool rotated);
	static hand GetHandleKey(Item* item);
	static void EraseKey(const hand& key);
	static void SwapItemDimensions(Item* item);

	// Pure dimension/state flip. No section interaction, no UI refresh.
	// Returns false on validation failure (square or equipped).
	// No-op (returns true) if already in the target state.
	static bool ApplyRotationState(Item* item, bool targetRotated);

	static bool hookSaveOK;
	static bool hookLoadOK;
	static bool hookCopyOK;

	// Rotation is refused unless every required hook installed.
	static bool HooksReady() { return hookSaveOK && hookLoadOK && hookCopyOK; }

private:
	static std::set<hand> s_rotatedItems;
};
