#include "CraftingSearchText.h"

#include "CraftingCore.h"
#include "CraftingTextUnicode.h"

#include <kenshi/Character.h>
#include <kenshi/GameData.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <cctype>
#include <sstream>

namespace
{
bool ParsePositiveIntFromText(const std::string& text, int* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    *outValue = 0;
    if (text.empty())
    {
        return false;
    }

    long long value = 0;
    bool hasDigit = false;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (std::isspace(ch) != 0 || ch == ',' || ch == '.')
        {
            continue;
        }

        if (std::isdigit(ch) == 0)
        {
            return false;
        }

        hasDigit = true;
        value = value * 10 + static_cast<long long>(ch - '0');
        if (value > 2147483647LL)
        {
            return false;
        }
    }

    if (!hasDigit || value <= 0)
    {
        return false;
    }

    *outValue = static_cast<int>(value);
    return true;
}

bool TryResolveItemQuantityFromWidgetRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    int* outQuantity)
{
    if (widget == 0 || outQuantity == 0 || depth > maxDepth)
    {
        return false;
    }

    const std::string widgetName = widget->getName();
    const std::string caption = WidgetCaptionForLog(widget);
    int parsedQuantity = 0;
    const bool looksLikeQuantityWidget =
        NameMatchesToken(widgetName, "QuantityText")
        || widgetName.find("QuantityText") != std::string::npos;
    if (looksLikeQuantityWidget && ParsePositiveIntFromText(caption, &parsedQuantity))
    {
        *outQuantity = parsedQuantity;
        return true;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (TryResolveItemQuantityFromWidgetRecursive(
                widget->getChildAt(childIndex),
                depth + 1,
                maxDepth,
                outQuantity))
        {
            return true;
        }
    }

    return false;
}

bool IsGenericCaptionToken(const std::string& token)
{
    return token == "trader"
        || token == "shop"
        || token == "merchant"
        || token == "store"
        || token == "the"
        || token == "of"
        || token == "and";
}

void LogWidgetDiagnosticNode(const char* tag, MyGUI::Widget* widget, std::size_t depth)
{
    if (widget == 0)
    {
        std::stringstream line;
        line << tag << " depth=" << depth << " widget=<null>";
        LogInfoLine(line.str());
        return;
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << tag
         << " depth=" << depth
         << " type=" << WidgetTypeForLog(widget)
         << " name=" << SafeWidgetName(widget)
         << " visible=" << (widget->getInheritedVisible() ? "true" : "false")
         << " children=" << widget->getChildCount()
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = TruncateForLog(WidgetCaptionForLog(widget), 48);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    LogInfoLine(line.str());
}

void DumpNamedDescendantDiagnosticsRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t* loggedCount,
    std::size_t maxEntries)
{
    if (widget == 0 || loggedCount == 0 || *loggedCount >= maxEntries || depth > maxDepth)
    {
        return;
    }

    const bool shouldLog = !widget->getName().empty() || !WidgetCaptionForLog(widget).empty();
    if (shouldLog)
    {
        LogWidgetDiagnosticNode("subtree", widget, depth);
        ++(*loggedCount);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            loggedCount,
            maxEntries);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }
}

void CollectNamedDescendantsByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::size_t maxResults,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (root == 0 || token == 0 || outWidgets == 0 || outWidgets->size() >= maxResults)
    {
        return;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        outWidgets->push_back(root);
        if (outWidgets->size() >= maxResults)
        {
            return;
        }
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectNamedDescendantsByTokenRecursive(
            root->getChildAt(childIndex),
            token,
            requireVisible,
            maxResults,
            outWidgets);
        if (outWidgets->size() >= maxResults)
        {
            return;
        }
    }
}
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    if (name.empty())
    {
        return "<unnamed>";
    }
    return name;
}

std::string BuildParentChainForLog(MyGUI::Widget* widget)
{
    std::stringstream out;
    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        if (depth > 0)
        {
            out << " <- ";
        }
        out << SafeWidgetName(widget);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        out << " <- ...";
    }

    return out.str();
}

