#include <Debug.h>

#include "src/MapMarkersInternal.h"
#include "src/MapMarkersModHub.h"

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include <core/Functions.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/gui/MapScreen.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/SaveManager.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kPluginName = "Map-markers";
const OIS::KeyCode kProbeSnapshotHotkey = OIS::KC_F7;
const OIS::KeyCode kProbeLiveHotkey = OIS::KC_F8;
const DWORD kPendingSaveTransitionTimeoutMs = 15000u;
const int kMarkerSize = 18;
const int kSelectedMarkerSize = 26;
const int kMinimumMapImageSize = 200;
const int kMarkerEditorPanelWidth = 344;
const int kMarkerEditorPanelHeight = 144;
const int kMarkerEditorPanelDepth = -1000;
const int kMarkerEditorBackgroundInset = 0;
const int kMarkerEditorOuterPadding = 16;
const int kMarkerEditorHeaderHeight = 18;
const int kMarkerEditorControlHeight = 24;
const int kMarkerEditorLabelTitleWidth = 42;
const int kMarkerEditorLabelGap = 12;
const int kMarkerEditorRowGap = 8;
const int kMarkerEditorHintHeight = 20;
const int kMarkerEditorDefaultTop = 48;
const int kMarkerEditorRightMargin = 18;
const int kMarkerToggleButtonWidth = 108;
const int kMarkerToggleButtonHeight = 24;
const int kMarkerToggleButtonMargin = 12;
const int kMarkerToggleButtonBottomMargin = 18;
const int kMarkerHoverLabelHeight = 30;
const int kMarkerHoverLabelHorizontalPadding = 8;
const int kMarkerHoverLabelVerticalOffset = 1;
const int kMarkerHoverLabelMinimumWidth = 40;
const int kMarkerHoverLabelMaximumWidth = 320;
const char* kMarkerEditorHintCaption = "MMB add | LMB move | Enter save | Del | RMB deselect";
const char* kModConfigFileName = "mod-config.json";
const char* kMarkerPersistenceFileName = "Map-markers.json";
const char* kMarkerWidgetNamePrefix = "MapMarkers_Marker_";
const char* kMarkerEditorPanelName = "MapMarkers_EditorPanel";
const char* kMarkerEditorBackgroundName = "MapMarkers_EditorBackground";
const char* kMarkerEditorHeaderName = "MapMarkers_EditorHeader";
const char* kMarkerEditorTypeButtonName = "MapMarkers_EditorTypeButton";
const char* kMarkerEditorLabelTitleName = "MapMarkers_EditorLabelTitle";
const char* kMarkerEditorLabelEditName = "MapMarkers_EditorLabelEdit";
const char* kMarkerEditorHintName = "MapMarkers_EditorHint";
const char* kMarkerToggleButtonName = "MapMarkers_ToggleButton";
const char* kMarkerHoverLabelName = "MapMarkers_HoverLabel";
const char* kMarkerHoverLabelTextName = "MapMarkers_HoverLabelText";
const MyGUI::Colour kDefaultHoverLabelBackgroundColour = MyGUI::Colour(0.10f, 0.10f, 0.10f, 0.95f);
const MyGUI::Colour kDefaultHoverLabelTextColour = MyGUI::Colour(0.96f, 0.96f, 0.96f, 1.0f);

void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;
void (*SaveManager_save_orig)(SaveManager*, const std::string&, bool) = 0;
void (*SaveManager_loadByInfo_orig)(SaveManager*, const SaveInfo&, bool) = 0;
void (*SaveManager_loadByName_orig)(SaveManager*, const std::string&) = 0;
void (*SaveManager_newGame_orig)(SaveManager*, const std::string&) = 0;
void (*SaveManager_import_orig)(SaveManager*, const SaveInfo&, int) = 0;

struct PendingMarkerLabelShortcut
{
    bool active;
    bool rewriteText;
    int keyValue;
    std::size_t cursorPosition;
    std::string label;
};

struct FooterControlCandidate
{
    MyGUI::Widget* widget;
    MyGUI::IntCoord absoluteCoord;
};

bool g_probeLive = false;
unsigned int g_probeLogScopeDepth = 0u;
DWORD g_lastVisibleRootsScanTick = 0;
DWORD g_lastHoverLogTick = 0;
std::string g_lastVisibleRootsSignature;
std::string g_lastHoveredSignature;
std::string g_lastSaveIdentity;
std::string g_lastMarkerRenderSignature;
std::string g_lastMarkerEditorSignature;
std::string g_lastToggleButtonSignature;
std::string g_lastHoverLabelSignature;
std::string g_lastToggleDiagnosticsSignature;
std::string g_lastOverlayDiagnosticsSignature;
std::string g_lastMarkerOcclusionSignature;
std::string g_pendingSaveTransitionDestinationPath;
std::vector<MarkerState> g_markers;
int g_selectedMarkerId = 0;
int g_nextMarkerId = 1;
bool g_lastLeftMouseDownObserved = false;
bool g_lastMiddleMouseDownObserved = false;
bool g_lastRightMouseDownObserved = false;
bool g_suppressNextMarkerLabelChangeEvent = false;
bool g_haveMarkerLabelSnapshot = false;
bool g_markerEditorDragging = false;
bool g_markerEditorPositionDirty = false;
bool g_markerEditorPositionCustomized = false;
bool g_markersVisible = true;
bool g_closeEditorOnMapClose = true;
bool g_modEnabled = true;
bool g_showHoverLabels = true;
bool g_disabledUiStateApplied = false;
bool g_mapWasVisible = false;
bool g_pendingSaveTransitionActive = false;
bool g_focusMarkerLabelEditOnShow = false;
MarkerType g_defaultMarkerType = MarkerType_Note;
MyGUI::Colour g_hoverLabelBackgroundColour = kDefaultHoverLabelBackgroundColour;
MyGUI::Colour g_hoverLabelTextColour = kDefaultHoverLabelTextColour;
DWORD g_pendingSaveTransitionStartedTick = 0;
int g_markerEditorDragLastMouseX = 0;
int g_markerEditorDragLastMouseY = 0;
int g_markerEditorCustomLeft = 0;
int g_markerEditorCustomTop = 0;
std::size_t g_markerLabelSnapshotCursorPosition = 0u;
std::string g_markerLabelSnapshotText;
PendingMarkerLabelShortcut g_pendingMarkerLabelShortcut = { false, false, 0, 0u, "" };
HMODULE g_moduleHandle = 0;

void LogProbeLine(const std::string& message);
bool ShouldEmitProbeLogs();
MyGUI::Widget* FindMapTabInParentChain(MyGUI::Widget* widget);
MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget);
MyGUI::ImageBox* FindActiveMapImage();
MyGUI::Widget* FindMarkerEditorPanel();
MyGUI::Widget* FindMarkerEditorParent(MyGUI::ImageBox* mapImage);
MyGUI::Widget* FindAnyWidgetByName(const std::string& name);
bool TryParseMarkerWidgetId(const std::string& widgetName, int& markerIdOut);
void StopMarkerEditorDrag();
void SetMarkerWidgetsVisible(MyGUI::ImageBox* mapImage, bool visible);
void ClearSelectedMarker(const char* reason);
void ResetMapMarkersUiSignatures();
void HideMapMarkersUi();
void ApplyModConfigSnapshotInternal(const MapMarkersModConfigSnapshot& snapshot);
std::string GetActiveSaveDirectory();
std::string ResolveSaveDestinationPath(SaveManager* saveManager, const std::string& saveName);
void ClearPendingSaveTransition(const char* reason);
void ArmPendingSaveTransition(SaveManager* saveManager, const std::string& saveName);
void TickPendingSaveTransition();

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

struct ScopedProbeLogging
{
    ScopedProbeLogging()
    {
        ++g_probeLogScopeDepth;
    }

    ~ScopedProbeLogging()
    {
        if (g_probeLogScopeDepth != 0u)
        {
            --g_probeLogScopeDepth;
        }
    }

private:
    ScopedProbeLogging(const ScopedProbeLogging&);
    ScopedProbeLogging& operator=(const ScopedProbeLogging&);
};

void ResetMarkerLabelSnapshot()
{
    g_haveMarkerLabelSnapshot = false;
    g_markerLabelSnapshotCursorPosition = 0u;
    g_markerLabelSnapshotText.clear();
}

void RememberMarkerLabelSnapshotValue(const std::string& text, std::size_t cursorPosition)
{
    const std::size_t textLength = MyGUI::UString(text).size();
    if (cursorPosition > textLength)
    {
        cursorPosition = textLength;
    }

    g_haveMarkerLabelSnapshot = true;
    g_markerLabelSnapshotText = text;
    g_markerLabelSnapshotCursorPosition = cursorPosition;
}

void RememberMarkerLabelSnapshot(MyGUI::EditBox* labelEdit)
{
    if (labelEdit == 0)
    {
        ResetMarkerLabelSnapshot();
        return;
    }

    std::size_t cursorPosition = labelEdit->getTextCursor();
    const std::size_t textLength = labelEdit->getTextLength();
    if (cursorPosition > textLength)
    {
        cursorPosition = textLength;
    }

    RememberMarkerLabelSnapshotValue(labelEdit->getOnlyText().asUTF8(), cursorPosition);
}

void ResetPendingMarkerLabelShortcut()
{
    g_pendingMarkerLabelShortcut.active = false;
    g_pendingMarkerLabelShortcut.rewriteText = false;
    g_pendingMarkerLabelShortcut.keyValue = 0;
    g_pendingMarkerLabelShortcut.cursorPosition = 0u;
    g_pendingMarkerLabelShortcut.label.clear();
}

const char* BuildMarkerToggleButtonCaption()
{
    return g_markersVisible ? "Markers: On" : "Markers: Off";
}

std::string ResolveSaveDestinationPath(SaveManager* saveManager, const std::string& saveName)
{
    if (saveName.empty())
    {
        return "";
    }

    const bool looksAbsolute =
        saveName.find(':') != std::string::npos
        || (!saveName.empty() && (saveName[0] == '\\' || saveName[0] == '/'));
    if (looksAbsolute)
    {
        return saveName;
    }

    std::string rootDirectory = GetParentDirectoryPath(GetActiveSaveDirectory());
    if (rootDirectory.empty() && saveManager != 0)
    {
        std::string savePath = saveManager->getSavePath();
        const std::string currentGame = saveManager->getCurrentGame();
        if (!savePath.empty())
        {
            if (!currentGame.empty() && PathsEqualIgnoreCase(GetPathLeafName(savePath), currentGame))
            {
                rootDirectory = GetParentDirectoryPath(savePath);
            }
            else
            {
                rootDirectory = savePath;
            }
        }
    }

    return JoinWindowsPath(rootDirectory, saveName.c_str());
}

void ClearPendingSaveTransition(const char*)
{
    g_pendingSaveTransitionActive = false;
    g_pendingSaveTransitionStartedTick = 0;
    g_pendingSaveTransitionDestinationPath.clear();
}

void ArmPendingSaveTransition(SaveManager* saveManager, const std::string& saveName)
{
    ClearPendingSaveTransition("rearmed");

    const std::string sourcePath = GetActiveSaveDirectory();
    const std::string destinationPath = ResolveSaveDestinationPath(saveManager, saveName);
    if (destinationPath.empty())
    {
        return;
    }

    if (!sourcePath.empty() && PathsEqualIgnoreCase(sourcePath, destinationPath))
    {
        return;
    }

    g_pendingSaveTransitionActive = true;
    g_pendingSaveTransitionStartedTick = GetTickCount();
    g_pendingSaveTransitionDestinationPath = destinationPath;
}

void TickPendingSaveTransition()
{
    if (!g_pendingSaveTransitionActive)
    {
        return;
    }

    const DWORD now = GetTickCount();
    if (now - g_pendingSaveTransitionStartedTick < kPendingSaveTransitionTimeoutMs)
    {
        return;
    }

    ClearPendingSaveTransition("timeout");
}

std::string GetActiveSaveDirectory()
{
    SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();
    return saveFileSystem == 0 ? "" : saveFileSystem->getActiveSave();
}

std::string GetMarkerPersistencePath()
{
    return JoinWindowsPath(GetActiveSaveDirectory(), kMarkerPersistenceFileName);
}

std::string GetPluginDirectory()
{
    if (g_moduleHandle == 0)
    {
        return "";
    }

    char modulePath[MAX_PATH] = {0};
    const DWORD length = GetModuleFileNameA(g_moduleHandle, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return "";
    }

    const std::string fullPath(modulePath, static_cast<std::size_t>(length));
    const std::string::size_type separator = fullPath.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        return "";
    }

    return fullPath.substr(0, separator);
}

std::string GetModConfigPath()
{
    return JoinWindowsPath(GetPluginDirectory(), kModConfigFileName);
}

void ResetMarkersForActiveSave()
{
    g_markers.clear();
    g_selectedMarkerId = 0;
    g_nextMarkerId = 1;
}

void ResetMapMarkersUiSignatures()
{
    g_lastMarkerRenderSignature.clear();
    g_lastMarkerEditorSignature.clear();
    g_lastToggleButtonSignature.clear();
    g_lastHoverLabelSignature.clear();
    g_lastToggleDiagnosticsSignature.clear();
    g_lastOverlayDiagnosticsSignature.clear();
    g_lastMarkerOcclusionSignature.clear();
}

void HideMapMarkersUi()
{
    StopMarkerEditorDrag();
    g_selectedMarkerId = 0;
    g_mapWasVisible = false;

    if (MyGUI::ImageBox* mapImage = FindActiveMapImage())
    {
        SetMarkerWidgetsVisible(mapImage, false);
    }

    if (MyGUI::Widget* panel = FindMarkerEditorPanel())
    {
        panel->setVisible(false);
    }

    if (MyGUI::Widget* toggleButton = FindAnyWidgetByName(kMarkerToggleButtonName))
    {
        toggleButton->setVisible(false);
    }

    ResetMapMarkersUiSignatures();
}

void ApplyModConfigSnapshotInternal(const MapMarkersModConfigSnapshot& snapshot)
{
    g_modEnabled = snapshot.enabled;
    g_markersVisible = snapshot.markersVisible;
    g_closeEditorOnMapClose = snapshot.closeEditorOnMapClose;
    g_showHoverLabels = snapshot.showHoverLabels;
    g_markerEditorPositionCustomized = snapshot.editorPositionCustomized;
    g_markerEditorCustomLeft = snapshot.editorLeft;
    g_markerEditorCustomTop = snapshot.editorTop;
    g_defaultMarkerType = MarkerTypeFromIndex(snapshot.defaultMarkerType);

    if (!g_modEnabled)
    {
        HideMapMarkersUi();
        g_disabledUiStateApplied = true;
        return;
    }

    g_disabledUiStateApplied = false;
    ResetMapMarkersUiSignatures();
}

