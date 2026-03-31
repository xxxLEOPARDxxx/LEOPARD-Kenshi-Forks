#include "MapMarkersInternal.h"

#include <fstream>
#include <sstream>

namespace
{
const char* kHoverLabelTextColorConfigKey = "hover_label_text_color_hex";
const char* kHoverLabelBackgroundColorConfigKey = "hover_label_background_color_hex";
}

bool SaveConfigFileData(const std::string& configPath, const MapMarkersConfigFileData& data)
{
    if (configPath.empty())
    {
        return false;
    }

    std::ofstream output(configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output << "{\n"
           << "  \"enabled\": " << (data.snapshot.enabled ? "true" : "false") << ",\n"
           << "  \"markers_visible\": " << (data.snapshot.markersVisible ? "true" : "false") << ",\n"
           << "  \"close_editor_on_map_close\": " << (data.snapshot.closeEditorOnMapClose ? "true" : "false") << ",\n"
           << "  \"show_hover_labels\": " << (data.snapshot.showHoverLabels ? "true" : "false") << ",\n"
           << "  \"" << kHoverLabelTextColorConfigKey << "\": \"" << data.hoverLabelTextColorHex << "\",\n"
           << "  \"" << kHoverLabelBackgroundColorConfigKey << "\": \"" << data.hoverLabelBackgroundColorHex << "\",\n"
           << "  \"default_marker_type\": \"" << MarkerTypeToJsonValue(MarkerTypeFromIndex(data.snapshot.defaultMarkerType)) << "\",\n"
           << "  \"editor_position_customized\": " << (data.snapshot.editorPositionCustomized ? "true" : "false") << ",\n"
           << "  \"editor_left\": " << data.snapshot.editorLeft << ",\n"
           << "  \"editor_top\": " << data.snapshot.editorTop << "\n"
           << "}\n";

    return output.good();
}

bool LoadConfigFileData(
    const std::string& configPath,
    const MapMarkersConfigFileData& defaults,
    MapMarkersConfigFileData& dataOut)
{
    dataOut = defaults;
    if (configPath.empty())
    {
        return false;
    }

    std::ifstream input(configPath.c_str(), std::ios::in);
    if (!input)
    {
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string contents = buffer.str();

    bool boolValue = false;
    if (ExtractJsonBoolField(contents, "enabled", boolValue))
    {
        dataOut.snapshot.enabled = boolValue;
    }
    if (ExtractJsonBoolField(contents, "markers_visible", boolValue))
    {
        dataOut.snapshot.markersVisible = boolValue;
    }
    if (ExtractJsonBoolField(contents, "close_editor_on_map_close", boolValue))
    {
        dataOut.snapshot.closeEditorOnMapClose = boolValue;
    }
    if (ExtractJsonBoolField(contents, "show_hover_labels", boolValue))
    {
        dataOut.snapshot.showHoverLabels = boolValue;
    }
    if (ExtractJsonBoolField(contents, "editor_position_customized", boolValue))
    {
        dataOut.snapshot.editorPositionCustomized = boolValue;
    }

    std::string stringValue;
    if (ExtractJsonStringField(contents, "default_marker_type", stringValue))
    {
        dataOut.snapshot.defaultMarkerType = MarkerTypeToIndex(MarkerTypeFromString(stringValue));
    }
    if (ExtractJsonStringField(contents, kHoverLabelTextColorConfigKey, stringValue))
    {
        dataOut.hoverLabelTextColorHex = stringValue;
    }
    if (ExtractJsonStringField(contents, kHoverLabelBackgroundColorConfigKey, stringValue))
    {
        dataOut.hoverLabelBackgroundColorHex = stringValue;
    }

    int intValue = 0;
    if (ExtractJsonIntField(contents, "editor_left", intValue))
    {
        dataOut.snapshot.editorLeft = intValue;
    }
    if (ExtractJsonIntField(contents, "editor_top", intValue))
    {
        dataOut.snapshot.editorTop = intValue;
    }
    if (ExtractJsonIntField(contents, "default_marker_type", intValue))
    {
        dataOut.snapshot.defaultMarkerType = intValue;
    }

    return true;
}