bool NameMatchesToken(const std::string& name, const char* token)
{
    if (token == 0 || *token == '\0' || name.empty())
    {
        return false;
    }

    const std::string tokenValue(token);
    if (name == tokenValue)
    {
        return true;
    }

    if (name.size() <= tokenValue.size() + 1)
    {
        return false;
    }

    const std::size_t offset = name.size() - tokenValue.size();
    if (name[offset - 1] != '_')
    {
        return false;
    }

    return name.compare(offset, tokenValue.size(), tokenValue) == 0;
}

std::string UpperAsciiCopy(const std::string& value)
{
    return CraftingTextUnicode::UppercaseUtf8OrAscii(value);
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    const std::string haystackUpper = UpperAsciiCopy(haystack);
    const std::string needleUpper = UpperAsciiCopy(std::string(needle));
    return haystackUpper.find(needleUpper) != std::string::npos;
}

int ExtractTaggedIntValue(const std::string& text, const char* tag)
{
    if (tag == 0 || *tag == '\0')
    {
        return -1;
    }

    const std::string token(tag);
    const std::size_t begin = text.find(token);
    if (begin == std::string::npos)
    {
        return -1;
    }

    std::size_t cursor = begin + token.size();
    int value = 0;
    bool hasDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        value = value * 10 + static_cast<int>(ch - '0');
        hasDigit = true;
        ++cursor;
    }

    return hasDigit ? value : -1;
}

bool TryExtractTaggedFraction(
    const std::string& text,
    const char* tag,
    int* outNumerator,
    int* outDenominator)
{
    if (outNumerator == 0 || outDenominator == 0 || tag == 0 || *tag == '\0')
    {
        return false;
    }

    *outNumerator = -1;
    *outDenominator = -1;

    const std::string token(tag);
    const std::size_t begin = text.find(token);
    if (begin == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = begin + token.size();
    int numerator = 0;
    bool hasNumeratorDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        numerator = numerator * 10 + static_cast<int>(ch - '0');
        hasNumeratorDigit = true;
        ++cursor;
    }
    if (!hasNumeratorDigit || cursor >= text.size() || text[cursor] != '/')
    {
        return false;
    }

    ++cursor;
    int denominator = 0;
    bool hasDenominatorDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        denominator = denominator * 10 + static_cast<int>(ch - '0');
        hasDenominatorDigit = true;
        ++cursor;
    }

    if (!hasDenominatorDigit)
    {
        return false;
    }

    *outNumerator = numerator;
    *outDenominator = denominator;
    return true;
}

std::string NormalizeSearchText(const std::string& text)
{
    return CraftingTextUnicode::NormalizeSearchTextUtf8OrAscii(text);
}

std::string ResolveCanonicalItemName(Item* item)
{
    if (item == 0)
    {
        return "";
    }

    if (!item->displayName.empty())
    {
        return item->displayName;
    }

    const std::string objectName = item->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (item->data != 0)
    {
        if (!item->data->name.empty())
        {
            return item->data->name;
        }
        if (!item->data->stringID.empty())
        {
            return item->data->stringID;
        }
    }

    Ogre::vector<StringPair>::type tooltipLines;
    item->getTooltipData1(tooltipLines);

    if (tooltipLines.empty())
    {
        item->getTooltipData2(tooltipLines);
    }

    for (std::size_t index = 0; index < tooltipLines.size(); ++index)
    {
        const StringPair& line = tooltipLines[index];
        if (!line.s1.empty() && ContainsAsciiLetter(line.s1))
        {
            return line.s1;
        }
        if (!line.s2.empty() && ContainsAsciiLetter(line.s2))
        {
            return line.s2;
        }
    }

    return "";
}

std::string CanonicalizeSearchToken(const std::string& token)
{
    if (token.empty())
    {
        return token;
    }

    if (!IsLikelyRuntimeWidgetToken(token))
    {
        return token;
    }

    const std::size_t underscore = token.find('_');
    if (underscore == std::string::npos || underscore + 1 >= token.size())
    {
        return std::string();
    }

    return token.substr(underscore + 1);
}

bool ContainsAsciiLetter(const std::string& value)
{
    return CraftingTextUnicode::ContainsLetterUtf8OrAscii(value);
}

bool ContainsAsciiDigit(const std::string& value)
{
    return CraftingTextUnicode::ContainsDigitUtf8OrAscii(value);
}

