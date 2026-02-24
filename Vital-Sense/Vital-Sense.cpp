#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

class UtilityT
{
public:
    UtilityT();
    bool worldToScreenPX(const Ogre::Vector3& pos, float& x, float& y);
};

namespace
{
const char* kPluginName = "Vital-Sense";
const char* kConfigFileName = "mod-config.json";

struct PluginConfig
{
    bool enabled;
    DWORD updateIntervalMs;
    bool onlyWhenAltHeld;
    DWORD maxHighlightDistanceMeters;
};

PluginConfig g_config = { true, 150, true, 3500 };
std::string g_settingsPath;
DWORD g_lastProbeTickMs = 0;
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;

struct CachedKoTarget
{
    hand targetHandle;
    Ogre::Vector3 worldPos;
    DWORD lastSeenMs;
};

std::vector<CachedKoTarget> g_koTargetCache;
std::vector<hand> g_visibleKoHandlesScratch;
std::vector<MyGUI::TextBox*> g_koMarkerWidgets;
UtilityT* g_projectionUtility = 0;
unsigned int g_koMarkerWidgetSerial = 0;
bool g_highlightRuntimeActive = false;

const size_t kMaxKoMarkerWidgets = 48;
const int kKoMarkerWidthPx = 36;
const int kKoMarkerHeightPx = 18;
const int kKoMarkerYOffsetPx = 24;

void LogWithPrefix(void (*sink)(const char*), const char* level, const std::string& message)
{
    if (!sink)
    {
        return;
    }

    std::stringstream line;
    line << kPluginName << " " << level << ": " << message;
    sink(line.str().c_str());
}

void LogInfo(const std::string& message)
{
    LogWithPrefix(&DebugLog, "INFO", message);
}

void LogWarn(const std::string& message)
{
    LogWithPrefix(&ErrorLog, "WARN", message);
}

void LogError(const std::string& message)
{
    LogWithPrefix(&ErrorLog, "ERROR", message);
}

std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

void SkipWhitespace(const std::string& body, size_t* pos)
{
    if (!pos)
    {
        return;
    }

    while (*pos < body.size() && std::isspace(static_cast<unsigned char>(body[*pos])) != 0)
    {
        ++(*pos);
    }
}

bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);

    if (pos + 4 <= body.size() && body.compare(pos, 4, "true") == 0)
    {
        *valueOut = true;
        return true;
    }

    if (pos + 5 <= body.size() && body.compare(pos, 5, "false") == 0)
    {
        *valueOut = false;
        return true;
    }

    return false;
}

bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || std::isdigit(static_cast<unsigned char>(body[pos])) == 0)
    {
        return false;
    }

    size_t end = pos;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0)
    {
        ++end;
    }

    DWORD parsed = 0;
    try
    {
        parsed = static_cast<DWORD>(std::stoul(body.substr(pos, end - pos)));
    }
    catch (...)
    {
        return false;
    }

    *valueOut = parsed;
    return true;
}

