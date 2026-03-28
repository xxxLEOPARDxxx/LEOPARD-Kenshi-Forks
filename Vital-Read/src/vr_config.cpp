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
const DWORD kDefaultRecoveryComaIconSizePx = 64u;
const DWORD kDefaultDyingIconSizePx = 64u;
const DWORD kDefaultPlayingDeadIconSizePx = 64u;
const DWORD kDefaultStarvingIconSizePx = 64u;
const DWORD kDefaultCrippledArmIconSizePx = 64u;
const DWORD kDefaultCrippledLegIconSizePx = 64u;
const DWORD kDefaultPortraitIconDisplaySizePx = 0u;
const DWORD kDefaultPortraitTextFontHeightPx = 14u;
const int kDefaultPortraitOverlayMarginXPx = 0;
const int kDefaultPortraitOverlayMarginYPx = 0;
const DWORD kMinPortraitIconDisplaySizePx = 0u;
const DWORD kMaxPortraitIconDisplaySizePx = 64u;
const DWORD kMinIconSizePx = 8u;
const DWORD kMaxIconSizePx = 512u;
const DWORD kMinPortraitTextFontHeightPx = 8u;
const DWORD kMaxPortraitTextFontHeightPx = 48u;
const int kMinPortraitOverlayMarginPx = -16;
const int kMaxPortraitOverlayMarginPx = 16;
const char* kDefaultRecoveryComaIconTexture = "gui/gfx/heart_64px_opt.png";
const char* kDefaultDyingIconTexture = "gui/gfx/death_64px_opt.png";
const char* kDefaultPlayingDeadIconTexture = "";
const char* kDefaultStarvingIconTexture = "gui/gfx/starving_64px_opt.png";
const char* kDefaultCrippledArmIconTexture = "gui/gfx/broken-arm_64px_opt.png";
const char* kDefaultCrippledLegIconTexture = "gui/gfx/broken-leg_64px_opt.png";
const char* kDefaultPortraitIconAnchor = "bottom_left";
const char* kDefaultPortraitTextAnchor = "top_right";

struct StateIconConfigBinding
{
    StateIconConfigBinding(
        const char* keyPrefixValue,
        std::string* textureValue,
        DWORD* sizePxValue,
        bool* hasImageCoordValue,
        int* coordLeftValue,
        int* coordTopValue,
        int* coordWidthValue,
        int* coordHeightValue)
        : keyPrefix(keyPrefixValue)
        , texture(textureValue)
        , sizePx(sizePxValue)
        , hasImageCoord(hasImageCoordValue)
        , coordLeft(coordLeftValue)
        , coordTop(coordTopValue)
        , coordWidth(coordWidthValue)
        , coordHeight(coordHeightValue)
    {
    }

    const char* keyPrefix;
    std::string* texture;
    DWORD* sizePx;
    bool* hasImageCoord;
    int* coordLeft;
    int* coordTop;
    int* coordWidth;
    int* coordHeight;
};

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

int ClampSigned(int value, int minimum, int maximum)
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

bool TryParseJsonIntByKey(const std::string& content, const char* key, int* outValue)
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

    bool negative = false;
    if (valuePos < content.size() && content[valuePos] == '-')
    {
        negative = true;
        ++valuePos;
    }

    int value = 0;
    bool parsedDigit = false;
    while (valuePos < content.size())
    {
        const unsigned char ch = static_cast<unsigned char>(content[valuePos]);
        if (!std::isdigit(ch))
        {
            break;
        }

        parsedDigit = true;
        value = (value * 10) + static_cast<int>(ch - '0');
        ++valuePos;
    }

    if (!parsedDigit)
    {
        return false;
    }

    *outValue = negative ? -value : value;
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

std::string BuildStateIconKey(const char* keyPrefix, const char* suffix)
{
    std::stringstream key;
    key << (keyPrefix == 0 ? "" : keyPrefix) << suffix;
    return key.str();
}

