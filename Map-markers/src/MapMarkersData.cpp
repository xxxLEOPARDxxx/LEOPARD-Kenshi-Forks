#include "MapMarkersInternal.h"

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Widget.h>

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace
{
bool IsMarkerLabelTokenSeparator(MyGUI::UString::unicode_char value)
{
    if (value < 0x80u)
    {
        const unsigned char byte = static_cast<unsigned char>(value);
        return byte == ':' || std::isspace(byte) != 0 || std::isalnum(byte) == 0;
    }

    return false;
}

bool TryParseHexNibble(char value, unsigned int* nibbleOut)
{
    if (nibbleOut == 0)
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

bool TryParseHexByte(const std::string& value, std::string::size_type pos, unsigned int* byteOut)
{
    if (byteOut == 0 || pos + 1 >= value.size())
    {
        return false;
    }

    unsigned int highNibble = 0;
    unsigned int lowNibble = 0;
    if (!TryParseHexNibble(value[pos], &highNibble) || !TryParseHexNibble(value[pos + 1], &lowNibble))
    {
        return false;
    }

    *byteOut = (highNibble << 4) | lowNibble;
    return true;
}
}

std::string ToLowerAscii(const std::string& value)
{
    std::string lowered(value);
    for (std::string::size_type index = 0; index < lowered.size(); ++index)
    {
        lowered[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[index])));
    }
    return lowered;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    const std::string needleLower = ToLowerAscii(needle);
    return ToLowerAscii(haystack).find(needleLower) != std::string::npos;
}

int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

std::string TrimAscii(const std::string& value)
{
    std::string::size_type start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
    {
        ++start;
    }

    std::string::size_type end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(start, end - start);
}

std::string SanitizeMarkerLabel(const std::string& value, bool trimTrailingSpaces, std::size_t maximumLength)
{
    std::string sanitized;
    sanitized.reserve(value.size());

    bool previousWasSpace = false;
    for (std::string::size_type index = 0; index < value.size(); ++index)
    {
        unsigned char ch = static_cast<unsigned char>(value[index]);
        char out = static_cast<char>(ch);

        if (ch == '\r' || ch == '\n' || ch == '\t')
        {
            out = ' ';
        }
        else if (out == '[' || out == '{')
        {
            out = '(';
        }
        else if (out == ']' || out == '}')
        {
            out = ')';
        }
        else if (ch < 32)
        {
            continue;
        }

        if (out == ' ')
        {
            if (sanitized.empty() || previousWasSpace)
            {
                continue;
            }
            previousWasSpace = true;
        }
        else
        {
            previousWasSpace = false;
        }

        sanitized.push_back(out);
        if (sanitized.size() >= maximumLength)
        {
            break;
        }
    }

    while (trimTrailingSpaces && !sanitized.empty() && sanitized[sanitized.size() - 1] == ' ')
    {
        sanitized.erase(sanitized.size() - 1);
    }

    return sanitized;
}

std::size_t FindPreviousMarkerLabelTokenBoundary(const MyGUI::UString& text, std::size_t cursor)
{
    const std::size_t length = text.size();
    if (cursor > length)
    {
        cursor = length;
    }

    while (cursor > 0u && IsMarkerLabelTokenSeparator(text[cursor - 1]))
    {
        --cursor;
    }

    while (cursor > 0u && !IsMarkerLabelTokenSeparator(text[cursor - 1]))
    {
        --cursor;
    }

    return cursor;
}

std::size_t FindNextMarkerLabelTokenBoundary(const MyGUI::UString& text, std::size_t cursor)
{
    const std::size_t length = text.size();
    if (cursor > length)
    {
        cursor = length;
    }

    std::size_t position = cursor;
    while (position < length && !IsMarkerLabelTokenSeparator(text[position]))
    {
        ++position;
    }

    while (position < length && IsMarkerLabelTokenSeparator(text[position]))
    {
        ++position;
    }

    return position;
}

bool IsInterestingMarkerLabelShortcutKey(MyGUI::KeyCode keyCode)
{
    return keyCode.getValue() == MyGUI::KeyCode::ArrowLeft
        || keyCode.getValue() == MyGUI::KeyCode::ArrowRight
        || keyCode.getValue() == MyGUI::KeyCode::Backspace;
}

bool IsMarkerLabelConfirmKey(MyGUI::KeyCode keyCode)
{
    return keyCode.getValue() == MyGUI::KeyCode::Return
        || keyCode.getValue() == MyGUI::KeyCode::NumpadEnter;
}