bool SaveModConfig(bool logSuccess = true)
{
    const std::string configPath = GetModConfigPath();
    if (configPath.empty())
    {
        LogProbeLine("config persist skipped: mod-config path unavailable");
        return false;
    }

    MapMarkersConfigFileData configData;
    configData.snapshot.enabled = g_modEnabled;
    configData.snapshot.markersVisible = g_markersVisible;
    configData.snapshot.closeEditorOnMapClose = g_closeEditorOnMapClose;
    configData.snapshot.showHoverLabels = g_showHoverLabels;
    configData.snapshot.defaultMarkerType = MarkerTypeToIndex(g_defaultMarkerType);
    configData.snapshot.editorPositionCustomized = g_markerEditorPositionCustomized;
    configData.snapshot.editorLeft = g_markerEditorCustomLeft;
    configData.snapshot.editorTop = g_markerEditorCustomTop;
    configData.hoverLabelTextColorHex = BuildColourHexString(g_hoverLabelTextColour);
    configData.hoverLabelBackgroundColorHex = BuildColourHexString(g_hoverLabelBackgroundColour);

    if (!SaveConfigFileData(configPath, configData))
    {
        std::stringstream line;
        line << "config persist failed path=\"" << configPath << "\"";
        LogProbeLine(line.str());
        return false;
    }

    g_markerEditorPositionDirty = false;

    if (logSuccess)
    {
        std::stringstream line;
        line << "config persisted path=\"" << configPath
             << "\" markers_visible=" << (g_markersVisible ? "true" : "false")
             << " close_editor_on_map_close=" << (g_closeEditorOnMapClose ? "true" : "false")
             << " show_hover_labels=" << (g_showHoverLabels ? "true" : "false")
             << " hover_label_text_color_hex=" << BuildColourHexString(g_hoverLabelTextColour)
             << " hover_label_background_color_hex=" << BuildColourHexString(g_hoverLabelBackgroundColour)
             << " default_marker_type=" << MarkerTypeToJsonValue(g_defaultMarkerType)
             << " editor_position_customized=" << (g_markerEditorPositionCustomized ? "true" : "false")
             << " editor_left=" << g_markerEditorCustomLeft
             << " editor_top=" << g_markerEditorCustomTop;
        LogProbeLine(line.str());
    }

    return true;
}

void LoadModConfig()
{
    MapMarkersConfigFileData defaults;
    defaults.snapshot.enabled = true;
    defaults.snapshot.markersVisible = true;
    defaults.snapshot.closeEditorOnMapClose = true;
    defaults.snapshot.showHoverLabels = true;
    defaults.snapshot.editorPositionCustomized = false;
    defaults.snapshot.editorLeft = 0;
    defaults.snapshot.editorTop = 0;
    defaults.snapshot.defaultMarkerType = MarkerTypeToIndex(MarkerType_Note);
    defaults.hoverLabelTextColorHex = BuildColourHexString(kDefaultHoverLabelTextColour);
    defaults.hoverLabelBackgroundColorHex = BuildColourHexString(kDefaultHoverLabelBackgroundColour);
    g_hoverLabelTextColour = kDefaultHoverLabelTextColour;
    g_hoverLabelBackgroundColour = kDefaultHoverLabelBackgroundColour;

    g_markerEditorPositionDirty = false;
    ApplyModConfigSnapshotInternal(defaults.snapshot);

    const std::string configPath = GetModConfigPath();
    if (configPath.empty())
    {
        return;
    }

    MapMarkersConfigFileData loadedConfig = defaults;
    if (!LoadConfigFileData(configPath, defaults, loadedConfig))
    {
        std::stringstream line;
        line << "config missing path=\"" << configPath << "\" using_defaults=true";
        LogProbeLine(line.str());
        return;
    }

    ApplyModConfigSnapshotInternal(loadedConfig.snapshot);

    g_hoverLabelTextColour = kDefaultHoverLabelTextColour;
    if (!TryParseColourHex(loadedConfig.hoverLabelTextColorHex, g_hoverLabelTextColour))
    {
        g_hoverLabelTextColour = kDefaultHoverLabelTextColour;
        ErrorLog("Map-markers WARN: hover_label_text_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
    }

    g_hoverLabelBackgroundColour = kDefaultHoverLabelBackgroundColour;
    if (!TryParseColourHex(loadedConfig.hoverLabelBackgroundColorHex, g_hoverLabelBackgroundColour))
    {
        g_hoverLabelBackgroundColour = kDefaultHoverLabelBackgroundColour;
        ErrorLog("Map-markers WARN: hover_label_background_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
    }

    std::stringstream line;
    line << "config loaded path=\"" << configPath
         << "\" markers_visible=" << (g_markersVisible ? "true" : "false")
         << " close_editor_on_map_close=" << (g_closeEditorOnMapClose ? "true" : "false")
         << " show_hover_labels=" << (g_showHoverLabels ? "true" : "false")
         << " hover_label_text_color_hex=" << BuildColourHexString(g_hoverLabelTextColour)
         << " hover_label_background_color_hex=" << BuildColourHexString(g_hoverLabelBackgroundColour)
         << " default_marker_type=" << MarkerTypeToJsonValue(g_defaultMarkerType)
         << " editor_position_customized=" << (g_markerEditorPositionCustomized ? "true" : "false")
         << " editor_left=" << g_markerEditorCustomLeft
         << " editor_top=" << g_markerEditorCustomTop;
    LogProbeLine(line.str());
}

int FindMarkerIndexById(int markerId)
{
    if (markerId <= 0)
    {
        return -1;
    }

    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        if (g_markers[index].id == markerId)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

MarkerState* FindMarkerById(int markerId)
{
    const int index = FindMarkerIndexById(markerId);
    return index < 0 ? 0 : &g_markers[static_cast<std::size_t>(index)];
}

void RefreshNextMarkerId()
{
    int nextMarkerId = 1;
    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        if (g_markers[index].id >= nextMarkerId)
        {
            nextMarkerId = g_markers[index].id + 1;
        }
    }
    g_nextMarkerId = nextMarkerId;
}

bool SaveMarkersToPath(const std::string& persistencePath, const std::vector<MarkerState>& markers, bool logSuccess)
{
    if (persistencePath.empty())
    {
        LogProbeLine("markers persist skipped: active save path unavailable");
        return false;
    }

    if (!SaveMarkersFile(persistencePath, markers))
    {
        std::stringstream line;
        line << "markers persist failed path=\"" << persistencePath << "\"";
        LogProbeLine(line.str());
        return false;
    }

    if (logSuccess)
    {
        std::stringstream line;
        line << "markers persisted path=\"" << persistencePath
             << "\" count=" << markers.size();
        LogProbeLine(line.str());
    }

    return true;
}

void SaveMarkersForActiveSave(bool logSuccess = true)
{
    SaveMarkersToPath(GetMarkerPersistencePath(), g_markers, logSuccess);
}

void ApplyMarkerStateForLoadedSave(const std::vector<MarkerState>& markers)
{
    g_markers = markers;
    g_selectedMarkerId = 0;
    RefreshNextMarkerId();
    g_lastMarkerRenderSignature.clear();
}

void ClearMarkerStateForLoadedSave()
{
    ResetMarkersForActiveSave();
    g_lastMarkerRenderSignature.clear();
}

MarkerPersistenceLoadAttempt LoadMarkersFromPersistencePath(const std::string& persistencePath)
{
    return LoadMarkersFile(persistencePath);
}

void LoadMarkersForActiveSave()
{
    const std::string activeSavePath = GetActiveSaveDirectory();
    const std::string persistencePath = JoinWindowsPath(activeSavePath, kMarkerPersistenceFileName);
    const MarkerPersistenceLoadAttempt loadAttempt = LoadMarkersFromPersistencePath(persistencePath);

    const bool pendingSaveMatches =
        g_pendingSaveTransitionActive
        && !activeSavePath.empty()
        && PathsEqualIgnoreCase(activeSavePath, g_pendingSaveTransitionDestinationPath);
    if (pendingSaveMatches)
    {
        SaveMarkersToPath(persistencePath, g_markers, false);
        ClearPendingSaveTransition("matched_identity_change");
        g_lastMarkerRenderSignature.clear();
        return;
    }

    if (g_pendingSaveTransitionActive)
    {
        ClearPendingSaveTransition("non_matching_identity_change");
    }

    switch (loadAttempt.result)
    {
    case MarkerPersistenceLoadResult_PathUnavailable:
        ClearMarkerStateForLoadedSave();
        return;

    case MarkerPersistenceLoadResult_MissingFile:
        ClearMarkerStateForLoadedSave();
        {
            std::stringstream line;
            line << "markers persistence missing path=\"" << persistencePath << "\" count=0";
            LogProbeLine(line.str());
        }
        return;

    case MarkerPersistenceLoadResult_LoadedArray:
        ApplyMarkerStateForLoadedSave(loadAttempt.markers);
        {
            std::stringstream line;
            line << "markers loaded path=\"" << persistencePath
                 << "\" count=" << g_markers.size()
                 << " format=array";
            LogProbeLine(line.str());
        }
        return;

    case MarkerPersistenceLoadResult_LoadedLegacySingle:
        ApplyMarkerStateForLoadedSave(loadAttempt.markers);
        {
            std::stringstream line;
            line << "markers loaded path=\"" << persistencePath
                 << "\" count=1 format=legacy_single";
            LogProbeLine(line.str());
        }
        return;

    case MarkerPersistenceLoadResult_InvalidFile:
    default:
        ClearMarkerStateForLoadedSave();
        {
            std::stringstream line;
            line << "markers persistence invalid path=\"" << persistencePath << "\" count=0";
            LogProbeLine(line.str());
        }
        return;
    }
}

void LogProbeLine(const std::string& message)
{
    if (!ShouldEmitProbeLogs())
    {
        return;
    }

    std::stringstream line;
    line << kPluginName << " PROBE: " << message;
    DebugLog(line.str().c_str());
}

bool ShouldEmitProbeLogs()
{
    return g_probeLive || g_probeLogScopeDepth != 0u;
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    return name.empty() ? "<unnamed>" : name;
}

std::string SafeWidgetType(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }
    return widget->getTypeName();
}

std::string SafeWindowCaption(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
    return window == 0 ? "" : window->getCaption().asUTF8();
}

std::string BuildWidgetDescriptor(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << SafeWidgetType(widget)
         << ":" << SafeWidgetName(widget)
         << "@(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = SafeWindowCaption(widget);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    const bool nameHasMap = ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "map");
    const bool typeHasMap = ContainsAsciiCaseInsensitive(SafeWidgetType(widget), "map");
    const bool captionHasMap = ContainsAsciiCaseInsensitive(caption, "map");
    if (nameHasMap || typeHasMap || captionHasMap)
    {
        line << " map_token=true";
    }

    return line.str();
}

std::string BuildWidgetChainForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<none>";
    }

    std::stringstream line;
    std::size_t depth = 0;
    for (MyGUI::Widget* current = widget; current != 0 && depth < 8; current = current->getParent(), ++depth)
    {
        if (depth != 0)
        {
            line << " <- ";
        }
        line << BuildWidgetDescriptor(current);
    }
    return line.str();
}

std::string BuildDetailedWidgetStateForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const MyGUI::IntCoord localCoord = widget->getCoord();
    const MyGUI::IntCoord absoluteCoord = widget->getAbsoluteCoord();
    int childIndex = -1;
    if (MyGUI::Widget* parent = widget->getParent())
    {
        const std::size_t siblingCount = parent->getChildCount();
        for (std::size_t index = 0; index < siblingCount; ++index)
        {
            if (parent->getChildAt(index) == widget)
            {
                childIndex = static_cast<int>(index);
                break;
            }
        }
    }

    std::stringstream line;
    line << "type=" << SafeWidgetType(widget)
         << " name=\"" << SafeWidgetName(widget) << "\""
         << " local=(" << localCoord.left << "," << localCoord.top << "," << localCoord.width << "," << localCoord.height << ")"
         << " abs=(" << absoluteCoord.left << "," << absoluteCoord.top << "," << absoluteCoord.width << "," << absoluteCoord.height << ")"
         << " depth=" << widget->getDepth()
         << " alpha=" << widget->getAlpha()
         << " inherits_alpha=" << (widget->getInheritsAlpha() ? "true" : "false")
         << " visible=" << (widget->getVisible() ? "true" : "false")
         << " inherited_visible=" << (widget->getInheritedVisible() ? "true" : "false")
         << " child_index=" << childIndex
         << " child_count=" << widget->getChildCount();

    const std::string caption = SafeWindowCaption(widget);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    return line.str();
}

bool RectanglesIntersect(const MyGUI::IntCoord& a, const MyGUI::IntCoord& b)
{
    return a.left < b.left + b.width
        && b.left < a.left + a.width
        && a.top < b.top + b.height
        && b.top < a.top + a.height;
}

void LogWidgetChildrenForDiagnostics(const char* prefix, MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return;
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        MyGUI::Widget* child = parent->getChildAt(index);
        std::stringstream line;
        line << prefix << "[" << index << "] "
             << BuildDetailedWidgetStateForLog(child)
             << " chain=" << BuildWidgetChainForLog(child);
        LogProbeLine(line.str());
    }
}

