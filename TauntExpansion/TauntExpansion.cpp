// Taunt Expansion by tata / Tata-Gari, MCM fork v1.
// Derived from RE_Kenshi_Plugins_Source commit b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8.
// GPL-3.0; original source/notices in upstream/, full license in LICENSE.
#include <Windows.h>

#include <Debug.h>
#include <PluginSettings.h>
#include <ModConfigMenu.h>
#define KLOC_DOMAIN "taunt_expansion"
#include <Localization.h>
#include "TauntPolicy.h"
#include "BonusTransaction.h"
#include "PatchSignature.h"
#include <core/Functions.h>

// Local SDK snapshot includes the current AI/Character enum guards.
#include <kenshi/AI/AI.h>

#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/RootObject.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
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
    using TauntExpansion::Config;
    using TauntExpansion::EffectiveBonus;

    Config g_config;

    typedef bool (*ChooseBestAttackTargetFn)(
        AI* self,
        RootObject** targetOut,
        lektor<Character*>& enemies,
        float& score,
        hand oldTarget,
        bool lawEnforcer,
        bool combatModeNow);

    ChooseBestAttackTargetFn g_originalChooseBestAttackTarget = NULL;

    TauntExpansion::BonusTransaction g_transaction;
    TauntExpansion::ObservationCache g_observations;
    bool g_started = false;
    bool g_stopped = false;
    unsigned g_activeDepth = 0;
    Config g_pendingConfig;
    bool g_configPending = false;

    bool ReadBool(const char* section, const char* key, bool defaultValue, const std::string& iniPath)
    {
        char buffer[32] = { 0 };
        const char* defaultText = defaultValue ? "true" : "false";
        GetPrivateProfileStringA(section, key, defaultText, buffer, sizeof(buffer), iniPath.c_str());

        std::string value(buffer);
        value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }), value.end());
        std::transform(value.begin(), value.end(), value.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });

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
        char text[64]={0};
        GetPrivateProfileStringA(section,key,"",text,sizeof(text),iniPath.c_str());
        return static_cast<unsigned>(TauntExpansion::ParseNumber(text,
            static_cast<float>(defaultValue),static_cast<float>(minimumValue),static_cast<float>(maximumValue)));
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

        return TauntExpansion::ParseNumber(valueBuffer, defaultValue, minimumValue, maximumValue);
    }

    void LoadConfig()
    {
        const std::string iniPath = PluginSettings::Path();
        Config next;
        next.enabled = ReadBool("General", "Enabled", true, iniPath);
        next.debug = ReadBool("Logging", "Debug", false, iniPath);
        next.logCandidates = ReadBool("Logging", "LogCandidates", false, iniPath);
        next.logOnlyWhenTauntPresent = ReadBool("Logging", "LogOnlyWhenTauntPresent", true, iniPath);
        next.logOnlyChangedSelection = ReadBool("Logging", "LogOnlyChangedSelection", true, iniPath);
        next.minimumLogIntervalMs = ReadUInt("Logging", "MinimumLogIntervalMs", 500, 0, 60000, iniPath);
        next.maximumCandidatesPerEntry = ReadUInt("Logging", "MaximumCandidatesPerEntry", 32, 1, 512, iniPath);
        next.tauntMultiplier = ReadFloat("Taunt", "Multiplier", 5.0f, 0, 100, iniPath);
        next.retargetMultiplier = ReadFloat("Taunt", "RetargetMultiplier", 2.0f, 0, 10, iniPath);
        // The same recursive lock protects settings and target evaluation.
        // If an unusual reentrant callback applies settings during evaluation,
        // defer until the outermost evaluation completes.
        EnterCriticalSection(&g_transaction.lock);
        if (g_activeDepth) { g_pendingConfig=next; g_configPending=true; }
        else { g_config=next; g_observations.Clear(); }
        LeaveCriticalSection(&g_transaction.lock);
        if (next.debug) DebugLog("[TauntExpansion] Settings loaded; detailed diagnostics enabled.");
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

    bool IsReadableRange(const void* memory, size_t bytes, bool executable)
    {
        uintptr_t at=reinterpret_cast<uintptr_t>(memory);
        if (!at || bytes > static_cast<uintptr_t>(-1)-at) return false;
        const uintptr_t end=at+bytes;
        while (at < end) {
            MEMORY_BASIC_INFORMATION m;
            if (!VirtualQuery(reinterpret_cast<void*>(at), &m, sizeof(m)) ||
                m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD|PAGE_NOACCESS))) return false;
            const DWORD p=m.Protect & 0xff;
            if (!(p==PAGE_READONLY || p==PAGE_READWRITE || p==PAGE_WRITECOPY ||
                  p==PAGE_EXECUTE_READ || p==PAGE_EXECUTE_READWRITE || p==PAGE_EXECUTE_WRITECOPY) ||
                (executable && !IsExecutableProtection(p))) return false;
            const uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
            if (next<=at) return false;
            at=next;
        }
        return true;
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

        const intptr_t displacement = reinterpret_cast<intptr_t>(destination) - reinterpret_cast<intptr_t>(source + 5);
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
        if (!VirtualProtect(source, overwrittenLength, oldProtection, &ignored))
            ErrorLog("[TauntExpansion] Failed to restore executable-page protection after patch installation.");
        FlushInstructionCache(GetCurrentProcess(), source, overwrittenLength);
        return true;
    }

    bool InstallTauntMultiplierPatch(intptr_t functionAddress)
    {
        unsigned char* function = reinterpret_cast<unsigned char*>(functionAddress);
        const size_t scanSize = 0x1200;
        if (!IsReadableRange(function,scanSize,true)) {
            ErrorLog("[TauntExpansion] Taunt patch aborted: target code range is not executable/readable.");
            return false;
        }
        const unsigned char* found=TauntExpansion::FindTauntInstruction(function,scanSize);
        if (!found)
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: verified signature missing or ambiguous.");
            return false;
        }
        unsigned char* addInstruction = const_cast<unsigned char*>(found);
        const int32_t vanillaDisplacement = *reinterpret_cast<int32_t*>(addInstruction + 4);
        float* vanillaBonusAddress = reinterpret_cast<float*>(addInstruction + 8 + vanillaDisplacement);

        if (!IsReadableRange(vanillaBonusAddress,sizeof(float),false))
        {
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: vanilla bonus address is invalid.");
            return false;
        }

        const float vanillaBonus = *vanillaBonusAddress;
        if (vanillaBonus != 2.0f)
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
        const intptr_t returnDisplacement = reinterpret_cast<intptr_t>(addInstruction + 8) - reinterpret_cast<intptr_t>(cave + 13);
        if (returnDisplacement < INT_MIN || returnDisplacement > INT_MAX)
        {
            VirtualFree(cave, 0, MEM_RELEASE);
            ErrorLog(std::string("[") + kPluginName + "] Taunt patch aborted: return jump is out of range.");
            return false;
        }
        *reinterpret_cast<int32_t*>(cave + 9) = static_cast<int32_t>(returnDisplacement);
        cave[13] = cave[14] = cave[15] = cave[16] = 0x90;

        const float modifiedBonus = vanillaBonus; // Vanilla until the fully installed hook enters its transaction.
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
                << ", DefaultMultiplier=" << g_config.tauntMultiplier
                << ", InitialBonus=" << modifiedBonus
                << ". Only the taunt-specific addss instruction is redirected; the shared vanilla constant is unchanged.";
        if (g_config.debug) DebugLog(message.str());
        return true;
    }

    bool RemoveTauntMultiplierPatch()
    {
        if (!g_tauntPatch.installed || !g_tauntPatch.patchAddress) return true;
        DWORD oldProtection=0;
        // Keep the cave allocated if restoration fails: game code may still jump to it.
        if (!VirtualProtect(g_tauntPatch.patchAddress, 8, PAGE_EXECUTE_READWRITE, &oldProtection)) {
            ErrorLog("[TauntExpansion] Failed to restore original taunt instruction; keeping its cave alive.");
            return false;
        }
        memcpy(g_tauntPatch.patchAddress, g_tauntPatch.originalBytes, 8);
        DWORD ignored=0;
        if (!VirtualProtect(g_tauntPatch.patchAddress, 8, oldProtection, &ignored))
            ErrorLog("[TauntExpansion] Failed to restore original executable-page protection.");
        FlushInstructionCache(GetCurrentProcess(), g_tauntPatch.patchAddress, 8);
        VirtualFree(g_tauntPatch.codeCave, 0, MEM_RELEASE);
        g_tauntPatch=TauntPatchState();
        g_transaction.address=NULL;
        return true;
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

    struct SelectionCall {
        AI* self;
        RootObject** targetOut;
        lektor<Character*>* enemies;
        float* score;
        const hand* oldTarget;
        bool lawEnforcer, combatModeNow;
    };

    bool RunSelection(void* context)
    {
        SelectionCall* c=static_cast<SelectionCall*>(context);
        bool result=g_originalChooseBestAttackTarget(c->self, c->targetOut,
            *c->enemies, *c->score, *c->oldTarget, c->lawEnforcer, c->combatModeNow);
        // No candidate scans, name/string construction or diagnostic-cache work
        // occur in normal play with Debug=0.
        if (!g_config.debug || g_stopped || !c->self) return result;
        if (g_config.logOnlyWhenTauntPresent && !AnyCandidateIsTaunting(*c->enemies)) return result;
        RootObject* selected=c->targetOut ? *c->targetOut : NULL;
        if (!g_observations.ShouldLog(reinterpret_cast<uintptr_t>(c->self),
            reinterpret_cast<uintptr_t>(selected), GetTickCount64(),
            g_config.minimumLogIntervalMs, g_config.logOnlyChangedSelection)) return result;
        LogObservation(c->self, selected, *c->enemies, *c->score, *c->oldTarget,
            c->lawEnforcer, c->combatModeNow, result);
        return result;
    }

    bool InvokeSelection(SelectionCall* c)
    {
        // Outermost lock also makes computing the bonus from config atomic with
        // respect to MCM apply. BonusTransaction handles recursion and unwinding.
        EnterCriticalSection(&g_transaction.lock);
        ++g_activeDepth;
        bool result=false;
        __try {
            const float bonus=EffectiveBonus(g_tauntPatch.vanillaBonus, g_config,
                *c->oldTarget ? true : false);
            result=g_transaction.InvokeLocked(bonus, RunSelection, c);
        }
        __finally {
            --g_activeDepth;
            if (!g_activeDepth && g_configPending) {
                g_config=g_pendingConfig; g_configPending=false; g_observations.Clear();
            }
            LeaveCriticalSection(&g_transaction.lock);
        }
        return result;
    }

    bool HookedChooseBestAttackTarget(AI* self, RootObject** targetOut,
        lektor<Character*>& enemies, float& score, hand oldTarget,
        bool lawEnforcer, bool combatModeNow)
    {
        SelectionCall c={self, targetOut, &enemies, &score, &oldTarget, lawEnforcer, combatModeNow};
        return InvokeSelection(&c);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

// RE_Kenshi::LoadPlugin looks up the exact MSVC C++ export ?startPlugin@@YAXXZ.
__declspec(dllexport) void startPlugin()
{
    if (g_started) return;
    g_started=true;
    g_transaction.Initialise(NULL);
    LoadConfig();
    const intptr_t targetAddress=KenshiLib::GetRealAddress(&AI::chooseBestAttackTarget);
    if (!targetAddress) {
        ErrorLog("[TauntExpansion] Failed: chooseBestAttackTarget address is null."); return;
    }
    // Install even when disabled so enabling the setting later needs no restart.
    if (!InstallTauntMultiplierPatch(targetAddress)) {
        ErrorLog("[TauntExpansion] Unsupported/conflicting target-selection code; leaving vanilla behavior unchanged."); return;
    }
    g_transaction.address=g_tauntPatch.activeBonusAddress;
    const KenshiLib::HookStatus status=KenshiLib::AddHook(targetAddress,
        reinterpret_cast<void*>(&HookedChooseBestAttackTarget), &g_originalChooseBestAttackTarget);
    if (status != KenshiLib::SUCCESS || !g_originalChooseBestAttackTarget) {
        ErrorLog("[TauntExpansion] Failed to hook target selection; restoring original taunt instruction.");
        RemoveTauntMultiplierPatch(); return;
    }
    if (g_config.debug) DebugLog("[TauntExpansion] MCM fork version 1 started.");
}

__declspec(dllexport) void stopPlugin()
{
    if (!g_started) return;
    EnterCriticalSection(&g_transaction.lock);
    g_stopped=true;
    g_config.enabled=false; g_config.debug=false; g_configPending=false;
    g_observations.Clear();
    if (g_transaction.address) *g_transaction.address=g_tauntPatch.vanillaBonus;
    // KenshiLib has no unhook API. Retain trampoline/cave/lock until process exit;
    // deleting them could leave executing game code pointing into freed memory.
    LeaveCriticalSection(&g_transaction.lock);
}

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string path=PluginSettings::Path();
    api->beginMod(api, "TauntExpansion", "Taunt Expansion", path.c_str(), LoadConfig);
    if (api->version>=2) api->info(api, Tr("Strengthen the vanilla Taunt bonus and help tanks draw enemies away from allies."));
    api->toggle(api, "General", "Enabled", Tr("Enabled"), Tr("Apply the Taunt multipliers. Disabling restores vanilla targeting strength immediately."), 1, 0);
    api->section(api, Tr("Taunt strength"));
    api->number(api, "Taunt", "Multiplier", Tr("Taunt multiplier"), Tr("Multiply the vanilla +2 Taunt score bonus. 1 is vanilla; default 5 gives +10."), 5, 0, 100, 2, 0);
    api->number(api, "Taunt", "RetargetMultiplier", Tr("Retarget multiplier"), Tr("Additional Taunt multiplier when the enemy already has a valid target. Default 2 gives +20 with the default Taunt multiplier."), 2, 0, 10, 2, 0);
    api->section(api, Tr("Diagnostics"));
    api->toggle(api, "Logging", "LogCandidates", Tr("Include candidates"), Tr("Include candidate names and Taunt state in detailed diagnostics."), 0, 0);
    api->toggle(api, "Logging", "LogOnlyWhenTauntPresent", Tr("Only when Taunt is present"), Tr("Write target diagnostics only when at least one candidate has Taunt enabled."), 1, 0);
    api->toggle(api, "Logging", "LogOnlyChangedSelection", Tr("Only changed selections"), Tr("Suppress repeated target selections. Diagnostic identities expire after 30 seconds."), 1, 0);
    api->integer(api, "Logging", "MinimumLogIntervalMs", Tr("Minimum log interval, ms"), Tr("Minimum time between diagnostic entries for the same actor. Applies only while detailed logging is enabled."), 500, 0, 60000, 0);
    api->integer(api, "Logging", "MaximumCandidatesPerEntry", Tr("Candidates per entry"), Tr("Maximum number of candidates included in one diagnostic entry."), 32, 1, 512, 0);
    api->toggle(api, "Logging", "Debug", Tr("Detailed log"), Tr("Log target choices and settings in RE_Kenshi_log.txt. Disabled by default. Errors are always logged."), 0, 0);
}
