#include "HiddenFactionRelationsConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ois/OISKeyboard.h>
#include <sstream>

namespace
{
const int kSearchInputWidthMin = 120;
const int kSearchInputWidthMax = 720;
const int kSearchInputHeightMin = 22;
const int kSearchInputHeightMax = 48;
const int kSearchInputWidthDefault = 270;
const int kSearchInputHeightDefault = 24;
const int kOpenMenuKeycodeDefault = static_cast<int>(OIS::KC_G);

std::string g_configPath;
HiddenFactionRelationsConfigSnapshot g_config;

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

    if (content.compare(valuePos, 4, "true") == 0)
    {
        *outValue = true;
        return true;
    }

    if (content.compare(valuePos, 5, "false") == 0)
    {
        *outValue = false;
        return true;
    }

    return false;
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

    std::string::size_type valueEnd = valuePos;
    if (valueEnd < content.size()
        && (content[valueEnd] == '-' || content[valueEnd] == '+'))
    {
        ++valueEnd;
    }

    while (valueEnd < content.size()
        && std::isdigit(static_cast<unsigned char>(content[valueEnd])) != 0)
    {
        ++valueEnd;
    }

    if (valueEnd == valuePos || (valueEnd == valuePos + 1 && (content[valuePos] == '-' || content[valuePos] == '+')))
    {
        return false;
    }

    std::stringstream valueText(content.substr(valuePos, valueEnd - valuePos));
    int parsedValue = 0;
    valueText >> parsedValue;
    if (!valueText || !valueText.eof())
    {
        return false;
    }

    *outValue = parsedValue;
    return true;
}

bool TryWriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}
}

HiddenFactionRelationsConfigSnapshot::HiddenFactionRelationsConfigSnapshot()
    : enabled(true)
    , debugLogging(false)
    , debugSearchLogging(false)
    , debugBindingLogging(false)
    , dumpHiddenFactionRelations(false)
    , autoFocusSearchOnOpen(true)
    , openMenuRequireCtrl(true)
    , openMenuRequireShift(false)
    , openMenuRequireAlt(true)
    , searchInputWidth(kSearchInputWidthDefault)
    , searchInputHeight(kSearchInputHeightDefault)
    , openMenuKeycode(kOpenMenuKeycodeDefault)
{
}

void HiddenFactionRelationsConfig_SetConfigPath(const std::string& configPath)
{
    g_configPath = configPath;
}

bool HiddenFactionRelationsConfig_Load(std::string* outError)
{
    g_config = HiddenFactionRelationsConfigSnapshot();

    if (g_configPath.empty())
    {
        if (outError != 0)
        {
            *outError = "config_path_unavailable";
        }
        return false;
    }

    std::string content;
    if (!TryReadTextFile(g_configPath, &content))
    {
        if (outError != 0)
        {
            *outError = "config_read_failed";
        }
        return false;
    }

    bool parsedBool = false;
    int parsedInt = 0;

    if (TryParseJsonBoolByKey(content, "enabled", &parsedBool))
    {
        g_config.enabled = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugLogging", &parsedBool))
    {
        g_config.debugLogging = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugSearchLogging", &parsedBool))
    {
        g_config.debugSearchLogging = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "debugBindingLogging", &parsedBool))
    {
        g_config.debugBindingLogging = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "dumpHiddenFactionRelations", &parsedBool))
    {
        g_config.dumpHiddenFactionRelations = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "autoFocusSearchOnOpen", &parsedBool))
    {
        g_config.autoFocusSearchOnOpen = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "openMenuRequireCtrl", &parsedBool))
    {
        g_config.openMenuRequireCtrl = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "openMenuRequireShift", &parsedBool))
    {
        g_config.openMenuRequireShift = parsedBool;
    }
    if (TryParseJsonBoolByKey(content, "openMenuRequireAlt", &parsedBool))
    {
        g_config.openMenuRequireAlt = parsedBool;
    }
    if (TryParseJsonIntByKey(content, "searchInputWidth", &parsedInt))
    {
        g_config.searchInputWidth = parsedInt;
    }
    if (TryParseJsonIntByKey(content, "searchInputHeight", &parsedInt))
    {
        g_config.searchInputHeight = parsedInt;
    }
    if (TryParseJsonIntByKey(content, "openMenuKeycode", &parsedInt))
    {
        g_config.openMenuKeycode = parsedInt;
    }

    HiddenFactionRelationsConfig_Normalize(&g_config);

    if (outError != 0)
    {
        outError->clear();
    }
    return true;
}

