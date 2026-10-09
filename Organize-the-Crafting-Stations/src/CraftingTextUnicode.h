#pragma once

#include <string>

namespace CraftingTextUnicode
{
std::string UppercaseUtf8OrAscii(const std::string& value);
std::string NormalizeSearchTextUtf8OrAscii(const std::string& text);
bool ContainsLetterUtf8OrAscii(const std::string& value);
bool ContainsDigitUtf8OrAscii(const std::string& value);
}
