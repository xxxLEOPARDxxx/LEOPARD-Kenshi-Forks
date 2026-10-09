#include "vs_parse.h"

#include <cctype>
#include <sstream>

namespace vs_parse
{

std::string ToLowerAsciiCopy(const std::string& value)
{
    std::string lowered = value;
    for (size_t i = 0; i < lowered.size(); ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

void SkipWhitespace(const std::string& body, size_t* pos)
{
    if (!pos)
    {
        return;
    }

    while (*pos < body.size() && std::isspace(static_cast<unsigned char>(body[*pos])) != 0)
    {
        ++(*pos);
    }
}

bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);

    if (pos + 4 <= body.size() && body.compare(pos, 4, "true") == 0)
    {
        *valueOut = true;
        return true;
    }

    if (pos + 5 <= body.size() && body.compare(pos, 5, "false") == 0)
    {
        *valueOut = false;
        return true;
    }

    return false;
}

bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || std::isdigit(static_cast<unsigned char>(body[pos])) == 0)
    {
        return false;
    }

    size_t end = pos;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0)
    {
        ++end;
    }

    DWORD parsed = 0;
    try
    {
        parsed = static_cast<DWORD>(std::stoul(body.substr(pos, end - pos)));
    }
    catch (...)
    {
        return false;
    }

    *valueOut = parsed;
    return true;
}

bool ParseStringFromJson(const std::string& body, const char* keyName, std::string* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != '"')
    {
        return false;
    }

    ++pos;
    std::string parsed;
    while (pos < body.size())
    {
        const char ch = body[pos];
        if (ch == '"')
        {
            *valueOut = parsed;
            return true;
        }
        if (ch == '\\')
        {
            ++pos;
            if (pos >= body.size())
            {
                return false;
            }
            const char esc = body[pos];
            if (esc == '"' || esc == '\\' || esc == '/')
            {
                parsed.push_back(esc);
            }
            else if (esc == 'b')
            {
                parsed.push_back('\b');
            }
            else if (esc == 'f')
            {
                parsed.push_back('\f');
            }
            else if (esc == 'n')
            {
                parsed.push_back('\n');
            }
            else if (esc == 'r')
            {
                parsed.push_back('\r');
            }
            else if (esc == 't')
            {
                parsed.push_back('\t');
            }
            else
            {
                return false;
            }
        }
        else
        {
            parsed.push_back(ch);
        }
        ++pos;
    }

    return false;
}

bool TryParseHexNibble(char value, unsigned int* nibbleOut)
{
    if (!nibbleOut)
    {
        return false;
    }

    if (value >= '0' && value <= '9')
    {
        *nibbleOut = static_cast<unsigned int>(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f')
    {
        *nibbleOut = static_cast<unsigned int>(10 + (value - 'a'));
        return true;
    }
    if (value >= 'A' && value <= 'F')
    {
        *nibbleOut = static_cast<unsigned int>(10 + (value - 'A'));
        return true;
    }
    return false;
}

bool TryParseHexByte(const std::string& value, size_t pos, unsigned int* byteOut)
{
    if (!byteOut || pos + 1 >= value.size())
    {
        return false;
    }

    unsigned int hi = 0;
    unsigned int lo = 0;
    if (!TryParseHexNibble(value[pos], &hi) || !TryParseHexNibble(value[pos + 1], &lo))
    {
        return false;
    }

    *byteOut = (hi << 4) | lo;
    return true;
}

bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour* colourOut)
{
    if (!colourOut)
    {
        return false;
    }

    std::string value = TrimAscii(rawValue);
    if (value.empty())
    {
        return false;
    }

    if (!value.empty() && value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned int a = 255;
    if (!TryParseHexByte(value, 0, &r) ||
        !TryParseHexByte(value, 2, &g) ||
        !TryParseHexByte(value, 4, &b))
    {
        return false;
    }
    if (value.size() == 8 && !TryParseHexByte(value, 6, &a))
    {
        return false;
    }

    *colourOut = MyGUI::Colour(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f);
    return true;
}

std::string ColourToHexRgb(const MyGUI::Colour& colour)
{
    auto toByte = [](float channel) -> unsigned int
    {
        if (channel < 0.0f)
        {
            channel = 0.0f;
        }
        if (channel > 1.0f)
        {
            channel = 1.0f;
        }
        return static_cast<unsigned int>(channel * 255.0f + 0.5f);
    };

    const unsigned int r = toByte(colour.red);
    const unsigned int g = toByte(colour.green);
    const unsigned int b = toByte(colour.blue);

    std::stringstream ss;
    ss << "#";
    const char* kHex = "0123456789ABCDEF";
    ss << kHex[(r >> 4) & 0xF] << kHex[r & 0xF]
       << kHex[(g >> 4) & 0xF] << kHex[g & 0xF]
       << kHex[(b >> 4) & 0xF] << kHex[b & 0xF];
    return ss.str();
}

} // namespace vs_parse
