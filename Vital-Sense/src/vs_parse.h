#pragma once

#include "vs_types.h"

#include <string>

namespace vs_parse
{

std::string ToLowerAsciiCopy(const std::string& value);
std::string TrimAscii(const std::string& value);
void SkipWhitespace(const std::string& body, size_t* pos);
bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut);
bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut);
bool ParseStringFromJson(const std::string& body, const char* keyName, std::string* valueOut);
bool TryParseHexNibble(char value, unsigned int* nibbleOut);
bool TryParseHexByte(const std::string& value, size_t pos, unsigned int* byteOut);
bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour* colourOut);
std::string ColourToHexRgb(const MyGUI::Colour& colour);

} // namespace vs_parse