const char* MarkerTypeToJsonValue(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Danger:
        return "danger";
    case MarkerType_Stash:
        return "stash";
    case MarkerType_Ruin:
        return "ruin";
    case MarkerType_Mine:
        return "mine";
    case MarkerType_Base:
        return "base";
    case MarkerType_Trader:
        return "trader";
    case MarkerType_SafeSpot:
        return "safe_spot";
    case MarkerType_Quest:
        return "quest";
    case MarkerType_Todo:
        return "todo";
    case MarkerType_Note:
    default:
        return "note";
    }
}

const char* MarkerTypeToDisplayName(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Danger:
        return "Danger";
    case MarkerType_Stash:
        return "Stash";
    case MarkerType_Ruin:
        return "Ruin";
    case MarkerType_Mine:
        return "Mine / Resource";
    case MarkerType_Base:
        return "Base / Outpost";
    case MarkerType_Trader:
        return "Trader / Shop";
    case MarkerType_SafeSpot:
        return "Safe Spot / Bed / Recovery";
    case MarkerType_Quest:
        return "Quest";
    case MarkerType_Todo:
        return "Todo";
    case MarkerType_Note:
    default:
        return "Note";
    }
}

const char* MarkerTypeToGlyph(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Danger:
        return "!";
    case MarkerType_Stash:
        return "s";
    case MarkerType_Ruin:
        return "r";
    case MarkerType_Mine:
        return "m";
    case MarkerType_Base:
        return "b";
    case MarkerType_Trader:
        return "$";
    case MarkerType_SafeSpot:
        return "+";
    case MarkerType_Quest:
        return "q";
    case MarkerType_Todo:
        return "t";
    case MarkerType_Note:
    default:
        return "n";
    }
}

MarkerType MarkerTypeFromString(const std::string& value)
{
    const std::string lowered = ToLowerAscii(value);
    if (lowered == "danger")
    {
        return MarkerType_Danger;
    }
    if (lowered == "stash")
    {
        return MarkerType_Stash;
    }
    if (lowered == "ruin")
    {
        return MarkerType_Ruin;
    }
    if (lowered == "mine" || lowered == "resource" || lowered == "mine_resource")
    {
        return MarkerType_Mine;
    }
    if (lowered == "base" || lowered == "outpost" || lowered == "base_outpost")
    {
        return MarkerType_Base;
    }
    if (lowered == "trader" || lowered == "shop" || lowered == "trader_shop")
    {
        return MarkerType_Trader;
    }
    if (lowered == "safe_spot" || lowered == "safe" || lowered == "recovery")
    {
        return MarkerType_SafeSpot;
    }
    if (lowered == "quest")
    {
        return MarkerType_Quest;
    }
    if (lowered == "todo")
    {
        return MarkerType_Todo;
    }
    return MarkerType_Note;
}

int MarkerTypeToIndex(MarkerType type)
{
    return static_cast<int>(type);
}

MarkerType MarkerTypeFromIndex(int value)
{
    if (value < static_cast<int>(MarkerType_Note)
        || value > static_cast<int>(MarkerType_Todo))
    {
        return MarkerType_Note;
    }

    return static_cast<MarkerType>(value);
}

MarkerType GetNextMarkerType(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Note:
        return MarkerType_Danger;
    case MarkerType_Danger:
        return MarkerType_Stash;
    case MarkerType_Stash:
        return MarkerType_Ruin;
    case MarkerType_Ruin:
        return MarkerType_Mine;
    case MarkerType_Mine:
        return MarkerType_Base;
    case MarkerType_Base:
        return MarkerType_Trader;
    case MarkerType_Trader:
        return MarkerType_SafeSpot;
    case MarkerType_SafeSpot:
        return MarkerType_Quest;
    case MarkerType_Quest:
        return MarkerType_Todo;
    case MarkerType_Todo:
    default:
        return MarkerType_Note;
    }
}

