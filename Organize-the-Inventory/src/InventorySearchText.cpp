#include "InventorySearchText.h"

#include <kenshi/GameData.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cctype>
#include <vector>

namespace
{
bool ConvertUtf8ToWide(const std::string& value, std::wstring* out)
{
    if (out == 0)
    {
        return false;
    }

    out->clear();
    if (value.empty())
    {
        return true;
    }

    if (value.size() > 0x7fffffffU)
    {
        return false;
    }

    const int valueLength = static_cast<int>(value.size());
    const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), valueLength, 0, 0);
    if (required <= 0)
    {
        return false;
    }

    std::wstring converted;
    converted.resize(static_cast<std::size_t>(required));
    const int convertedLength =
        MultiByteToWideChar(CP_UTF8, 0, value.data(), valueLength, &converted[0], required);
    if (convertedLength != required)
    {
        return false;
    }

    out->swap(converted);
    return true;
}

bool ConvertWideToUtf8(const std::wstring& value, std::string* out)
{
    if (out == 0)
    {
        return false;
    }

    out->clear();
    if (value.empty())
    {
        return true;
    }

    if (value.size() > 0x7fffffffU)
    {
        return false;
    }

    const int valueLength = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(), valueLength, 0, 0, 0, 0);
    if (required <= 0)
    {
        return false;
    }

    std::string converted;
    converted.resize(static_cast<std::size_t>(required));
    const int convertedLength =
        WideCharToMultiByte(CP_UTF8, 0, value.data(), valueLength, &converted[0], required, 0, 0);
    if (convertedLength != required)
    {
        return false;
    }

    out->swap(converted);
    return true;
}

bool MapWideStringCase(const std::wstring& value, DWORD flags, std::wstring* out)
{
    if (out == 0)
    {
        return false;
    }

    out->clear();
    if (value.empty())
    {
        return true;
    }

    if (value.size() > 0x7fffffffU)
    {
        return false;
    }

    const LCID invariantLocale = MAKELCID(MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL), SORT_DEFAULT);
    const int valueLength = static_cast<int>(value.size());
    const int required = LCMapStringW(invariantLocale, flags, value.data(), valueLength, 0, 0);
    if (required <= 0)
    {
        return false;
    }

    std::wstring mapped;
    mapped.resize(static_cast<std::size_t>(required));
    const int mappedLength =
        LCMapStringW(invariantLocale, flags, value.data(), valueLength, &mapped[0], required);
    if (mappedLength != required)
    {
        return false;
    }

    out->swap(mapped);
    return true;
}

bool IsWideLetter(wchar_t value)
{
    WORD charType = 0;
    if (GetStringTypeW(CT_CTYPE1, &value, 1, &charType) == 0)
    {
        return false;
    }

    return (charType & C1_ALPHA) != 0;
}

bool IsWideDigit(wchar_t value)
{
    WORD charType = 0;
    if (GetStringTypeW(CT_CTYPE1, &value, 1, &charType) == 0)
    {
        return false;
    }

    return (charType & C1_DIGIT) != 0;
}

std::string NormalizeSearchTextAsciiFallback(const std::string& text)
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