void LogMapOverlayDiagnostics(MyGUI::ImageBox* mapImage, const char* reason, bool force)
{
    if (!ShouldEmitProbeLogs())
    {
        return;
    }

    if (mapImage == 0)
    {
        return;
    }

    MyGUI::Widget* mapTab = FindMapTabInParentChain(mapImage);
    MyGUI::Window* mapWindow = FindOwningWindow(mapImage);
    MyGUI::Widget* panel = FindMarkerEditorPanel();
    MyGUI::Widget* panelParent = panel == 0 ? FindMarkerEditorParent(mapImage) : panel->getParent();
    MyGUI::Widget* toggleButton = FindAnyWidgetByName(kMarkerToggleButtonName);

    std::stringstream signature;
    signature << BuildWidgetDescriptor(mapWindow)
              << "|" << BuildWidgetDescriptor(mapTab)
              << "|" << BuildWidgetDescriptor(mapImage)
              << "|" << BuildWidgetDescriptor(panelParent)
              << "|" << BuildWidgetDescriptor(panel)
              << "|" << BuildWidgetDescriptor(toggleButton)
              << "|" << g_markers.size()
              << "|" << g_selectedMarkerId
              << "|" << (g_markersVisible ? "visible" : "hidden");
    if (!force && signature.str() == g_lastOverlayDiagnosticsSignature)
    {
        return;
    }

    g_lastOverlayDiagnosticsSignature = signature.str();

    std::stringstream header;
    header << "overlay_diagnostics reason=" << (reason == 0 ? "<unknown>" : reason)
           << " selected=" << g_selectedMarkerId
           << " markers_visible=" << (g_markersVisible ? "true" : "false")
           << " marker_count=" << g_markers.size();
    LogProbeLine(header.str());

    if (mapWindow != 0)
    {
        std::stringstream line;
        line << "overlay_map_window " << BuildDetailedWidgetStateForLog(mapWindow)
             << " chain=" << BuildWidgetChainForLog(mapWindow);
        LogProbeLine(line.str());
    }

    if (mapTab != 0)
    {
        std::stringstream line;
        line << "overlay_map_tab " << BuildDetailedWidgetStateForLog(mapTab)
             << " chain=" << BuildWidgetChainForLog(mapTab);
        LogProbeLine(line.str());
        LogWidgetChildrenForDiagnostics("overlay_map_tab_child", mapTab);
    }

    {
        std::stringstream line;
        line << "overlay_map_image " << BuildDetailedWidgetStateForLog(mapImage)
             << " chain=" << BuildWidgetChainForLog(mapImage);
        LogProbeLine(line.str());
    }

    if (panelParent != 0)
    {
        std::stringstream line;
        line << "overlay_editor_parent " << BuildDetailedWidgetStateForLog(panelParent)
             << " chain=" << BuildWidgetChainForLog(panelParent);
        LogProbeLine(line.str());
    }

    if (panel != 0)
    {
        std::stringstream line;
        line << "overlay_editor_panel " << BuildDetailedWidgetStateForLog(panel)
             << " chain=" << BuildWidgetChainForLog(panel)
             << " configured_skin=\"Kenshi_GenericTextBoxFlatSkin\""
             << " customized=" << (g_markerEditorPositionCustomized ? "true" : "false")
             << " custom_left=" << g_markerEditorCustomLeft
             << " custom_top=" << g_markerEditorCustomTop;
        LogProbeLine(line.str());
        LogWidgetChildrenForDiagnostics("overlay_editor_child", panel);
    }

    if (toggleButton != 0)
    {
        std::stringstream line;
        line << "overlay_toggle_button " << BuildDetailedWidgetStateForLog(toggleButton)
             << " chain=" << BuildWidgetChainForLog(toggleButton)
             << " configured_skin=\"Kenshi_Button1\"";
        LogProbeLine(line.str());
    }

    const MyGUI::IntCoord panelAbsolute = panel == 0 ? MyGUI::IntCoord() : panel->getAbsoluteCoord();
    int overlappingMapChildren = 0;
    int overlappingMarkers = 0;
    const std::size_t childCount = mapImage->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        MyGUI::Widget* child = mapImage->getChildAt(index);
        const bool overlapsEditor = panel != 0 && RectanglesIntersect(child->getAbsoluteCoord(), panelAbsolute);
        int markerId = 0;
        const bool isMarkerWidget = TryParseMarkerWidgetId(SafeWidgetName(child), markerId);
        if (overlapsEditor)
        {
            ++overlappingMapChildren;
            if (isMarkerWidget)
            {
                ++overlappingMarkers;
            }
        }

        std::stringstream line;
        line << "overlay_map_image_child[" << index << "] "
             << BuildDetailedWidgetStateForLog(child)
             << " chain=" << BuildWidgetChainForLog(child)
             << " configured_skin=\"" << (isMarkerWidget ? "Kenshi_Button1" : "<unknown>") << "\""
             << " overlaps_editor=" << (overlapsEditor ? "true" : "false");
        if (isMarkerWidget)
        {
            line << " marker_id=" << markerId;
        }
        LogProbeLine(line.str());
    }

    if (panel != 0)
    {
        std::stringstream summary;
        summary << "overlay_overlap_summary"
                << " panel_parent_is_map_tab=" << ((panelParent != 0 && panelParent == mapTab) ? "true" : "false")
                << " overlapping_map_image_children=" << overlappingMapChildren
                << " overlapping_marker_widgets=" << overlappingMarkers
                << " likely_cause="
                << ((panelParent != 0 && panelParent == mapTab && overlappingMarkers > 0)
                        ? "panel_skin_transparency_or_child_background_transparency"
                        : "layering_or_parent_mismatch");
        LogProbeLine(summary.str());
    }
}

bool WidgetNameContains(MyGUI::Widget* widget, const char* token)
{
    return widget != 0 && ContainsAsciiCaseInsensitive(SafeWidgetName(widget), token);
}

bool WidgetChainHasMapIdentity(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        if (WidgetNameContains(current, "mapscrollview")
            || WidgetNameContains(current, "maptab")
            || WidgetNameContains(current, "tabsmain")
            || ContainsAsciiCaseInsensitive(SafeWindowCaption(current), "map"))
        {
            return true;
        }
    }

    return false;
}

MyGUI::Widget* FindDescendantByName(MyGUI::Widget* root, const char* name)
{
    if (root == 0 || name == 0 || *name == '\0')
    {
        return 0;
    }

    if (SafeWidgetName(root) == name)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::Widget* found = FindDescendantByName(root->getChildAt(index), name))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindMapImageRecursive(MyGUI::Widget* root)
{
    if (root == 0)
    {
        return 0;
    }

    MyGUI::ImageBox* imageBox = root->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0
        && WidgetNameContains(imageBox, "mapimage")
        && WidgetChainHasMapIdentity(imageBox))
    {
        return imageBox;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::ImageBox* found = FindMapImageRecursive(root->getChildAt(index)))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindMapImageInParentChain(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        MyGUI::ImageBox* imageBox = current->castType<MyGUI::ImageBox>(false);
        if (imageBox != 0
            && WidgetNameContains(imageBox, "mapimage")
            && WidgetChainHasMapIdentity(imageBox))
        {
            return imageBox;
        }
    }

    return 0;
}

MyGUI::Widget* FindMapTabInParentChain(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        if (WidgetNameContains(current, "maptab"))
        {
            return current;
        }
    }

    return 0;
}

// Картинку карты ищут обходом всех видимых окон. Внутри одного тика её
// спрашивали четыре раза - теперь находим один раз и отдаём запомненную.
// Вне тика (обработчики клавиш) - честный поиск.
bool g_mapImageTickCacheActive = false;
bool g_mapImageTickCacheFilled = false;
MyGUI::ImageBox* g_mapImageTickCache = 0;

MyGUI::ImageBox* FindActiveMapImageScan();

MyGUI::ImageBox* FindActiveMapImage()
{
    if (!g_mapImageTickCacheActive)
    {
        return FindActiveMapImageScan();
    }
    if (!g_mapImageTickCacheFilled)
    {
        g_mapImageTickCache = FindActiveMapImageScan();
        g_mapImageTickCacheFilled = true;
    }
    return g_mapImageTickCache;
}

MyGUI::ImageBox* FindActiveMapImageScan()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        if (MyGUI::ImageBox* found = FindMapImageRecursive(root))
        {
            return found;
        }
    }

    return 0;
}

std::string BuildMarkerWidgetName(int markerId)
{
    std::stringstream name;
    name << kMarkerWidgetNamePrefix << markerId;
    return name.str();
}

bool TryParseMarkerWidgetId(const std::string& widgetName, int& markerIdOut)
{
    const std::string prefix = kMarkerWidgetNamePrefix;
    if (widgetName.size() <= prefix.size()
        || widgetName.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    char* parseEnd = 0;
    const long parsedId = std::strtol(widgetName.c_str() + prefix.size(), &parseEnd, 10);
    if (parseEnd == widgetName.c_str() + prefix.size()
        || *parseEnd != '\0'
        || parsedId <= 0)
    {
        return false;
    }

    markerIdOut = static_cast<int>(parsedId);
    return true;
}

int FindMarkerWidgetIdInChain(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        int markerId = 0;
        if (TryParseMarkerWidgetId(SafeWidgetName(current), markerId))
        {
            return markerId;
        }
    }

    return 0;
}

int FindHoveredMarkerId()
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return 0;
    }

    return FindMarkerWidgetIdInChain(inputManager->getMouseFocusWidget());
}

MyGUI::Widget* FindDirectChildByName(MyGUI::Widget* parent, const std::string& name)
{
    if (parent == 0 || name.empty())
    {
        return 0;
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        MyGUI::Widget* child = parent->getChildAt(index);
        if (child != 0 && SafeWidgetName(child) == name)
        {
            return child;
        }
    }

    return 0;
}

MyGUI::Widget* FindWidgetByNameRecursive(MyGUI::Widget* parent, const std::string& name)
{
    if (parent == 0 || name.empty())
    {
        return 0;
    }

    if (SafeWidgetName(parent) == name)
    {
        return parent;
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::Widget* found = FindWidgetByNameRecursive(parent->getChildAt(index), name))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindAnyWidgetByName(const std::string& name)
{
    if (name.empty())
    {
        return 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        if (MyGUI::Widget* found = FindWidgetByNameRecursive(roots.current(), name))
        {
            return found;
        }
    }

    return 0;
}

void CollectWidgetsByNameRecursive(MyGUI::Widget* parent, const std::string& name, std::vector<MyGUI::Widget*>& out)
{
    if (parent == 0 || name.empty())
    {
        return;
    }

    if (SafeWidgetName(parent) == name)
    {
        out.push_back(parent);
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        CollectWidgetsByNameRecursive(parent->getChildAt(index), name, out);
    }
}

void CollectWidgetsByNameTokenRecursive(MyGUI::Widget* parent, const char* token, std::vector<MyGUI::Widget*>& out)
{
    if (parent == 0 || token == 0 || *token == '\0')
    {
        return;
    }

    if (WidgetNameContains(parent, token))
    {
        out.push_back(parent);
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        CollectWidgetsByNameTokenRecursive(parent->getChildAt(index), token, out);
    }
}

void SetAllWidgetsVisibleByName(const std::string& name, bool visible)
{
    if (name.empty())
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        std::vector<MyGUI::Widget*> matches;
        CollectWidgetsByNameRecursive(roots.current(), name, matches);
        for (std::size_t index = 0; index < matches.size(); ++index)
        {
            if (matches[index] != 0)
            {
                matches[index]->setVisible(visible);
            }
        }
    }
}

void SetAllWidgetsVisibleByNameExcept(const std::string& name, bool visible, MyGUI::Widget* keepWidget)
{
    if (name.empty())
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        std::vector<MyGUI::Widget*> matches;
        CollectWidgetsByNameRecursive(roots.current(), name, matches);
        for (std::size_t index = 0; index < matches.size(); ++index)
        {
            if (matches[index] != 0 && matches[index] != keepWidget)
            {
                matches[index]->setVisible(visible);
            }
        }
    }
}

MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        MyGUI::Window* window = current->castType<MyGUI::Window>(false);
        if (window != 0)
        {
            return window;
        }
    }

    return 0;
}

MyGUI::Widget* FindTopLevelParent(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    MyGUI::Widget* topLevel = widget;
    while (topLevel->getParent() != 0)
    {
        topLevel = topLevel->getParent();
    }

    return topLevel;
}

MyGUI::Widget* FindMarkerEditorParent(MyGUI::ImageBox* mapImage)
{
    if (mapImage == 0)
    {
        return 0;
    }

    if (MyGUI::Widget* mapTab = FindMapTabInParentChain(mapImage))
    {
        return mapTab;
    }

    if (MyGUI::Window* window = FindOwningWindow(mapImage))
    {
        return window;
    }

    if (MyGUI::Widget* root = FindTopLevelParent(mapImage))
    {
        return root;
    }

    return mapImage->getParent() == 0 ? mapImage : mapImage->getParent();
}

MyGUI::Widget* FindMarkerToggleParent(MyGUI::ImageBox* mapImage)
{
    if (mapImage == 0)
    {
        return 0;
    }

    if (MyGUI::Window* window = FindOwningWindow(mapImage))
    {
        if (MyGUI::Widget* root = FindTopLevelParent(window))
        {
            return root;
        }
        return window;
    }

    if (MyGUI::Widget* root = FindTopLevelParent(mapImage))
    {
        return root;
    }

    return mapImage->getParent() == 0 ? mapImage : mapImage->getParent();
}

bool IsMapMarkersOwnedWidget(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return false;
    }

    const std::string name = SafeWidgetName(widget);
    return name.size() >= 11 && name.compare(0, 11, "MapMarkers_") == 0;
}

bool IsWidgetInMapFooterRegion(MyGUI::Widget* widget, const MyGUI::IntCoord& windowAbsolute)
{
    if (widget == 0)
    {
        return false;
    }

    const MyGUI::IntCoord absolute = widget->getAbsoluteCoord();
    const int windowRight = windowAbsolute.left + windowAbsolute.width;
    const int windowBottom = windowAbsolute.top + windowAbsolute.height;

    return absolute.left + absolute.width >= windowRight - 260
        && absolute.left <= windowRight
        && absolute.top + absolute.height >= windowBottom - 140
        && absolute.top <= windowBottom;
}

bool TryBuildMapFooterControlCandidate(
    MyGUI::Widget* widget,
    const MyGUI::IntCoord& windowAbsolute,
    FooterControlCandidate& candidateOut,
    std::string* reasonOut)
{
    if (widget == 0)
    {
        if (reasonOut != 0)
        {
            *reasonOut = "widget_null";
        }
        return false;
    }

    if (IsMapMarkersOwnedWidget(widget))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "map_markers_owned";
        }
        return false;
    }

    if (!widget->getVisible())
    {
        if (reasonOut != 0)
        {
            *reasonOut = "visible_false";
        }
        return false;
    }

    if (!widget->getInheritedVisible())
    {
        if (reasonOut != 0)
        {
            *reasonOut = "inherited_visible_false";
        }
        return false;
    }

    MyGUI::Button* button = widget->castType<MyGUI::Button>(false);
    if (button == 0)
    {
        if (reasonOut != 0)
        {
            *reasonOut = "not_button";
        }
        return false;
    }

    const MyGUI::IntCoord absolute = widget->getAbsoluteCoord();
    const int windowRight = windowAbsolute.left + windowAbsolute.width;
    const int windowBottom = windowAbsolute.top + windowAbsolute.height;
    const int distanceFromRight = windowRight - (absolute.left + absolute.width);
    const int distanceFromBottom = windowBottom - (absolute.top + absolute.height);

    if (absolute.width < 12 || absolute.width > 40)
    {
        if (reasonOut != 0)
        {
            std::stringstream line;
            line << "width_out_of_range(" << absolute.width << ")";
            *reasonOut = line.str();
        }
        return false;
    }

    if (absolute.height < 18 || absolute.height > 40)
    {
        if (reasonOut != 0)
        {
            std::stringstream line;
            line << "height_out_of_range(" << absolute.height << ")";
            *reasonOut = line.str();
        }
        return false;
    }

    if (distanceFromRight < 0 || distanceFromRight > 80)
    {
        if (reasonOut != 0)
        {
            std::stringstream line;
            line << "right_distance_out_of_range(" << distanceFromRight << ")";
            *reasonOut = line.str();
        }
        return false;
    }

    if (distanceFromBottom < 0 || distanceFromBottom > 48)
    {
        if (reasonOut != 0)
        {
            std::stringstream line;
            line << "bottom_distance_out_of_range(" << distanceFromBottom << ")";
            *reasonOut = line.str();
        }
        return false;
    }

    candidateOut.widget = widget;
    candidateOut.absoluteCoord = absolute;
    if (reasonOut != 0)
    {
        *reasonOut = "accepted";
    }
    return true;
}