MyGUI::Colour BuildMarkerColour(MarkerType type, bool selected)
{
    switch (type)
    {
    case MarkerType_Danger:
        return selected
            ? MyGUI::Colour(1.0f, 0.45f, 0.40f, 1.0f)
            : MyGUI::Colour(0.88f, 0.18f, 0.18f, 1.0f);
    case MarkerType_Stash:
        return selected
            ? MyGUI::Colour(0.44f, 0.70f, 1.0f, 1.0f)
            : MyGUI::Colour(0.18f, 0.42f, 0.95f, 1.0f);
    case MarkerType_Ruin:
        return selected
            ? MyGUI::Colour(1.0f, 0.96f, 0.38f, 1.0f)
            : MyGUI::Colour(0.92f, 0.82f, 0.18f, 1.0f);
    case MarkerType_Mine:
        return selected
            ? MyGUI::Colour(1.0f, 0.72f, 0.30f, 1.0f)
            : MyGUI::Colour(0.95f, 0.54f, 0.16f, 1.0f);
    case MarkerType_Base:
        return selected
            ? MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f)
            : MyGUI::Colour(0.84f, 0.84f, 0.84f, 1.0f);
    case MarkerType_Trader:
        return selected
            ? MyGUI::Colour(0.48f, 1.0f, 1.0f, 1.0f)
            : MyGUI::Colour(0.18f, 0.84f, 0.84f, 1.0f);
    case MarkerType_SafeSpot:
        return selected
            ? MyGUI::Colour(0.78f, 1.0f, 0.68f, 1.0f)
            : MyGUI::Colour(0.52f, 0.90f, 0.40f, 1.0f);
    case MarkerType_Quest:
        return selected
            ? MyGUI::Colour(0.82f, 0.52f, 1.0f, 1.0f)
            : MyGUI::Colour(0.60f, 0.30f, 0.88f, 1.0f);
    case MarkerType_Todo:
        return selected
            ? MyGUI::Colour(0.78f, 0.78f, 0.78f, 1.0f)
            : MyGUI::Colour(0.54f, 0.54f, 0.54f, 1.0f);
    case MarkerType_Note:
    default:
        return selected
            ? MyGUI::Colour(0.58f, 1.0f, 0.58f, 1.0f)
            : MyGUI::Colour(0.18f, 0.78f, 0.20f, 1.0f);
    }
}

std::string BuildMarkerEditorHeader(const MarkerState& marker)
{
    std::stringstream caption;
    caption << "Marker " << marker.id;
    if (!marker.label.empty())
    {
        caption << ": " << marker.label;
    }
    return caption.str();
}

int BuildMarkerLabelDisplayWidth(
    const std::string& label,
    int minimumWidth,
    int maximumWidth,
    int horizontalPadding)
{
    int width = minimumWidth;
    if (!label.empty())
    {
        int textWidth = 0;
        for (std::string::size_type index = 0; index < label.size(); ++index)
        {
            const unsigned char ch = static_cast<unsigned char>(label[index]);
            if (ch == ' ')
            {
                textWidth += 4;
            }
            else if (std::islower(ch))
            {
                textWidth += 6;
            }
            else if (std::isupper(ch) || std::isdigit(ch) || ch == '!' || ch == '+' || ch == '$')
            {
                textWidth += 7;
            }
            else
            {
                textWidth += 5;
            }
        }

        width = textWidth + (horizontalPadding * 2) + 4;
    }

    return ClampInt(width, minimumWidth, maximumWidth);
}

std::string BuildMarkerHoverDisplayText(const std::string& caption)
{
    return caption;
}

std::string BuildMarkerHoverCaption(const MarkerState& marker)
{
    std::stringstream caption;
    caption << MarkerTypeToDisplayName(marker.type);
    if (!marker.label.empty())
    {
        caption << ": " << marker.label;
    }
    return caption.str();
}

