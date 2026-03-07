#include "TraderSearchText.h"

#include "TraderCore.h"

#include <kenshi/Character.h>
#include <kenshi/GameData.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
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
    std::string upper = value;
    for (std::size_t index = 0; index < upper.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(upper[index]);
        upper[index] = static_cast<char>(std::toupper(ch));
    }
    return upper;
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
    std::string normalized;
    normalized.reserve(text.size());

    bool previousWasSpace = true;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (std::isalnum(ch) == 0)
        {
            if (!normalized.empty() && !previousWasSpace)
            {
                normalized.push_back(' ');
                previousWasSpace = true;
            }
            continue;
        }

        normalized.push_back(static_cast<char>(std::tolower(ch)));
        previousWasSpace = false;
    }

    if (!normalized.empty() && normalized[normalized.size() - 1] == ' ')
    {
        normalized.resize(normalized.size() - 1);
    }

    return normalized;
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
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isalpha(ch) != 0)
        {
            return true;
        }
    }

    return false;
}

bool ContainsAsciiDigit(const std::string& value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isdigit(ch) != 0)
        {
            return true;
        }
    }

    return false;
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