int ComputeCaptionNameMatchBias(const std::string& captionNormalized, const std::string& nameNormalized)
{
    if (captionNormalized.empty() || nameNormalized.empty())
    {
        return 0;
    }

    int score = 0;
    if (captionNormalized == nameNormalized)
    {
        score += 2200;
    }
    if (captionNormalized.find(nameNormalized) != std::string::npos)
    {
        score += 1400;
    }
    if (nameNormalized.find(captionNormalized) != std::string::npos)
    {
        score += 900;
    }

    std::size_t start = 0;
    while (start < captionNormalized.size())
    {
        while (start < captionNormalized.size() && captionNormalized[start] == ' ')
        {
            ++start;
        }
        if (start >= captionNormalized.size())
        {
            break;
        }

        std::size_t end = start;
        while (end < captionNormalized.size() && captionNormalized[end] != ' ')
        {
            ++end;
        }

        const std::string token = captionNormalized.substr(start, end - start);
        if (token.size() >= 3
            && !IsGenericCaptionToken(token)
            && nameNormalized.find(token) != std::string::npos)
        {
            score += 220;
        }

        start = end + 1;
    }

    return score;
}

bool IsLikelyRuntimeWidgetToken(const std::string& token)
{
    const std::size_t underscore = token.find('_');
    if (underscore == std::string::npos || underscore < 3)
    {
        return false;
    }

    bool sawDigitOrComma = false;
    for (std::size_t index = 0; index < underscore; ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(token[index]);
        const bool hexAlpha = (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
        if (std::isdigit(ch) != 0 || ch == ',' || ch == 'x' || ch == 'X' || hexAlpha)
        {
            if (std::isdigit(ch) != 0 || ch == ',')
            {
                sawDigitOrComma = true;
            }
            continue;
        }

        return false;
    }

    return sawDigitOrComma;
}

bool ShouldIndexSearchToken(const std::string& token)
{
    if (token.empty())
    {
        return false;
    }

    const std::string normalized = NormalizeSearchText(token);
    if (normalized.empty())
    {
        return false;
    }

    if (normalized == "root"
        || normalized == "background"
        || normalized == "itemimage"
        || normalized == "quantitytext"
        || normalized == "chargebar"
        || normalized == "baselayoutprefix"
        || normalized.find("quantitytext") != std::string::npos
        || normalized.find("itemimage") != std::string::npos
        || normalized.find("background") != std::string::npos
        || normalized.find("chargebar") != std::string::npos)
    {
        return false;
    }

    if (!ContainsAsciiLetter(normalized) && ContainsAsciiDigit(normalized))
    {
        return false;
    }

    return true;
}

bool IsNumericOnlyQuery(const std::string& normalizedQuery)
{
    if (normalizedQuery.empty())
    {
        return false;
    }

    bool hasDigit = false;
    for (std::size_t index = 0; index < normalizedQuery.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(normalizedQuery[index]);
        if (std::isdigit(ch) != 0)
        {
            hasDigit = true;
            continue;
        }
        if (std::isspace(ch) != 0 || ch == '.' || ch == ',' || ch == '-' || ch == '+')
        {
            continue;
        }
        return false;
    }

    return hasDigit;
}

void AppendSearchToken(std::string* text, const std::string& token)
{
    if (text == 0 || token.empty())
    {
        return;
    }

    const std::string canonicalToken = CanonicalizeSearchToken(token);
    if (canonicalToken.empty() || !ShouldIndexSearchToken(canonicalToken))
    {
        return;
    }

    if (!text->empty())
    {
        text->push_back(' ');
    }
    text->append(canonicalToken);
}

bool TryResolveItemQuantityFromWidget(MyGUI::Widget* itemWidget, int* outQuantity)
{
    if (outQuantity == 0)
    {
        return false;
    }

    *outQuantity = 0;
    return TryResolveItemQuantityFromWidgetRecursive(itemWidget, 0, 5, outQuantity);
}

MyGUI::Widget* FindNamedDescendantByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible)
{
    if (root == 0 || token == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(child, token, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

void CollectNamedDescendantsByToken(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::size_t maxResults,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (outWidgets == 0)
    {
        return;
    }

    outWidgets->clear();
    if (root == 0 || token == 0 || maxResults == 0)
    {
        return;
    }

    CollectNamedDescendantsByTokenRecursive(
        root,
        token,
        requireVisible,
        maxResults,
        outWidgets);
}

MyGUI::Widget* FindAncestorByToken(MyGUI::Widget* widget, const char* token)
{
    if (widget == 0 || token == 0)
    {
        return 0;
    }

    MyGUI::Widget* current = widget;
    while (current != 0)
    {
        if (NameMatchesToken(current->getName(), token))
        {
            return current;
        }
        current = current->getParent();
    }

    return 0;
}

std::string TruncateForLog(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength)
    {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

std::string WidgetTypeForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "null";
    }
    if (widget->castType<MyGUI::Window>(false) != 0)
    {
        return "Window";
    }
    if (widget->castType<MyGUI::Button>(false) != 0)
    {
        return "Button";
    }
    if (widget->castType<MyGUI::EditBox>(false) != 0)
    {
        return "EditBox";
    }
    if (widget->castType<MyGUI::ComboBox>(false) != 0)
    {
        return "ComboBox";
    }
    if (widget->castType<MyGUI::TextBox>(false) != 0)
    {
        return "TextBox";
    }

    return "Widget";
}

std::string WidgetCaptionForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Button* button = widget->castType<MyGUI::Button>(false);
    if (button != 0)
    {
        return button->getCaption().asUTF8();
    }

    MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false);
    if (textBox != 0)
    {
        return textBox->getCaption().asUTF8();
    }

    return "";
}

