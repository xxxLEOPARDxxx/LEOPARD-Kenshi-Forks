#pragma once

#include "MapMarkersModHub.h"

#include <cstddef>
#include <string>
#include <vector>

namespace MyGUI
{
class Colour;
class KeyCode;
class UString;
}

enum MarkerType
{
    MarkerType_Note = 0,
    MarkerType_Danger = 1,
    MarkerType_Stash = 2,
    MarkerType_Ruin = 3,
    MarkerType_Mine = 4,
    MarkerType_Base = 5,
    MarkerType_Trader = 6,
    MarkerType_SafeSpot = 7,
    MarkerType_Quest = 8,
    MarkerType_Todo = 9
};

struct MarkerState
{
    int id;
    float normalizedX;
    float normalizedY;
    MarkerType type;
    std::string label;
};

enum MarkerPersistenceLoadResult
{
    MarkerPersistenceLoadResult_PathUnavailable = 0,
    MarkerPersistenceLoadResult_MissingFile = 1,
    MarkerPersistenceLoadResult_LoadedArray = 2,
    MarkerPersistenceLoadResult_LoadedLegacySingle = 3,
    MarkerPersistenceLoadResult_InvalidFile = 4
};

struct MarkerPersistenceLoadAttempt
{
    MarkerPersistenceLoadResult result;
    std::vector<MarkerState> markers;
};

struct MapMarkersConfigFileData
{
    MapMarkersModConfigSnapshot snapshot;
    std::string hoverLabelTextColorHex;
    std::string hoverLabelBackgroundColorHex;
};

const std::size_t kMapMarkersLabelMaxLength = 48u;

std::string ToLowerAscii(const std::string& value);
bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle);
int ClampInt(int value, int minimum, int maximum);
float ClampFloat(float value, float minimum, float maximum);
std::string TrimAscii(const std::string& value);
std::string SanitizeMarkerLabel(const std::string& value, bool trimTrailingSpaces, std::size_t maximumLength);
std::size_t FindPreviousMarkerLabelTokenBoundary(const MyGUI::UString& text, std::size_t cursor);
std::size_t FindNextMarkerLabelTokenBoundary(const MyGUI::UString& text, std::size_t cursor);
bool IsInterestingMarkerLabelShortcutKey(MyGUI::KeyCode keyCode);
bool IsMarkerLabelConfirmKey(MyGUI::KeyCode keyCode);
const char* MarkerTypeToJsonValue(MarkerType type);
const char* MarkerTypeToDisplayName(MarkerType type);
const char* MarkerTypeToGlyph(MarkerType type);
MarkerType MarkerTypeFromString(const std::string& value);
int MarkerTypeToIndex(MarkerType type);
MarkerType MarkerTypeFromIndex(int value);
MarkerType GetNextMarkerType(MarkerType type);
MyGUI::Colour BuildMarkerColour(MarkerType type, bool selected);
std::string BuildMarkerEditorHeader(const MarkerState& marker);
int BuildMarkerLabelDisplayWidth(
    const std::string& label,
    int minimumWidth,
    int maximumWidth,
    int horizontalPadding);
std::string BuildMarkerHoverDisplayText(const std::string& caption);
std::string BuildMarkerHoverCaption(const MarkerState& marker);
std::string JsonEscapeString(const std::string& value);
std::string JoinWindowsPath(const std::string& directory, const char* fileName);
std::string NormalizePathForComparison(const std::string& path);
bool PathsEqualIgnoreCase(const std::string& left, const std::string& right);
std::string GetParentDirectoryPath(const std::string& path);
std::string GetPathLeafName(const std::string& path);
bool ExtractJsonFloatField(const std::string& contents, const char* key, float& valueOut);
bool ExtractJsonIntField(const std::string& contents, const char* key, int& valueOut);
bool ExtractJsonBoolField(const std::string& contents, const char* key, bool& valueOut);
bool ExtractJsonStringField(const std::string& contents, const char* key, std::string& valueOut);
bool TryExtractJsonArrayContents(const std::string& contents, const char* key, std::string& arrayContentsOut);
bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour& colourOut);
std::string BuildColourHexString(const MyGUI::Colour& colour);

bool SaveMarkersFile(const std::string& persistencePath, const std::vector<MarkerState>& markers);
MarkerPersistenceLoadAttempt LoadMarkersFile(const std::string& persistencePath);

bool SaveConfigFileData(const std::string& configPath, const MapMarkersConfigFileData& data);
bool LoadConfigFileData(
    const std::string& configPath,
    const MapMarkersConfigFileData& defaults,
    MapMarkersConfigFileData& dataOut);