bool ContainsLetterUtf8OrAscii(const std::string& value)
{
    std::wstring wideValue;
    if (ConvertUtf8ToWide(value, &wideValue))
    {
        for (std::size_t index = 0; index < wideValue.size(); ++index)
        {
            if (IsWideLetter(wideValue[index]))
            {
                return true;
            }
        }
    }

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

bool ContainsDigitUtf8OrAscii(const std::string& value)
{
    std::wstring wideValue;
    if (ConvertUtf8ToWide(value, &wideValue))
    {
        for (std::size_t index = 0; index < wideValue.size(); ++index)
        {
            if (IsWideDigit(wideValue[index]))
            {
                return true;
            }
        }
    }

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

std::string WidgetCaptionForSearch(MyGUI::Widget* widget)
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

bool ShouldIndexSearchToken(const std::string& token)
{
    if (token.empty())
    {
        return false;
    }

    const std::string normalized = NormalizeInventorySearchText(token);
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

    if (!ContainsLetterUtf8OrAscii(normalized) && ContainsDigitUtf8OrAscii(normalized))
    {
        return false;
    }

    return true;
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

void AppendUniqueNormalizedSearchToken(
    const std::string& token,
    std::vector<std::string>* seenNormalizedTokens,
    std::string* searchText)
{
    if (seenNormalizedTokens == 0 || searchText == 0 || token.empty())
    {
        return;
    }

    const std::string normalizedToken = NormalizeInventorySearchText(token);
    if (normalizedToken.empty())
    {
        return;
    }

    for (std::size_t index = 0; index < seenNormalizedTokens->size(); ++index)
    {
        if ((*seenNormalizedTokens)[index] == normalizedToken)
        {
            return;
        }
    }

    seenNormalizedTokens->push_back(normalizedToken);
    AppendSearchToken(searchText, token);
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
        if (!line.s1.empty() && ContainsLetterUtf8OrAscii(line.s1))
        {
            return line.s1;
        }
        if (!line.s2.empty() && ContainsLetterUtf8OrAscii(line.s2))
        {
            return line.s2;
        }
    }

    return "";
}

std::string BuildItemSearchSourceText(Item* item)
{
    if (item == 0)
    {
        return "";
    }

    std::string searchText;
    std::vector<std::string> seenNormalizedTokens;
    seenNormalizedTokens.reserve(8);

    AppendUniqueNormalizedSearchToken(ResolveCanonicalItemName(item), &seenNormalizedTokens, &searchText);
    AppendUniqueNormalizedSearchToken(item->displayName, &seenNormalizedTokens, &searchText);
    AppendUniqueNormalizedSearchToken(item->getName(), &seenNormalizedTokens, &searchText);
    if (item->data != 0)
    {
        AppendUniqueNormalizedSearchToken(item->data->name, &seenNormalizedTokens, &searchText);
        AppendUniqueNormalizedSearchToken(item->data->stringID, &seenNormalizedTokens, &searchText);
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
        AppendUniqueNormalizedSearchToken(line.s1, &seenNormalizedTokens, &searchText);
        AppendUniqueNormalizedSearchToken(line.s2, &seenNormalizedTokens, &searchText);
    }

    return searchText;
}

void AppendWidgetSearchTokensRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::string* searchText)
{
    if (widget == 0 || searchText == 0 || depth > maxDepth)
    {
        return;
    }

    AppendSearchToken(searchText, widget->getName());
    AppendSearchToken(searchText, WidgetCaptionForSearch(widget));

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    for (MyGUI::MapString::const_iterator it = userStrings.begin(); it != userStrings.end(); ++it)
    {
        AppendSearchToken(searchText, it->first);
        AppendSearchToken(searchText, it->second);
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        AppendWidgetSearchTokensRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            searchText);
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
T* ReadWidgetAnyDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T* internalPointer = ReadWidgetInternalDataPointer<T>(widget);
    if (internalPointer != 0)
    {
        return internalPointer;
    }

    return ReadWidgetUserDataPointer<T>(widget);
}

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
    int parsedQuantity = 0;
    const bool looksLikeQuantityWidget =
        widgetName.find("QuantityText") != std::string::npos
        || widgetName.find("quantity") != std::string::npos;
    if (looksLikeQuantityWidget && ParsePositiveIntFromText(WidgetCaptionForSearch(widget), &parsedQuantity))
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
}

std::string NormalizeInventorySearchText(const std::string& text)
{
    std::wstring wideText;
    std::wstring lowerText;
    if (!ConvertUtf8ToWide(text, &wideText)
        || !MapWideStringCase(wideText, LCMAP_LOWERCASE, &lowerText))
    {
        return NormalizeSearchTextAsciiFallback(text);
    }

    std::wstring normalizedWide;
    normalizedWide.reserve(lowerText.size());

    bool previousWasSpace = true;
    for (std::size_t index = 0; index < lowerText.size(); ++index)
    {
        const wchar_t ch = lowerText[index];
        if (!IsWideLetter(ch) && !IsWideDigit(ch))
        {
            if (!normalizedWide.empty() && !previousWasSpace)
            {
                normalizedWide.push_back(L' ');
                previousWasSpace = true;
            }
            continue;
        }

        normalizedWide.push_back(ch);
        previousWasSpace = false;
    }

    if (!normalizedWide.empty() && normalizedWide[normalizedWide.size() - 1] == L' ')
    {
        normalizedWide.resize(normalizedWide.size() - 1);
    }

    std::string normalized;
    if (ConvertWideToUtf8(normalizedWide, &normalized))
    {
        return normalized;
    }

    return NormalizeSearchTextAsciiFallback(text);
}

std::string BuildInventoryItemSearchText(MyGUI::Widget* itemWidget)
{
    return BuildInventoryItemSearchTextFromResolvedItem(
        itemWidget,
        ResolveInventoryWidgetItemPointer(itemWidget));
}

std::string BuildInventoryItemSearchTextFromResolvedItem(MyGUI::Widget* itemWidget, Item* resolvedItem)
{
    std::string searchText;
    if (itemWidget == 0)
    {
        return searchText;
    }

    if (resolvedItem != 0)
    {
        AppendSearchToken(&searchText, BuildItemSearchSourceText(resolvedItem));
    }

    AppendWidgetSearchTokensRecursive(itemWidget, 0, 5, &searchText);
    return searchText;
}

bool InventorySearchTextMatchesQuery(
    const std::string& searchableTextNormalized,
    const std::string& normalizedQuery)
{
    if (normalizedQuery.empty())
    {
        return true;
    }

    if (searchableTextNormalized.empty())
    {
        return true;
    }

    return searchableTextNormalized.find(normalizedQuery) != std::string::npos;
}

bool TryResolveInventoryItemQuantityFromWidget(MyGUI::Widget* itemWidget, int* outQuantity)
{
    if (outQuantity == 0)
    {
        return false;
    }

    *outQuantity = 0;
    return TryResolveItemQuantityFromWidgetRecursive(itemWidget, 0, 5, outQuantity);
}

Item* ResolveInventoryWidgetItemPointer(MyGUI::Widget* widget)
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