void DumpAncestorDiagnostics(const char* label, MyGUI::Widget* widget)
{
    std::stringstream header;
    header << "diagnostic " << label << " ancestor-chain";
    LogInfoLine(header.str());

    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        LogWidgetDiagnosticNode(label, widget, depth);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        std::stringstream line;
        line << label << " depth=" << depth << " ...";
        LogInfoLine(line.str());
    }
}

void DumpWidgetSubtreeDiagnostics(const char* label, MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return;
    }

    std::stringstream header;
    header << "diagnostic " << label << " subtree-begin";
    LogInfoLine(header.str());
    LogWidgetDiagnosticNode(label, widget, 0);

    std::size_t loggedCount = 0;
    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            1,
            3,
            &loggedCount,
            36);
        if (loggedCount >= 36)
        {
            break;
        }
    }

    if (loggedCount >= 36)
    {
        std::stringstream line;
        line << "diagnostic " << label << " subtree-truncated=true";
        LogInfoLine(line.str());
    }

    std::stringstream footer;
    footer << "diagnostic " << label << " subtree-end";
    LogInfoLine(footer.str());
}

void DumpHoveredAttachDiagnostics(MyGUI::Widget* hovered, MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    LogInfoLine("diagnostic hovered-attach begin");
    DumpAncestorDiagnostics("hovered", hovered);

    if (anchor != 0 && anchor != hovered)
    {
        DumpAncestorDiagnostics("anchor", anchor);
    }

    if (parent != 0 && parent != anchor)
    {
        DumpAncestorDiagnostics("parent", parent);
    }

    DumpWidgetSubtreeDiagnostics("anchor", anchor);
    if (parent != 0 && parent != anchor)
    {
        DumpWidgetSubtreeDiagnostics("parent", parent);
    }

    LogInfoLine("diagnostic hovered-attach end");
}

std::string BuildKeyPreviewForLog(const std::vector<std::string>& keys, std::size_t limit)
{
    if (keys.empty() || limit == 0)
    {
        return "";
    }

    std::stringstream out;
    const std::size_t count = keys.size() < limit ? keys.size() : limit;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index > 0)
        {
            out << " | ";
        }
        out << TruncateForLog(keys[index], 28);
    }
    if (keys.size() > count)
    {
        out << " | ...";
    }
    return out.str();
}

std::size_t InventoryItemCountForLog(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    const lektor<Item*>& items = inventory->getAllItems();
    if (!items.valid())
    {
        return 0;
    }

    return items.size();
}

std::string CharacterNameForLog(Character* character)
{
    if (character == 0)
    {
        return "<null>";
    }

    if (!character->displayName.empty())
    {
        return character->displayName;
    }

    const std::string objectName = character->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (character->data != 0 && !character->data->name.empty())
    {
        return character->data->name;
    }

    return "<unnamed>";
}