void CollectFooterRegionWidgets(
    MyGUI::Widget* root,
    const MyGUI::IntCoord& windowAbsolute,
    std::vector<MyGUI::Widget*>& out)
{
    if (root == 0)
    {
        return;
    }

    if (IsWidgetInMapFooterRegion(root, windowAbsolute))
    {
        out.push_back(root);
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        CollectFooterRegionWidgets(root->getChildAt(index), windowAbsolute, out);
    }
}

void CollectLikelyMapFooterControlCandidates(
    MyGUI::Widget* root,
    const MyGUI::IntCoord& windowAbsolute,
    std::vector<FooterControlCandidate>& out)
{
    if (root == 0)
    {
        return;
    }

    FooterControlCandidate candidate;
    if (TryBuildMapFooterControlCandidate(root, windowAbsolute, candidate, 0))
    {
        out.push_back(candidate);
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        CollectLikelyMapFooterControlCandidates(root->getChildAt(index), windowAbsolute, out);
    }
}

void LogMapFooterDiagnostics(MyGUI::Window* mapWindow, const char* reason)
{
    if (!ShouldEmitProbeLogs())
    {
        return;
    }

    if (mapWindow == 0)
    {
        return;
    }

    const MyGUI::IntCoord windowAbsolute = mapWindow->getAbsoluteCoord();

    std::stringstream header;
    header << "toggle_footer_diagnostics reason=" << (reason == 0 ? "<unknown>" : reason)
           << " map_window=" << BuildWidgetDescriptor(mapWindow)
           << " state={" << BuildDetailedWidgetStateForLog(mapWindow) << "}";
    LogProbeLine(header.str());

    for (std::size_t index = 0; index < mapWindow->getChildCount(); ++index)
    {
        MyGUI::Widget* child = mapWindow->getChildAt(index);
        std::stringstream line;
        line << "toggle_footer_direct_child[" << index << "] "
             << BuildDetailedWidgetStateForLog(child)
             << " chain=" << BuildWidgetChainForLog(child);
        LogProbeLine(line.str());
    }

    std::vector<MyGUI::Widget*> footerRegionWidgets;
    CollectFooterRegionWidgets(mapWindow, windowAbsolute, footerRegionWidgets);
    for (std::size_t index = 0; index < footerRegionWidgets.size(); ++index)
    {
        FooterControlCandidate candidate;
        std::string candidateReason;
        const bool accepted =
            TryBuildMapFooterControlCandidate(footerRegionWidgets[index], windowAbsolute, candidate, &candidateReason);

        std::stringstream line;
        line << "toggle_footer_region[" << index << "] "
             << BuildDetailedWidgetStateForLog(footerRegionWidgets[index])
             << " candidate=" << (accepted ? "true" : "false")
             << " reason=" << candidateReason
             << " chain=" << BuildWidgetChainForLog(footerRegionWidgets[index]);
        LogProbeLine(line.str());
    }
}

bool TryResolveMarkerToggleFooterPlacement(
    MyGUI::ImageBox* mapImage,
    MyGUI::Widget*& parentOut,
    int& buttonLeftOut,
    int& buttonTopOut,
    std::string& modeOut,
    int& candidateCountOut)
{
    parentOut = 0;
    buttonLeftOut = 0;
    buttonTopOut = 0;
    candidateCountOut = 0;
    modeOut.clear();

    MyGUI::Window* window = FindOwningWindow(mapImage);
    if (window == 0)
    {
        return false;
    }

    std::vector<MyGUI::Widget*> namedControls;
    CollectWidgetsByNameTokenRecursive(window, "mapzoominbutton", namedControls);
    CollectWidgetsByNameTokenRecursive(window, "mapzoomoutbutton", namedControls);
    CollectWidgetsByNameTokenRecursive(window, "mapcenterbutton", namedControls);

    MyGUI::Widget* namedParent = 0;
    int namedCount = 0;
    int namedTopTotal = 0;
    int namedHeightTotal = 0;
    for (std::size_t index = 0; index < namedControls.size(); ++index)
    {
        MyGUI::Widget* control = namedControls[index];
        if (control == 0 || !control->getVisible() || !control->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* parent = control->getParent();
        if (parent == 0)
        {
            continue;
        }

        if (namedParent == 0)
        {
            namedParent = parent;
        }

        if (parent != namedParent)
        {
            continue;
        }

        const MyGUI::IntCoord controlCoord = control->getCoord();
        namedTopTotal += controlCoord.top;
        namedHeightTotal += controlCoord.height;
        ++namedCount;
    }

    if (namedParent != 0 && namedCount >= 2)
    {
        const MyGUI::IntCoord parentCoord = namedParent->getCoord();
        if (parentCoord.width >= kMarkerToggleButtonWidth + (kMarkerToggleButtonMargin * 2))
        {
            const int averageTop = namedTopTotal / namedCount;
            const int averageHeight = namedHeightTotal / namedCount;
            const int verticalOffset = averageHeight > kMarkerToggleButtonHeight
                ? (averageHeight - kMarkerToggleButtonHeight) / 2
                : 0;
            const int alignedTop = averageTop + verticalOffset;
            const int maxLeft = parentCoord.width > kMarkerToggleButtonWidth ? parentCoord.width - kMarkerToggleButtonWidth : 0;
            const int maxTop = parentCoord.height > kMarkerToggleButtonHeight ? parentCoord.height - kMarkerToggleButtonHeight : 0;

            parentOut = namedParent;
            buttonLeftOut = ClampInt(kMarkerToggleButtonMargin, 0, maxLeft);
            buttonTopOut = ClampInt(alignedTop, 0, maxTop);
            candidateCountOut = namedCount;
            modeOut = "named_zoom_controls";
            return true;
        }
    }

    std::vector<FooterControlCandidate> candidates;
    CollectLikelyMapFooterControlCandidates(window, window->getAbsoluteCoord(), candidates);
    candidateCountOut = static_cast<int>(candidates.size());
    if (candidates.size() < 3u)
    {
        return false;
    }

    MyGUI::Widget* bestParent = 0;
    int bestCount = 0;
    int bestWidth = 0;
    int bestTop = 0;

    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        MyGUI::Widget* parent = candidates[index].widget == 0 ? 0 : candidates[index].widget->getParent();
        if (parent == 0)
        {
            continue;
        }

        const MyGUI::IntCoord parentCoord = parent->getCoord();
        const MyGUI::IntCoord parentAbsolute = parent->getAbsoluteCoord();
        if (parentCoord.width < kMarkerToggleButtonWidth + (kMarkerToggleButtonMargin * 2))
        {
            continue;
        }

        int count = 0;
        int topTotal = 0;
        for (std::size_t otherIndex = 0; otherIndex < candidates.size(); ++otherIndex)
        {
            if (candidates[otherIndex].widget == 0 || candidates[otherIndex].widget->getParent() != parent)
            {
                continue;
            }

            const int relativeTop = candidates[otherIndex].absoluteCoord.top - parentAbsolute.top;
            if (std::abs(relativeTop - (candidates[index].absoluteCoord.top - parentAbsolute.top)) > 8)
            {
                continue;
            }

            ++count;
            topTotal += relativeTop;
        }

        if (count < 3)
        {
            continue;
        }

        if (count > bestCount || (count == bestCount && parentCoord.width > bestWidth))
        {
            bestParent = parent;
            bestCount = count;
            bestWidth = parentCoord.width;
            bestTop = topTotal / count;
        }
    }

    if (bestParent == 0)
    {
        return false;
    }

    const MyGUI::IntCoord parentCoord = bestParent->getCoord();
    const int maxLeft = parentCoord.width > kMarkerToggleButtonWidth ? parentCoord.width - kMarkerToggleButtonWidth : 0;
    const int maxTop = parentCoord.height > kMarkerToggleButtonHeight ? parentCoord.height - kMarkerToggleButtonHeight : 0;

    parentOut = bestParent;
    buttonLeftOut = ClampInt(kMarkerToggleButtonMargin, 0, maxLeft);
    buttonTopOut = ClampInt(bestTop, 0, maxTop);
    modeOut = "footer_controls";
    return true;
}

bool TryGetCurrentMousePosition(int& mouseXOut, int& mouseYOut)
{
    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = input->getMousePosition();
    mouseXOut = mouse.left;
    mouseYOut = mouse.top;
    return true;
}

bool IsWidgetKeyFocused(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    return input != 0 && input->getKeyFocusWidget() == widget;
}

MyGUI::Widget* FindMarkerEditorPanel()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0)
    {
        return FindAnyWidgetByName(kMarkerEditorPanelName);
    }

    MyGUI::Widget* panelParent = FindMarkerEditorParent(mapImage);
    if (panelParent != 0)
    {
        if (MyGUI::Widget* panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName))
        {
            return panel;
        }
    }

    return FindAnyWidgetByName(kMarkerEditorPanelName);
}

void StopMarkerEditorDrag()
{
    if (g_markerEditorDragging && g_markerEditorPositionDirty)
    {
        SaveModConfig(false);
    }

    g_markerEditorDragging = false;
}

void MoveMarkerEditorByDelta(int deltaX, int deltaY)
{
    MyGUI::Widget* panel = FindMarkerEditorPanel();
    if (panel == 0 || panel->getParent() == 0)
    {
        return;
    }

    const MyGUI::IntCoord panelCoord = panel->getCoord();
    const MyGUI::IntCoord parentCoord = panel->getParent()->getCoord();
    const int maxLeft = parentCoord.width > panelCoord.width ? parentCoord.width - panelCoord.width : 0;
    const int maxTop = parentCoord.height > panelCoord.height ? parentCoord.height - panelCoord.height : 0;
    const int nextLeft = ClampInt(panelCoord.left + deltaX, 0, maxLeft);
    const int nextTop = ClampInt(panelCoord.top + deltaY, 0, maxTop);

    panel->setCoord(nextLeft, nextTop, panelCoord.width, panelCoord.height);
    g_markerEditorPositionCustomized = true;
    g_markerEditorPositionDirty = true;
    g_markerEditorCustomLeft = nextLeft;
    g_markerEditorCustomTop = nextTop;
}

int BuildDefaultEditorLeft(MyGUI::Widget* panelParent, MyGUI::ImageBox* mapImage)
{
    if (panelParent == 0 || mapImage == 0)
    {
        return 0;
    }

    const MyGUI::IntCoord parentCoord = panelParent->getCoord();
    const int maxLeft = parentCoord.width > kMarkerEditorPanelWidth ? parentCoord.width - kMarkerEditorPanelWidth : 0;

    MyGUI::Widget* anchorWidget = FindOwningWindow(mapImage);
    if (anchorWidget == 0)
    {
        anchorWidget = mapImage;
    }

    const MyGUI::IntCoord parentAbsolute = panelParent->getAbsoluteCoord();
    const MyGUI::IntCoord anchorAbsolute = anchorWidget->getAbsoluteCoord();
    const int preferredRight =
        anchorAbsolute.left - parentAbsolute.left + anchorAbsolute.width + kMarkerEditorRightMargin;
    if (preferredRight + kMarkerEditorPanelWidth <= parentCoord.width)
    {
        return ClampInt(preferredRight, 0, maxLeft);
    }

    const int preferredLeft =
        anchorAbsolute.left - parentAbsolute.left - kMarkerEditorPanelWidth - kMarkerEditorRightMargin;
    if (preferredLeft >= 0)
    {
        return ClampInt(preferredLeft, 0, maxLeft);
    }

    const int fallbackInside =
        anchorAbsolute.left - parentAbsolute.left + anchorAbsolute.width - kMarkerEditorPanelWidth - kMarkerEditorRightMargin;
    return ClampInt(fallbackInside, 0, maxLeft);
}

int BuildDefaultEditorTop(MyGUI::Widget* panelParent, MyGUI::ImageBox* mapImage)
{
    if (panelParent == 0 || mapImage == 0)
    {
        return 0;
    }

    const MyGUI::IntCoord parentCoord = panelParent->getCoord();
    const int maxTop = parentCoord.height > kMarkerEditorPanelHeight ? parentCoord.height - kMarkerEditorPanelHeight : 0;

    return ClampInt(kMarkerEditorDefaultTop, 0, maxTop);
}

void OnMarkerEditorHeaderMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    g_markerEditorDragging = true;
    if (!TryGetCurrentMousePosition(g_markerEditorDragLastMouseX, g_markerEditorDragLastMouseY))
    {
        g_markerEditorDragLastMouseX = left;
        g_markerEditorDragLastMouseY = top;
    }
}

void OnMarkerEditorHeaderMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_markerEditorDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(mouseX, mouseY);

    const int deltaX = mouseX - g_markerEditorDragLastMouseX;
    const int deltaY = mouseY - g_markerEditorDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveMarkerEditorByDelta(deltaX, deltaY);
    g_markerEditorDragLastMouseX = mouseX;
    g_markerEditorDragLastMouseY = mouseY;
}

void OnMarkerEditorHeaderMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_markerEditorDragging)
    {
        return;
    }

    OnMarkerEditorHeaderMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnMarkerEditorHeaderMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    StopMarkerEditorDrag();
}

void OnMarkerTypeButtonClicked(MyGUI::Widget*)
{
    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    selectedMarker->type = GetNextMarkerType(selectedMarker->type);
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker type_changed marker_id=" << selectedMarker->id
         << " type=\"" << MarkerTypeToJsonValue(selectedMarker->type) << "\"";
    LogProbeLine(line.str());
}

