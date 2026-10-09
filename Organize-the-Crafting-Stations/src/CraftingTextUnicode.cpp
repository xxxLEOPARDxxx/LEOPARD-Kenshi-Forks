#include "CraftingTextUnicode.h"

#include <Windows.h>

#include <cctype>
#include <cstddef>

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

std::string UpperAsciiCopyFallback(const std::string& value)
{
    std::string upper = value;
    for (std::size_t index = 0; index < upper.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(upper[index]);
        upper[index] = static_cast<char>(std::toupper(ch));
    }
    return upper;
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

bool ContainsAsciiLetterFallback(const std::string& value)
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

bool ContainsAsciiDigitFallback(const std::string& value)
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
}

namespace CraftingTextUnicode
{
std::string UppercaseUtf8OrAscii(const std::string& value)
{
    std::wstring wideValue;
    std::wstring wideUpper;
    std::string upper;
    if (ConvertUtf8ToWide(value, &wideValue)
        && MapWideStringCase(wideValue, LCMAP_UPPERCASE, &wideUpper)
        && ConvertWideToUtf8(wideUpper, &upper))
    {
        return upper;
    }

    return UpperAsciiCopyFallback(value);
}

std::string NormalizeSearchTextUtf8OrAscii(const std::string& text)
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

    return ContainsAsciiLetterFallback(value);
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

    return ContainsAsciiDigitFallback(value);
}
}