void AppendWidgetSearchTokens(MyGUI::Widget* widget, std::string* searchText)
{
    if (widget == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, widget->getName());
    AppendSearchToken(searchText, WidgetCaptionForLog(widget));

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    for (MyGUI::MapString::const_iterator it = userStrings.begin(); it != userStrings.end(); ++it)
    {
        AppendSearchToken(searchText, it->first);
        AppendSearchToken(searchText, it->second);
    }
}

template <typename T>
T* ReadWidgetUserDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->getUserData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetInternalDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->_getInternalData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetUserDataObject(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    return widget->getUserData<T>(false);
}

template <typename T>
T* ReadWidgetInternalDataObject(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    return widget->_getInternalData<T>(false);
}

template <typename T>
T* ReadWidgetAnyDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T* pointerInternal = ReadWidgetInternalDataPointer<T>(widget);
    if (pointerInternal != 0)
    {
        return pointerInternal;
    }

    T* pointerUser = ReadWidgetUserDataPointer<T>(widget);
    if (pointerUser != 0)
    {
        return pointerUser;
    }
    return 0;
}

Item* ResolveWidgetItemPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Item* item = ReadWidgetAnyDataPointer<Item>(widget);
    if (item != 0)
    {
        return item;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointer<InventoryItemBase>(widget);
    if (itemBase == 0)
    {
        return 0;
    }

    return dynamic_cast<Item*>(itemBase);
}

RootObjectBase* ResolveWidgetObjectBasePointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    RootObjectBase* objectBase = ReadWidgetAnyDataPointer<RootObjectBase>(widget);
    if (objectBase != 0)
    {
        return objectBase;
    }

    RootObject* object = ReadWidgetAnyDataPointer<RootObject>(widget);
    if (object != 0)
    {
        return object;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointer<InventoryItemBase>(widget);
    if (itemBase != 0)
    {
        return itemBase;
    }

    return 0;
}

Inventory* ResolveWidgetInventoryPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Inventory* inventory = ReadWidgetAnyDataPointer<Inventory>(widget);
    if (inventory != 0)
    {
        return inventory;
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        inventory = item->getInventory();
        if (inventory != 0)
        {
            return inventory;
        }
    }

    InventorySection* section = ReadWidgetAnyDataPointer<InventorySection>(widget);
    if (section != 0)
    {
        Inventory* sectionInventory = section->getInventory();
        if (sectionInventory != 0)
        {
            return sectionInventory;
        }
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    RootObject* object = dynamic_cast<RootObject*>(objectBase);
    if (object != 0)
    {
        return object->getInventory();
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = handValue->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = (*handPointer)->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    return 0;
}

void AppendGameDataSearchTokens(GameData* data, std::string* searchText)
{
    if (data == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, data->name);
    AppendSearchToken(searchText, data->stringID);
}

void AppendRootObjectSearchTokens(RootObjectBase* objectBase, std::string* searchText)
{
    if (objectBase == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, objectBase->displayName);
    AppendSearchToken(searchText, objectBase->getName());
    AppendGameDataSearchTokens(objectBase->data, searchText);
}

void AppendWidgetObjectDataTokens(MyGUI::Widget* widget, std::string* searchText)
{
    if (widget == 0 || searchText == 0)
    {
        return;
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        AppendRootObjectSearchTokens(item, searchText);
        return;
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    if (objectBase != 0)
    {
        AppendRootObjectSearchTokens(objectBase, searchText);
    }

    Inventory* inventory = ResolveWidgetInventoryPointer(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }
        if (owner != 0)
        {
            AppendRootObjectSearchTokens(owner, searchText);
        }
    }

    GameData* data = ReadWidgetInternalDataPointer<GameData>(widget);
    if (data == 0)
    {
        data = ReadWidgetUserDataPointer<GameData>(widget);
    }
    if (data != 0)
    {
        AppendGameDataSearchTokens(data, searchText);
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            AppendRootObjectSearchTokens(handItem, searchText);
        }
        else
        {
            RootObjectBase* handObject = handValue->getRootObjectBase();
            if (handObject != 0)
            {
                AppendRootObjectSearchTokens(handObject, searchText);
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            AppendRootObjectSearchTokens(handItem, searchText);
        }
        else
        {
            RootObjectBase* handObject = (*handPointer)->getRootObjectBase();
            if (handObject != 0)
            {
                AppendRootObjectSearchTokens(handObject, searchText);
            }
        }
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            std::size_t groupCount = 0;
            while (groups.next() && groupCount < 8)
            {
                const MyGUI::GroupImage& group = groups.current();
                AppendSearchToken(searchText, group.name);
                AppendSearchToken(searchText, group.texture);

                if (imageIndex < group.indexes.size())
                {
                    AppendSearchToken(searchText, group.indexes[imageIndex].name);
                }

                ++groupCount;
            }
        }
    }
}

