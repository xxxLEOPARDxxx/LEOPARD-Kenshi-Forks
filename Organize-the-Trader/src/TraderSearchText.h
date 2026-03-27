#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace MyGUI
{
class Widget;
}

class Character;
class Inventory;
class Item;

std::string SafeWidgetName(MyGUI::Widget* widget);
std::string BuildParentChainForLog(MyGUI::Widget* widget);
bool NameMatchesToken(const std::string& name, const char* token);
std::string UpperAsciiCopy(const std::string& value);
bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle);
int ExtractTaggedIntValue(const std::string& text, const char* tag);
bool TryExtractTaggedFraction(
    const std::string& text,
    const char* tag,
    int* outNumerator,
    int* outDenominator);
std::string NormalizeSearchText(const std::string& text);
std::string ResolveCanonicalItemName(Item* item);
bool IsLikelyRuntimeWidgetToken(const std::string& token);
std::string CanonicalizeSearchToken(const std::string& token);
bool ContainsAsciiLetter(const std::string& value);
bool ContainsAsciiDigit(const std::string& value);
int ComputeCaptionNameMatchBias(
    const std::string& captionNormalized,
    const std::string& nameNormalized);
std::size_t CountNonEmptyKeys(const std::vector<std::string>& keys);
void AppendNormalizedSearchChunk(const std::string& normalizedChunk, std::string* out);
bool ShouldIndexSearchToken(const std::string& token);
bool IsNumericOnlyQuery(const std::string& normalizedQuery);
void AppendSearchToken(std::string* text, const std::string& token);
std::string ResolveItemNameHintRecursive(MyGUI::Widget* widget, std::size_t depth, std::size_t maxDepth);
std::string BuildItemSearchText(MyGUI::Widget* itemWidget);
std::string BuildItemRawProbe(MyGUI::Widget* itemWidget);
bool TryResolveItemQuantityFromWidget(MyGUI::Widget* itemWidget, int* outQuantity);
Item* ResolveWidgetItemPointer(MyGUI::Widget* widget);
MyGUI::Widget* FindNamedDescendantByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible);
void CollectNamedDescendantsByToken(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::size_t maxResults,
    std::vector<MyGUI::Widget*>* outWidgets);
MyGUI::Widget* FindAncestorByToken(MyGUI::Widget* widget, const char* token);
std::string TruncateForLog(const std::string& value, std::size_t maxLength);
std::string WidgetTypeForLog(MyGUI::Widget* widget);
std::string WidgetCaptionForLog(MyGUI::Widget* widget);
void DumpAncestorDiagnostics(const char* label, MyGUI::Widget* widget);
void DumpWidgetSubtreeDiagnostics(const char* label, MyGUI::Widget* widget);
void DumpHoveredAttachDiagnostics(MyGUI::Widget* hovered, MyGUI::Widget* anchor, MyGUI::Widget* parent);
std::string BuildKeyPreviewForLog(const std::vector<std::string>& keys, std::size_t limit);
std::size_t InventoryItemCountForLog(Inventory* inventory);
std::string CharacterNameForLog(Character* character);
