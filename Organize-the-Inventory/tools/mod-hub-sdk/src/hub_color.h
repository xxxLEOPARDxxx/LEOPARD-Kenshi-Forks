#ifndef EMC_HUB_COLOR_H
#define EMC_HUB_COLOR_H

#include "emc/mod_hub_api.h"

#include <cctype>
#include <cstdint>
#include <string>

namespace hub_color
{
inline bool TryParseHexNibble(char value, uint8_t* out_nibble)
{
    if (out_nibble == nullptr)
    {
        return false;
    }

    if (value >= '0' && value <= '9')
    {
        *out_nibble = static_cast<uint8_t>(value - '0');
        return true;
    }

    if (value >= 'a' && value <= 'f')
    {
        *out_nibble = static_cast<uint8_t>(10 + (value - 'a'));
        return true;
    }

    if (value >= 'A' && value <= 'F')
    {
        *out_nibble = static_cast<uint8_t>(10 + (value - 'A'));
        return true;
    }

    return false;
}

inline char UppercaseHexDigit(char value)
{
    if (value >= 'a' && value <= 'f')
    {
        return static_cast<char>(value - ('a' - 'A'));
    }

    return value;
}

inline bool TryNormalizeColorHex(const char* value, std::string* out_hex)
{
    if (value == nullptr || out_hex == nullptr)
    {
        return false;
    }

    size_t start = 0u;
    while (value[start] != '\0' && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = start;
    while (value[end] != '\0')
    {
        ++end;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1u])) != 0)
    {
        --end;
    }

    if (start == end)
    {
        return false;
    }

    if (value[start] == '#')
    {
        ++start;
    }

    if ((end - start) != 6u)
    {
        return false;
    }

    std::string normalized;
    normalized.reserve(7u);
    normalized.push_back('#');

    for (size_t index = start; index < end; ++index)
    {
        uint8_t nibble = 0u;
        if (!TryParseHexNibble(value[index], &nibble))
        {
            return false;
        }

        normalized.push_back(UppercaseHexDigit(value[index]));
    }

    *out_hex = normalized;
    return true;
}

inline bool TryParseColorRgb(const char* value, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b)
{
    std::string normalized;
    if (!TryNormalizeColorHex(value, &normalized))
    {
        return false;
    }

    uint8_t channels[3] = { 0u, 0u, 0u };
    for (size_t channel_index = 0u; channel_index < 3u; ++channel_index)
    {
        const size_t digit_index = 1u + (channel_index * 2u);
        uint8_t hi = 0u;
        uint8_t lo = 0u;
        if (!TryParseHexNibble(normalized[digit_index], &hi)
            || !TryParseHexNibble(normalized[digit_index + 1u], &lo))
        {
            return false;
        }

        channels[channel_index] = static_cast<uint8_t>((hi << 4u) | lo);
    }

    if (out_r != nullptr)
    {
        *out_r = channels[0];
    }
    if (out_g != nullptr)
    {
        *out_g = channels[1];
    }
    if (out_b != nullptr)
    {
        *out_b = channels[2];
    }

    return true;
}

inline bool IsValidPreviewKind(uint32_t preview_kind)
{
    return preview_kind == EMC_COLOR_PREVIEW_KIND_SWATCH
        || preview_kind == EMC_COLOR_PREVIEW_KIND_TEXT;
}

inline const char* GetPreviewSampleText()
{
    return "Aa";
}

inline const EMC_ColorPresetV1* GetDefaultPalette(uint32_t* out_count)
{
    static const EMC_ColorPresetV1 kDefaultPalette[] = {
        { "#FFFFFF", 0 },
        { "#C9C9C9", 0 },
        { "#8C8C8C", 0 },
        { "#4D4D4D", 0 },
        { "#111111", 0 },
        { "#FF3333", 0 },
        { "#FF7A00", 0 },
        { "#FFD400", 0 },
        { "#DEE85A", 0 },
        { "#40FF40", 0 },
        { "#00B894", 0 },
        { "#00D2D3", 0 },
        { "#3399FF", 0 },
        { "#355CFF", 0 },
        { "#7A4CFF", 0 },
        { "#B84DFF", 0 },
        { "#FF4FD8", 0 },
        { "#FF6B9D", 0 },
        { "#B87333", 0 },
        { "#F5E6C8", 0 },
        { "#8C8CFF", 0 },
        { "#56CFE1", 0 },
        { "#2EC4B6", 0 },
        { "#F4A261", 0 },
        { "#E76F51", 0 } };

    if (out_count != nullptr)
    {
        *out_count = static_cast<uint32_t>(sizeof(kDefaultPalette) / sizeof(kDefaultPalette[0]));
    }

    return kDefaultPalette;
}
}

#endif