void ApplyMarkerLabelTextAndCursor(
    MyGUI::EditBox* labelEdit,
    const std::string& text,
    std::size_t cursorPosition,
    const char* reason)
{
    if (labelEdit == 0)
    {
        return;
    }

    const std::string sanitized = SanitizeMarkerLabel(text, false, kMapMarkersLabelMaxLength);
    const std::size_t textLength = MyGUI::UString(sanitized).size();
    if (cursorPosition > textLength)
    {
        cursorPosition = textLength;
    }

    const std::string currentText = labelEdit->getOnlyText().asUTF8();
    if (currentText != sanitized)
    {
        g_suppressNextMarkerLabelChangeEvent = true;
        labelEdit->setOnlyText(sanitized);
    }
    labelEdit->setTextCursor(cursorPosition);
    labelEdit->setTextSelection(cursorPosition, cursorPosition);

    if (MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId))
    {
        if (selectedMarker->label != sanitized)
        {
            selectedMarker->label = sanitized;
            SaveMarkersForActiveSave(false);
        }
    }

    RememberMarkerLabelSnapshotValue(sanitized, cursorPosition);

    std::stringstream line;
    line << "marker label_shortcut_applied"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " cursor=" << cursorPosition
         << " length=" << sanitized.size();
    LogProbeLine(line.str());
}

bool ScheduleMarkerLabelShortcut(MyGUI::EditBox* labelEdit, MyGUI::KeyCode keyCode)
{
    if (labelEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0 || !inputManager->isControlPressed())
    {
        return false;
    }

    MyGUI::UString text = labelEdit->getOnlyText();
    std::size_t textLength = text.size();
    std::size_t cursorPosition = labelEdit->getTextCursor();
    if (cursorPosition > textLength)
    {
        cursorPosition = textLength;
    }

    ResetPendingMarkerLabelShortcut();
    g_pendingMarkerLabelShortcut.active = true;
    g_pendingMarkerLabelShortcut.keyValue = keyCode.getValue();

    if (keyCode.getValue() == MyGUI::KeyCode::ArrowLeft)
    {
        g_pendingMarkerLabelShortcut.rewriteText = false;
        g_pendingMarkerLabelShortcut.cursorPosition =
            FindPreviousMarkerLabelTokenBoundary(text, cursorPosition);
        return true;
    }

    if (keyCode.getValue() == MyGUI::KeyCode::ArrowRight)
    {
        g_pendingMarkerLabelShortcut.rewriteText = false;
        g_pendingMarkerLabelShortcut.cursorPosition =
            FindNextMarkerLabelTokenBoundary(text, cursorPosition);
        return true;
    }

    if (keyCode.getValue() != MyGUI::KeyCode::Backspace)
    {
        ResetPendingMarkerLabelShortcut();
        return false;
    }

    if (g_haveMarkerLabelSnapshot)
    {
        text = MyGUI::UString(g_markerLabelSnapshotText);
        textLength = text.size();
        cursorPosition = g_markerLabelSnapshotCursorPosition;
        if (cursorPosition > textLength)
        {
            cursorPosition = textLength;
        }
    }

    g_pendingMarkerLabelShortcut.rewriteText = true;
    MyGUI::UString updated = text;

    if (labelEdit->isTextSelection())
    {
        std::size_t selectionStart = labelEdit->getTextSelectionStart();
        std::size_t selectionLength = labelEdit->getTextSelectionLength();
        if (selectionStart != MyGUI::ITEM_NONE)
        {
            if (selectionStart > textLength)
            {
                selectionStart = textLength;
            }
            if (selectionStart + selectionLength > textLength)
            {
                selectionLength = textLength - selectionStart;
            }
        }

        if (selectionStart != MyGUI::ITEM_NONE && selectionLength != 0u)
        {
            updated.erase(selectionStart, selectionLength);
            g_pendingMarkerLabelShortcut.cursorPosition = selectionStart;
        }
        else
        {
            g_pendingMarkerLabelShortcut.cursorPosition = cursorPosition;
        }
    }
    else
    {
        const std::size_t deleteStart = FindPreviousMarkerLabelTokenBoundary(text, cursorPosition);
        if (deleteStart != cursorPosition)
        {
            updated.erase(deleteStart, cursorPosition - deleteStart);
        }
        g_pendingMarkerLabelShortcut.cursorPosition = deleteStart;
    }

    g_pendingMarkerLabelShortcut.label = updated.asUTF8();
    return true;
}

void ApplyPendingMarkerLabelShortcut(MyGUI::EditBox* labelEdit, MyGUI::KeyCode keyCode)
{
    if (!g_pendingMarkerLabelShortcut.active || g_pendingMarkerLabelShortcut.keyValue != keyCode.getValue())
    {
        return;
    }

    const PendingMarkerLabelShortcut pending = g_pendingMarkerLabelShortcut;
    ResetPendingMarkerLabelShortcut();

    if (labelEdit == 0)
    {
        return;
    }

    if (pending.rewriteText)
    {
        ApplyMarkerLabelTextAndCursor(
            labelEdit,
            pending.label,
            pending.cursorPosition,
            "ctrl_backspace");
        return;
    }

    std::size_t cursorPosition = pending.cursorPosition;
    const std::size_t textLength = labelEdit->getTextLength();
    if (cursorPosition > textLength)
    {
        cursorPosition = textLength;
    }

    labelEdit->setTextCursor(cursorPosition);
    labelEdit->setTextSelection(cursorPosition, cursorPosition);
    RememberMarkerLabelSnapshot(labelEdit);

    std::stringstream line;
    line << "marker label_shortcut_applied"
         << " reason=" << (keyCode.getValue() == MyGUI::KeyCode::ArrowLeft ? "ctrl_left" : "ctrl_right")
         << " cursor=" << cursorPosition
         << " length=" << labelEdit->getOnlyText().asUTF8().size();
    LogProbeLine(line.str());
}

void CommitMarkerLabelEdit(MyGUI::EditBox* labelEdit, bool deselectAfterCommit, const char* reason)
{
    if (labelEdit == 0)
    {
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    const std::string currentText = labelEdit->getOnlyText().asUTF8();
    const std::string committedText = SanitizeMarkerLabel(currentText, true, kMapMarkersLabelMaxLength);
    if (committedText != currentText)
    {
        g_suppressNextMarkerLabelChangeEvent = true;
        labelEdit->setOnlyText(committedText);
    }

    if (selectedMarker->label != committedText || deselectAfterCommit)
    {
        selectedMarker->label = committedText;
        SaveMarkersForActiveSave(false);
    }

    RememberMarkerLabelSnapshotValue(committedText, labelEdit->getTextCursor());

    std::stringstream line;
    line << "marker label_committed marker_id=" << selectedMarker->id
         << " length=" << selectedMarker->label.size();
    if (reason != 0 && *reason != '\0')
    {
        line << " reason=" << reason;
    }
    LogProbeLine(line.str());

    if (!deselectAfterCommit)
    {
        return;
    }

    ClearSelectedMarker(reason);

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager != 0 && inputManager->getKeyFocusWidget() == labelEdit)
    {
        inputManager->resetKeyFocusWidget(labelEdit);
    }
}

void OnMarkerLabelKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char)
{
    if (sender == 0)
    {
        return;
    }

    MyGUI::EditBox* labelEdit = sender->castType<MyGUI::EditBox>(false);
    if (labelEdit == 0)
    {
        return;
    }

    if (IsMarkerLabelConfirmKey(keyCode))
    {
        ResetPendingMarkerLabelShortcut();
        CommitMarkerLabelEdit(labelEdit, true, "enter_confirm");
        return;
    }

    if (!IsInterestingMarkerLabelShortcutKey(keyCode))
    {
        return;
    }

    ScheduleMarkerLabelShortcut(labelEdit, keyCode);
}

void OnMarkerLabelKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode)
{
    if (sender == 0 || !IsInterestingMarkerLabelShortcutKey(keyCode))
    {
        return;
    }

    ApplyPendingMarkerLabelShortcut(sender->castType<MyGUI::EditBox>(false), keyCode);
}

void OnMarkerLabelFocusChanged(MyGUI::Widget* sender, MyGUI::Widget*)
{
    MyGUI::EditBox* labelEdit = sender == 0 ? 0 : sender->castType<MyGUI::EditBox>(false);
    if (labelEdit == 0 || IsWidgetKeyFocused(labelEdit))
    {
        return;
    }

    CommitMarkerLabelEdit(labelEdit, false, "focus_lost");
}

void OnMarkerLabelChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    if (g_suppressNextMarkerLabelChangeEvent)
    {
        g_suppressNextMarkerLabelChangeEvent = false;
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    const std::string currentText = sender->getOnlyText().asUTF8();
    const std::string sanitized = SanitizeMarkerLabel(currentText, false, kMapMarkersLabelMaxLength);
    if (sanitized != currentText)
    {
        g_suppressNextMarkerLabelChangeEvent = true;
        sender->setOnlyText(sanitized);
    }

    if (selectedMarker->label == sanitized)
    {
        RememberMarkerLabelSnapshot(sender);
        return;
    }

    selectedMarker->label = sanitized;
    SaveMarkersForActiveSave(false);
    RememberMarkerLabelSnapshot(sender);
}

void OnMarkerToggleButtonClicked(MyGUI::Widget*)
{
    g_markersVisible = !g_markersVisible;
    g_lastMarkerRenderSignature.clear();
    StopMarkerEditorDrag();
    SaveModConfig();

    std::stringstream line;
    line << "markers visibility_changed visible=" << (g_markersVisible ? "true" : "false");
    LogProbeLine(line.str());
}

bool BuildMarkerToggleButtonUi(MyGUI::Widget* buttonParent)
{
    if (buttonParent == 0)
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Button* button = buttonParent->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(0, 0, kMarkerToggleButtonWidth, kMarkerToggleButtonHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerToggleButtonName);
    if (button == 0)
    {
        return false;
    }

    button->setNeedMouseFocus(true);
    button->eventMouseButtonClick += MyGUI::newDelegate(&OnMarkerToggleButtonClicked);
    button->setCaption(BuildMarkerToggleButtonCaption());
    return true;
}

void EnsureMarkerToggleButtonUi()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        SetAllWidgetsVisibleByName(kMarkerToggleButtonName, false);
        if (!g_lastToggleButtonSignature.empty())
        {
            g_lastToggleButtonSignature.clear();
            LogProbeLine("toggle_button hidden reason=map_not_visible");
        }
        g_lastToggleDiagnosticsSignature.clear();
        return;
    }

    MyGUI::Widget* buttonParent = FindMarkerToggleParent(mapImage);
    if (buttonParent == 0 || !buttonParent->getInheritedVisible())
    {
        SetAllWidgetsVisibleByName(kMarkerToggleButtonName, false);
        if (!g_lastToggleButtonSignature.empty())
        {
            g_lastToggleButtonSignature.clear();
            LogProbeLine("toggle_button hidden reason=parent_not_visible");
        }
        g_lastToggleDiagnosticsSignature.clear();
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    MyGUI::Window* mapWindow = FindOwningWindow(mapImage);
    int buttonLeft = 0;
    int buttonTop = 0;
    int candidateCount = 0;
    std::string anchorMode;
    MyGUI::Widget* footerParent = 0;
    if (TryResolveMarkerToggleFooterPlacement(
            mapImage,
            footerParent,
            buttonLeft,
            buttonTop,
            anchorMode,
            candidateCount))
    {
        buttonParent = footerParent;
    }

    MyGUI::Widget* buttonWidget = FindDirectChildByName(buttonParent, kMarkerToggleButtonName);
    MyGUI::Button* button = buttonWidget == 0 ? 0 : buttonWidget->castType<MyGUI::Button>(false);
    if (button == 0)
    {
        if (!BuildMarkerToggleButtonUi(buttonParent))
        {
            return;
        }
        buttonWidget = FindDirectChildByName(buttonParent, kMarkerToggleButtonName);
        button = buttonWidget == 0 ? 0 : buttonWidget->castType<MyGUI::Button>(false);
        if (button == 0)
        {
            return;
        }
    }

    MyGUI::Widget* anchorWidget = FindOwningWindow(mapImage);
    if (anchorWidget == 0)
    {
        anchorWidget = mapImage;
    }

    const MyGUI::IntCoord parentCoord = buttonParent->getCoord();
    const MyGUI::IntCoord parentAbsolute = buttonParent->getAbsoluteCoord();
    const MyGUI::IntCoord anchorAbsolute = anchorWidget->getAbsoluteCoord();
    if (anchorMode.empty())
    {
        anchorMode = "window_fallback";
        const int maxLeft = parentCoord.width > kMarkerToggleButtonWidth ? parentCoord.width - kMarkerToggleButtonWidth : 0;
        const int maxTop = parentCoord.height > kMarkerToggleButtonHeight ? parentCoord.height - kMarkerToggleButtonHeight : 0;
        buttonLeft = ClampInt(
            anchorAbsolute.left - parentAbsolute.left + kMarkerToggleButtonMargin,
            0,
            maxLeft);
        buttonTop = ClampInt(
            anchorAbsolute.top - parentAbsolute.top + anchorAbsolute.height - kMarkerToggleButtonHeight - kMarkerToggleButtonBottomMargin,
            0,
            maxTop);
    }

    SetAllWidgetsVisibleByNameExcept(kMarkerToggleButtonName, false, button);
    button->setVisible(true);
    button->setCaption(BuildMarkerToggleButtonCaption());
    button->setCoord(buttonLeft, buttonTop, kMarkerToggleButtonWidth, kMarkerToggleButtonHeight);

    std::stringstream signature;
    signature << SafeWidgetName(buttonParent)
              << "|" << buttonLeft << "," << buttonTop
              << "|" << parentCoord.width << "x" << parentCoord.height
              << "|" << BuildMarkerToggleButtonCaption();
    if (signature.str() != g_lastToggleButtonSignature)
    {
        g_lastToggleButtonSignature = signature.str();

        std::stringstream line;
        line << "toggle_button placed"
             << " parent=" << BuildWidgetDescriptor(buttonParent)
             << " parent_abs=(" << parentAbsolute.left << "," << parentAbsolute.top << "," << parentAbsolute.width << "," << parentAbsolute.height << ")"
             << " anchor=" << BuildWidgetDescriptor(anchorWidget)
             << " anchor_abs=(" << anchorAbsolute.left << "," << anchorAbsolute.top << "," << anchorAbsolute.width << "," << anchorAbsolute.height << ")"
             << " mode=" << anchorMode
             << " footer_candidates=" << candidateCount
             << " coord=(" << buttonLeft << "," << buttonTop << "," << kMarkerToggleButtonWidth << "," << kMarkerToggleButtonHeight << ")"
             << " caption=\"" << BuildMarkerToggleButtonCaption() << "\"";
        LogProbeLine(line.str());

        std::stringstream finalLine;
        finalLine << "toggle_button final_state "
                  << BuildDetailedWidgetStateForLog(button)
                  << " chain=" << BuildWidgetChainForLog(button);
        LogProbeLine(finalLine.str());
    }

    std::stringstream diagnosticsSignature;
    diagnosticsSignature << BuildWidgetDescriptor(mapWindow)
                         << "|" << anchorMode
                         << "|" << candidateCount
                         << "|" << buttonLeft << "," << buttonTop
                         << "|" << parentCoord.width << "x" << parentCoord.height;
    if (diagnosticsSignature.str() != g_lastToggleDiagnosticsSignature)
    {
        g_lastToggleDiagnosticsSignature = diagnosticsSignature.str();
        if (mapWindow != 0)
        {
            LogMapFooterDiagnostics(mapWindow, anchorMode.c_str());
        }
    }
}

