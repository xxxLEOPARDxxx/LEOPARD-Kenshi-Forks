#pragma once

#include <istream>
#include <string>
#include <vector>

struct IniEntry
{
    int line;
    std::string key;
    std::string value;
};

// Lex a flat key=value stream. '#' starts a comment anywhere on a line, a
// UTF-8 BOM and trailing CR are stripped, and keys/values are trimmed.
// Section headers and lines without '=' are skipped with a line-numbered
// warning instead of failing the parse, so one bad line never costs the user
// the rest of the file. Warnings are prefixed "<line>: " and the caller adds
// the filename.
void IniLex(std::istream& in, std::vector<IniEntry>& out, std::vector<std::string>& warnings);

// Strict: the whole value must parse as an int, with no trailing text.
bool IniParseInt(const std::string& value, int& out);

// true/1/yes/on and false/0/no/off, case-insensitive. Returns false on
// anything else, leaving out untouched.
bool IniParseBool(const std::string& value, bool& out);
