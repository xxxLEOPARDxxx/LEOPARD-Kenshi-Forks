#pragma once

struct MapMarkersModConfigSnapshot
{
    bool enabled;
    bool markersVisible;
    bool closeEditorOnMapClose;
    bool showHoverLabels;
    int defaultMarkerType;
    bool editorPositionCustomized;
    int editorLeft;
    int editorTop;
};

MapMarkersModConfigSnapshot MapMarkers_CaptureModConfigSnapshot();
void MapMarkers_ApplyModConfigSnapshot(const MapMarkersModConfigSnapshot& snapshot);
bool MapMarkers_PersistCurrentModConfig(bool logSuccess);
void MapMarkers_LogProbeMessage(const char* message);

void MapMarkersModHub_OnStartup();
void MapMarkersModHub_TickAttachRetry();