bool LoadConfigState()
{
    g_config.enabled = true;
    g_config.updateIntervalMs = 150;
    g_config.onlyWhenAltHeld = true;
    g_config.maxHighlightDistanceMeters = 3500;

    if (g_settingsPath.empty())
    {
        LogWarn("settings path is empty; using defaults");
        return false;
    }

    std::ifstream in(g_settingsPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        LogWarn("mod-config.json not found; using defaults");
        return true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool parsedEnabled = true;
    if (!ParseBoolFromJson(body, "enabled", &parsedEnabled))
    {
        LogWarn("invalid/missing key \"enabled\"; using default");
        return true;
    }

    g_config.enabled = parsedEnabled;
    DWORD parsedInterval = 0;
    if (ParseUnsignedFromJson(body, "update_interval_ms", &parsedInterval))
    {
        if (parsedInterval < 50)
        {
            g_config.updateIntervalMs = 50;
            LogWarn("update_interval_ms too low; clamped to 50");
        }
        else if (parsedInterval > 2000)
        {
            g_config.updateIntervalMs = 2000;
            LogWarn("update_interval_ms too high; clamped to 2000");
        }
        else
        {
            g_config.updateIntervalMs = parsedInterval;
        }
    }

    DWORD parsedDistanceMeters = 0;
    if (ParseUnsignedFromJson(body, "max_highlight_distance_m", &parsedDistanceMeters))
    {
        if (parsedDistanceMeters < 5)
        {
            g_config.maxHighlightDistanceMeters = 5;
            LogWarn("max_highlight_distance_m too low; clamped to 5");
        }
        else if (parsedDistanceMeters > 20000)
        {
            g_config.maxHighlightDistanceMeters = 20000;
            LogWarn("max_highlight_distance_m too high; clamped to 20000");
        }
        else
        {
            g_config.maxHighlightDistanceMeters = parsedDistanceMeters;
        }
    }

    bool parsedAltGate = true;
    if (ParseBoolFromJson(body, "only_when_alt_held", &parsedAltGate))
    {
        g_config.onlyWhenAltHeld = parsedAltGate;
    }

    return true;
}

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

int FindCachedKoTargetIndex(const hand& targetHandle)
{
    for (size_t i = 0; i < g_koTargetCache.size(); ++i)
    {
        const hand& cached = g_koTargetCache[i].targetHandle;
        if (cached.type == targetHandle.type
            && cached.index == targetHandle.index
            && cached.serial == targetHandle.serial)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool VisibleHandleListContains(const hand& targetHandle)
{
    for (size_t i = 0; i < g_visibleKoHandlesScratch.size(); ++i)
    {
        const hand& visible = g_visibleKoHandlesScratch[i];
        if (visible.type == targetHandle.type
            && visible.index == targetHandle.index
            && visible.serial == targetHandle.serial)
        {
            return true;
        }
    }
    return false;
}

void SetKoMarkerVisible(MyGUI::TextBox* marker, bool visible)
{
    if (!marker)
    {
        return;
    }

    __try
    {
        marker->setVisible(visible);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void SetKoMarkerCaption(MyGUI::TextBox* marker, const char* caption)
{
    if (!marker || !caption)
    {
        return;
    }

    try
    {
        marker->setCaption(caption);
    }
    catch (...)
    {
    }
}

void SetKoMarkerPosition(MyGUI::TextBox* marker, int left, int top)
{
    if (!marker)
    {
        return;
    }

    try
    {
        marker->setCoord(left, top, kKoMarkerWidthPx, kKoMarkerHeightPx);
    }
    catch (...)
    {
    }
}

void HideAllKoMarkerWidgets()
{
    for (size_t i = 0; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false);
    }
}

bool CreateKoMarkerWidgetAt(size_t index)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        return false;
    }

    try
    {
        std::stringstream name;
        name << "VS_KOMarker_" << index << "_" << g_koMarkerWidgetSerial++;

        MyGUI::TextBox* marker = gui->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(0, 0, kKoMarkerWidthPx, kKoMarkerHeightPx),
            MyGUI::Align::Default,
            "Popup",
            name.str());
        if (!marker)
        {
            marker = gui->createWidget<MyGUI::TextBox>(
                "TextBox",
                MyGUI::IntCoord(0, 0, kKoMarkerWidthPx, kKoMarkerHeightPx),
                MyGUI::Align::Default,
                "Popup",
                name.str() + "_fallback");
        }
        if (!marker)
        {
            return false;
        }

        marker->setNeedMouseFocus(false);
        marker->setCaption("KO");
        marker->setTextAlign(MyGUI::Align::Center);
        marker->setTextColour(MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f));
        marker->setTextShadow(true);
        marker->setVisible(false);

        if (index >= g_koMarkerWidgets.size())
        {
            g_koMarkerWidgets.push_back(marker);
        }
        else
        {
            g_koMarkerWidgets[index] = marker;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool EnsureKoMarkerPool(size_t requiredCount)
{
    if (requiredCount > kMaxKoMarkerWidgets)
    {
        requiredCount = kMaxKoMarkerWidgets;
    }

    while (g_koMarkerWidgets.size() < requiredCount)
    {
        if (!CreateKoMarkerWidgetAt(g_koMarkerWidgets.size()))
        {
            return false;
        }
    }

    for (size_t i = 0; i < requiredCount; ++i)
    {
        if (!g_koMarkerWidgets[i] && !CreateKoMarkerWidgetAt(i))
        {
            return false;
        }
    }

    return true;
}

bool EnsureProjectionUtility()
{
    if (g_projectionUtility)
    {
        return true;
    }

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    uintptr_t utilityOffset = 0;
    if (platform == KenshiLib::BinaryVersion::STEAM)
    {
        if (version == "1.0.65")
        {
            utilityOffset = 0x02134b10;
        }
        else if (version == "1.0.68")
        {
            utilityOffset = 0x02135b70;
        }
    }
    else if (platform == KenshiLib::BinaryVersion::GOG)
    {
        if (version == "1.0.65")
        {
            utilityOffset = 0x02132a80;
        }
        else if (version == "1.0.68")
        {
            utilityOffset = 0x02134aa0;
        }
    }

    if (utilityOffset == 0)
    {
        return false;
    }

    HMODULE exeHandle = GetModuleHandleA(0);
    if (!exeHandle)
    {
        return false;
    }

    const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(exeHandle);
    if (!baseAddress)
    {
        return false;
    }

    g_projectionUtility = reinterpret_cast<UtilityT*>(baseAddress + utilityOffset);
    return g_projectionUtility != 0;
}

int ClampInt(int value, int minValue, int maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

bool TryGetViewSize(int* widthOut, int* heightOut)
{
    if (!widthOut || !heightOut)
    {
        return false;
    }

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (!renderManager)
    {
        return false;
    }

    const MyGUI::IntSize& viewSize = renderManager->getViewSize();
    if (viewSize.width <= 0 || viewSize.height <= 0)
    {
        return false;
    }

    *widthOut = viewSize.width;
    *heightOut = viewSize.height;
    return true;
}

bool ConvertProjectionToPixels(float rawX, float rawY, int viewWidth, int viewHeight, float* pixelXOut, float* pixelYOut, bool* normalizedUsedOut)
{
    if (!pixelXOut || !pixelYOut || viewWidth <= 0 || viewHeight <= 0)
    {
        return false;
    }

    if (normalizedUsedOut)
    {
        *normalizedUsedOut = false;
    }

    if (rawX >= 0.0f && rawX <= 1.0f && rawY >= 0.0f && rawY <= 1.0f)
    {
        *pixelXOut = rawX * static_cast<float>(viewWidth);
        *pixelYOut = rawY * static_cast<float>(viewHeight);
        if (normalizedUsedOut)
        {
            *normalizedUsedOut = true;
        }
        return true;
    }

    if (rawX >= -1.0f && rawX <= 1.0f && rawY >= -1.0f && rawY <= 1.0f)
    {
        *pixelXOut = (rawX * 0.5f + 0.5f) * static_cast<float>(viewWidth);
        *pixelYOut = (rawY * 0.5f + 0.5f) * static_cast<float>(viewHeight);
        if (normalizedUsedOut)
        {
            *normalizedUsedOut = true;
        }
        return true;
    }

    *pixelXOut = rawX;
    *pixelYOut = rawY;
    return true;
}

bool TryProjectWorldToScreenPx(const Ogre::Vector3& worldPos, float* xOut, float* yOut)
{
    if (!xOut || !yOut)
    {
        return false;
    }

    if (!EnsureProjectionUtility())
    {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    bool projected = false;
    __try
    {
        projected = g_projectionUtility->worldToScreenPX(worldPos, x, y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_projectionUtility = 0;
        return false;
    }

    if (!projected)
    {
        return false;
    }

    *xOut = x;
    *yOut = y;
    return true;
}

void TickKoMarkerRender()
{
    if (!g_config.enabled)
    {
        HideAllKoMarkerWidgets();
        return;
    }

    size_t renderableTargetCount = g_koTargetCache.size();
    if (renderableTargetCount > kMaxKoMarkerWidgets)
    {
        renderableTargetCount = kMaxKoMarkerWidgets;
    }

    if (renderableTargetCount == 0)
    {
        HideAllKoMarkerWidgets();
        return;
    }

    if (!EnsureKoMarkerPool(renderableTargetCount))
    {
        HideAllKoMarkerWidgets();
        return;
    }

    int viewWidth = 0;
    int viewHeight = 0;
    const bool hasViewSize = TryGetViewSize(&viewWidth, &viewHeight);

    size_t visibleMarkerCount = 0;
    for (size_t i = 0; i < renderableTargetCount; ++i)
    {
        const CachedKoTarget& cached = g_koTargetCache[i];

        Ogre::Vector3 anchor = cached.worldPos;
        Character* targetCharacter = cached.targetHandle.getCharacter();
        if (targetCharacter && targetCharacter->isValid())
        {
            anchor = targetCharacter->getPosition();
        }
        anchor.y += 2.0f;

        float screenX = 0.0f;
        float screenY = 0.0f;
        if (!TryProjectWorldToScreenPx(anchor, &screenX, &screenY))
        {
            continue;
        }

        float pixelX = screenX;
        float pixelY = screenY;
        if (hasViewSize)
        {
            if (!ConvertProjectionToPixels(screenX, screenY, viewWidth, viewHeight, &pixelX, &pixelY, 0))
            {
                continue;
            }
        }

        if (visibleMarkerCount >= g_koMarkerWidgets.size())
        {
            break;
        }

        int markerLeft = static_cast<int>(pixelX) - (kKoMarkerWidthPx / 2);
        int markerTop = static_cast<int>(pixelY) - kKoMarkerYOffsetPx;
        if (hasViewSize)
        {
            const int maxLeft = (viewWidth > kKoMarkerWidthPx) ? (viewWidth - kKoMarkerWidthPx) : 0;
            const int maxTop = (viewHeight > kKoMarkerHeightPx) ? (viewHeight - kKoMarkerHeightPx) : 0;
            const int clampedLeft = ClampInt(markerLeft, 0, maxLeft);
            const int clampedTop = ClampInt(markerTop, 0, maxTop);
            markerLeft = clampedLeft;
            markerTop = clampedTop;
        }

        MyGUI::TextBox* marker = g_koMarkerWidgets[visibleMarkerCount];
        SetKoMarkerCaption(marker, "KO");
        SetKoMarkerPosition(marker, markerLeft, markerTop);
        SetKoMarkerVisible(marker, true);

        ++visibleMarkerCount;
    }

    for (size_t i = visibleMarkerCount; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false);
    }
}

bool IsKoOrUnconscious(Character* candidate, bool* isUnconsciousOut, bool* isLiteralOut)
{
    if (isUnconsciousOut)
    {
        *isUnconsciousOut = false;
    }
    if (isLiteralOut)
    {
        *isLiteralOut = false;
    }

    if (!candidate)
    {
        return false;
    }

    bool isUnconscious = false;
    bool isLiteral = false;
    __try
    {
        isUnconscious = candidate->isUnconcious();
        isLiteral = candidate->isLiterallyUnconciousNotPretending();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (isUnconsciousOut)
    {
        *isUnconsciousOut = isUnconscious;
    }
    if (isLiteralOut)
    {
        *isLiteralOut = isLiteral;
    }
    return isUnconscious || isLiteral;
}

bool IsHighlightGateOpen()
{
    if (!g_config.onlyWhenAltHeld)
    {
        return true;
    }

    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

bool IsWithinHighlightRange(const Ogre::Vector3& sourcePos, const Ogre::Vector3& targetPos)
{
    if (g_config.maxHighlightDistanceMeters == 0)
    {
        return true;
    }

    const float maxDistance = static_cast<float>(g_config.maxHighlightDistanceMeters);
    const float maxDistanceSq = maxDistance * maxDistance;
    const float dx = targetPos.x - sourcePos.x;
    const float dz = targetPos.z - sourcePos.z;
    return (dx * dx + dz * dz) <= maxDistanceSq;
}

void TickKoProbe()
{
    const bool canRun = g_config.enabled && IsHighlightGateOpen() && ou;
    if (!canRun)
    {
        if (g_highlightRuntimeActive)
        {
            g_koTargetCache.clear();
            HideAllKoMarkerWidgets();
            g_highlightRuntimeActive = false;
        }
        return;
    }
    g_highlightRuntimeActive = true;

    const DWORD nowMs = GetTickCount();
    if (g_lastProbeTickMs != 0 && (nowMs - g_lastProbeTickMs) < g_config.updateIntervalMs)
    {
        TickKoMarkerRender();
        return;
    }
    g_lastProbeTickMs = nowMs;

    const Ogre::Vector3 cameraCenter = ou->getCameraCenter();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    g_visibleKoHandlesScratch.clear();

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (!candidate || !candidate->isValid())
        {
            continue;
        }

        if (!candidate->isOnScreen)
        {
            continue;
        }

        if (candidate->isDead())
        {
            continue;
        }

        if (!IsWithinHighlightRange(cameraCenter, candidate->getPosition()))
        {
            continue;
        }

        if (IsKoOrUnconscious(candidate, 0, 0))
        {
            const hand targetHandle = candidate->getHandle();
            if (targetHandle.isNull())
            {
                continue;
            }

            if (!VisibleHandleListContains(targetHandle))
            {
                g_visibleKoHandlesScratch.push_back(targetHandle);
            }

            const int existingIndex = FindCachedKoTargetIndex(targetHandle);
            if (existingIndex >= 0)
            {
                CachedKoTarget& existing = g_koTargetCache[existingIndex];
                existing.worldPos = candidate->getPosition();
                existing.lastSeenMs = nowMs;
            }
            else
            {
                CachedKoTarget created = { targetHandle, candidate->getPosition(), nowMs };
                g_koTargetCache.push_back(created);
            }
        }
    }

    for (int i = static_cast<int>(g_koTargetCache.size()) - 1; i >= 0; --i)
    {
        if (!VisibleHandleListContains(g_koTargetCache[static_cast<size_t>(i)].targetHandle))
        {
            g_koTargetCache.erase(g_koTargetCache.begin() + i);
        }
    }
    TickKoMarkerRender();
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }
    TickKoProbe();
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfo("startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogError("unsupported Kenshi version/platform");
        return;
    }

    LoadConfigState();

    std::stringstream info;
    info << "loaded (enabled=" << (g_config.enabled ? "true" : "false")
         << ", update_interval_ms=" << g_config.updateIntervalMs
         << ", only_when_alt_held=" << (g_config.onlyWhenAltHeld ? "true" : "false")
         << ", max_highlight_distance_m=" << g_config.maxHighlightDistanceMeters
         << ")";
    LogInfo(info.str());

    g_koTargetCache.reserve(128);
    g_visibleKoHandlesScratch.reserve(128);
    g_koMarkerWidgets.reserve(kMaxKoMarkerWidgets);

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        LogError("could not hook PlayerInterface::updateUT");
        return;
    }

    LogInfo("update hook installed");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            const std::string fullPath = TrimAscii(std::string(dllPath));
            const size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string pluginDir = fullPath.substr(0, sep);
                g_settingsPath = pluginDir + "\\" + kConfigFileName;
            }
        }
    }

    return TRUE;
}