bool HasAnyCoordValue(const int coordLeft, const int coordTop, const int coordWidth, const int coordHeight)
{
    return coordLeft != 0
        || coordTop != 0
        || coordWidth != 0
        || coordHeight != 0;
}

void WriteStateIconConfig(
    std::ostream& out,
    const char* keyPrefix,
    const std::string& texture,
    const DWORD sizePx,
    const bool hasImageCoord,
    const int coordLeft,
    const int coordTop,
    const int coordWidth,
    const int coordHeight,
    const bool withTrailingComma)
{
    out << "  \"" << BuildStateIconKey(keyPrefix, "Texture") << "\": ";
    WriteEscapedJsonString(out, texture);
    out << ",\n";
    out << "  \"" << BuildStateIconKey(keyPrefix, "SizePx") << "\": " << sizePx;

    const bool writeCoords = hasImageCoord || HasAnyCoordValue(coordLeft, coordTop, coordWidth, coordHeight);
    if (writeCoords)
    {
        out << ",\n";
        out << "  \"" << BuildStateIconKey(keyPrefix, "CoordLeft") << "\": " << coordLeft << ",\n";
        out << "  \"" << BuildStateIconKey(keyPrefix, "CoordTop") << "\": " << coordTop << ",\n";
        out << "  \"" << BuildStateIconKey(keyPrefix, "CoordWidth") << "\": " << coordWidth << ",\n";
        out << "  \"" << BuildStateIconKey(keyPrefix, "CoordHeight") << "\": " << coordHeight;
    }

    if (withTrailingComma)
    {
        out << ",";
    }
    out << "\n";
}

void LoadStateIconConfig(const std::string& content, const StateIconConfigBinding& binding)
{
    if (binding.texture == 0
        || binding.sizePx == 0
        || binding.hasImageCoord == 0
        || binding.coordLeft == 0
        || binding.coordTop == 0
        || binding.coordWidth == 0
        || binding.coordHeight == 0)
    {
        return;
    }

    std::string parsedString;
    if (TryParseJsonStringByKey(content, BuildStateIconKey(binding.keyPrefix, "Texture").c_str(), &parsedString))
    {
        *binding.texture = TrimAscii(parsedString);
    }

    DWORD parsedUnsigned = 0u;
    if (TryParseJsonUnsignedByKey(content, BuildStateIconKey(binding.keyPrefix, "SizePx").c_str(), &parsedUnsigned))
    {
        *binding.sizePx = ClampUnsigned(parsedUnsigned, kMinIconSizePx, kMaxIconSizePx);
    }

    DWORD coordValue = 0u;
    bool haveCoord = false;
    if (TryParseJsonUnsignedByKey(content, BuildStateIconKey(binding.keyPrefix, "CoordLeft").c_str(), &coordValue))
    {
        *binding.coordLeft = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, BuildStateIconKey(binding.keyPrefix, "CoordTop").c_str(), &coordValue))
    {
        *binding.coordTop = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, BuildStateIconKey(binding.keyPrefix, "CoordWidth").c_str(), &coordValue))
    {
        *binding.coordWidth = static_cast<int>(coordValue);
        haveCoord = true;
    }
    if (TryParseJsonUnsignedByKey(content, BuildStateIconKey(binding.keyPrefix, "CoordHeight").c_str(), &coordValue))
    {
        *binding.coordHeight = static_cast<int>(coordValue);
        haveCoord = true;
    }

    *binding.hasImageCoord =
        haveCoord
        && *binding.coordWidth > 0
        && *binding.coordHeight > 0;
}

