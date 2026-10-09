#include "MapMarkersInternal.h"

#include <fstream>
#include <sstream>

namespace
{
void NormalizeLoadedMarkers(std::vector<MarkerState>& markers)
{
    for (std::size_t index = 0; index < markers.size(); ++index)
    {
        if (markers[index].id <= 0)
        {
            markers[index].id = static_cast<int>(index) + 1;
        }

        markers[index].normalizedX = ClampFloat(markers[index].normalizedX, 0.0f, 1.0f);
        markers[index].normalizedY = ClampFloat(markers[index].normalizedY, 0.0f, 1.0f);
    }
}

bool TryParseMarkersArray(const std::string& contents, std::vector<MarkerState>& markersOut)
{
    std::string arrayContents;
    if (!TryExtractJsonArrayContents(contents, "markers", arrayContents))
    {
        return false;
    }

    markersOut.clear();
    std::string::size_type searchPos = 0;
    while (true)
    {
        const std::string::size_type objectStart = arrayContents.find('{', searchPos);
        if (objectStart == std::string::npos)
        {
            break;
        }

        int depth = 0;
        std::string::size_type objectEnd = std::string::npos;
        for (std::string::size_type index = objectStart; index < arrayContents.size(); ++index)
        {
            if (arrayContents[index] == '{')
            {
                ++depth;
            }
            else if (arrayContents[index] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    objectEnd = index;
                    break;
                }
            }
        }

        if (objectEnd == std::string::npos)
        {
            return false;
        }

        const std::string objectText = arrayContents.substr(objectStart, objectEnd - objectStart + 1);
        MarkerState marker;
        marker.id = 0;
        marker.normalizedX = 0.0f;
        marker.normalizedY = 0.0f;
        marker.type = MarkerType_Note;
        marker.label.clear();
        if (!ExtractJsonIntField(objectText, "id", marker.id)
            || !ExtractJsonFloatField(objectText, "x", marker.normalizedX)
            || !ExtractJsonFloatField(objectText, "y", marker.normalizedY))
        {
            return false;
        }

        std::string typeValue;
        if (ExtractJsonStringField(objectText, "type", typeValue))
        {
            marker.type = MarkerTypeFromString(typeValue);
        }

        std::string labelValue;
        if (ExtractJsonStringField(objectText, "label", labelValue))
        {
            marker.label = SanitizeMarkerLabel(labelValue, true, kMapMarkersLabelMaxLength);
        }

        markersOut.push_back(marker);
        searchPos = objectEnd + 1;
    }

    return true;
}
}

bool SaveMarkersFile(const std::string& persistencePath, const std::vector<MarkerState>& markers)
{
    if (persistencePath.empty())
    {
        return false;
    }

    std::ofstream output(persistencePath.c_str(), std::ios::out | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output << "{\n"
           << "  \"version\": 3,\n"
           << "  \"markers\": [\n";
    for (std::size_t index = 0; index < markers.size(); ++index)
    {
        const MarkerState& marker = markers[index];
        output << "    {\n"
               << "      \"id\": " << marker.id << ",\n"
               << "      \"x\": " << marker.normalizedX << ",\n"
               << "      \"y\": " << marker.normalizedY << ",\n"
               << "      \"type\": \"" << MarkerTypeToJsonValue(marker.type) << "\",\n"
               << "      \"label\": \"" << JsonEscapeString(marker.label) << "\"\n"
               << "    }";
        if (index + 1 != markers.size())
        {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n"
           << "}\n";

    return output.good();
}

MarkerPersistenceLoadAttempt LoadMarkersFile(const std::string& persistencePath)
{
    MarkerPersistenceLoadAttempt attempt;
    attempt.result = MarkerPersistenceLoadResult_PathUnavailable;

    if (persistencePath.empty())
    {
        return attempt;
    }

    std::ifstream input(persistencePath.c_str(), std::ios::in);
    if (!input)
    {
        attempt.result = MarkerPersistenceLoadResult_MissingFile;
        return attempt;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string contents = buffer.str();

    if (TryParseMarkersArray(contents, attempt.markers))
    {
        NormalizeLoadedMarkers(attempt.markers);
        attempt.result = MarkerPersistenceLoadResult_LoadedArray;
        return attempt;
    }

    float loadedX = 0.0f;
    float loadedY = 0.0f;
    if (ExtractJsonFloatField(contents, "x", loadedX)
        && ExtractJsonFloatField(contents, "y", loadedY))
    {
        MarkerState marker;
        marker.id = 1;
        marker.normalizedX = ClampFloat(loadedX, 0.0f, 1.0f);
        marker.normalizedY = ClampFloat(loadedY, 0.0f, 1.0f);
        marker.type = MarkerType_Note;
        marker.label.clear();
        attempt.markers.push_back(marker);
        attempt.result = MarkerPersistenceLoadResult_LoadedLegacySingle;
        return attempt;
    }

    attempt.result = MarkerPersistenceLoadResult_InvalidFile;
    return attempt;
}