bool BuildMarkerEditorUi(MyGUI::Widget* panelParent)
{
    if (panelParent == 0)
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* panel = panelParent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        MyGUI::IntCoord(0, 0, kMarkerEditorPanelWidth, kMarkerEditorPanelHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorPanelName);
    if (panel == 0)
    {
        return false;
    }

    panel->setAlpha(1.0f);
    panel->setDepth(kMarkerEditorPanelDepth);

    MyGUI::Widget* background = panel->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        MyGUI::IntCoord(
            kMarkerEditorBackgroundInset,
            kMarkerEditorBackgroundInset,
            kMarkerEditorPanelWidth - (kMarkerEditorBackgroundInset * 2),
            kMarkerEditorPanelHeight - (kMarkerEditorBackgroundInset * 2)),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorBackgroundName);
    if (background == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    background->setNeedMouseFocus(false);
    background->setAlpha(1.0f);
    background->setColour(MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f));

    const int headerTop = kMarkerEditorOuterPadding;
    const int headerLeft = kMarkerEditorOuterPadding;
    const int contentWidth = kMarkerEditorPanelWidth - (kMarkerEditorOuterPadding * 2);
    const int typeTop = headerTop + kMarkerEditorHeaderHeight + kMarkerEditorRowGap;
    const int labelTop = typeTop + kMarkerEditorControlHeight + kMarkerEditorRowGap;
    const int hintTop = kMarkerEditorPanelHeight - kMarkerEditorOuterPadding - kMarkerEditorHintHeight;
    const int labelEditLeft = kMarkerEditorOuterPadding + kMarkerEditorLabelTitleWidth + kMarkerEditorLabelGap;
    const int labelEditWidth =
        kMarkerEditorPanelWidth - labelEditLeft - kMarkerEditorOuterPadding;

    MyGUI::TextBox* header = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(headerLeft, headerTop, contentWidth, kMarkerEditorHeaderHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorHeaderName);
    if (header == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    header->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    header->setNeedMouseFocus(true);
    header->eventMouseButtonPressed += MyGUI::newDelegate(&OnMarkerEditorHeaderMousePressed);
    header->eventMouseMove += MyGUI::newDelegate(&OnMarkerEditorHeaderMouseMove);
    header->eventMouseDrag += MyGUI::newDelegate(&OnMarkerEditorHeaderMouseDrag);
    header->eventMouseButtonReleased += MyGUI::newDelegate(&OnMarkerEditorHeaderMouseReleased);

    MyGUI::Button* typeButton = panel->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(kMarkerEditorOuterPadding, typeTop, contentWidth, kMarkerEditorControlHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorTypeButtonName);
    if (typeButton == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    typeButton->setNeedMouseFocus(true);
    typeButton->eventMouseButtonClick += MyGUI::newDelegate(&OnMarkerTypeButtonClicked);

    MyGUI::TextBox* labelTitle = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(kMarkerEditorOuterPadding, labelTop, kMarkerEditorLabelTitleWidth, kMarkerEditorControlHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorLabelTitleName);
    if (labelTitle == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    labelTitle->setCaption("Label");
    labelTitle->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    MyGUI::EditBox* labelEdit = panel->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(labelEditLeft, labelTop, labelEditWidth, kMarkerEditorControlHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorLabelEditName);
    if (labelEdit == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    labelEdit->setNeedKeyFocus(true);
    labelEdit->eventEditTextChange += MyGUI::newDelegate(&OnMarkerLabelChanged);
    labelEdit->eventKeySetFocus += MyGUI::newDelegate(&OnMarkerLabelFocusChanged);
    labelEdit->eventKeyLostFocus += MyGUI::newDelegate(&OnMarkerLabelFocusChanged);
    labelEdit->eventKeyButtonPressed += MyGUI::newDelegate(&OnMarkerLabelKeyPressed);
    labelEdit->eventKeyButtonReleased += MyGUI::newDelegate(&OnMarkerLabelKeyReleased);

    MyGUI::TextBox* hint = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(kMarkerEditorOuterPadding, hintTop, contentWidth, kMarkerEditorHintHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorHintName);
    if (hint == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    hint->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    hint->setCaption(kMarkerEditorHintCaption);

    return true;
}

void EnsureMarkerEditorUi()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        StopMarkerEditorDrag();
        if (MyGUI::Widget* panel = FindMarkerEditorPanel())
        {
            panel->setVisible(false);
        }
        if (!g_lastMarkerEditorSignature.empty())
        {
            g_lastMarkerEditorSignature.clear();
            LogProbeLine("marker_editor hidden reason=map_not_visible");
        }
        g_lastOverlayDiagnosticsSignature.clear();
        return;
    }

    MyGUI::Widget* panelParent = FindMarkerEditorParent(mapImage);
    if (panelParent == 0 || !panelParent->getInheritedVisible())
    {
        StopMarkerEditorDrag();
        if (MyGUI::Widget* panel = FindMarkerEditorPanel())
        {
            panel->setVisible(false);
        }
        if (!g_lastMarkerEditorSignature.empty())
        {
            g_lastMarkerEditorSignature.clear();
            LogProbeLine("marker_editor hidden reason=parent_not_visible");
        }
        g_lastOverlayDiagnosticsSignature.clear();
        return;
    }

    MyGUI::Widget* panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (!g_markersVisible || selectedMarker == 0)
    {
        StopMarkerEditorDrag();
        if (panel != 0)
        {
            panel->setVisible(false);
        }
        if (!g_lastMarkerEditorSignature.empty())
        {
            g_lastMarkerEditorSignature.clear();
            LogProbeLine("marker_editor hidden reason=no_selected_marker_or_markers_hidden");
        }
        g_lastOverlayDiagnosticsSignature.clear();
        return;
    }

    if (panel == 0)
    {
        if (!BuildMarkerEditorUi(panelParent))
        {
            return;
        }
        panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
        if (panel == 0)
        {
            return;
        }
    }

    const MyGUI::IntCoord parentCoord = panelParent->getCoord();
    const int maxLeft = parentCoord.width > kMarkerEditorPanelWidth ? parentCoord.width - kMarkerEditorPanelWidth : 0;
    const int maxTop = parentCoord.height > kMarkerEditorPanelHeight ? parentCoord.height - kMarkerEditorPanelHeight : 0;
    const int defaultLeft = BuildDefaultEditorLeft(panelParent, mapImage);
    const int defaultTop = BuildDefaultEditorTop(panelParent, mapImage);
    const int panelLeft = g_markerEditorPositionCustomized
        ? ClampInt(g_markerEditorCustomLeft, 0, maxLeft)
        : defaultLeft;
    const int panelTop = g_markerEditorPositionCustomized
        ? ClampInt(g_markerEditorCustomTop, 0, maxTop)
        : defaultTop;

    g_markerEditorCustomLeft = panelLeft;
    g_markerEditorCustomTop = panelTop;

    panel->setVisible(true);
    panel->setDepth(kMarkerEditorPanelDepth);
    panel->setCoord(panelLeft, panelTop, kMarkerEditorPanelWidth, kMarkerEditorPanelHeight);

    std::stringstream panelSignature;
    panelSignature << SafeWidgetName(panelParent)
                   << "|" << selectedMarker->id
                   << "|" << panelLeft << "," << panelTop
                   << "|" << defaultLeft << "," << defaultTop
                   << "|" << (g_markerEditorPositionCustomized ? "custom" : "default");
    if (panelSignature.str() != g_lastMarkerEditorSignature)
    {
        g_lastMarkerEditorSignature = panelSignature.str();

        std::stringstream line;
        line << "marker_editor placed"
             << " parent=" << BuildWidgetDescriptor(panelParent)
             << " marker_id=" << selectedMarker->id
             << " coord=(" << panelLeft << "," << panelTop << "," << kMarkerEditorPanelWidth << "," << kMarkerEditorPanelHeight << ")"
             << " default=(" << defaultLeft << "," << defaultTop << ")"
             << " mode=" << (g_markerEditorPositionCustomized ? "custom" : "default");
        LogProbeLine(line.str());

        if (!g_markerEditorDragging)
        {
            LogMapOverlayDiagnostics(mapImage, "editor_placed", false);
        }
    }

    MyGUI::Widget* headerWidget = FindDirectChildByName(panel, kMarkerEditorHeaderName);
    MyGUI::Widget* typeButtonWidget = FindDirectChildByName(panel, kMarkerEditorTypeButtonName);
    MyGUI::Widget* labelEditWidget = FindDirectChildByName(panel, kMarkerEditorLabelEditName);
    MyGUI::Widget* hintWidget = FindDirectChildByName(panel, kMarkerEditorHintName);

    MyGUI::TextBox* header = headerWidget == 0 ? 0 : headerWidget->castType<MyGUI::TextBox>(false);
    MyGUI::Button* typeButton = typeButtonWidget == 0 ? 0 : typeButtonWidget->castType<MyGUI::Button>(false);
    MyGUI::EditBox* labelEdit = labelEditWidget == 0 ? 0 : labelEditWidget->castType<MyGUI::EditBox>(false);
    MyGUI::TextBox* hint = hintWidget == 0 ? 0 : hintWidget->castType<MyGUI::TextBox>(false);

    if (header != 0)
    {
        header->setCaption(BuildMarkerEditorHeader(*selectedMarker));
    }

    if (typeButton != 0)
    {
        std::stringstream caption;
        caption << "Type: " << MarkerTypeToDisplayName(selectedMarker->type);
        typeButton->setCaption(caption.str());
        typeButton->setColour(BuildMarkerColour(selectedMarker->type, true));
    }

    if (labelEdit != 0 && !IsWidgetKeyFocused(labelEdit))
    {
        const std::string onlyText = labelEdit->getOnlyText().asUTF8();
        if (onlyText != selectedMarker->label)
        {
            g_suppressNextMarkerLabelChangeEvent = true;
            labelEdit->setOnlyText(selectedMarker->label);
        }
        RememberMarkerLabelSnapshot(labelEdit);
    }

    if (labelEdit != 0
        && g_focusMarkerLabelEditOnShow
        && (GetAsyncKeyState(VK_MBUTTON) & 0x8000) == 0)
    {
        MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
        if (inputManager != 0)
        {
            labelEdit->setNeedKeyFocus(true);
            const std::size_t cursorPosition = labelEdit->getTextLength();
            labelEdit->setTextCursor(cursorPosition);
            labelEdit->setTextSelection(cursorPosition, cursorPosition);
            inputManager->setKeyFocusWidget(labelEdit);
            if (inputManager->getKeyFocusWidget() == labelEdit)
            {
                RememberMarkerLabelSnapshot(labelEdit);
                g_focusMarkerLabelEditOnShow = false;
            }
        }
    }

    if (hint != 0)
    {
        hint->setCaption(kMarkerEditorHintCaption);
    }
}

void DestroyStaleMarkerWidgets(MyGUI::ImageBox* mapImage)
{
    if (mapImage == 0)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    for (std::size_t index = mapImage->getChildCount(); index > 0; --index)
    {
        MyGUI::Widget* child = mapImage->getChildAt(index - 1);
        if (child == 0)
        {
            continue;
        }

        int markerId = 0;
        if (!TryParseMarkerWidgetId(SafeWidgetName(child), markerId))
        {
            continue;
        }

        if (FindMarkerById(markerId) == 0)
        {
            gui->destroyWidget(child);
        }
    }
}

void SetMarkerWidgetsVisible(MyGUI::ImageBox* mapImage, bool visible)
{
    if (mapImage == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < mapImage->getChildCount(); ++index)
    {
        MyGUI::Widget* child = mapImage->getChildAt(index);
        if (child == 0)
        {
            continue;
        }

        int markerId = 0;
        if (TryParseMarkerWidgetId(SafeWidgetName(child), markerId)
            || SafeWidgetName(child) == kMarkerHoverLabelName)
        {
            child->setVisible(visible);
        }
    }
}

void ComputeMarkerLocalCoord(
    const MyGUI::IntCoord& imageCoord,
    const MarkerState& marker,
    bool isSelected,
    int& markerLeftOut,
    int& markerTopOut,
    int& markerSizeOut)
{
    markerSizeOut = isSelected ? kSelectedMarkerSize : kMarkerSize;
    const int maxLeft = imageCoord.width > markerSizeOut ? imageCoord.width - markerSizeOut : 0;
    const int maxTop = imageCoord.height > markerSizeOut ? imageCoord.height - markerSizeOut : 0;
    markerLeftOut = ClampInt(
        static_cast<int>(imageCoord.width * marker.normalizedX + 0.5f) - (markerSizeOut / 2),
        0,
        maxLeft);
    markerTopOut = ClampInt(
        static_cast<int>(imageCoord.height * marker.normalizedY + 0.5f) - (markerSizeOut / 2),
        0,
        maxTop);
}

void EnsureMarkerHoverLabelAttached(MyGUI::ImageBox* mapImage, const MyGUI::IntCoord& imageCoord)
{
    if (mapImage == 0)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    MyGUI::Widget* hoverLabel = FindDirectChildByName(mapImage, kMarkerHoverLabelName);
    const int hoveredMarkerId = FindHoveredMarkerId();
    MarkerState* hoveredMarker = FindMarkerById(hoveredMarkerId);
    if (!g_markersVisible || !g_showHoverLabels || hoveredMarker == 0)
    {
        if (hoverLabel != 0)
        {
            hoverLabel->setVisible(false);
        }
        if (!g_lastHoverLabelSignature.empty())
        {
            g_lastHoverLabelSignature.clear();
            LogProbeLine("hover_label hidden reason=no_hovered_marker_or_markers_hidden");
        }
        return;
    }

    if (hoverLabel == 0)
    {
        hoverLabel = mapImage->createWidget<MyGUI::Widget>(
            "Kenshi_GenericTextBoxFlatSkin",
            MyGUI::IntCoord(0, 0, kMarkerHoverLabelMinimumWidth, kMarkerHoverLabelHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kMarkerHoverLabelName);
        if (hoverLabel == 0)
        {
            return;
        }

        hoverLabel->setAlpha(0.88f);
        hoverLabel->setNeedMouseFocus(false);
        hoverLabel->setColour(g_hoverLabelBackgroundColour);

        MyGUI::TextBox* text = hoverLabel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(
                kMarkerHoverLabelHorizontalPadding,
                3,
                kMarkerHoverLabelMinimumWidth - (kMarkerHoverLabelHorizontalPadding * 2),
                kMarkerHoverLabelHeight - 6),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kMarkerHoverLabelTextName);
        if (text == 0)
        {
            gui->destroyWidget(hoverLabel);
            return;
        }

        text->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        text->setNeedMouseFocus(false);
        text->setTextColour(g_hoverLabelTextColour);
    }

    MyGUI::Widget* hoverLabelTextWidget = FindDirectChildByName(hoverLabel, kMarkerHoverLabelTextName);
    MyGUI::TextBox* hoverLabelText =
        hoverLabelTextWidget == 0 ? 0 : hoverLabelTextWidget->castType<MyGUI::TextBox>(false);
    if (hoverLabelText == 0)
    {
        hoverLabel->setVisible(false);
        return;
    }

    const std::string hoverCaption = BuildMarkerHoverCaption(*hoveredMarker);
    const std::string hoverDisplayText = BuildMarkerHoverDisplayText(hoverCaption);
    if (hoverDisplayText.empty())
    {
        hoverLabel->setVisible(false);
        return;
    }

    int markerLeft = 0;
    int markerTop = 0;
    int markerSize = 0;
    ComputeMarkerLocalCoord(
        imageCoord,
        *hoveredMarker,
        hoveredMarker->id == g_selectedMarkerId,
        markerLeft,
        markerTop,
        markerSize);

    const int labelWidth = BuildMarkerLabelDisplayWidth(
        hoverDisplayText,
        kMarkerHoverLabelMinimumWidth,
        kMarkerHoverLabelMaximumWidth,
        kMarkerHoverLabelHorizontalPadding);
    const int maxLeft = imageCoord.width > labelWidth ? imageCoord.width - labelWidth : 0;
    const int maxTop = imageCoord.height > kMarkerHoverLabelHeight ? imageCoord.height - kMarkerHoverLabelHeight : 0;
    const int labelLeft = ClampInt(
        markerLeft + (markerSize / 2) - (labelWidth / 2),
        0,
        maxLeft);
    const int labelTop = ClampInt(
        markerTop - kMarkerHoverLabelHeight - kMarkerHoverLabelVerticalOffset,
        0,
        maxTop);

    hoverLabel->setVisible(true);
    hoverLabel->setColour(g_hoverLabelBackgroundColour);
    hoverLabel->setCoord(labelLeft, labelTop, labelWidth, kMarkerHoverLabelHeight);
    hoverLabelText->setCoord(
        kMarkerHoverLabelHorizontalPadding,
        3,
        labelWidth - (kMarkerHoverLabelHorizontalPadding * 2),
        kMarkerHoverLabelHeight - 6);
    hoverLabelText->setTextColour(g_hoverLabelTextColour);
    hoverLabelText->setCaption(hoverDisplayText);

    std::stringstream hoverSignature;
    hoverSignature << hoveredMarker->id
                   << "|" << labelLeft << "," << labelTop
                   << "|" << labelWidth << "x" << kMarkerHoverLabelHeight
                   << "|" << hoverDisplayText;
    if (hoverSignature.str() != g_lastHoverLabelSignature)
    {
        g_lastHoverLabelSignature = hoverSignature.str();

        std::stringstream line;
        line << "hover_label placed"
             << " marker_id=" << hoveredMarker->id
             << " marker_coord=(" << markerLeft << "," << markerTop << "," << markerSize << ")"
             << " label_coord=(" << labelLeft << "," << labelTop << "," << labelWidth << "," << kMarkerHoverLabelHeight << ")"
             << " caption=\"" << hoverDisplayText << "\"";
        LogProbeLine(line.str());
    }
}

void EnsureMarkerWidgetsAttached()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        g_lastMarkerRenderSignature.clear();
        g_lastMarkerOcclusionSignature.clear();
        g_lastOverlayDiagnosticsSignature.clear();
        return;
    }

    const MyGUI::IntCoord imageCoord = mapImage->getCoord();
    if (imageCoord.width < kMinimumMapImageSize || imageCoord.height < kMinimumMapImageSize)
    {
        return;
    }

    DestroyStaleMarkerWidgets(mapImage);
    if (!g_markersVisible)
    {
        SetMarkerWidgetsVisible(mapImage, false);
        g_lastMarkerRenderSignature.clear();
        g_lastMarkerOcclusionSignature.clear();
        g_lastOverlayDiagnosticsSignature.clear();
        return;
    }

    std::stringstream signature;
    signature << SafeWidgetName(mapImage)
              << "|" << imageCoord.width << "x" << imageCoord.height
              << "|selected=" << g_selectedMarkerId
              << "|count=" << g_markers.size();

    MyGUI::Widget* editorPanel = FindMarkerEditorPanel();
    const bool editorPanelVisible =
        editorPanel != 0 && editorPanel->getVisible() && editorPanel->getInheritedVisible();
    const MyGUI::IntCoord editorPanelAbsolute =
        editorPanelVisible ? editorPanel->getAbsoluteCoord() : MyGUI::IntCoord();
    std::stringstream occlusionSignature;
    occlusionSignature << "editor_visible=" << (editorPanelVisible ? "true" : "false");

    std::stringstream line;
    line << "markers rendered"
         << " parent=" << BuildWidgetDescriptor(mapImage)
         << " count=" << g_markers.size()
         << " selected=" << g_selectedMarkerId;

    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        const MarkerState& marker = g_markers[index];
        const bool isSelected = marker.id == g_selectedMarkerId;
        int markerLeft = 0;
        int markerTop = 0;
        int markerSize = 0;
        ComputeMarkerLocalCoord(imageCoord, marker, isSelected, markerLeft, markerTop, markerSize);

        const std::string widgetName = BuildMarkerWidgetName(marker.id);
        MyGUI::Widget* widgetBase = FindDirectChildByName(mapImage, widgetName);
        MyGUI::Button* widget = widgetBase == 0 ? 0 : widgetBase->castType<MyGUI::Button>(false);
        if (widget == 0)
        {
            widget = mapImage->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(0, 0, markerSize, markerSize),
                MyGUI::Align::Default,
                widgetName);
        }

        widget->setNeedMouseFocus(true);
        widget->setAlpha(isSelected ? 1.0f : 0.92f);
        widget->setColour(BuildMarkerColour(marker.type, isSelected));
        widget->setCaption(MarkerTypeToGlyph(marker.type));
        widget->setCoord(markerLeft, markerTop, markerSize, markerSize);

        bool occludedByEditor = false;
        if (editorPanelVisible)
        {
            occludedByEditor = RectanglesIntersect(widget->getAbsoluteCoord(), editorPanelAbsolute);
        }

        widget->setVisible(!occludedByEditor);
        occlusionSignature << "|" << marker.id << ":" << (occludedByEditor ? "hidden" : "shown");

        signature << "|" << marker.id << ":" << markerLeft << "," << markerTop << "," << markerSize
                  << ":" << MarkerTypeToJsonValue(marker.type);
        line << " marker[" << marker.id << "]=(" << markerLeft << "," << markerTop << "," << markerSize << ")"
             << ":" << MarkerTypeToJsonValue(marker.type);
    }

    EnsureMarkerHoverLabelAttached(mapImage, imageCoord);

    const std::string occlusionSignatureString = occlusionSignature.str();
    if (occlusionSignatureString != g_lastMarkerOcclusionSignature)
    {
        g_lastMarkerOcclusionSignature = occlusionSignatureString;

        std::stringstream occlusionLine;
        occlusionLine << "marker_editor_occlusion"
                      << " editor_visible=" << (editorPanelVisible ? "true" : "false");
        if (editorPanelVisible)
        {
            occlusionLine << " editor_abs=("
                          << editorPanelAbsolute.left << ","
                          << editorPanelAbsolute.top << ","
                          << editorPanelAbsolute.width << ","
                          << editorPanelAbsolute.height << ")";
        }

        for (std::size_t index = 0; index < g_markers.size(); ++index)
        {
            const MarkerState& marker = g_markers[index];
            const std::string widgetName = BuildMarkerWidgetName(marker.id);
            MyGUI::Widget* widget = FindDirectChildByName(mapImage, widgetName);
            occlusionLine << " marker[" << marker.id << "]="
                          << ((widget != 0 && widget->getVisible()) ? "shown" : "hidden");
        }
        LogProbeLine(occlusionLine.str());
    }

    const std::string signatureString = signature.str();
    if (signatureString != g_lastMarkerRenderSignature)
    {
        g_lastMarkerRenderSignature = signatureString;
        LogProbeLine(line.str());

        if (!g_markerEditorDragging && FindMarkerEditorPanel() != 0)
        {
            LogMapOverlayDiagnostics(mapImage, "markers_rendered", false);
        }
    }
}

