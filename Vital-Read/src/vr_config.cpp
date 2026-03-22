#include "vr_config.h"

#include <cctype>
#include <fstream>
#include <ostream>
#include <sstream>

namespace vr_config
{
namespace
{
const DWORD kDefaultUnconsciousIconSizePx = 64u;
const DWORD kMinIconSizePx = 8u;
const DWORD kMaxIconSizePx = 512u;

bool TryReadTextFile(const std::string& path, std::string* outContent)
{
    if (outContent == 0)
    {
        return false;
    }

    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input)
    {
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        return false;
    }

    *outContent = buffer.str();
    return true;
}

bool TryWriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output << content;
    output.flush();
    return output.good();
}

std::string TrimAscii(const std::string& value)
{
    std::string::size_type start = 0u;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    std::string::size_type end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1u])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

DWORD ClampUnsigned(DWORD value, DWORD minimum, DWORD maximum)
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

bool TryParseJsonBoolByKey(const std::string& content, const char* key, bool* outValue)
{
    if (key == 0 || outValue == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = content.find(':', keyPos + needle.size());
    if (valuePos == std::string::npos)
    {
        return false;
    }

    ++valuePos;
    while (valuePos < content.size()
        && std::isspace(static_cast<unsigned char>(content[valuePos])) != 0)
    {
        ++valuePos;
    }

    if (content.compare(valuePos, 4u, "true") == 0)
    {
        *outValue = true;
        return true;
    }

    if (content.compare(valuePos, 5u, "false") == 0)
    {
        *outValue = false;
        return true;
    }

    return false;
}

bool TryParseJsonUnsignedByKey(const std::string& content, const char* key, DWORD* outValue)
{
    if (key == 0 || outValue == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = content.find(':', keyPos + needle.size());
    if (valuePos == std::string::npos)
    {
        return false;
    }

    ++valuePos;
    while (valuePos < content.size()
        && std::isspace(static_cast<unsigned char>(content[valuePos])) != 0)
    {
        ++valuePos;
    }

    DWORD value = 0u;
    bool parsedDigit = false;
    while (valuePos < content.size())
    {
        const unsigned char ch = static_cast<unsigned char>(content[valuePos]);
        if (!std::isdigit(ch))
        {
            break;
        }

        parsedDigit = true;
        value = (value * 10u) + static_cast<DWORD>(ch - '0');
        ++valuePos;
    }

    if (!parsedDigit)
    {
        return false;
    }

    *outValue = value;
    return true;
}

bool TryParseJsonStringByKey(const std::string& content, const char* key, std::string* outValue)
{
    if (key == 0 || outValue == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = content.find(':', keyPos + needle.size());
    if (valuePos == std::string::npos)
    {
        return false;
    }

    ++valuePos;
    while (valuePos < content.size()
        && std::isspace(static_cast<unsigned char>(content[valuePos])) != 0)
    {
        ++valuePos;
    }

    if (valuePos >= content.size() || content[valuePos] != '\"')
    {
        return false;
    }

    ++valuePos;
    std::string value;
    while (valuePos < content.size())
    {
        const char ch = content[valuePos++];
        if (ch == '\\')
        {
            if (valuePos >= content.size())
            {
                return false;
            }

            const char escaped = content[valuePos++];
            switch (escaped)
            {
            case '\\':
            case '\"':
            case '/':
                value.push_back(escaped);
                break;
            case 'b':
                value.push_back('\b');
                break;
            case 'f':
                value.push_back('\f');
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                value.push_back(escaped);
                break;
            }
            continue;
        }

        if (ch == '\"')
        {
            *outValue = value;
            return true;
        }

        value.push_back(ch);
    }

    return false;
}

void WriteEscapedJsonString(std::ostream& out, const std::string& value)
{
    out << '"';
    for (std::string::size_type index = 0u; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        switch (ch)
        {
        case '\"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (ch < 0x20u)
            {
                const char* kHex = "0123456789ABCDEF";
                out << "\\u00"
                    << kHex[(ch >> 4u) & 0xFu]
                    << kHex[ch & 0xFu];
            }
            else
            {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    out << '"';
}

std::string BuildConfigText(const PluginConfig& config)
{
    std::stringstream out;
    out << "{\n";
    out << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";
    out << "  \"debugLogging\": " << (config.debugLogging ? "true" : "false") << ",\n";
    out << "  \"debugSearchLogging\": " << (config.debugSearchLogging ? "true" : "false") << ",\n";
    out << "  \"debugBindingLogging\": " << (config.debugBindingLogging ? "true" : "false") << ",\n";
    out << "  \"unconsciousIconTexture\": ";
    WriteEscapedJsonString(out, config.unconsciousIconTexture);
    out << ",\n";
    out << "  \"unconsciousIconSizePx\": " << config.unconsciousIconSizePx << "\n";
    out << "}\n";
    return out.str();
}
}

PluginConfig::PluginConfig()
    : enabled(true)
    , debugLogging(false)
    , debugSearchLogging(false)
    , debugBindingLogging(false)
    , unconsciousIconTexture()
    , unconsciousIconSizePx(kDefaultUnconsciousIconSizePx)
    , unconsciousIconHasImageCoord(false)
    , unconsciousIconCoordLeft(0)
    , unconsciousIconCoordTop(0)
    , unconsciousIconCoordWidth(0)
    , unconsciousIconCoordHeight(0)
{
}

LoadStatus LoadFromFile(const std::string& path, PluginConfig* outConfig)
{
    if (outConfig == 0)
    {
        return LOAD_READ_FAILED;
    }

    std::string content;
    if (!TryReadTextFile(path, &content))
    {
        return LOAD_READ_FAILED;
    }

    bool parsedBool = false;
    if (TryParseJsonBoolByKey(content, "enabled", &parsedBool))
    {
        outConfig->enabled = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugLogging", &parsedBool))
    {
        outConfig->debugLogging = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugSearchLogging", &parsedBool))
    {
        outConfig->debugSearchLogging = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugBindingLogging", &parsedBool))
    {
        outConfig->debugBindingLogging = parsedBool;
    }

    std::string parsedString;
    if (TryParseJsonStringByKey(content, "unconsciousIconTexture", &parsedString))
    {
        outConfig->unconsciousIconTexture = TrimAscii(parsedString);
    }

    DWORD parsedUnsigned = 0u;
    if (TryParseJsonUnsignedByKey(content, "unconsciousIconSizePx", &parsedUnsigned))
    {
        outConfig->unconsciousIconSizePx = ClampUnsigned(parsedUnsigned, kMinIconSizePx, kMaxIconSizePx);
    }

    DWORD coordValue = 0u;
    bool haveCoord = false;
    if (TryParseJsonUnsignedByKey(content, "unconsciousIconCoordLeft", &coordValue))
    {
        outConfig->unconsciousIconCoordLeft = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, "unconsciousIconCoordTop", &coordValue))
    {
        outConfig->unconsciousIconCoordTop = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, "unconsciousIconCoordWidth", &coordValue))
    {
        outConfig->unconsciousIconCoordWidth = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, "unconsciousIconCoordHeight", &coordValue))
    {
        outConfig->unconsciousIconCoordHeight = static_cast<int>(coordValue);
        haveCoord = true;
    }

    outConfig->unconsciousIconHasImageCoord =
        haveCoord
        && outConfig->unconsciousIconCoordWidth > 0
        && outConfig->unconsciousIconCoordHeight > 0;

    return LOAD_OK;
}

bool SaveToFile(const std::string& path, const PluginConfig& config)
{
    return TryWriteTextFile(path, BuildConfigText(config));
}
}