bool HiddenFactionRelationsConfig_Save(
    const HiddenFactionRelationsConfigSnapshot& snapshot,
    std::string* outError)
{
    if (g_configPath.empty())
    {
        if (outError != 0)
        {
            *outError = "config_path_unavailable";
        }
        return false;
    }

    HiddenFactionRelationsConfigSnapshot normalized = snapshot;
    HiddenFactionRelationsConfig_Normalize(&normalized);

    std::stringstream content;
    content << "{\n"
            << "  \"enabled\": " << HiddenFactionRelationsConfig_BoolToString(normalized.enabled) << ",\n"
            << "  \"debugLogging\": " << HiddenFactionRelationsConfig_BoolToString(normalized.debugLogging) << ",\n"
            << "  \"debugSearchLogging\": " << HiddenFactionRelationsConfig_BoolToString(normalized.debugSearchLogging) << ",\n"
            << "  \"debugBindingLogging\": " << HiddenFactionRelationsConfig_BoolToString(normalized.debugBindingLogging) << ",\n"
            << "  \"dumpHiddenFactionRelations\": " << HiddenFactionRelationsConfig_BoolToString(normalized.dumpHiddenFactionRelations) << ",\n"
            << "  \"autoFocusSearchOnOpen\": " << HiddenFactionRelationsConfig_BoolToString(normalized.autoFocusSearchOnOpen) << ",\n"
            << "  \"openMenuRequireCtrl\": " << HiddenFactionRelationsConfig_BoolToString(normalized.openMenuRequireCtrl) << ",\n"
            << "  \"openMenuRequireShift\": " << HiddenFactionRelationsConfig_BoolToString(normalized.openMenuRequireShift) << ",\n"
            << "  \"openMenuRequireAlt\": " << HiddenFactionRelationsConfig_BoolToString(normalized.openMenuRequireAlt) << ",\n"
            << "  \"searchInputWidth\": " << normalized.searchInputWidth << ",\n"
            << "  \"searchInputHeight\": " << normalized.searchInputHeight << ",\n"
            << "  \"openMenuKeycode\": " << normalized.openMenuKeycode << "\n"
            << "}\n";

    if (!TryWriteTextFile(g_configPath, content.str()))
    {
        if (outError != 0)
        {
            *outError = "config_write_failed";
        }
        return false;
    }

    g_config = normalized;
    if (outError != 0)
    {
        outError->clear();
    }
    return true;
}

HiddenFactionRelationsConfigSnapshot HiddenFactionRelationsConfig_Capture()
{
    return g_config;
}

void HiddenFactionRelationsConfig_Apply(const HiddenFactionRelationsConfigSnapshot& snapshot)
{
    g_config = snapshot;
    HiddenFactionRelationsConfig_Normalize(&g_config);
}

void HiddenFactionRelationsConfig_Normalize(HiddenFactionRelationsConfigSnapshot* snapshot)
{
    if (snapshot == 0)
    {
        return;
    }

    snapshot->searchInputWidth = std::max(
        kSearchInputWidthMin,
        std::min(kSearchInputWidthMax, snapshot->searchInputWidth));
    snapshot->searchInputHeight = std::max(
        kSearchInputHeightMin,
        std::min(kSearchInputHeightMax, snapshot->searchInputHeight));
    if (snapshot->openMenuKeycode < -1 || snapshot->openMenuKeycode > 255)
    {
        snapshot->openMenuKeycode = kOpenMenuKeycodeDefault;
    }
}

bool HiddenFactionRelationsConfig_IsDebugLoggingEnabled()
{
    return g_config.debugLogging;
}

bool HiddenFactionRelationsConfig_IsDebugSearchLoggingEnabled()
{
    return g_config.debugLogging && g_config.debugSearchLogging;
}

bool HiddenFactionRelationsConfig_IsDebugBindingLoggingEnabled()
{
    return g_config.debugLogging && g_config.debugBindingLogging;
}

bool HiddenFactionRelationsConfig_ShouldDumpHiddenFactionRelations()
{
    return g_config.dumpHiddenFactionRelations;
}

bool HiddenFactionRelationsConfig_ShouldAutoFocusSearchOnOpen()
{
    return g_config.autoFocusSearchOnOpen;
}

bool HiddenFactionRelationsConfig_ShouldRequireCtrlForOpenMenu()
{
    return g_config.openMenuRequireCtrl;
}

bool HiddenFactionRelationsConfig_ShouldRequireShiftForOpenMenu()
{
    return g_config.openMenuRequireShift;
}

bool HiddenFactionRelationsConfig_ShouldRequireAltForOpenMenu()
{
    return g_config.openMenuRequireAlt;
}

int HiddenFactionRelationsConfig_GetSearchInputWidth()
{
    return g_config.searchInputWidth;
}

int HiddenFactionRelationsConfig_GetSearchInputHeight()
{
    return g_config.searchInputHeight;
}

int HiddenFactionRelationsConfig_GetOpenMenuKeycode()
{
    return g_config.openMenuKeycode;
}

const char* HiddenFactionRelationsConfig_BoolToString(bool value)
{
    return value ? "true" : "false";
}