std::string BuildVisibleRootsSignature()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return "<no_gui>";
    }

    std::stringstream signature;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        signature << BuildWidgetDescriptor(root)
                  << " child_count=" << root->getChildCount()
                  << "\n";
    }

    return signature.str();
}

void LogVisibleRootsSnapshot(const char* reason)
{
    if (!ShouldEmitProbeLogs())
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogProbeLine("visible_roots unavailable: MyGUI::Gui singleton missing");
        return;
    }

    std::size_t rootCount = 0;
    std::stringstream header;
    header << "visible_roots reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(header.str());

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        std::stringstream line;
        line << "visible_root[" << rootCount << "] "
             << BuildWidgetDescriptor(root)
             << " child_count=" << root->getChildCount();
        LogProbeLine(line.str());
        ++rootCount;
    }

    if (rootCount == 0)
    {
        LogProbeLine("visible_root scan found no visible MyGUI roots");
    }
}

void LogSaveIdentityIfChanged(bool force)
{
    SaveManager* saveManager = SaveManager::getSingleton();
    SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();

    const std::string currentGame = saveManager == 0 ? "" : saveManager->getCurrentGame();
    const std::string activeSave = saveFileSystem == 0 ? "" : saveFileSystem->getActiveSave();

    std::stringstream identity;
    identity << currentGame << "\n" << activeSave;
    const std::string identityString = identity.str();
    const bool identityChanged = identityString != g_lastSaveIdentity;
    if (!force && !identityChanged)
    {
        return;
    }

    if (identityChanged)
    {
        g_lastSaveIdentity = identityString;
        LoadMarkersForActiveSave();
    }

    if (!force && currentGame.empty() && activeSave.empty())
    {
        return;
    }

    std::stringstream line;
    line << "save_identity current_game=\"" << currentGame
         << "\" active_save=\"" << activeSave << "\"";
    LogProbeLine(line.str());
}

void LogHoveredWidgetState(bool force)
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (force)
        {
            LogProbeLine("hover unavailable: MyGUI::InputManager singleton missing");
        }
        return;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();

    std::stringstream signature;
    signature << mouse.left << "," << mouse.top << "\n" << BuildWidgetChainForLog(hovered);
    const std::string signatureString = signature.str();
    if (!force && signatureString == g_lastHoveredSignature)
    {
        return;
    }

    g_lastHoveredSignature = signatureString;

    std::stringstream line;
    line << "hover mouse=(" << mouse.left << "," << mouse.top << ")"
         << " focus_chain=" << BuildWidgetChainForLog(hovered);
    LogProbeLine(line.str());
}

void TriggerManualSnapshot(const char* reason)
{
    ScopedProbeLogging scopedProbeLogging;

    std::stringstream line;
    line << "snapshot reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(line.str());
    LogSaveIdentityIfChanged(true);
    LogHoveredWidgetState(true);
    LogVisibleRootsSnapshot(reason);

    if (MyGUI::ImageBox* mapImage = FindActiveMapImage())
    {
        if (MyGUI::Window* mapWindow = FindOwningWindow(mapImage))
        {
            LogMapFooterDiagnostics(mapWindow, reason);
        }
        LogMapOverlayDiagnostics(mapImage, reason, true);
    }
}

bool AreProbeModifiersPressed(const InputHandler* inputHandler)
{
    return inputHandler != 0
        && inputHandler->ctrl
        && inputHandler->alt
        && !inputHandler->shift;
}

struct MapPointerContext
{
    MyGUI::Widget* hoveredWidget;
    MyGUI::ImageBox* mapImage;
    MyGUI::IntCoord absoluteCoord;
    MyGUI::IntPoint mouse;
    int localLeft;
    int localTop;
    int hoveredMarkerId;
};

bool DidMouseButtonJustGoDown(int virtualKeyCode, bool& lastObserved)
{
    const bool mouseDown = (GetAsyncKeyState(virtualKeyCode) & 0x8000) != 0;
    const bool justWentDown = mouseDown && !lastObserved;
    lastObserved = mouseDown;
    return justWentDown;
}

bool TryBuildMapPointerContext(MapPointerContext& contextOut)
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    MyGUI::Widget* hoveredWidget = inputManager->getMouseFocusWidget();
    MyGUI::ImageBox* mapImage = FindMapImageInParentChain(hoveredWidget);
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        return false;
    }

    const MyGUI::IntCoord absoluteCoord = mapImage->getAbsoluteCoord();
    if (absoluteCoord.width <= 0 || absoluteCoord.height <= 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    if (mouse.left < absoluteCoord.left
        || mouse.top < absoluteCoord.top
        || mouse.left >= absoluteCoord.left + absoluteCoord.width
        || mouse.top >= absoluteCoord.top + absoluteCoord.height)
    {
        return false;
    }

    contextOut.hoveredWidget = hoveredWidget;
    contextOut.mapImage = mapImage;
    contextOut.absoluteCoord = absoluteCoord;
    contextOut.mouse = mouse;
    contextOut.localLeft = mouse.left - absoluteCoord.left;
    contextOut.localTop = mouse.top - absoluteCoord.top;
    contextOut.hoveredMarkerId = FindMarkerWidgetIdInChain(hoveredWidget);
    return true;
}