std::string BuildConfigText(const PluginConfig& config)
{
    std::stringstream out;
    out << "{\n";
    out << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";
    out << "  \"debugLogging\": " << (config.debugLogging ? "true" : "false") << ",\n";
    out << "  \"debugSearchLogging\": " << (config.debugSearchLogging ? "true" : "false") << ",\n";
    out << "  \"debugBindingLogging\": " << (config.debugBindingLogging ? "true" : "false") << ",\n";
    out << "  \"showIcons\": " << (config.showIcons ? "true" : "false") << ",\n";
    out << "  \"showText\": " << (config.showText ? "true" : "false") << ",\n";
    out << "  \"portraitIconDisplaySizePx\": " << config.portraitIconDisplaySizePx << ",\n";
    out << "  \"portraitTextFontHeightPx\": " << config.portraitTextFontHeightPx << ",\n";
    out << "  \"portraitOverlayMarginXPx\": " << config.portraitOverlayMarginXPx << ",\n";
    out << "  \"portraitOverlayMarginYPx\": " << config.portraitOverlayMarginYPx << ",\n";
    out << "  \"portraitIconAnchor\": ";
    WriteEscapedJsonString(out, config.portraitIconAnchor);
    out << ",\n";
    out << "  \"portraitTextAnchor\": ";
    WriteEscapedJsonString(out, config.portraitTextAnchor);
    out << ",\n";
    WriteStateIconConfig(
        out,
        "unconsciousIcon",
        config.unconsciousIconTexture,
        config.unconsciousIconSizePx,
        config.unconsciousIconHasImageCoord,
        config.unconsciousIconCoordLeft,
        config.unconsciousIconCoordTop,
        config.unconsciousIconCoordWidth,
        config.unconsciousIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "recoveryComaIcon",
        config.recoveryComaIconTexture,
        config.recoveryComaIconSizePx,
        config.recoveryComaIconHasImageCoord,
        config.recoveryComaIconCoordLeft,
        config.recoveryComaIconCoordTop,
        config.recoveryComaIconCoordWidth,
        config.recoveryComaIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "dyingIcon",
        config.dyingIconTexture,
        config.dyingIconSizePx,
        config.dyingIconHasImageCoord,
        config.dyingIconCoordLeft,
        config.dyingIconCoordTop,
        config.dyingIconCoordWidth,
        config.dyingIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "playingDeadIcon",
        config.playingDeadIconTexture,
        config.playingDeadIconSizePx,
        config.playingDeadIconHasImageCoord,
        config.playingDeadIconCoordLeft,
        config.playingDeadIconCoordTop,
        config.playingDeadIconCoordWidth,
        config.playingDeadIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "starvingIcon",
        config.starvingIconTexture,
        config.starvingIconSizePx,
        config.starvingIconHasImageCoord,
        config.starvingIconCoordLeft,
        config.starvingIconCoordTop,
        config.starvingIconCoordWidth,
        config.starvingIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "crippledArmIcon",
        config.crippledArmIconTexture,
        config.crippledArmIconSizePx,
        config.crippledArmIconHasImageCoord,
        config.crippledArmIconCoordLeft,
        config.crippledArmIconCoordTop,
        config.crippledArmIconCoordWidth,
        config.crippledArmIconCoordHeight,
        true);
    WriteStateIconConfig(
        out,
        "crippledLegIcon",
        config.crippledLegIconTexture,
        config.crippledLegIconSizePx,
        config.crippledLegIconHasImageCoord,
        config.crippledLegIconCoordLeft,
        config.crippledLegIconCoordTop,
        config.crippledLegIconCoordWidth,
        config.crippledLegIconCoordHeight,
        false);
    out << "}\n";
    return out.str();
}
}

