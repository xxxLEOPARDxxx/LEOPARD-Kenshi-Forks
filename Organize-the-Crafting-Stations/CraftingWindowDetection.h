#pragma once

#include <cstddef>
#include <string>

namespace MyGUI
{
class Widget;
class Window;
}

class Character;

MyGUI::Widget* FindWidgetByName(const char* widgetName);
MyGUI::Widget* FindNamedDescendantRecursive(
    MyGUI::Widget* root,
    const char* widgetName,
    bool requireVisible);
MyGUI::Widget* FindFirstVisibleWidgetByName(const char* widgetName);
MyGUI::Widget* FindFirstVisibleWidgetByToken(const char* token);
MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token);
MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget);
bool HasTraderInventoryMarkers(MyGUI::Widget* parent);
bool HasTraderMoneyMarkers(MyGUI::Widget* parent);
int ComputeTraderWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason);
bool IsLikelyTraderWindow(MyGUI::Widget* parent);
bool HasTraderStructure(MyGUI::Widget* parent);
void DumpTraderTargetProbe();
void DumpVisibleWindowCandidateDiagnostics();
void DumpVisibleCraftingWindowCandidateDiagnostics();
bool TryResolveVisibleCraftingTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent);
bool TryResolveVisibleTraderTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent);
bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures);
MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget);
MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor);
MyGUI::Widget* ResolveTraderParentFromControlsContainer(MyGUI::Widget* controlsContainer);
MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* backpackContent);
std::size_t CountOccupiedEntriesInEntriesRoot(MyGUI::Widget* entriesRoot);
MyGUI::Widget* ResolveBestBackpackContentWidget(
    MyGUI::Widget* traderParent,
    bool logDiagnostics,
    bool allowSelectionLogging = true);
bool TryResolveCharacterInventoryVisible(Character* character, bool* visibleOut);
bool TryResolvePreferredDialogueTraderTarget(
    Character** outTarget,
    Character** outSpeaker,
    std::string* outReason);
bool TryResolveCaptionMatchedTraderCharacter(
    MyGUI::Widget* traderParent,
    Character** outCharacter,
    int* outCaptionScore);
std::string BuildTraderTargetIdentity(MyGUI::Widget* anchor, MyGUI::Widget* parent);