std::string ResolveItemNameHintFromObjectBase(RootObjectBase* objectBase)
{
    if (objectBase == 0)
    {
        return "";
    }

    if (!objectBase->displayName.empty())
    {
        return objectBase->displayName;
    }

    const std::string objectName = objectBase->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (objectBase->data != 0)
    {
        if (!objectBase->data->name.empty())
        {
            return objectBase->data->name;
        }

        if (!objectBase->data->stringID.empty())
        {
            return objectBase->data->stringID;
        }
    }

    return "";
}

std::string ResolveItemNameHintRecursive(MyGUI::Widget* widget, std::size_t depth, std::size_t maxDepth)
{
    if (widget == 0 || depth > maxDepth)
    {
        return "";
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        const std::string itemName = ResolveItemNameHintFromObjectBase(item);
        if (!itemName.empty())
        {
            return itemName;
        }
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    if (objectBase != 0)
    {
        const std::string objectName = ResolveItemNameHintFromObjectBase(objectBase);
        if (!objectName.empty())
        {
            return objectName;
        }
    }

    Inventory* inventory = ResolveWidgetInventoryPointer(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }
        if (owner != 0)
        {
            const std::string ownerName = ResolveItemNameHintFromObjectBase(owner);
            if (!ownerName.empty())
            {
                return ownerName;
            }
        }
    }

    GameData* data = ReadWidgetInternalDataPointer<GameData>(widget);
    if (data == 0)
    {
        data = ReadWidgetUserDataPointer<GameData>(widget);
    }
    if (data != 0)
    {
        if (!data->name.empty())
        {
            return data->name;
        }
        if (!data->stringID.empty())
        {
            return data->stringID;
        }
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            const std::string itemName = ResolveItemNameHintFromObjectBase(handItem);
            if (!itemName.empty())
            {
                return itemName;
            }
        }

        RootObjectBase* handObject = handValue->getRootObjectBase();
        if (handObject != 0)
        {
            const std::string objectName = ResolveItemNameHintFromObjectBase(handObject);
            if (!objectName.empty())
            {
                return objectName;
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            const std::string itemName = ResolveItemNameHintFromObjectBase(handItem);
            if (!itemName.empty())
            {
                return itemName;
            }
        }

        RootObjectBase* handObject = (*handPointer)->getRootObjectBase();
        if (handObject != 0)
        {
            const std::string objectName = ResolveItemNameHintFromObjectBase(handObject);
            if (!objectName.empty())
            {
                return objectName;
            }
        }
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            while (groups.next())
            {
                const MyGUI::GroupImage& group = groups.current();
                if (imageIndex < group.indexes.size() && !group.indexes[imageIndex].name.empty())
                {
                    return group.indexes[imageIndex].name;
                }

                if (!group.name.empty())
                {
                    return group.name;
                }
            }
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        const std::string childHint = ResolveItemNameHintRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth);
        if (!childHint.empty())
        {
            return childHint;
        }
    }

    return "";
}

void AppendNormalizedSearchChunk(const std::string& normalizedChunk, std::string* out)
{
    if (out == 0 || normalizedChunk.empty())
    {
        return;
    }

    if (!ContainsAsciiLetter(normalizedChunk))
    {
        return;
    }

    if (!out->empty())
    {
        out->push_back(' ');
    }
    out->append(normalizedChunk);
}

std::size_t CountNonEmptyKeys(const std::vector<std::string>& keys)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        if (!keys[index].empty())
        {
            ++count;
        }
    }
    return count;
}



void BuildItemSearchTextRecursive(MyGUI::Widget* widget, std::size_t depth, std::size_t maxDepth, std::string* searchText)
{
    if (widget == 0 || searchText == 0 || depth > maxDepth)
    {
        return;
    }

    AppendWidgetSearchTokens(widget, searchText);
    AppendWidgetObjectDataTokens(widget, searchText);

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        BuildItemSearchTextRecursive(widget->getChildAt(childIndex), depth + 1, maxDepth, searchText);
    }
}