float BuildNormalizedMapCoordinate(int localCoordinate, int extent)
{
    if (extent <= 0)
    {
        return 0.0f;
    }

    return ClampFloat(static_cast<float>(localCoordinate) / static_cast<float>(extent), 0.0f, 1.0f);
}

void ClearSelectedMarker(const char* reason)
{
    if (g_selectedMarkerId == 0)
    {
        return;
    }

    std::stringstream line;
    line << "marker deselected reason=" << (reason == 0 ? "<unknown>" : reason)
         << " marker_id=" << g_selectedMarkerId;

    g_selectedMarkerId = 0;
    g_focusMarkerLabelEditOnShow = false;
    g_lastMarkerRenderSignature.clear();
    LogProbeLine(line.str());
}

void SelectMarker(int markerId, const char* reason)
{
    if (markerId <= 0 || FindMarkerById(markerId) == 0 || g_selectedMarkerId == markerId)
    {
        return;
    }

    g_selectedMarkerId = markerId;
    g_lastMarkerRenderSignature.clear();

    std::stringstream line;
    line << "marker selected reason=" << (reason == 0 ? "<unknown>" : reason)
         << " marker_id=" << markerId;
    LogProbeLine(line.str());
}

void TryAddMarkerFromMiddleClick()
{
    if (!g_markersVisible)
    {
        return;
    }

    if (!DidMouseButtonJustGoDown(VK_MBUTTON, g_lastMiddleMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context) || context.hoveredMarkerId != 0)
    {
        return;
    }

    MarkerState marker;
    marker.id = g_nextMarkerId++;
    marker.normalizedX = BuildNormalizedMapCoordinate(context.localLeft, context.absoluteCoord.width);
    marker.normalizedY = BuildNormalizedMapCoordinate(context.localTop, context.absoluteCoord.height);
        marker.type = g_defaultMarkerType;
    marker.label.clear();
    g_markers.push_back(marker);
    g_selectedMarkerId = marker.id;
    g_focusMarkerLabelEditOnShow = true;
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker added trigger=MIDDLE_CLICK"
         << " marker_id=" << marker.id
         << " mouse=(" << context.mouse.left << "," << context.mouse.top << ")"
         << " local=(" << context.localLeft << "," << context.localTop << ")"
         << " normalized=(" << marker.normalizedX << "," << marker.normalizedY << ")"
         << " parent=" << BuildWidgetDescriptor(context.mapImage);
    LogProbeLine(line.str());
}

void TryHandleLeftClickSelectionOrMove()
{
    if (!g_markersVisible)
    {
        return;
    }

    if (!DidMouseButtonJustGoDown(VK_LBUTTON, g_lastLeftMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context))
    {
        return;
    }

    if (context.hoveredMarkerId != 0)
    {
        SelectMarker(context.hoveredMarkerId, "left_click_marker");
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    selectedMarker->normalizedX = BuildNormalizedMapCoordinate(context.localLeft, context.absoluteCoord.width);
    selectedMarker->normalizedY = BuildNormalizedMapCoordinate(context.localTop, context.absoluteCoord.height);
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker moved trigger=LEFT_CLICK"
         << " marker_id=" << selectedMarker->id
         << " mouse=(" << context.mouse.left << "," << context.mouse.top << ")"
         << " local=(" << context.localLeft << "," << context.localTop << ")"
         << " normalized=(" << selectedMarker->normalizedX << "," << selectedMarker->normalizedY << ")"
         << " parent=" << BuildWidgetDescriptor(context.mapImage);
    LogProbeLine(line.str());
}

void TryDeselectMarkerFromRightClick()
{
    if (!g_markersVisible
        || g_selectedMarkerId == 0
        || !DidMouseButtonJustGoDown(VK_RBUTTON, g_lastRightMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context))
    {
        return;
    }

    ClearSelectedMarker("right_click_map");
}

MyGUI::EditBox* FindActiveMarkerLabelEdit()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0)
    {
        return 0;
    }

    MyGUI::Widget* panelParent = FindMarkerEditorParent(mapImage);
    if (panelParent == 0)
    {
        return 0;
    }

    MyGUI::Widget* panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
    if (panel == 0)
    {
        return 0;
    }

    MyGUI::Widget* labelEditWidget = FindDirectChildByName(panel, kMarkerEditorLabelEditName);
    return labelEditWidget == 0 ? 0 : labelEditWidget->castType<MyGUI::EditBox>(false);
}

bool TryHandleMarkerKeyDown(OIS::KeyCode keyCode)
{
    if (!g_markersVisible || FindActiveMapImage() == 0 || g_selectedMarkerId == 0)
    {
        return false;
    }

    if (IsWidgetKeyFocused(FindActiveMarkerLabelEdit()))
    {
        return false;
    }

    if (keyCode != OIS::KC_DELETE)
    {
        return false;
    }

    const int markerIndex = FindMarkerIndexById(g_selectedMarkerId);
    if (markerIndex < 0)
    {
        ClearSelectedMarker("delete_missing_selection");
        return true;
    }

    const int deletedMarkerId = g_selectedMarkerId;
    g_markers.erase(g_markers.begin() + markerIndex);
    g_selectedMarkerId = 0;
    RefreshNextMarkerId();
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker deleted trigger=DELETE"
         << " marker_id=" << deletedMarkerId
         << " remaining=" << g_markers.size();
    LogProbeLine(line.str());
    return true;
}

void SaveManager_save_hook(SaveManager* thisptr, const std::string& saveName, bool autosave)
{
    ArmPendingSaveTransition(thisptr, saveName);
    if (SaveManager_save_orig)
    {
        SaveManager_save_orig(thisptr, saveName, autosave);
    }
}

void SaveManager_loadByInfo_hook(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos)
{
    ClearPendingSaveTransition("load_by_info");
    if (SaveManager_loadByInfo_orig)
    {
        SaveManager_loadByInfo_orig(thisptr, saveInfo, resetPos);
    }
}

void SaveManager_loadByName_hook(SaveManager* thisptr, const std::string& saveName)
{
    ClearPendingSaveTransition("load_by_name");
    if (SaveManager_loadByName_orig)
    {
        SaveManager_loadByName_orig(thisptr, saveName);
    }
}

void SaveManager_newGame_hook(SaveManager* thisptr, const std::string& startId)
{
    ClearPendingSaveTransition("new_game");
    if (SaveManager_newGame_orig)
    {
        SaveManager_newGame_orig(thisptr, startId);
    }
}

void SaveManager_import_hook(SaveManager* thisptr, const SaveInfo& saveInfo, int flags)
{
    ClearPendingSaveTransition("import");
    if (SaveManager_import_orig)
    {
        SaveManager_import_orig(thisptr, saveInfo, flags);
    }
}

// Открыта ли карта - по вызовам MapScreen::update: игра зовёт его только
// пока карта на экране. Без этого мод каждый кадр по нескольку раз обходил
// весь интерфейс в поисках карты, которой нет.
bool g_mapScreenHooked = false;
DWORD g_lastMapScreenUpdateMs = 0;

bool IsMapScreenLikelyOpen(DWORD now)
{
    if (!g_mapScreenHooked)
    {
        return true;                    // без перехвата - по-старому, искать каждый кадр
    }
    return g_lastMapScreenUpdateMs != 0 && now - g_lastMapScreenUpdateMs < 300;
}

struct MapImageTickCacheScope
{
    MapImageTickCacheScope()
    {
        g_mapImageTickCacheActive = true;
        g_mapImageTickCacheFilled = false;
        g_mapImageTickCache = 0;
    }
    ~MapImageTickCacheScope()
    {
        g_mapImageTickCacheActive = false;
        g_mapImageTickCacheFilled = false;
        g_mapImageTickCache = 0;
    }
};

void TickUiDiagnostics()
{
    if (g_markerEditorDragging && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        StopMarkerEditorDrag();
    }

    TickPendingSaveTransition();

    const DWORD tickNow = GetTickCount();
    if (!IsMapScreenLikelyOpen(tickNow) && !g_mapWasVisible && !g_markerEditorDragging && !g_probeLive)
    {
        return;                         // карта закрыта и была закрыта: делать нечего
    }

    MapImageTickCacheScope mapImageCache;

    // Смена сохранения: при открытии карты - сразу, дальше раз в секунду.
    static DWORD s_lastSaveIdentityCheckMs = 0;
    if (!g_mapWasVisible || tickNow - s_lastSaveIdentityCheckMs >= 1000)
    {
        s_lastSaveIdentityCheckMs = tickNow;
        LogSaveIdentityIfChanged(false);
    }
    const MyGUI::ImageBox* activeMapImage = FindActiveMapImage();
    const bool mapVisibleNow = activeMapImage != 0 && activeMapImage->getInheritedVisible();
    if (!mapVisibleNow && g_mapWasVisible && g_closeEditorOnMapClose)
    {
        ClearSelectedMarker("map_closed");
    }
    g_mapWasVisible = mapVisibleNow;

    TryAddMarkerFromMiddleClick();
    TryHandleLeftClickSelectionOrMove();
    TryDeselectMarkerFromRightClick();
    EnsureMarkerToggleButtonUi();
    EnsureMarkerWidgetsAttached();
    EnsureMarkerEditorUi();

    const DWORD now = GetTickCount();
    if (ShouldEmitProbeLogs() && now - g_lastVisibleRootsScanTick >= 500)
    {
        g_lastVisibleRootsScanTick = now;
        const std::string signature = BuildVisibleRootsSignature();
        if (signature != g_lastVisibleRootsSignature)
        {
            g_lastVisibleRootsSignature = signature;
            LogVisibleRootsSnapshot("visible_root_change");
        }
    }

    if (g_probeLive && now - g_lastHoverLogTick >= 200)
    {
        g_lastHoverLogTick = now;
        LogHoveredWidgetState(false);
    }
}

void (*MapScreen_update_orig)(MapScreen* thisptr) = 0;

void MapScreen_update_hook(MapScreen* thisptr)
{
    g_lastMapScreenUpdateMs = GetTickCount();
    if (MapScreen_update_orig)
    {
        MapScreen_update_orig(thisptr);
    }
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }

    if (!g_modEnabled)
    {
        if (!g_disabledUiStateApplied)
        {
            HideMapMarkersUi();
            g_disabledUiStateApplied = true;
        }
        return;
    }

    g_disabledUiStateApplied = false;
    TickUiDiagnostics();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    const bool modifiersPressed = AreProbeModifiersPressed(thisptr);

    if (modifiersPressed && keyCode == kProbeSnapshotHotkey)
    {
        TriggerManualSnapshot("manual_hotkey");
        return;
    }

    if (modifiersPressed && keyCode == kProbeLiveHotkey)
    {
        g_probeLive = !g_probeLive;

        {
            ScopedProbeLogging scopedProbeLogging;

            std::stringstream line;
            line << "live_hover_probe=" << (g_probeLive ? "enabled" : "disabled");
            LogProbeLine(line.str());
        }

        if (g_probeLive)
        {
            TriggerManualSnapshot("live_hover_enabled");
        }
        return;
    }

    if (TryHandleMarkerKeyDown(keyCode))
    {
        return;
    }

    if (InputHandler_keyDownEvent_orig)
    {
        InputHandler_keyDownEvent_orig(thisptr, keyCode);
    }
}
}

MapMarkersModConfigSnapshot MapMarkers_CaptureModConfigSnapshot()
{
    MapMarkersModConfigSnapshot snapshot;
    snapshot.enabled = g_modEnabled;
    snapshot.markersVisible = g_markersVisible;
    snapshot.closeEditorOnMapClose = g_closeEditorOnMapClose;
    snapshot.showHoverLabels = g_showHoverLabels;
    snapshot.editorPositionCustomized = g_markerEditorPositionCustomized;
    snapshot.editorLeft = g_markerEditorCustomLeft;
    snapshot.editorTop = g_markerEditorCustomTop;
    snapshot.defaultMarkerType = MarkerTypeToIndex(g_defaultMarkerType);
    return snapshot;
}

void MapMarkers_ApplyModConfigSnapshot(const MapMarkersModConfigSnapshot& snapshot)
{
    ApplyModConfigSnapshotInternal(snapshot);
}

bool MapMarkers_PersistCurrentModConfig(bool logSuccess)
{
    return SaveModConfig(logSuccess);
}

void MapMarkers_LogProbeMessage(const char* message)
{
    if (message != 0)
    {
        LogProbeLine(message);
    }
}

__declspec(dllexport) void startPlugin()
{
    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Map-markers: unsupported Kenshi version/platform");
        return;
    }

    LoadModConfig();

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Map-markers: could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Map-markers: could not hook InputHandler::keyDownEvent");
        return;
    }

    // Признак открытой карты. Не встал - мод работает по-старому, только дороже.
    g_mapScreenHooked = KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&MapScreen::update),
        MapScreen_update_hook,
        &MapScreen_update_orig);
    if (!g_mapScreenHooked)
    {
        ErrorLog("Map-markers WARN: could not hook MapScreen::update; the map is searched every frame");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&, bool)>(&SaveManager::save)),
        SaveManager_save_hook,
        &SaveManager_save_orig))
    {
        ErrorLog("Map-markers WARN: could not hook SaveManager::save(std::string,bool)");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const SaveInfo&, bool)>(&SaveManager::load)),
        SaveManager_loadByInfo_hook,
        &SaveManager_loadByInfo_orig))
    {
        ErrorLog("Map-markers WARN: could not hook SaveManager::load(SaveInfo,bool)");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&)>(&SaveManager::load)),
        SaveManager_loadByName_hook,
        &SaveManager_loadByName_orig))
    {
        ErrorLog("Map-markers WARN: could not hook SaveManager::load(std::string)");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&)>(&SaveManager::newGame)),
        SaveManager_newGame_hook,
        &SaveManager_newGame_orig))
    {
        ErrorLog("Map-markers WARN: could not hook SaveManager::newGame(std::string)");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const SaveInfo&, int)>(&SaveManager::import)),
        SaveManager_import_hook,
        &SaveManager_import_orig))
    {
        ErrorLog("Map-markers WARN: could not hook SaveManager::import(SaveInfo,int)");
    }

    MapMarkersModHub_OnStartup();

    DebugLog("Map-markers INFO: initialized");
}

BOOL APIENTRY DllMain(HMODULE moduleHandle, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_moduleHandle = moduleHandle;
    }

    return TRUE;
}