std::string JsonEscapeString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (std::string::size_type index = 0; index < value.size(); ++index)
    {
        const char ch = value[index];
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

std::string JoinWindowsPath(const std::string& directory, const char* fileName)
{
    if (directory.empty() || fileName == 0 || *fileName == '\0')
    {
        return "";
    }

    if (directory[directory.size() - 1] == '\\' || directory[directory.size() - 1] == '/')
    {
        return directory + fileName;
    }

    return directory + "\\" + fileName;
}

std::string NormalizePathForComparison(const std::string& path)
{
    std::string normalized(path);
    for (std::string::size_type index = 0; index < normalized.size(); ++index)
    {
        char ch = normalized[index];
        if (ch == '/')
        {
            normalized[index] = '\\';
        }
        else
        {
            normalized[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    return normalized;
}

bool PathsEqualIgnoreCase(const std::string& left, const std::string& right)
{
    return NormalizePathForComparison(left) == NormalizePathForComparison(right);
}

std::string GetParentDirectoryPath(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        return "";
    }

    return path.substr(0, separator);
}

std::string GetPathLeafName(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        return path;
    }

    return path.substr(separator + 1);
}

bool ExtractJsonFloatField(const std::string& contents, const char* key, float& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    char* parseEnd = 0;
    const double parsedValue = std::strtod(contents.c_str() + valuePos, &parseEnd);
    if (parseEnd == contents.c_str() + valuePos)
    {
        return false;
    }

    valueOut = ClampFloat(static_cast<float>(parsedValue), 0.0f, 1.0f);
    return true;
}

bool ExtractJsonIntField(const std::string& contents, const char* key, int& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    char* parseEnd = 0;
    const long parsedValue = std::strtol(contents.c_str() + valuePos, &parseEnd, 10);
    if (parseEnd == contents.c_str() + valuePos)
    {
        return false;
    }

    valueOut = static_cast<int>(parsedValue);
    return true;
}

bool ExtractJsonBoolField(const std::string& contents, const char* key, bool& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    if (contents.compare(valuePos, 4, "true") == 0)
    {
        valueOut = true;
        return true;
    }

    if (contents.compare(valuePos, 5, "false") == 0)
    {
        valueOut = false;
        return true;
    }

    return false;
}

bool ExtractJsonStringField(const std::string& contents, const char* key, std::string& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    if (valuePos >= contents.size() || contents[valuePos] != '"')
    {
        return false;
    }

    std::string parsed;
    bool escaping = false;
    for (std::string::size_type index = valuePos + 1; index < contents.size(); ++index)
    {
        const char ch = contents[index];
        if (escaping)
        {
            switch (ch)
            {
            case 'n':
                parsed.push_back('\n');
                break;
            case 'r':
                parsed.push_back('\r');
                break;
            case 't':
                parsed.push_back('\t');
                break;
            case '\\':
            case '"':
                parsed.push_back(ch);
                break;
            default:
                parsed.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\')
        {
            escaping = true;
            continue;
        }

        if (ch == '"')
        {
            valueOut = parsed;
            return true;
        }

        parsed.push_back(ch);
    }

    return false;
}

bool TryExtractJsonArrayContents(const std::string& contents, const char* key, std::string& arrayContentsOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type arrayStart = colonPos + 1;
    while (arrayStart < contents.size() && std::isspace(static_cast<unsigned char>(contents[arrayStart])))
    {
        ++arrayStart;
    }

    if (arrayStart >= contents.size() || contents[arrayStart] != '[')
    {
        return false;
    }

    int depth = 0;
    for (std::string::size_type index = arrayStart; index < contents.size(); ++index)
    {
        if (contents[index] == '[')
        {
            ++depth;
        }
        else if (contents[index] == ']')
        {
            --depth;
            if (depth == 0)
            {
                arrayContentsOut = contents.substr(arrayStart + 1, index - arrayStart - 1);
                return true;
            }
        }
    }

    return false;
}

bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour& colourOut)
{
    std::string value = TrimAscii(rawValue);
    if (value.empty())
    {
        return false;
    }

    if (value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
    unsigned int alpha = 255;
    if (!TryParseHexByte(value, 0, &red)
        || !TryParseHexByte(value, 2, &green)
        || !TryParseHexByte(value, 4, &blue))
    {
        return false;
    }

    if (value.size() == 8 && !TryParseHexByte(value, 6, &alpha))
    {
        return false;
    }

    colourOut = MyGUI::Colour(
        static_cast<float>(red) / 255.0f,
        static_cast<float>(green) / 255.0f,
        static_cast<float>(blue) / 255.0f,
        static_cast<float>(alpha) / 255.0f);
    return true;
}

std::string BuildColourHexString(const MyGUI::Colour& colour)
{
    const char* kHexDigits = "0123456789ABCDEF";
    const auto toByte = [](float channel) -> unsigned int
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

    const unsigned int red = toByte(colour.red);
    const unsigned int green = toByte(colour.green);
    const unsigned int blue = toByte(colour.blue);
    const unsigned int alpha = toByte(colour.alpha);

    std::string value("#");
    value.push_back(kHexDigits[(red >> 4) & 0xFu]);
    value.push_back(kHexDigits[red & 0xFu]);
    value.push_back(kHexDigits[(green >> 4) & 0xFu]);
    value.push_back(kHexDigits[green & 0xFu]);
    value.push_back(kHexDigits[(blue >> 4) & 0xFu]);
    value.push_back(kHexDigits[blue & 0xFu]);
    if (alpha < 255u)
    {
        value.push_back(kHexDigits[(alpha >> 4) & 0xFu]);
        value.push_back(kHexDigits[alpha & 0xFu]);
    }

    return value;
}