std::string BuildItemSearchText(MyGUI::Widget* itemWidget)
{
    std::string searchText;
    BuildItemSearchTextRecursive(itemWidget, 0, 5, &searchText);
    return searchText;
}

void AppendRawTokenForProbe(const std::string& token, std::string* probeText, std::size_t* tokenCount, std::size_t maxTokens)
{
    if (probeText == 0 || tokenCount == 0 || token.empty() || *tokenCount >= maxTokens)
    {
        return;
    }

    if (!probeText->empty())
    {
        probeText->append(" | ");
    }
    probeText->append(token);
    ++(*tokenCount);
}

void AppendObjectNameProbeToken(
    const char* tag,
    RootObjectBase* objectBase,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (tag == 0 || objectBase == 0)
    {
        return;
    }

    std::stringstream objectToken;
    objectToken << tag << "=" << TruncateForLog(ResolveItemNameHintFromObjectBase(objectBase), 48);
    AppendRawTokenForProbe(objectToken.str(), probeText, tokenCount, maxTokens);
}

void AppendGameDataProbeToken(
    const char* tag,
    GameData* data,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (tag == 0 || data == 0)
    {
        return;
    }

    if (!data->name.empty())
    {
        std::stringstream token;
        token << tag << "_name=" << TruncateForLog(data->name, 48);
        AppendRawTokenForProbe(token.str(), probeText, tokenCount, maxTokens);
    }

    if (!data->stringID.empty())
    {
        std::stringstream token;
        token << tag << "_id=" << TruncateForLog(data->stringID, 48);
        AppendRawTokenForProbe(token.str(), probeText, tokenCount, maxTokens);
    }
}