PluginConfig::PluginConfig()
    : enabled(true)
    , debugLogging(false)
    , debugSearchLogging(false)
    , debugBindingLogging(false)
    , showIcons(true)
    , showText(true)
    , portraitIconDisplaySizePx(kDefaultPortraitIconDisplaySizePx)
    , portraitTextFontHeightPx(kDefaultPortraitTextFontHeightPx)
    , portraitOverlayMarginXPx(kDefaultPortraitOverlayMarginXPx)
    , portraitOverlayMarginYPx(kDefaultPortraitOverlayMarginYPx)
    , portraitIconAnchor(kDefaultPortraitIconAnchor)
    , portraitTextAnchor(kDefaultPortraitTextAnchor)
    , unconsciousIconTexture()
    , unconsciousIconSizePx(kDefaultUnconsciousIconSizePx)
    , unconsciousIconHasImageCoord(false)
    , unconsciousIconCoordLeft(0)
    , unconsciousIconCoordTop(0)
    , unconsciousIconCoordWidth(0)
    , unconsciousIconCoordHeight(0)
    , recoveryComaIconTexture(kDefaultRecoveryComaIconTexture)
    , recoveryComaIconSizePx(kDefaultRecoveryComaIconSizePx)
    , recoveryComaIconHasImageCoord(false)
    , recoveryComaIconCoordLeft(0)
    , recoveryComaIconCoordTop(0)
    , recoveryComaIconCoordWidth(0)
    , recoveryComaIconCoordHeight(0)
    , dyingIconTexture(kDefaultDyingIconTexture)
    , dyingIconSizePx(kDefaultDyingIconSizePx)
    , dyingIconHasImageCoord(false)
    , dyingIconCoordLeft(0)
    , dyingIconCoordTop(0)
    , dyingIconCoordWidth(0)
    , dyingIconCoordHeight(0)
    , playingDeadIconTexture(kDefaultPlayingDeadIconTexture)
    , playingDeadIconSizePx(kDefaultPlayingDeadIconSizePx)
    , playingDeadIconHasImageCoord(false)
    , playingDeadIconCoordLeft(0)
    , playingDeadIconCoordTop(0)
    , playingDeadIconCoordWidth(0)
    , playingDeadIconCoordHeight(0)
    , starvingIconTexture(kDefaultStarvingIconTexture)
    , starvingIconSizePx(kDefaultStarvingIconSizePx)
    , starvingIconHasImageCoord(false)
    , starvingIconCoordLeft(0)
    , starvingIconCoordTop(0)
    , starvingIconCoordWidth(0)
    , starvingIconCoordHeight(0)
    , crippledArmIconTexture(kDefaultCrippledArmIconTexture)
    , crippledArmIconSizePx(kDefaultCrippledArmIconSizePx)
    , crippledArmIconHasImageCoord(false)
    , crippledArmIconCoordLeft(0)
    , crippledArmIconCoordTop(0)
    , crippledArmIconCoordWidth(0)
    , crippledArmIconCoordHeight(0)
    , crippledLegIconTexture(kDefaultCrippledLegIconTexture)
    , crippledLegIconSizePx(kDefaultCrippledLegIconSizePx)
    , crippledLegIconHasImageCoord(false)
    , crippledLegIconCoordLeft(0)
    , crippledLegIconCoordTop(0)
    , crippledLegIconCoordWidth(0)
    , crippledLegIconCoordHeight(0)
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
    if (TryParseJsonBoolByKey(content, "showIcons", &parsedBool))
    {
        outConfig->showIcons = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "showText", &parsedBool))
    {
        outConfig->showText = parsedBool;
    }
    DWORD parsedUnsigned = 0u;
    if (TryParseJsonUnsignedByKey(content, "portraitIconDisplaySizePx", &parsedUnsigned))
    {
        outConfig->portraitIconDisplaySizePx = ClampUnsigned(
            parsedUnsigned,
            kMinPortraitIconDisplaySizePx,
            kMaxPortraitIconDisplaySizePx);
    }
    if (TryParseJsonUnsignedByKey(content, "portraitTextFontHeightPx", &parsedUnsigned))
    {
        outConfig->portraitTextFontHeightPx = ClampUnsigned(
            parsedUnsigned,
            kMinPortraitTextFontHeightPx,
            kMaxPortraitTextFontHeightPx);
    }
    int parsedSigned = 0;
    if (TryParseJsonIntByKey(content, "portraitOverlayMarginXPx", &parsedSigned))
    {
        outConfig->portraitOverlayMarginXPx = ClampSigned(
            parsedSigned,
            kMinPortraitOverlayMarginPx,
            kMaxPortraitOverlayMarginPx);
    }
    if (TryParseJsonIntByKey(content, "portraitOverlayMarginYPx", &parsedSigned))
    {
        outConfig->portraitOverlayMarginYPx = ClampSigned(
            parsedSigned,
            kMinPortraitOverlayMarginPx,
            kMaxPortraitOverlayMarginPx);
    }
    std::string parsedString;
    if (TryParseJsonStringByKey(content, "portraitIconAnchor", &parsedString))
    {
        outConfig->portraitIconAnchor = TrimAscii(parsedString);
    }
    if (TryParseJsonStringByKey(content, "portraitTextAnchor", &parsedString))
    {
        outConfig->portraitTextAnchor = TrimAscii(parsedString);
    }

    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "unconsciousIcon",
            &outConfig->unconsciousIconTexture,
            &outConfig->unconsciousIconSizePx,
            &outConfig->unconsciousIconHasImageCoord,
            &outConfig->unconsciousIconCoordLeft,
            &outConfig->unconsciousIconCoordTop,
            &outConfig->unconsciousIconCoordWidth,
            &outConfig->unconsciousIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "recoveryComaIcon",
            &outConfig->recoveryComaIconTexture,
            &outConfig->recoveryComaIconSizePx,
            &outConfig->recoveryComaIconHasImageCoord,
            &outConfig->recoveryComaIconCoordLeft,
            &outConfig->recoveryComaIconCoordTop,
            &outConfig->recoveryComaIconCoordWidth,
            &outConfig->recoveryComaIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "dyingIcon",
            &outConfig->dyingIconTexture,
            &outConfig->dyingIconSizePx,
            &outConfig->dyingIconHasImageCoord,
            &outConfig->dyingIconCoordLeft,
            &outConfig->dyingIconCoordTop,
            &outConfig->dyingIconCoordWidth,
            &outConfig->dyingIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "playingDeadIcon",
            &outConfig->playingDeadIconTexture,
            &outConfig->playingDeadIconSizePx,
            &outConfig->playingDeadIconHasImageCoord,
            &outConfig->playingDeadIconCoordLeft,
            &outConfig->playingDeadIconCoordTop,
            &outConfig->playingDeadIconCoordWidth,
            &outConfig->playingDeadIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "starvingIcon",
            &outConfig->starvingIconTexture,
            &outConfig->starvingIconSizePx,
            &outConfig->starvingIconHasImageCoord,
            &outConfig->starvingIconCoordLeft,
            &outConfig->starvingIconCoordTop,
            &outConfig->starvingIconCoordWidth,
            &outConfig->starvingIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "crippledArmIcon",
            &outConfig->crippledArmIconTexture,
            &outConfig->crippledArmIconSizePx,
            &outConfig->crippledArmIconHasImageCoord,
            &outConfig->crippledArmIconCoordLeft,
            &outConfig->crippledArmIconCoordTop,
            &outConfig->crippledArmIconCoordWidth,
            &outConfig->crippledArmIconCoordHeight));
    LoadStateIconConfig(
        content,
        StateIconConfigBinding(
            "crippledLegIcon",
            &outConfig->crippledLegIconTexture,
            &outConfig->crippledLegIconSizePx,
            &outConfig->crippledLegIconHasImageCoord,
            &outConfig->crippledLegIconCoordLeft,
            &outConfig->crippledLegIconCoordTop,
            &outConfig->crippledLegIconCoordWidth,
            &outConfig->crippledLegIconCoordHeight));

    return LOAD_OK;
}

bool SaveToFile(const std::string& path, const PluginConfig& config)
{
    return TryWriteTextFile(path, BuildConfigText(config));
}
}
