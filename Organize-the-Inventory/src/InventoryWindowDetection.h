#pragma once

#include <string>

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
MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token);
MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget);
MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor);
bool IsLikelyInventoryWindow(MyGUI::Widget* parent);
void DumpInventoryTargetProbe();
void DumpVisibleInventoryWindowCandidateDiagnostics();
bool TryResolveHoveredInventoryTarget(
    MyGUI::Widget** outAnchor,
    MyGUI::Widget** outParent,
    bool logFailures);
bool TryResolveVisibleInventoryTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent);
