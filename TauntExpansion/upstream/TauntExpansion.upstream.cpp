#include <Windows.h>

#include <Debug.h>
#include <core/Functions.h>

// KenshiLib's AI.h uses AI before declaring it and incorrectly forward-declares
// CharacterMessage as a class, while Character.h defines CharacterMessage as an enum.
// Keep the workaround local to AI.h so KenshiLib itself does not need modification.
class AI;
#define CharacterMessage KenshiLibAICharacterMessageWorkaround
#include <kenshi/AI/AI.h>
#undef CharacterMessage

#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/RootObject.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <sstream>
#include <string>
#include <stdint.h>
#include <limits.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace
{
    const char* const kPluginName = "TauntExpansion";
    const char* const kPluginVersion = "1.0.0-rc1";

    struct Config
    {
        bool enabled;
        bool logCandidates;
        bool logOnlyWhenTauntPresent;
        bool logOnlyChangedSelection;
        unsigned int minimumLogIntervalMs;
        unsigned int maximumCandidatesPerEntry;
        float tauntMultiplier;
        float retargetMultiplier;

        Config()
            : enabled(true),
              logCandidates(true),
              logOnlyWhenTauntPresent(true),
              logOnlyChangedSelection(true),
              minimumLogIntervalMs(500),
              maximumCandidatesPerEntry(32),
              tauntMultiplier(1.0f),
              retargetMultiplier(1.0f)
        {
        }
    };

    Config g_config;
    HMODULE g_module = NULL;

    typedef bool (*ChooseBestAttackTargetFn)(
        AI* self,
        RootObject** targetOut,
        lektor<Character*>& enemies,
        float& score,
        hand oldTarget,
        bool lawEnforcer,
        bool combatModeNow);

    ChooseBestAttackTargetFn g_originalChooseBestAttackTarget = NULL;

    struct ObservationState
    {
        ULONGLONG lastLogTime;
        RootObject* lastSelectedTarget;
        float lastScore;

        ObservationState()
            : lastLogTime(0), lastSelectedTarget(NULL), lastScore(0.0f)
        {
        }
    };

    CRITICAL_SECTION g_stateLock;
    CRITICAL_SECTION g_patchExecutionLock;
    bool g_stateLockInitialised = false;
    bool g_patchExecutionLockInitialised = false;
    std::map<AI*, ObservationState> g_observationState;

    std::string GetModuleDirectory()
    {
        char path[MAX_PATH] = { 0 };
        const DWORD length = GetModuleFileNameA(g_module, path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return ".";
        }

        std::string result(path, length);
        const std::string::size_type separator = result.find_last_of("\\/");
        if (separator == std::string::npos)
        {
            return ".";
        }

        return result.substr(0, separator);
    }

    bool ReadBool(const char* section, const char* key, bool defaultValue, const std::string& iniPath)
    {
        char buffer[32] = { 0 };
        const char* defaultText = defaultValue ? "true" : "false";
        GetPrivateProfileStringA(section, key, defaultText, buffer, sizeof(buffer), iniPath.c_str());

        std::string value(buffer);
        value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);

        if (value == "true" || value == "yes" || value == "on" || value == "1")
        {
            return true;
        }
        if (value == "false" || value == "no" || value == "off" || value == "0")
        {
            return false;
        }

        return defaultValue;
    }

    unsigned int ReadUInt(
        const char* section,
        const char* key,
        unsigned int defaultValue,
        unsigned int minimumValue,
        unsigned int maximumValue,
        const std::string& iniPath)
    {
        const int rawValue = GetPrivateProfileIntA(section, key, static_cast<int>(defaultValue), iniPath.c_str());
        const int clamped = std::max(
            static_cast<int>(minimumValue),
            std::min(rawValue, static_cast<int>(maximumValue)));
        return static_cast<unsigned int>(clamped);
    }

    float ReadFloat(
        const char* section,
        const char* key,
        float defaultValue,
        float minimumValue,
        float maximumValue,
        const std::string& iniPath)
    {
        char defaultBuffer[64] = { 0 };
        char valueBuffer[64] = { 0 };
        sprintf_s(defaultBuffer, sizeof(defaultBuffer), "%.6f", defaultValue);
        GetPrivateProfileStringA(section, key, defaultBuffer, valueBuffer, sizeof(valueBuffer), iniPath.c_str());

        char* end = NULL;
        const double parsed = strtod(valueBuffer, &end);
        if (end == valueBuffer)
        {
            return defaultValue;
        }

        const float value = static_cast<float>(parsed);
        return std::max(minimumValue, std::min(value, maximumValue));
    }

    void LoadConfig()
    {
        const std::string iniPath = GetModuleDirectory() + "\\TauntExpansion.ini";

        g_config.enabled = ReadBool("General", "Enabled", true, iniPath);
        g_config.logCandidates = ReadBool("Logging", "LogCandidates", true, iniPath);
        g_config.logOnlyWhenTauntPresent = ReadBool("Logging", "LogOnlyWhenTauntPresent", true, iniPath);
        g_config.logOnlyChangedSelection = ReadBool("Logging", "LogOnlyChangedSelection", true, iniPath);
        g_config.minimumLogIntervalMs = ReadUInt(
            "Logging", "MinimumLogIntervalMs", 500, 0, 60000, iniPath);
        g_config.maximumCandidatesPerEntry = ReadUInt(
            "Logging", "MaximumCandidatesPerEntry", 32, 1, 512, iniPath);
        g_config.tauntMultiplier = ReadFloat(
            "Taunt", "Multiplier", 1.0f, 0.0f, 100.0f, iniPath);
        g_config.retargetMultiplier = ReadFloat(
            "Taunt", "RetargetMultiplier", 1.0f, 0.0f, 10.0f, iniPath);

        std::ostringstream message;
        message << "[" << kPluginName << "] Config loaded: Enabled=" << (g_config.enabled ? "true" : "false")
                << ", LogCandidates=" << (g_config.logCandidates ? "true" : "false")
                << ", LogOnlyWhenTauntPresent=" << (g_config.logOnlyWhenTauntPresent ? "true" : "false")
                << ", LogOnlyChangedSelection=" << (g_config.logOnlyChangedSelection ? "true" : "false")
                << ", MinimumLogIntervalMs=" << g_config.minimumLogIntervalMs
                << ", MaximumCandidatesPerEntry=" << g_config.maximumCandidatesPerEntry
                << ", TauntMultiplier=" << std::fixed << std::setprecision(3) << g_config.tauntMultiplier
                << ", RetargetMultiplier=" << g_config.retargetMultiplier;
        DebugLog(message.str());
    }



    struct TauntPatchState
    {
        unsigned char* patchAddress;
        unsigned char originalBytes[8];
        unsigned char* codeCave;
        float vanillaBonus;
        float modifiedBonus;
        float* activeBonusAddress;
        bool installed;

        TauntPatchState()
            : patchAddress(NULL), codeCave(NULL), vanillaBonus(0.0f),
              modifiedBonus(0.0f), activeBonusAddress(NULL), installed(false)
        {
            ZeroMemory(originalBytes, sizeof(originalBytes));
        }
    };

    TauntPatchState g_tauntPatch;

    bool IsExecutableProtection(DWORD protection)
    {
        const DWORD value = protection & 0xFF;
        return value == PAGE_EXECUTE ||
               value == PAGE_EXECUTE_READ ||
               value == PAGE_EXECUTE_READWRITE ||
               value == PAGE_EXECUTE_WRITECOPY;
    }

    unsigned char* AllocateNearAddress(unsigned char* target, size_t size)
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        const uintptr_t granularity = static_cast<uintptr_t>(info.dwAllocationGranularity);
        const uintptr_t targetAddress = reinterpret_cast<uintptr_t>(target);
        const uintptr_t maximumDistance = 0x70000000ULL;
        const uintptr_t minimumAddress = targetAddress > maximumDistance
            ? targetAddress - maximumDistance
            : reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
        const uintptr_t maximumAddress = std::min(
            targetAddress + maximumDistance,
            reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress));

        for (uintptr_t distance = 0; distance < maximumDistance; distance += granularity)
        {
            uintptr_t candidates[2] = { 0, 0 };
            candidates[0] = targetAddress >= distance ? targetAddress - distance : 0;
            candidates[1] = targetAddress + distance;

            for (int i = 0; i < 2; ++i)
            {
                uintptr_t address = candidates[i] & ~(granularity - 1);
                if (address < minimumAddress || address > maximumAddress)
                {
                    continue;
                }

                void* memory = VirtualAlloc(
                    reinterpret_cast<void*>(address),
                    size,
                    MEM_RESERVE | MEM_COMMIT,
                    PAGE_EXECUTE_READWRITE);
                if (memory != NULL)
                {
                    return static_cast<unsigned char*>(memory);
                }
            }
        }

        return NULL;
    }

    bool WriteRelativeJump(unsigned char* source, unsigned char* destination, size_t overwrittenLength)
    {
        if (overwrittenLength < 5)
        {
            return false;
        }

        const intptr_t displacement = destination - (source + 5);
        if (displacement < INT_MIN || displacement > INT_MAX)
        {
            return false;
        }

        DWORD oldProtection = 0;
        if (!VirtualProtect(source, overwrittenLength, PAGE_EXECUTE_READWRITE, &oldProtection))
        {
            return false;
        }

        source[0] = 0xE9;
        *reinterpret_cast<int32_t*>(source + 1) = static_cast<int32_t>(displacement);
        for (size_t i = 5; i < overwrittenLength; ++i)
        {
            source[i] = 0x90;
        }

        DWORD ignored = 0;
        VirtualProtect(source, overwrittenLength, oldProtection, &ignored);
        FlushInstructionCache(GetCurrentProcess(), source, overwrittenLength);
        return true;
    }

    bool InstallTauntMultiplierPatch(intptr_t functionAddress)
    {
        // Verified Kenshi 1.0.65 pattern:
        // cmp byte ptr [rax+0x12A],0; je +0x10; addss xmm7,[rip+disp32]
        const unsigned char pattern[] = {
            0x80, 0xB8, 0x2A, 0x01, 0x00, 0x00, 0x00,
            0x74, 0x10,
            0xF3, 0x0F, 0x58, 0x3D
        };

        unsigned char* function = reinterpret_cast<unsigned char*>(functionAddress);
        unsigned char* match = NULL;
        const size_t scanSize = 0x1200;

        for (size_t i = 0; i + sizeof(pattern) <= scanSize; ++i)
        {
            if (memcmp(function + i, pattern, sizeof(pattern)) == 0)
            {
                if (match != NULL)
                {
                    ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: signature is not unique.");
                    return false;
                }
                match = function + i;
            }
        }

        if (match == NULL)
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: verified signature not found.");
            return false;
        }

        unsigned char* addInstruction = match + 9;
        const int32_t vanillaDisplacement = *reinterpret_cast<int32_t*>(addInstruction + 4);
        float* vanillaBonusAddress = reinterpret_cast<float*>(addInstruction + 8 + vanillaDisplacement);

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(vanillaBonusAddress, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT)
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: vanilla bonus address is invalid.");
            return false;
        }

        const float vanillaBonus = *vanillaBonusAddress;
        if (!(vanillaBonus > -1000.0f && vanillaBonus < 1000.0f))
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: vanilla bonus failed sanity check.");
            return false;
        }

        unsigned char* cave = AllocateNearAddress(addInstruction, 64);
        if (cave == NULL)
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: could not allocate nearby code cave.");
            return false;
        }

        // Cave layout:
        // 00: addss xmm7,[rip+9]  (points to float at cave+17)
        // 08: jmp back
        // 13: padding
        // 17: modified float
        cave[0] = 0xF3;
        cave[1] = 0x0F;
        cave[2] = 0x58;
        cave[3] = 0x3D;
        *reinterpret_cast<int32_t*>(cave + 4) = 9;
        cave[8] = 0xE9;
        const intptr_t returnDisplacement = (addInstruction + 8) - (cave + 13);
        if (returnDisplacement < INT_MIN || returnDisplacement > INT_MAX)
        {
            VirtualFree(cave, 0, MEM_RELEASE);
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: return jump is out of range.");
            return false;
        }
        *reinterpret_cast<int32_t*>(cave + 9) = static_cast<int32_t>(returnDisplacement);
        cave[13] = cave[14] = cave[15] = cave[16] = 0x90;

        const float modifiedBonus = vanillaBonus * g_config.tauntMultiplier;
        *reinterpret_cast<float*>(cave + 17) = modifiedBonus;
        FlushInstructionCache(GetCurrentProcess(), cave, 21);

        memcpy(g_tauntPatch.originalBytes, addInstruction, 8);
        if (!WriteRelativeJump(addInstruction, cave, 8))
        {
            VirtualFree(cave, 0, MEM_RELEASE);
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: failed to write branch.");
            return false;
        }

        g_tauntPatch.patchAddress = addInstruction;
        g_tauntPatch.codeCave = cave;
        g_tauntPatch.vanillaBonus = vanillaBonus;
        g_tauntPatch.modifiedBonus = modifiedBonus;
        g_tauntPatch.activeBonusAddress = reinterpret_cast<float*>(cave + 17);
        g_tauntPatch.installed = true;

        std::ostringstream message;
        message << "[" << kPluginName << "] Taunt multiplier patch installed: Instruction=0x"
                << std::hex << std::uppercase << reinterpret_cast<intptr_t>(addInstruction)
                << std::dec << std::fixed << std::setprecision(6)
                << ", VanillaBonus=" << vanillaBonus
                << ", Multiplier=" << g_config.tauntMultiplier
                << ", EffectiveBonus=" << modifiedBonus
                << ". Only the taunt-specific addss instruction is redirected; the shared vanilla constant is unchanged.";
        DebugLog(message.str());
        return true;
    }

    void RemoveTauntMultiplierPatch()
    {
        if (!g_tauntPatch.installed || g_tauntPatch.patchAddress == NULL)
        {
            return;
        }

        DWORD oldProtection = 0;
        if (VirtualProtect(g_tauntPatch.patchAddress, 8, PAGE_EXECUTE_READWRITE, &oldProtection))
        {
            memcpy(g_tauntPatch.patchAddress, g_tauntPatch.originalBytes, 8);
            DWORD ignored = 0;
            VirtualProtect(g_tauntPatch.patchAddress, 8, oldProtection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), g_tauntPatch.patchAddress, 8);
        }

        if (g_tauntPatch.codeCave != NULL)
        {
            VirtualFree(g_tauntPatch.codeCave, 0, MEM_RELEASE);
        }

        g_tauntPatch = TauntPatchState();
    }

    std::string SafeName(RootObjectBase* object)
    {
        if (object == NULL)
        {
            return "<null>";
        }

        try
        {
            const std::string name = object->getName();
            return name.empty() ? "<unnamed>" : name;
        }
        catch (...)
        {
            return "<name-error>";
        }
    }

    bool IsTaunting(Character* character)
    {
        return character != NULL &&
               character->stats != NULL &&
               character->stats->tauntMode;
    }

    bool AnyCandidateIsTaunting(const lektor<Character*>& enemies)
    {
        for (uint32_t i = 0; i < enemies.size(); ++i)
        {
            if (IsTaunting(enemies[i]))
            {
                return true;
            }
        }
        return false;
    }

    bool ShouldLog(AI* self, RootObject* selectedTarget, float score)
    {
        const ULONGLONG now = GetTickCount64();
        bool shouldLog = true;

        EnterCriticalSection(&g_stateLock);
        ObservationState& state = g_observationState[self];

        if (g_config.logOnlyChangedSelection && state.lastSelectedTarget == selectedTarget)
        {
            shouldLog = false;
        }

        if (shouldLog && g_config.minimumLogIntervalMs > 0 &&
            now - state.lastLogTime < g_config.minimumLogIntervalMs)
        {
            shouldLog = false;
        }

        if (shouldLog)
        {
            state.lastLogTime = now;
            state.lastSelectedTarget = selectedTarget;
            state.lastScore = score;
        }

        LeaveCriticalSection(&g_stateLock);
        return shouldLog;
    }

    void LogObservation(
        AI* self,
        RootObject* selectedTarget,
        const lektor<Character*>& enemies,
        float score,
        const hand& oldTarget,
        bool lawEnforcer,
        bool combatModeNow,
        bool result)
    {
        std::ostringstream message;
        message << std::fixed << std::setprecision(3);
        message << "[TauntExpansion:Target] Actor=\"" << SafeName(self != NULL ? self->me : NULL) << "\""
                << " Result=" << (result ? "true" : "false")
                << " Selected=\"" << SafeName(selectedTarget) << "\""
                << " Score=" << score
                << " CandidateCount=" << enemies.size()
                << " LawEnforcer=" << (lawEnforcer ? "true" : "false")
                << " CombatMode=" << (combatModeNow ? "true" : "false")
                << " OldTargetValid=" << (oldTarget ? "true" : "false");

        if (g_config.logCandidates)
        {
            const uint32_t count = std::min(
                enemies.size(),
                static_cast<uint32_t>(g_config.maximumCandidatesPerEntry));

            message << " Candidates=[";
            for (uint32_t i = 0; i < count; ++i)
            {
                Character* candidate = enemies[i];
                if (i != 0)
                {
                    message << "; ";
                }

                message << "{Name=\"" << SafeName(candidate) << "\""
                        << ",Taunt=" << (IsTaunting(candidate) ? "true" : "false")
                        << ",Selected=" << (candidate == selectedTarget ? "true" : "false")
                        << "}";
            }

            if (enemies.size() > count)
            {
                message << "; ... truncated=" << (enemies.size() - count);
            }
            message << "]";
        }

        DebugLog(message.str());
    }

    void SetActiveTauntBonus(float value)
    {
        if (!g_tauntPatch.installed || g_tauntPatch.activeBonusAddress == NULL)
        {
            return;
        }

        *g_tauntPatch.activeBonusAddress = value;
        // The cave reads data, not instructions, so no instruction-cache flush is required.
    }

    bool HookedChooseBestAttackTarget(
        AI* self,
        RootObject** targetOut,
        lektor<Character*>& enemies,
        float& score,
        hand oldTarget,
        bool lawEnforcer,
        bool combatModeNow)
    {
        bool result = false;

        // RetargetMultiplier is implemented without modifying Kenshi's old-target logic.
        // While this specific chooseBestAttackTarget call has a valid old target, only the
        // existing taunt bonus constant used by our private code cave is temporarily increased.
        // A recursive critical section prevents cross-thread or nested-call races.
        const bool useRetargetBonus =
            g_tauntPatch.installed &&
            g_config.retargetMultiplier != 1.0f &&
            (oldTarget ? true : false);

        if (useRetargetBonus)
        {
            EnterCriticalSection(&g_patchExecutionLock);
            SetActiveTauntBonus(
                g_tauntPatch.vanillaBonus *
                g_config.tauntMultiplier *
                g_config.retargetMultiplier);
        }

        result = g_originalChooseBestAttackTarget(
            self,
            targetOut,
            enemies,
            score,
            oldTarget,
            lawEnforcer,
            combatModeNow);

        if (useRetargetBonus)
        {
            SetActiveTauntBonus(g_tauntPatch.modifiedBonus);
            LeaveCriticalSection(&g_patchExecutionLock);
        }

        if (!g_config.enabled || self == NULL)
        {
            return result;
        }

        if (g_config.logOnlyWhenTauntPresent && !AnyCandidateIsTaunting(enemies))
        {
            return result;
        }

        RootObject* selectedTarget = targetOut != NULL ? *targetOut : NULL;
        if (!ShouldLog(self, selectedTarget, score))
        {
            return result;
        }

        LogObservation(
            self,
            selectedTarget,
            enemies,
            score,
            oldTarget,
            lawEnforcer,
            combatModeNow,
            result);

        return result;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

__declspec(dllexport) void startPlugin()
{
    DebugLog(std::string("[") + kPluginName + "] Starting version " + kPluginVersion);

    if (!g_stateLockInitialised)
    {
        InitializeCriticalSection(&g_stateLock);
        g_stateLockInitialised = true;
    }
    if (!g_patchExecutionLockInitialised)
    {
        InitializeCriticalSection(&g_patchExecutionLock);
        g_patchExecutionLockInitialised = true;
    }

    LoadConfig();

    const intptr_t targetAddress = KenshiLib::GetRealAddress(&AI::chooseBestAttackTarget);
    if (targetAddress == 0)
    {
        ErrorLog(std::string("[") + kPluginName + "] Failed: chooseBestAttackTarget address was null.");
        return;
    }

    if (g_config.enabled)
    {
        if (!InstallTauntMultiplierPatch(targetAddress))
        {
            ErrorLog(std::string("[") + kPluginName + "] Plugin will remain observation-only because the taunt patch was not installed.");
        }
    }
    else
    {
        DebugLog(std::string("[") + kPluginName + "] Disabled by INI; multiplier patch not installed.");
    }

    const KenshiLib::HookStatus status = KenshiLib::AddHook(
        targetAddress,
        reinterpret_cast<void*>(&HookedChooseBestAttackTarget),
        &g_originalChooseBestAttackTarget);

    if (status != KenshiLib::SUCCESS || g_originalChooseBestAttackTarget == NULL)
    {
        ErrorLog(std::string("[") + kPluginName + "] Failed to install chooseBestAttackTarget hook.");
        return;
    }

    std::ostringstream message;
    message << "[" << kPluginName << "] Observation hook installed at 0x"
            << std::hex << std::uppercase << targetAddress
            << ". Taunt and optional retarget multipliers are applied only if the verified internal patch succeeded.";
    DebugLog(message.str());
}


__declspec(dllexport) void stopPlugin()
{
    RemoveTauntMultiplierPatch();
}