void AppendWidgetObjectProbeTokens(
    MyGUI::Widget* widget,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (widget == 0 || probeText == 0 || tokenCount == 0 || *tokenCount >= maxTokens)
    {
        return;
    }

    Item* itemInternal = ReadWidgetInternalDataPointer<Item>(widget);
    Item* itemUser = ReadWidgetUserDataPointer<Item>(widget);
    InventoryItemBase* itemBaseInternal = ReadWidgetInternalDataPointer<InventoryItemBase>(widget);
    InventoryItemBase* itemBaseUser = ReadWidgetUserDataPointer<InventoryItemBase>(widget);
    RootObjectBase* objectInternal = ReadWidgetInternalDataPointer<RootObjectBase>(widget);
    RootObjectBase* objectUser = ReadWidgetUserDataPointer<RootObjectBase>(widget);
    RootObject* rootInternal = ReadWidgetInternalDataPointer<RootObject>(widget);
    RootObject* rootUser = ReadWidgetUserDataPointer<RootObject>(widget);
    Inventory* inventoryInternal = ReadWidgetInternalDataPointer<Inventory>(widget);
    Inventory* inventoryUser = ReadWidgetUserDataPointer<Inventory>(widget);
    InventorySection* sectionInternal = ReadWidgetInternalDataPointer<InventorySection>(widget);
    InventorySection* sectionUser = ReadWidgetUserDataPointer<InventorySection>(widget);
    GameData* dataInternal = ReadWidgetInternalDataPointer<GameData>(widget);
    GameData* dataUser = ReadWidgetUserDataPointer<GameData>(widget);

    AppendObjectNameProbeToken("idata_item", itemInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_item", itemUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_itembase", itemBaseInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_itembase", itemBaseUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_obj", objectInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_obj", objectUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_root", rootInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_root", rootUser, probeText, tokenCount, maxTokens);

    if (inventoryInternal != 0)
    {
        RootObject* owner = inventoryInternal->getOwner();
        if (owner == 0)
        {
            owner = inventoryInternal->getCallbackObject();
        }
        AppendObjectNameProbeToken("idata_inv_owner", owner, probeText, tokenCount, maxTokens);
    }
    if (inventoryUser != 0)
    {
        RootObject* owner = inventoryUser->getOwner();
        if (owner == 0)
        {
            owner = inventoryUser->getCallbackObject();
        }
        AppendObjectNameProbeToken("udata_inv_owner", owner, probeText, tokenCount, maxTokens);
    }
    if (sectionInternal != 0)
    {
        Inventory* sectionInventory = sectionInternal->getInventory();
        RootObject* owner = sectionInventory == 0 ? 0 : sectionInventory->getOwner();
        if (owner == 0 && sectionInventory != 0)
        {
            owner = sectionInventory->getCallbackObject();
        }
        AppendObjectNameProbeToken("idata_section_inv_owner", owner, probeText, tokenCount, maxTokens);
        if (!sectionInternal->name.empty())
        {
            AppendRawTokenForProbe(
                std::string("idata_section_name=") + TruncateForLog(sectionInternal->name, 48),
                probeText,
                tokenCount,
                maxTokens);
        }
    }
    if (sectionUser != 0)
    {
        Inventory* sectionInventory = sectionUser->getInventory();
        RootObject* owner = sectionInventory == 0 ? 0 : sectionInventory->getOwner();
        if (owner == 0 && sectionInventory != 0)
        {
            owner = sectionInventory->getCallbackObject();
        }
        AppendObjectNameProbeToken("udata_section_inv_owner", owner, probeText, tokenCount, maxTokens);
        if (!sectionUser->name.empty())
        {
            AppendRawTokenForProbe(
                std::string("udata_section_name=") + TruncateForLog(sectionUser->name, 48),
                probeText,
                tokenCount,
                maxTokens);
        }
    }
    AppendGameDataProbeToken("idata_data", dataInternal, probeText, tokenCount, maxTokens);
    AppendGameDataProbeToken("udata_data", dataUser, probeText, tokenCount, maxTokens);

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        AppendObjectNameProbeToken("hand_item", handValue->getItem(), probeText, tokenCount, maxTokens);
        AppendObjectNameProbeToken("hand_obj", handValue->getRootObjectBase(), probeText, tokenCount, maxTokens);
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        AppendObjectNameProbeToken("handptr_item", (*handPointer)->getItem(), probeText, tokenCount, maxTokens);
        AppendObjectNameProbeToken("handptr_obj", (*handPointer)->getRootObjectBase(), probeText, tokenCount, maxTokens);
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        std::stringstream imageIndexToken;
        imageIndexToken << "image_index=" << imageIndex;
        AppendRawTokenForProbe(imageIndexToken.str(), probeText, tokenCount, maxTokens);

        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            std::size_t groupCount = 0;
            while (groups.next() && groupCount < 8 && *tokenCount < maxTokens)
            {
                const MyGUI::GroupImage& group = groups.current();
                if (!group.name.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_group=") + TruncateForLog(group.name, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                if (!group.texture.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_tex=") + TruncateForLog(group.texture, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                if (imageIndex < group.indexes.size() && !group.indexes[imageIndex].name.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_name=") + TruncateForLog(group.indexes[imageIndex].name, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                ++groupCount;
            }
        }
    }
}

void BuildItemRawProbeRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (widget == 0 || probeText == 0 || tokenCount == 0 || depth > maxDepth || *tokenCount >= maxTokens)
    {
        return;
    }

    AppendRawTokenForProbe(widget->getName(), probeText, tokenCount, maxTokens);
    AppendRawTokenForProbe(WidgetCaptionForLog(widget), probeText, tokenCount, maxTokens);

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    for (MyGUI::MapString::const_iterator it = userStrings.begin(); it != userStrings.end(); ++it)
    {
        AppendRawTokenForProbe(it->first, probeText, tokenCount, maxTokens);
        AppendRawTokenForProbe(it->second, probeText, tokenCount, maxTokens);
        if (*tokenCount >= maxTokens)
        {
            break;
        }
    }

    AppendWidgetObjectProbeTokens(widget, probeText, tokenCount, maxTokens);

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        BuildItemRawProbeRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            probeText,
            tokenCount,
            maxTokens);
        if (*tokenCount >= maxTokens)
        {
            break;
        }
    }
}

std::string BuildItemRawProbe(MyGUI::Widget* itemWidget)
{
    std::string probeText;
    std::size_t tokenCount = 0;
    BuildItemRawProbeRecursive(itemWidget, 0, 4, &probeText, &tokenCount, 56);
    return probeText;
}
