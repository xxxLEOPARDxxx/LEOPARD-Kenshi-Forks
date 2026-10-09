#pragma once

#include <string>

#include <vector>

namespace MyGUI
{
class Widget;
class EditBox;
class Window;
}

std::string SafeWidgetName(MyGUI::Widget* widget);
MyGUI::Widget* FindNamedDescendantRecursive(
    MyGUI::Widget* root,
    const char* widgetName,
    bool requireVisible);
MyGUI::Widget* FindNamedDescendantByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible);
void CollectNamedDescendantsByToken(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::vector<MyGUI::Widget*>* outWidgets);
void CollectVisibleWidgetsByToken(const char* token, std::vector<MyGUI::Widget*>* outWidgets);
MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token);
MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget);
MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor);
MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* inventoryContentRoot);
bool IsLikelyInventoryWindow(MyGUI::Widget* parent);
void DumpInventoryTargetProbe();
void DumpVisibleInventoryWindowCandidateDiagnostics();
bool TryResolveCompanionControlsParentForTarget(
    MyGUI::Widget* targetAnchor,
    MyGUI::Widget* targetParent,
    MyGUI::Widget** outParent);
bool TryResolveCompanionBackpackFilterRootForTarget(
    MyGUI::Widget* targetAnchor,
    MyGUI::Widget* targetParent,
    MyGUI::Widget** outFilterRoot);
bool TryResolveHoveredInventoryTarget(
    MyGUI::Widget** outAnchor,
    MyGUI::Widget** outParent,
    bool logFailures);
bool TryResolveVisibleInventoryTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent);
