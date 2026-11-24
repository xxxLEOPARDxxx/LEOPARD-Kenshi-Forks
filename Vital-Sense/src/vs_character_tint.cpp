#include "vs_character_tint.h"

#include "vs_appearance_extern.h"
#include "vs_character_lookup.h"
#include "vs_log.h"
#include "vs_marker_render.h"

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/PlayerInterface.h>
#include <Windows.h>
#include <ogre/OgreColourValue.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreGpuProgramParams.h>
#include <ogre/OgreMaterial.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgrePass.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreTechnique.h>

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

namespace vs_character_tint
{
namespace
{
const char* kColorOverrideParam = "coloroverride";
const char* kColourOverrideParam = "colouroverride";
const char* kColorOverrideParamCamel = "colorOverride";
const char* kColourOverrideParamCamel = "colourOverride";
const char* kDepthOverrideParam = "overrideDepth";
const char* kOverrideDepthLowerParam = "overridedepth";
const Ogre::ColourValue kClearTintColour(1.0f, 1.0f, 1.0f, 0.0f);
const DWORD kTintDiagLogIntervalMs = 2000;
const DWORD kTintPostLoadWarmupMs = 1500;
bool gTintDiagnosticsEnabled = false;

int gAppearanceEntityOffsetBytes = -1;
int gAppearanceBodyMaterialOffsetBytes = -1;
bool gTintEntityOffsetLogged = false;
bool gTintEntityOffsetFailureLogged = false;
bool gTintBodyMaterialOffsetLogged = false;
bool gTintBodyMaterialOffsetFailureLogged = false;
bool gTintNoShaderParamWarned = false;
DWORD gTintDiagLastLogMs = 0;
unsigned int gTintDiagSyncCalls = 0;
unsigned int gTintDiagApplyAttempts = 0;
unsigned int gTintDiagApplied = 0;
unsigned int gTintDiagNoEntity = 0;
unsigned int gTintDiagNoShader = 0;
unsigned int gTintDiagSuppressed = 0;
unsigned int gTintDiagCleared = 0;
unsigned int gTintDiagAppliedBodyMaterial = 0;
unsigned int gTintDiagAppliedEntityMaterial = 0;
unsigned int gTintDiagAppliedSkeletonFallback = 0;
bool gTintSkeletonFallbackWarned = false;
bool gTintApplyExceptionWarned = false;
DWORD gTintAnimalSampleLogWindowStartMs = 0;
unsigned int gTintAnimalSampleLogCount = 0;
DWORD gTintRelationSampleLogWindowStartMs = 0;
unsigned int gTintRelationSampleLogCount = 0;
DWORD gTintWarmupUntilMs = 0;
const DWORD kFailedTintClearRetryMs = 250;
const unsigned int kMaxTintClearAttempts = 3;
std::set<ProbeHandleKey> gUnavailableTintHandles;
unsigned int gUnavailableTintGeneration = 0;
size_t gTintLastObservedActiveCharacterCount = 0;
unsigned int gTintDiagHandleResolveCalls = 0;
unsigned int gTintDiagHandleResolveHits = 0;
unsigned int gTintDiagHandleResolveMisses = 0;
unsigned int gTintDiagConfigRefreshes = 0;
unsigned int gTintDiagDesiredEntries = 0;
unsigned int gTintDiagRetainedEntries = 0;
unsigned int gTintDiagRetainedReused = 0;
unsigned int gTintDiagDesiredSkippedRetained = 0;
unsigned int gTintDiagDesiredIndexHits = 0;
unsigned int gTintDiagDesiredIndexMisses = 0;

struct ResolvedTintCandidate
{
    CharacterTintEntry tintEntry;
    Character* character;
};

struct AppearanceMaterialOffsetCacheEntry
{
    void* appearanceVtable;
    std::vector<int> materialOffsets;
};

struct AnimalTintMaterialCloneEntry
{
    hand targetHandle;
    std::vector<Ogre::MaterialPtr> originalMaterials;
    std::vector<Ogre::MaterialPtr> cloneMaterials;
};

std::vector<AppearanceMaterialOffsetCacheEntry> gAppearanceMaterialOffsetCache;
std::vector<AnimalTintMaterialCloneEntry> gAnimalTintMaterialCloneEntries;
unsigned int gAnimalTintMaterialCloneSerial = 0;
bool gAnimalTintCloneFallbackWarned = false;

// Клон материала остаётся в MaterialManager под своим уникальным именем,
// пока его оттуда не убрать, - обнулить указатель мало. Раньше каждый
// подсвеченный зверь оставлял там клоны навсегда. Убираем не сразу, а в
// обычном кадре (FlushAnimalTintCloneReleases): сброс бывает и посреди
// загрузки сохранения, а тогда Ogre лучше не трогать.
std::vector<Ogre::MaterialPtr> gAnimalTintClonesToRelease;

void ReleaseAnimalTintCloneLater(Ogre::MaterialPtr& clone)
{
    if (!clone.isNull())
    {
        gAnimalTintClonesToRelease.push_back(clone);
        clone.setNull();
    }
}

void ReleaseAnimalTintClonesLater(std::vector<Ogre::MaterialPtr>& clones)
{
    for (size_t i = 0; i < clones.size(); ++i)
    {
        ReleaseAnimalTintCloneLater(clones[i]);
    }
    clones.clear();
}

void FlushAnimalTintCloneReleases()
{
    for (size_t i = 0; i < gAnimalTintClonesToRelease.size(); ++i)
    {
        try
        {
            Ogre::MaterialManager::getSingleton().remove(gAnimalTintClonesToRelease[i]->getHandle());
        }
        catch (...)
        {
        }
    }
    gAnimalTintClonesToRelease.clear();
}

void ResetTintRuntimeTracking(RuntimeStateView& state)
{
    state.characterTintEntries.clear();
    gUnavailableTintHandles.clear();
    gUnavailableTintGeneration = 0;
    for (size_t i = 0; i < gAnimalTintMaterialCloneEntries.size(); ++i)
    {
        ReleaseAnimalTintClonesLater(gAnimalTintMaterialCloneEntries[i].cloneMaterials);
    }
    gAnimalTintMaterialCloneEntries.clear();
}

size_t GetActiveCharacterCountSafe()
{
    if (!ou)
    {
        return 0;
    }

    size_t activeCharacterCount = 0;
    __try
    {
        activeCharacterCount = ou->getCharacterUpdateList().size();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        activeCharacterCount = 0;
    }

    return activeCharacterCount;
}

bool IsTintWarmupActive()
{
    return gTintWarmupUntilMs != 0 && GetTickCount() < gTintWarmupUntilMs;
}

void EmitTintDiagLogIfDue(const char* pluginName)
{
    if (!gTintDiagnosticsEnabled)
    {
        gTintDiagSyncCalls = 0;
        gTintDiagApplyAttempts = 0;
        gTintDiagApplied = 0;
        gTintDiagNoEntity = 0;
        gTintDiagNoShader = 0;
        gTintDiagSuppressed = 0;
        gTintDiagCleared = 0;
        gTintDiagAppliedBodyMaterial = 0;
        gTintDiagAppliedEntityMaterial = 0;
        gTintDiagAppliedSkeletonFallback = 0;
        gTintDiagHandleResolveCalls = 0;
        gTintDiagHandleResolveHits = 0;
        gTintDiagHandleResolveMisses = 0;
        gTintDiagConfigRefreshes = 0;
        gTintDiagDesiredEntries = 0;
        gTintDiagRetainedEntries = 0;
        gTintDiagRetainedReused = 0;
        gTintDiagDesiredSkippedRetained = 0;
        gTintDiagDesiredIndexHits = 0;
        gTintDiagDesiredIndexMisses = 0;
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (gTintDiagLastLogMs != 0 && (nowMs - gTintDiagLastLogMs) < kTintDiagLogIntervalMs)
    {
        return;
    }
    gTintDiagLastLogMs = nowMs;

    if (gTintDiagSyncCalls == 0 && gTintDiagApplyAttempts == 0 && gTintDiagCleared == 0)
    {
        return;
    }

    std::stringstream ss;
    ss << "tint diag sync_calls=" << gTintDiagSyncCalls
       << " apply_attempts=" << gTintDiagApplyAttempts
       << " applied=" << gTintDiagApplied
       << " applied_body_material=" << gTintDiagAppliedBodyMaterial
       << " applied_entity_material=" << gTintDiagAppliedEntityMaterial
       << " applied_skeleton_fallback=" << gTintDiagAppliedSkeletonFallback
       << " no_entity=" << gTintDiagNoEntity
       << " no_shader_param=" << gTintDiagNoShader
       << " suppressed=" << gTintDiagSuppressed
       << " cleared=" << gTintDiagCleared
       << " handle_resolves=" << gTintDiagHandleResolveCalls
       << " handle_hits=" << gTintDiagHandleResolveHits
       << " handle_misses=" << gTintDiagHandleResolveMisses
       << " config_refreshes=" << gTintDiagConfigRefreshes
       << " desired_entries=" << gTintDiagDesiredEntries
       << " retained_entries=" << gTintDiagRetainedEntries
       << " retained_reused=" << gTintDiagRetainedReused
       << " desired_skipped_retained=" << gTintDiagDesiredSkippedRetained
       << " desired_index_hits=" << gTintDiagDesiredIndexHits
       << " desired_index_misses=" << gTintDiagDesiredIndexMisses
       << " entity_offset=0x";
    ss << std::hex;
    if (gAppearanceEntityOffsetBytes >= 0)
    {
        ss << gAppearanceEntityOffsetBytes;
    }
    else
    {
        ss << "unknown";
    }
    ss << " body_material_offset=0x";
    if (gAppearanceBodyMaterialOffsetBytes >= 0)
    {
        ss << gAppearanceBodyMaterialOffsetBytes;
    }
    else
    {
        ss << "unknown";
    }
    ss << std::dec;

    vs_log::LogInfo(pluginName, ss.str());

    gTintDiagSyncCalls = 0;
    gTintDiagApplyAttempts = 0;
    gTintDiagApplied = 0;
    gTintDiagNoEntity = 0;
    gTintDiagNoShader = 0;
    gTintDiagSuppressed = 0;
    gTintDiagCleared = 0;
    gTintDiagAppliedBodyMaterial = 0;
    gTintDiagAppliedEntityMaterial = 0;
    gTintDiagAppliedSkeletonFallback = 0;
    gTintDiagHandleResolveCalls = 0;
    gTintDiagHandleResolveHits = 0;
    gTintDiagHandleResolveMisses = 0;
    gTintDiagConfigRefreshes = 0;
    gTintDiagDesiredEntries = 0;
    gTintDiagRetainedEntries = 0;
    gTintDiagRetainedReused = 0;
    gTintDiagDesiredSkippedRetained = 0;
    gTintDiagDesiredIndexHits = 0;
    gTintDiagDesiredIndexMisses = 0;
}

bool HandlesEqualByKey(const hand& a, const hand& b)
{
    return a.type == b.type
        && a.index == b.index
        && a.serial == b.serial;
}

ProbeHandleKey MakeProbeHandleKey(const hand& source)
{
    ProbeHandleKey key;
    key.type = static_cast<unsigned int>(source.type);
    key.index = static_cast<unsigned int>(source.index);
    key.serial = static_cast<unsigned int>(source.serial);
    return key;
}

int FindTintEntryByHandle(const std::vector<CharacterTintEntry>& entries, const hand& targetHandle)
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (HandlesEqualByKey(entries[i].targetHandle, targetHandle))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool TintEntriesContainExact(
    const std::vector<CharacterTintEntry>& entries,
    const hand& targetHandle,
    int markerRelation)
{
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].markerRelation == markerRelation
            && HandlesEqualByKey(entries[i].targetHandle, targetHandle))
        {
            return true;
        }
    }

    return false;
}

int FindResolvedTintCandidateIndex(
    const std::vector<ResolvedTintCandidate>& resolvedCandidates,
    const hand& targetHandle)
{
    for (size_t i = 0; i < resolvedCandidates.size(); ++i)
    {
        if (HandlesEqualByKey(resolvedCandidates[i].tintEntry.targetHandle, targetHandle))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

Character* ResolveTintCharacterByHandle(const hand& targetHandle)
{
    Character* candidate = vs_character_lookup::ResolveCharacterByHandleSafe(targetHandle);
    if (gTintDiagnosticsEnabled)
    {
        ++gTintDiagHandleResolveCalls;
        if (candidate)
        {
            ++gTintDiagHandleResolveHits;
        }
        else
        {
            ++gTintDiagHandleResolveMisses;
        }
    }
    return candidate;
}


bool HasFragmentColourOverrideConstant(Ogre::Pass* pass)
{
    if (!pass || !pass->hasFragmentProgram())
    {
        return false;
    }

    try
    {
        Ogre::GpuProgramParametersSharedPtr fragmentParams = pass->getFragmentProgramParameters();
        if (fragmentParams.isNull() || !fragmentParams->hasNamedParameters())
        {
            return false;
        }

        const char* colourParamNames[] = {
            kColorOverrideParam,
            kColourOverrideParam,
            kColorOverrideParamCamel,
            kColourOverrideParamCamel
        };
        for (size_t i = 0; i < (sizeof(colourParamNames) / sizeof(colourParamNames[0])); ++i)
        {
            if (fragmentParams->_findNamedConstantDefinition(colourParamNames[i], false))
            {
                return true;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return false;
}

bool HasVertexDepthOverrideConstant(Ogre::Pass* pass)
{
    if (!pass || !pass->hasVertexProgram())
    {
        return false;
    }

    try
    {
        Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
        if (vertexParams.isNull() || !vertexParams->hasNamedParameters())
        {
            return false;
        }

        const char* depthParamNames[] = {
            kDepthOverrideParam,
            kOverrideDepthLowerParam
        };
        for (size_t i = 0; i < (sizeof(depthParamNames) / sizeof(depthParamNames[0])); ++i)
        {
            if (vertexParams->_findNamedConstantDefinition(depthParamNames[i], false))
            {
                return true;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return false;
}

bool PassHasTintConstants(Ogre::Pass* pass)
{
    return HasFragmentColourOverrideConstant(pass) || HasVertexDepthOverrideConstant(pass);
}

bool GpuParamsHaveNamedConstant(Ogre::GpuProgramParametersSharedPtr& params, const char* paramName)
{
    if (params.isNull() || !paramName)
    {
        return false;
    }

    bool hasConstant = false;
    try
    {
        hasConstant = params->hasNamedParameters()
            && params->_findNamedConstantDefinition(paramName, false) != 0;
    }
    catch (...)
    {
        hasConstant = false;
    }
    return hasConstant;
}

bool SetColourConstantIfPresent(
    Ogre::GpuProgramParametersSharedPtr& params,
    const char* paramName,
    const Ogre::ColourValue& colour)
{
    if (!GpuParamsHaveNamedConstant(params, paramName))
    {
        return false;
    }

    try
    {
        params->setNamedConstant(paramName, colour);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool SetBoolConstantIfPresent(
    Ogre::GpuProgramParametersSharedPtr& params,
    const char* paramName,
    bool value)
{
    if (!GpuParamsHaveNamedConstant(params, paramName))
    {
        return false;
    }

    try
    {
        params->setNamedConstant(paramName, value ? 1 : 0);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ApplyTintConstantsToPass(Ogre::Pass* pass, const Ogre::ColourValue& colour, bool depthOverride)
{
    if (!pass)
    {
        return false;
    }

    bool appliedAnyConstant = false;
    if (pass->hasFragmentProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr fragmentParams = pass->getFragmentProgramParameters();
            if (!fragmentParams.isNull())
            {
                const char* colourParamNames[] = {
                    kColorOverrideParam,
                    kColourOverrideParam,
                    kColorOverrideParamCamel,
                    kColourOverrideParamCamel
                };

                for (size_t i = 0; i < (sizeof(colourParamNames) / sizeof(colourParamNames[0])); ++i)
                {
                    if (SetColourConstantIfPresent(fragmentParams, colourParamNames[i], colour))
                    {
                        appliedAnyConstant = true;
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    if (pass->hasVertexProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
            if (!vertexParams.isNull())
            {
                const char* depthParamNames[] = {
                    kDepthOverrideParam,
                    kOverrideDepthLowerParam
                };

                for (size_t i = 0; i < (sizeof(depthParamNames) / sizeof(depthParamNames[0])); ++i)
                {
                    if (SetBoolConstantIfPresent(vertexParams, depthParamNames[i], depthOverride))
                    {
                        appliedAnyConstant = true;
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    return appliedAnyConstant;
}

bool ApplyTintConstantsToPassColourRequired(
    Ogre::Pass* pass,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (!pass)
    {
        return false;
    }

    bool appliedColourConstant = false;
    if (pass->hasFragmentProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr fragmentParams = pass->getFragmentProgramParameters();
            if (!fragmentParams.isNull())
            {
                const char* colourParamNames[] = {
                    kColorOverrideParam,
                    kColourOverrideParam,
                    kColorOverrideParamCamel,
                    kColourOverrideParamCamel
                };

                for (size_t i = 0; i < (sizeof(colourParamNames) / sizeof(colourParamNames[0])); ++i)
                {
                    if (SetColourConstantIfPresent(fragmentParams, colourParamNames[i], colour))
                    {
                        appliedColourConstant = true;
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    // Depth override remains best-effort and does not define tint-apply success.
    if (pass->hasVertexProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
            if (!vertexParams.isNull())
            {
                const char* depthParamNames[] = {
                    kDepthOverrideParam,
                    kOverrideDepthLowerParam
                };

                for (size_t i = 0; i < (sizeof(depthParamNames) / sizeof(depthParamNames[0])); ++i)
                {
                    SetBoolConstantIfPresent(vertexParams, depthParamNames[i], depthOverride);
                }
            }
        }
        catch (...)
        {
        }
    }

    return appliedColourConstant;
}

bool MaterialHasTintConstants(Ogre::Material* material)
{
    if (!material)
    {
        return false;
    }

    unsigned short techniqueCount = 0;
    try
    {
        techniqueCount = material->getNumTechniques();
    }
    catch (...)
    {
        return false;
    }
    if (techniqueCount == 0)
    {
        return false;
    }

    for (unsigned short techniqueIndex = 0; techniqueIndex < techniqueCount; ++techniqueIndex)
    {
        Ogre::Technique* technique = 0;
        try
        {
            technique = material->getTechnique(techniqueIndex);
        }
        catch (...)
        {
            technique = 0;
        }
        if (!technique)
        {
            continue;
        }

        unsigned short passCount = 0;
        try
        {
            passCount = technique->getNumPasses();
        }
        catch (...)
        {
            passCount = 0;
        }

        for (unsigned short passIndex = 0; passIndex < passCount; ++passIndex)
        {
            Ogre::Pass* pass = 0;
            try
            {
                pass = technique->getPass(passIndex);
            }
            catch (...)
            {
                pass = 0;
            }
            if (PassHasTintConstants(pass))
            {
                return true;
            }
        }
    }

    return false;
}

bool ApplyTintToMaterial(Ogre::Material* material, const Ogre::ColourValue& colour, bool depthOverride)
{
    if (!material)
    {
        return false;
    }

    bool appliedAnyConstant = false;
    unsigned short techniqueCount = 0;
    try
    {
        techniqueCount = material->getNumTechniques();
    }
    catch (...)
    {
        return false;
    }
    if (techniqueCount == 0)
    {
        return false;
    }

    for (unsigned short techniqueIndex = 0; techniqueIndex < techniqueCount; ++techniqueIndex)
    {
        Ogre::Technique* technique = 0;
        try
        {
            technique = material->getTechnique(techniqueIndex);
        }
        catch (...)
        {
            technique = 0;
        }
        if (!technique)
        {
            continue;
        }

        unsigned short passCount = 0;
        try
        {
            passCount = technique->getNumPasses();
        }
        catch (...)
        {
            passCount = 0;
        }

        for (unsigned short passIndex = 0; passIndex < passCount; ++passIndex)
        {
            Ogre::Pass* pass = 0;
            try
            {
                pass = technique->getPass(passIndex);
            }
            catch (...)
            {
                pass = 0;
            }

            if (ApplyTintConstantsToPass(pass, colour, depthOverride))
            {
                appliedAnyConstant = true;
            }
        }
    }

    return appliedAnyConstant;
}

bool ApplyTintToMaterialPrimaryPassLikeExample(
    const Ogre::MaterialPtr& material,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (material.isNull())
    {
        return false;
    }

    bool applied = false;
    try
    {
        Ogre::Technique* technique = material->getTechnique(0);
        if (!technique)
        {
            return false;
        }

        Ogre::Pass* pass = technique->getPass(0);
        if (!pass)
        {
            return false;
        }

        if (pass->hasFragmentProgram())
        {
            Ogre::GpuProgramParametersSharedPtr fragmentParams = pass->getFragmentProgramParameters();
            if (!fragmentParams.isNull())
            {
                applied = SetColourConstantIfPresent(fragmentParams, kColorOverrideParam, colour) || applied;
                applied = SetColourConstantIfPresent(fragmentParams, kColourOverrideParam, colour) || applied;
                applied = SetColourConstantIfPresent(fragmentParams, kColorOverrideParamCamel, colour) || applied;
                applied = SetColourConstantIfPresent(fragmentParams, kColourOverrideParamCamel, colour) || applied;
            }
        }

        if (pass->hasVertexProgram())
        {
            Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
            if (!vertexParams.isNull())
            {
                applied = SetBoolConstantIfPresent(vertexParams, kDepthOverrideParam, depthOverride) || applied;
                applied = SetBoolConstantIfPresent(vertexParams, kOverrideDepthLowerParam, depthOverride) || applied;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return applied;
}

bool ApplyTintToMaterialPrimaryPassLikeExampleColourRequired(
    const Ogre::MaterialPtr& material,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (material.isNull())
    {
        return false;
    }

    try
    {
        Ogre::Technique* technique = material->getTechnique(0);
        if (!technique)
        {
            return false;
        }

        Ogre::Pass* pass = technique->getPass(0);
        if (!pass)
        {
            return false;
        }

        return ApplyTintConstantsToPassColourRequired(pass, colour, depthOverride);
    }
    catch (...)
    {
        return false;
    }
}

bool ApplyTintToMaterialColourRequired(
    Ogre::Material* material,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (!material)
    {
        return false;
    }

    bool appliedAnyColourConstant = false;
    unsigned short techniqueCount = 0;
    try
    {
        techniqueCount = material->getNumTechniques();
    }
    catch (...)
    {
        return false;
    }
    if (techniqueCount == 0)
    {
        return false;
    }

    for (unsigned short techniqueIndex = 0; techniqueIndex < techniqueCount; ++techniqueIndex)
    {
        Ogre::Technique* technique = 0;
        try
        {
            technique = material->getTechnique(techniqueIndex);
        }
        catch (...)
        {
            technique = 0;
        }
        if (!technique)
        {
            continue;
        }

        unsigned short passCount = 0;
        try
        {
            passCount = technique->getNumPasses();
        }
        catch (...)
        {
            passCount = 0;
        }

        for (unsigned short passIndex = 0; passIndex < passCount; ++passIndex)
        {
            Ogre::Pass* pass = 0;
            try
            {
                pass = technique->getPass(passIndex);
            }
            catch (...)
            {
                pass = 0;
            }

            if (ApplyTintConstantsToPassColourRequired(pass, colour, depthOverride))
            {
                appliedAnyColourConstant = true;
            }
        }
    }

    return appliedAnyColourConstant;
}

bool ApplyTintToMaterialFieldLikeExample(
    Ogre::MaterialPtr* materialField,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (!materialField)
    {
        return false;
    }

    bool applied = false;
    __try
    {
        if (!materialField->isNull())
        {
            // Match CharacterHighlight first: write pass(0) constants, then fall back to generic scan.
            applied = ApplyTintToMaterialPrimaryPassLikeExample(*materialField, colour, depthOverride);
            if (!applied)
            {
                applied = ApplyTintToMaterial(materialField->getPointer(), colour, depthOverride);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        applied = false;
    }

    return applied;
}

bool MaterialPtrsReferSameObject(const Ogre::MaterialPtr& a, const Ogre::MaterialPtr& b)
{
    if (a.isNull() || b.isNull())
    {
        return false;
    }

    bool sameObject = false;
    __try
    {
        sameObject = (a.getPointer() == b.getPointer());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sameObject = false;
    }
    return sameObject;
}

int FindAnimalTintCloneEntryByHandle(const hand& targetHandle)
{
    for (size_t i = 0; i < gAnimalTintMaterialCloneEntries.size(); ++i)
    {
        if (HandlesEqualByKey(gAnimalTintMaterialCloneEntries[i].targetHandle, targetHandle))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string BuildAnimalTintCloneName(const hand& targetHandle, size_t subEntityIndex)
{
    std::stringstream ss;
    ss << "VitalSenseAnimalTint_"
       << targetHandle.type << "_"
       << targetHandle.index << "_"
       << targetHandle.serial << "_"
       << subEntityIndex << "_"
       << gAnimalTintMaterialCloneSerial++;
    return ss.str();
}

void RetireAnimalTintClone(Ogre::MaterialPtr& clone)
{
    // An old entity or portrait can still own a reference after its handle disappears.
    // Neutralize that private material before giving up the registration.
    ApplyTintToMaterialColourRequired(clone.getPointer(), kClearTintColour, false);
    ReleaseAnimalTintCloneLater(clone);
}

bool HasAnimalTintClones(const AnimalTintMaterialCloneEntry& entry)
{
    for (size_t i = 0; i < entry.cloneMaterials.size(); ++i)
    {
        if (!entry.cloneMaterials[i].isNull())
        {
            return true;
        }
    }
    return false;
}

bool RestoreAnimalTintMaterialClonesForEntity(const hand& targetHandle, Ogre::Entity* characterEntity)
{
    const int entryIndex = FindAnimalTintCloneEntryByHandle(targetHandle);
    if (entryIndex < 0)
    {
        return false;
    }

    AnimalTintMaterialCloneEntry& entry = gAnimalTintMaterialCloneEntries[static_cast<size_t>(entryIndex)];
    size_t subEntityCount = 0;
    if (characterEntity)
    {
        try
        {
            subEntityCount = characterEntity->getNumSubEntities();
        }
        catch (...)
        {
            return false;
        }
    }

    bool restoredAll = true;
    for (size_t i = 0; i < entry.cloneMaterials.size(); ++i)
    {
        Ogre::MaterialPtr& clone = entry.cloneMaterials[i];
        if (clone.isNull())
        {
            continue;
        }

        if (i < subEntityCount)
        {
            try
            {
                Ogre::SubEntity* subEntity = characterEntity->getSubEntity(i);
                if (subEntity && MaterialPtrsReferSameObject(subEntity->getMaterial(), clone))
                {
                    subEntity->setMaterial(entry.originalMaterials[i]);
                }
            }
            catch (...)
            {
                ApplyTintToMaterialColourRequired(clone.getPointer(), kClearTintColour, false);
                restoredAll = false;
                continue;
            }
        }

        RetireAnimalTintClone(clone);
        entry.originalMaterials[i].setNull();
    }

    if (restoredAll)
    {
        gAnimalTintMaterialCloneEntries.erase(gAnimalTintMaterialCloneEntries.begin() + entryIndex);
    }
    return restoredAll;
}

bool ApplyTintToEntityUsingAnimalMaterialClones(
    const hand& targetHandle,
    Ogre::Entity* characterEntity,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (!characterEntity)
    {
        return false;
    }

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    int entryIndex = FindAnimalTintCloneEntryByHandle(targetHandle);
    if (entryIndex < 0)
    {
        AnimalTintMaterialCloneEntry created;
        created.targetHandle = targetHandle;
        created.originalMaterials.reserve(subEntityCount);
        created.cloneMaterials.reserve(subEntityCount);
        gAnimalTintMaterialCloneEntries.push_back(created);
        entryIndex = static_cast<int>(gAnimalTintMaterialCloneEntries.size() - 1);
    }

    AnimalTintMaterialCloneEntry& entry = gAnimalTintMaterialCloneEntries[static_cast<size_t>(entryIndex)];
    for (size_t i = subEntityCount; i < entry.cloneMaterials.size(); ++i)
    {
        if (!entry.cloneMaterials[i].isNull())
        {
            RetireAnimalTintClone(entry.cloneMaterials[i]);
        }
    }
    entry.originalMaterials.resize(subEntityCount);
    entry.cloneMaterials.resize(subEntityCount);

    bool appliedAny = false;
    for (size_t i = 0; i < subEntityCount; ++i)
    {
        Ogre::SubEntity* subEntity = 0;
        try
        {
            subEntity = characterEntity->getSubEntity(i);
        }
        catch (...)
        {
            subEntity = 0;
        }
        if (!subEntity)
        {
            continue;
        }

        Ogre::MaterialPtr currentMaterial;
        try
        {
            currentMaterial = subEntity->getMaterial();
        }
        catch (...)
        {
            continue;
        }
        if (currentMaterial.isNull())
        {
            continue;
        }

        Ogre::MaterialPtr& cloneMaterial = entry.cloneMaterials[i];
        const bool currentIsClone =
            (!cloneMaterial.isNull() && MaterialPtrsReferSameObject(currentMaterial, cloneMaterial));
        if (!currentIsClone)
        {
            entry.originalMaterials[i] = currentMaterial;
            if (!cloneMaterial.isNull())
            {
                RetireAnimalTintClone(cloneMaterial);
            }
        }
        if (entry.originalMaterials[i].isNull())
        {
            entry.originalMaterials[i] = currentMaterial;
        }

        if (cloneMaterial.isNull())
        {
            try
            {
                cloneMaterial = entry.originalMaterials[i]->clone(
                    BuildAnimalTintCloneName(targetHandle, i));
            }
            catch (...)
            {
                cloneMaterial.setNull();
            }
        }
        if (cloneMaterial.isNull())
        {
            continue;
        }

        bool appliedTint = ApplyTintToMaterialPrimaryPassLikeExampleColourRequired(
            cloneMaterial,
            colour,
            depthOverride);
        if (!appliedTint)
        {
            appliedTint = ApplyTintToMaterialColourRequired(cloneMaterial.getPointer(), colour, depthOverride);
        }
        if (!appliedTint)
        {
            if (!currentIsClone)
            {
                ReleaseAnimalTintCloneLater(cloneMaterial);
                entry.originalMaterials[i].setNull();
            }
            continue;
        }
        if (currentIsClone)
        {
            appliedAny = true;
            continue;
        }

        try
        {
            subEntity->setMaterial(cloneMaterial);
            appliedAny = true;
        }
        catch (...)
        {
            // Check the actual binding in case the engine threw after assigning it.
            try
            {
                if (!MaterialPtrsReferSameObject(subEntity->getMaterial(), cloneMaterial))
                {
                    ReleaseAnimalTintCloneLater(cloneMaterial);
                    entry.originalMaterials[i].setNull();
                }
            }
            catch (...)
            {
                // Keep an unconfirmed binding for the regular clear path.
            }
        }
    }

    if (!appliedAny && !HasAnimalTintClones(entry))
    {
        gAnimalTintMaterialCloneEntries.erase(gAnimalTintMaterialCloneEntries.begin() + entryIndex);
    }
    return appliedAny;
}


bool ApplyTintToEntity(Ogre::Entity* characterEntity, const Ogre::ColourValue& colour, bool depthOverride)
{
    if (!characterEntity)
    {
        return false;
    }

    bool appliedAnyConstant = false;
    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    for (size_t i = 0; i < subEntityCount; ++i)
    {
        Ogre::SubEntity* subEntity = 0;
        try
        {
            subEntity = characterEntity->getSubEntity(i);
        }
        catch (...)
        {
            subEntity = 0;
        }
        if (!subEntity)
        {
            continue;
        }

        Ogre::MaterialPtr material;
        try
        {
            material = subEntity->getMaterial();
        }
        catch (...)
        {
            continue;
        }
        if (material.isNull())
        {
            continue;
        }

        if (ApplyTintToMaterial(material.getPointer(), colour, depthOverride))
        {
            appliedAnyConstant = true;
        }
    }

    return appliedAnyConstant;
}

bool EntityHasAnyTintShaderConstants(Ogre::Entity* characterEntity)
{
    if (!characterEntity)
    {
        return false;
    }

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    for (size_t i = 0; i < subEntityCount; ++i)
    {
        Ogre::SubEntity* subEntity = 0;
        try
        {
            subEntity = characterEntity->getSubEntity(i);
        }
        catch (...)
        {
            subEntity = 0;
        }
        if (!subEntity)
        {
            continue;
        }

        Ogre::MaterialPtr material;
        try
        {
            material = subEntity->getMaterial();
        }
        catch (...)
        {
            material.setNull();
        }
        if (material.isNull())
        {
            continue;
        }

        if (MaterialHasTintConstants(material.getPointer()))
        {
            return true;
        }
    }

    return false;
}

bool TryReadAppearancePointerField(AppearanceBase* appearance, int offsetBytes, Ogre::Entity** entityOut)
{
    if (!appearance || !entityOut || offsetBytes < 0)
    {
        return false;
    }

    Ogre::Entity* candidate = 0;
    __try
    {
        candidate = *reinterpret_cast<Ogre::Entity**>(reinterpret_cast<unsigned char*>(appearance) + offsetBytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    *entityOut = candidate;
    return true;
}

bool IsLikelyCharacterEntity(Ogre::Entity* candidateEntity)
{
    if (!candidateEntity)
    {
        return false;
    }

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = candidateEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    return subEntityCount > 0 && subEntityCount <= 64;
}

bool TryReadAppearanceMaterialField(AppearanceBase* appearance, int offsetBytes, Ogre::MaterialPtr** materialFieldOut)
{
    if (!appearance || !materialFieldOut || offsetBytes < 0)
    {
        return false;
    }

    Ogre::MaterialPtr* candidate = 0;
    __try
    {
        candidate = reinterpret_cast<Ogre::MaterialPtr*>(reinterpret_cast<unsigned char*>(appearance) + offsetBytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    *materialFieldOut = candidate;
    return candidate != 0;
}

bool TryReadPointerValue(const void* address, void** valueOut)
{
    // The material scan treats adjacent appearance fields as candidate object
    // pointers. Reject values that cannot address an aligned Windows x64 object.
    const uintptr_t candidate = reinterpret_cast<uintptr_t>(address);
    if (!valueOut || candidate < 0x10000 || (candidate & (sizeof(void*) - 1)) != 0)
    {
        return false;
    }

    void* value = 0;
    __try
    {
        value = *reinterpret_cast<void* const*>(address);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    *valueOut = value;
    return true;
}

bool TryReadAppearanceVtable(AppearanceBase* appearance, void** vtableOut)
{
    return TryReadPointerValue(appearance, vtableOut);
}

int FindAppearanceMaterialOffsetCacheEntry(void* appearanceVtable)
{
    if (!appearanceVtable)
    {
        return -1;
    }

    for (size_t i = 0; i < gAppearanceMaterialOffsetCache.size(); ++i)
    {
        if (gAppearanceMaterialOffsetCache[i].appearanceVtable == appearanceVtable)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool TryReadAppearanceMaterialFieldSignature(
    AppearanceBase* appearance,
    int offsetBytes,
    void** materialPtrOut,
    void** materialVtableOut,
    void** infoPtrOut,
    void** infoVtableOut)
{
    if (!appearance || !materialPtrOut || !materialVtableOut || !infoPtrOut || !infoVtableOut)
    {
        return false;
    }

    const unsigned char* appearanceBytes = reinterpret_cast<const unsigned char*>(appearance);
    void* materialPtr = 0;
    void* infoPtr = 0;
    void* materialVtable = 0;
    void* infoVtable = 0;
    if (!TryReadPointerValue(appearanceBytes + offsetBytes, &materialPtr)
        || !TryReadPointerValue(appearanceBytes + offsetBytes + sizeof(void*), &infoPtr))
    {
        return false;
    }

    if (!materialPtr || !infoPtr)
    {
        return false;
    }

    if (!TryReadPointerValue(materialPtr, &materialVtable)
        || !TryReadPointerValue(infoPtr, &infoVtable))
    {
        return false;
    }

    if (!materialVtable || !infoVtable)
    {
        return false;
    }

    *materialPtrOut = materialPtr;
    *materialVtableOut = materialVtable;
    *infoPtrOut = infoPtr;
    *infoVtableOut = infoVtable;
    return true;
}

bool TryExtractReferenceMaterialSignatures(
    Ogre::Entity* characterEntity,
    void** materialVtableOut,
    void** infoVtableOut)
{
    if (!characterEntity || !materialVtableOut || !infoVtableOut)
    {
        return false;
    }

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    for (size_t i = 0; i < subEntityCount; ++i)
    {
        Ogre::SubEntity* subEntity = 0;
        try
        {
            subEntity = characterEntity->getSubEntity(i);
        }
        catch (...)
        {
            subEntity = 0;
        }
        if (!subEntity)
        {
            continue;
        }

        Ogre::MaterialPtr material;
        try
        {
            material = subEntity->getMaterial();
        }
        catch (...)
        {
            continue;
        }
        if (material.isNull() || !material.getPointer())
        {
            continue;
        }

        void* materialVtable = 0;
        if (!TryReadPointerValue(material.getPointer(), &materialVtable) || !materialVtable)
        {
            continue;
        }

        const void* const* rawMaterialPtr = reinterpret_cast<const void* const*>(&material);
        void* infoPtr = const_cast<void*>(rawMaterialPtr[1]);
        if (!infoPtr)
        {
            continue;
        }

        void* infoVtable = 0;
        if (!TryReadPointerValue(infoPtr, &infoVtable) || !infoVtable)
        {
            continue;
        }

        *materialVtableOut = materialVtable;
        *infoVtableOut = infoVtable;
        return true;
    }

    return false;
}

bool ResolveTintMaterialOffsetsFromAppearance(
    AppearanceBase* appearance,
    Ogre::Entity* characterEntity,
    const char* pluginName,
    std::vector<int>& resolvedOffsetsOut)
{
    resolvedOffsetsOut.clear();

    if (!appearance || !characterEntity)
    {
        return false;
    }

    void* referenceMaterialVtable = 0;
    void* referenceInfoVtable = 0;
    if (!TryExtractReferenceMaterialSignatures(characterEntity, &referenceMaterialVtable, &referenceInfoVtable))
    {
        return false;
    }

    const int firstOffset = 0x20;
    const int lastOffset = 0x800;
    std::vector<int> resolvedOffsets;
    resolvedOffsets.reserve(8);
    for (int offset = firstOffset; offset <= lastOffset; offset += 8)
    {
        void* candidateMaterialPtr = 0;
        void* candidateMaterialVtable = 0;
        void* candidateInfoPtr = 0;
        void* candidateInfoVtable = 0;
        if (!TryReadAppearanceMaterialFieldSignature(
                appearance,
                offset,
                &candidateMaterialPtr,
                &candidateMaterialVtable,
                &candidateInfoPtr,
                &candidateInfoVtable))
        {
            continue;
        }
        if (candidateMaterialVtable != referenceMaterialVtable
            || candidateInfoVtable != referenceInfoVtable)
        {
            continue;
        }

        Ogre::MaterialPtr* materialField = 0;
        if (!TryReadAppearanceMaterialField(appearance, offset, &materialField))
        {
            continue;
        }
        if (!ApplyTintToMaterialFieldLikeExample(materialField, kClearTintColour, false))
        {
            continue;
        }

        bool alreadyRecorded = false;
        for (size_t i = 0; i < resolvedOffsets.size(); ++i)
        {
            if (resolvedOffsets[i] == offset)
            {
                alreadyRecorded = true;
                break;
            }
        }
        if (alreadyRecorded)
        {
            continue;
        }

        resolvedOffsets.push_back(offset);
        if (resolvedOffsets.size() >= 8)
        {
            break;
        }
    }

    if (!resolvedOffsets.empty())
    {
        resolvedOffsetsOut.swap(resolvedOffsets);
        gAppearanceBodyMaterialOffsetBytes = resolvedOffsetsOut[0];
        if (gTintDiagnosticsEnabled && !gTintBodyMaterialOffsetLogged)
        {
            std::stringstream ss;
            ss << "tint appearance material offsets resolved count=" << resolvedOffsetsOut.size()
               << " first=0x" << std::hex << gAppearanceBodyMaterialOffsetBytes << std::dec;
            vs_log::LogInfo(pluginName, ss.str());
            gTintBodyMaterialOffsetLogged = true;
        }
        return true;
    }

    if (!gTintBodyMaterialOffsetFailureLogged)
    {
        vs_log::LogWarn(pluginName, "tint appearance material offset resolution failed; falling back to entity material scan");
        gTintBodyMaterialOffsetFailureLogged = true;
    }

    return false;
}

bool ApplyTintToAppearanceMaterialOffsets(
    AppearanceBase* appearance,
    const std::vector<int>& materialOffsets,
    const Ogre::ColourValue& colour,
    bool depthOverride)
{
    if (!appearance || materialOffsets.empty())
    {
        return false;
    }

    bool applied = false;
    for (size_t i = 0; i < materialOffsets.size(); ++i)
    {
        Ogre::MaterialPtr* materialField = 0;
        if (!TryReadAppearanceMaterialField(appearance, materialOffsets[i], &materialField))
        {
            continue;
        }
        if (ApplyTintToMaterialFieldLikeExample(materialField, colour, depthOverride))
        {
            applied = true;
        }
    }

    return applied;
}

bool ApplyTintToAppearanceMaterials(
    AppearanceBase* appearance,
    Ogre::Entity* characterEntity,
    const Ogre::ColourValue& colour,
    bool depthOverride,
    const char* pluginName)
{
    if (!appearance)
    {
        return false;
    }

    void* appearanceVtable = 0;
    const bool hasAppearanceVtable = TryReadAppearanceVtable(appearance, &appearanceVtable);
    if (hasAppearanceVtable)
    {
        const int cacheIndex = FindAppearanceMaterialOffsetCacheEntry(appearanceVtable);
        if (cacheIndex >= 0)
        {
            const std::vector<int>& cachedOffsets =
                gAppearanceMaterialOffsetCache[static_cast<size_t>(cacheIndex)].materialOffsets;
            if (!cachedOffsets.empty())
            {
                gAppearanceBodyMaterialOffsetBytes = cachedOffsets[0];
            }
            if (ApplyTintToAppearanceMaterialOffsets(appearance, cachedOffsets, colour, depthOverride))
            {
                return true;
            }

            gAppearanceMaterialOffsetCache.erase(
                gAppearanceMaterialOffsetCache.begin() + cacheIndex);
            gAppearanceBodyMaterialOffsetBytes = -1;
            gTintBodyMaterialOffsetLogged = false;
        }
    }

    std::vector<int> resolvedOffsets;
    if (!ResolveTintMaterialOffsetsFromAppearance(appearance, characterEntity, pluginName, resolvedOffsets))
    {
        return false;
    }

    if (hasAppearanceVtable)
    {
        const int cacheIndex = FindAppearanceMaterialOffsetCacheEntry(appearanceVtable);
        if (cacheIndex >= 0)
        {
            gAppearanceMaterialOffsetCache[static_cast<size_t>(cacheIndex)].materialOffsets = resolvedOffsets;
        }
        else
        {
            AppearanceMaterialOffsetCacheEntry cacheEntry = {
                appearanceVtable,
                resolvedOffsets
            };
            gAppearanceMaterialOffsetCache.push_back(cacheEntry);
        }
    }

    return ApplyTintToAppearanceMaterialOffsets(appearance, resolvedOffsets, colour, depthOverride);
}

Ogre::Entity* ResolveCharacterEntityFromAppearance(AppearanceBase* appearance, const char* pluginName)
{
    if (!appearance)
    {
        return 0;
    }

    if (gAppearanceEntityOffsetBytes >= 0)
    {
        Ogre::Entity* cachedEntity = 0;
        if (TryReadAppearancePointerField(appearance, gAppearanceEntityOffsetBytes, &cachedEntity)
            && IsLikelyCharacterEntity(cachedEntity))
        {
            return cachedEntity;
        }
        gAppearanceEntityOffsetBytes = -1;
        gTintEntityOffsetLogged = false;
    }

    const int candidateOffsets[] = {
        0xD8, 0xE0,
        0xD0, 0xE8, 0xF0, 0xF8,
        0x100, 0x108, 0x110, 0x118,
        0x120, 0x128, 0x130, 0x138, 0x140, 0x148
    };

    for (size_t i = 0; i < (sizeof(candidateOffsets) / sizeof(candidateOffsets[0])); ++i)
    {
        Ogre::Entity* probedEntity = 0;
        if (!TryReadAppearancePointerField(appearance, candidateOffsets[i], &probedEntity))
        {
            continue;
        }
        if (!IsLikelyCharacterEntity(probedEntity))
        {
            continue;
        }

        gAppearanceEntityOffsetBytes = candidateOffsets[i];
        if (gTintDiagnosticsEnabled && !gTintEntityOffsetLogged)
        {
            std::stringstream ss;
            ss << "tint entity offset resolved offset=0x" << std::hex << candidateOffsets[i] << std::dec;
            vs_log::LogInfo(pluginName, ss.str());
            gTintEntityOffsetLogged = true;
        }
        return probedEntity;
    }

    if (!gTintEntityOffsetFailureLogged)
    {
        vs_log::LogWarn(pluginName, "tint entity offset resolution failed; tint may be unavailable");
        gTintEntityOffsetFailureLogged = true;
    }
    return 0;
}

bool SetEntitySkeletonVisible(Ogre::Entity* entity, bool visible)
{
    if (!entity)
    {
        return false;
    }

    bool applied = false;
    try
    {
        entity->setDisplaySkeleton(visible);
        applied = true;
    }
    catch (...)
    {
        applied = false;
    }

    return applied;
}

AppearanceBase* GetCharacterAppearanceSafe(Character* candidate)
{
    if (!candidate)
    {
        return 0;
    }

    AppearanceBase* appearance = 0;
    __try
    {
        appearance = candidate->getAppearance();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        appearance = 0;
    }

    return appearance;
}

int ResolveTintApplyPriority(int markerRelation)
{
    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return 0;
    }
    if (markerRelation == CachedKoTarget::RELATION_ALLY)
    {
        return 1;
    }
    if (markerRelation == CachedKoTarget::RELATION_NEUTRAL)
    {
        return 2;
    }
    return 3;
}

bool ApplyTintToCharacter(Character* candidate, const Ogre::ColourValue& colour, bool depthOverride, const char* pluginName)
{
    if (!candidate)
    {
        return false;
    }

    AppearanceBase* appearance = GetCharacterAppearanceSafe(candidate);
    if (!appearance)
    {
        return false;
    }

    Ogre::Entity* characterEntity = ResolveCharacterEntityFromAppearance(appearance, pluginName);
    const bool isAnimalCharacter = vs_character_lookup::IsAnimalCharacterSafe(candidate);
    const bool isSquadCharacter = vs_character_lookup::IsCharacterInPlayerSquadSafe(candidate);
    hand targetHandle;
    const bool hasTargetHandle = vs_character_lookup::TryReadCharacterHandleSafe(candidate, &targetHandle);
    const bool wantsBodyHighlight = colour.a > 0.0f;
    if (!wantsBodyHighlight)
    {
        SetEntitySkeletonVisible(characterEntity, false);
        if (hasTargetHandle && FindAnimalTintCloneEntryByHandle(targetHandle) >= 0)
        {
            return RestoreAnimalTintMaterialClonesForEntity(targetHandle, characterEntity);
        }
        return ApplyTintToAppearanceMaterials(appearance, characterEntity, colour, depthOverride, pluginName)
            || ApplyTintToEntity(characterEntity, colour, depthOverride);
    }

    if (hasTargetHandle && (isAnimalCharacter || isSquadCharacter))
    {
        if (ApplyTintToEntityUsingAnimalMaterialClones(
                targetHandle,
                characterEntity,
                colour,
                depthOverride))
        {
            SetEntitySkeletonVisible(characterEntity, false);
            ++gTintDiagAppliedEntityMaterial;
            return true;
        }

        if (isAnimalCharacter || isSquadCharacter)
        {
            if (isAnimalCharacter && !gAnimalTintCloneFallbackWarned)
            {
                vs_log::LogWarn(
                    pluginName,
                    "animal tint clone path failed; skipping shared-material fallback to avoid cross-animal tint bleed");
                gAnimalTintCloneFallbackWarned = true;
            }

            return false;
        }
    }
    else if (isAnimalCharacter || isSquadCharacter)
    {
        // Never apply shared-material fallback to animals or squad members; it can bleed tint into unrelated
        // world creatures or shared UI portrait materials.
        return false;
    }

    if (ApplyTintToAppearanceMaterials(appearance, characterEntity, colour, depthOverride, pluginName))
    {
        SetEntitySkeletonVisible(characterEntity, false);
        ++gTintDiagAppliedBodyMaterial;
        return true;
    }

    if (ApplyTintToEntity(characterEntity, colour, depthOverride))
    {
        SetEntitySkeletonVisible(characterEntity, false);
        ++gTintDiagAppliedEntityMaterial;
        return true;
    }

    SetEntitySkeletonVisible(characterEntity, false);
    if (wantsBodyHighlight && !gTintSkeletonFallbackWarned)
    {
        vs_log::LogWarn(pluginName, "shader tint unavailable; skeleton highlight fallback disabled");
        gTintSkeletonFallbackWarned = true;
    }

    return false;
}

bool TryApplyTintToCharacterSeh(
    Character* candidate,
    const Ogre::ColourValue* colour,
    bool depthOverride,
    const char* pluginName,
    bool* hadExceptionOut)
{
    if (hadExceptionOut)
    {
        *hadExceptionOut = false;
    }
    if (!colour)
    {
        return false;
    }

    bool tinted = false;
    __try
    {
        tinted = ApplyTintToCharacter(candidate, *colour, depthOverride, pluginName);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        tinted = false;
        if (hadExceptionOut)
        {
            *hadExceptionOut = true;
        }
    }

    return tinted;
}

Ogre::ColourValue ResolveTintColour(RuntimeStateView& state, int markerRelation)
{
    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return Ogre::ColourValue(
            state.config.squadMarkerColour.red,
            state.config.squadMarkerColour.green,
            state.config.squadMarkerColour.blue,
            1.0f);
    }

    if (markerRelation == CachedKoTarget::RELATION_NEUTRAL)
    {
        return Ogre::ColourValue(
            state.config.neutralMarkerColour.red,
            state.config.neutralMarkerColour.green,
            state.config.neutralMarkerColour.blue,
            1.0f);
    }

    if (markerRelation == CachedKoTarget::RELATION_ALLY)
    {
        return Ogre::ColourValue(
            state.config.allyMarkerColour.red,
            state.config.allyMarkerColour.green,
            state.config.allyMarkerColour.blue,
            1.0f);
    }

    return Ogre::ColourValue(
        state.config.enemyMarkerColour.red,
        state.config.enemyMarkerColour.green,
        state.config.enemyMarkerColour.blue,
        1.0f);
}

// LEOPARD (16.10.2026): «Окрашивать цели с наградой» - цель с наградой
// (стоящая или лежащая) красится цветом тира награды, как значок $; раньше
// стоящие красились цветом отношения и не отличались от обычных врагов.
int TintBountyOf(RuntimeStateView& state, const CachedKoTarget& target)
{
    return (state.config.characterTintIncludeBountyOnly && target.totalBounty > 0) ? target.totalBounty : 0;
}

// Цвет окраски записи: тир награды (если есть) или отношение.
Ogre::ColourValue DesiredTintColour(RuntimeStateView& state, const CharacterTintEntry& entry)
{
    Ogre::ColourValue colour = ResolveTintColour(state, entry.markerRelation);
    if (entry.totalBounty > 0)
    {
        const MyGUI::Colour tier = vs_marker_render::BountyTierColour(state, entry.totalBounty);
        if (tier.alpha > 0.0f)
        {
            colour = Ogre::ColourValue(tier.red, tier.green, tier.blue, 1.0f);
        }
    }
    return colour;
}

bool ShouldTintMarkerState(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_BOUNTY_ONLY)
    {
        return state.config.characterTintIncludeBountyOnly;
    }

    return true;
}

void BuildDesiredTintEntries(RuntimeStateView& state, std::vector<CharacterTintEntry>& desiredEntries)
{
    desiredEntries.clear();
    desiredEntries.reserve(state.koTargetCache.size());

    for (size_t i = 0; i < state.koTargetCache.size(); ++i)
    {
        const CachedKoTarget& target = state.koTargetCache[i];
        if (target.targetHandle.isNull() || !ShouldTintMarkerState(state, target.markerState))
        {
            continue;
        }
        if (target.markerRelation == CachedKoTarget::RELATION_SQUAD
            && !state.config.characterTintIncludeSquad)
        {
            continue;
        }

        const int existingIndex = FindTintEntryByHandle(desiredEntries, target.targetHandle);
        if (existingIndex >= 0)
        {
            desiredEntries[static_cast<size_t>(existingIndex)].markerRelation = target.markerRelation;
            desiredEntries[static_cast<size_t>(existingIndex)].totalBounty = TintBountyOf(state, target);
            continue;
        }

        CharacterTintEntry entry = {
            target.targetHandle,
            target.markerRelation,
            TintBountyOf(state, target),
            0,
            0
        };
        desiredEntries.push_back(entry);
    }
}

bool SetTintForCharacter(
    Character* candidate,
    const Ogre::ColourValue& colour,
    bool depthOverride,
    const char* pluginName)
{
    if (!candidate)
    {
        ++gTintDiagNoEntity;
        return false;
    }

    ++gTintDiagApplyAttempts;

    bool hadApplyException = false;
    bool tinted = TryApplyTintToCharacterSeh(
        candidate,
        &colour,
        depthOverride,
        pluginName,
        &hadApplyException);
    if (hadApplyException)
    {
        ++gTintDiagSuppressed;
        if (gTintDiagnosticsEnabled && !gTintApplyExceptionWarned)
        {
            vs_log::LogWarn(pluginName, "tint apply hit exception; suppressing this candidate for stability");
            gTintApplyExceptionWarned = true;
        }

        // Exception paths are transient by nature; avoid no-shader warning noise.
        return false;
    }

    if (tinted)
    {
        ++gTintDiagApplied;
    }
    else
    {
        AppearanceBase* appearance = GetCharacterAppearanceSafe(candidate);
        Ogre::Entity* characterEntity = ResolveCharacterEntityFromAppearance(appearance, pluginName);
        const bool likelyTintCapable = EntityHasAnyTintShaderConstants(characterEntity);
        if (!likelyTintCapable)
        {
            ++gTintDiagSuppressed;
            return false;
        }

        ++gTintDiagNoShader;
        if (gTintDiagnosticsEnabled && !gTintNoShaderParamWarned)
        {
            vs_log::LogWarn(pluginName, "tint shader constants unavailable on resolved appearance/entity materials; no visual tint applied");
            gTintNoShaderParamWarned = true;
        }
    }
    return tinted;
}
} // namespace

void TickKoCharacterTintRuntime(RuntimeStateView& state)
{
    const size_t activeCharacterCount = GetActiveCharacterCountSafe();
    const DWORD nowMs = GetTickCount();

    if (activeCharacterCount == 0)
    {
        if (gTintLastObservedActiveCharacterCount != 0)
        {
            ResetTintRuntimeTracking(state);
            gTintWarmupUntilMs = nowMs + kTintPostLoadWarmupMs;
        }
        gTintLastObservedActiveCharacterCount = 0;
        return;
    }

    if (gTintLastObservedActiveCharacterCount == 0)
    {
        gTintWarmupUntilMs = nowMs + kTintPostLoadWarmupMs;
    }

    gTintLastObservedActiveCharacterCount = activeCharacterCount;

    // Мир загружен и не в паузе загрузки - клоны можно убрать из менеджера.
    if (!gAnimalTintClonesToRelease.empty() && nowMs >= gTintWarmupUntilMs)
    {
        FlushAnimalTintCloneReleases();
    }
}

void ResetKoCharacterTintRuntime(RuntimeStateView& state)
{
    ResetTintRuntimeTracking(state);
    gTintWarmupUntilMs = GetTickCount() + kTintPostLoadWarmupMs;
    gTintLastObservedActiveCharacterCount = 0;
}

static bool IsTintClearDue(const CharacterTintEntry& entry, DWORD nowMs)
{
    return entry.nextClearAttemptMs == 0
        || static_cast<LONG>(nowMs - entry.nextClearAttemptMs) >= 0;
}

static DWORD NextTintClearAttempt(DWORD nowMs)
{
    const DWORD nextMs = nowMs + kFailedTintClearRetryMs;
    return nextMs != 0 ? nextMs : 1;
}

static bool ClearTintEntry(CharacterTintEntry& entry, Character* candidate, DWORD nowMs, const char* pluginName)
{
    const bool cleared = candidate
        ? SetTintForCharacter(candidate, kClearTintColour, false, pluginName)
        : RestoreAnimalTintMaterialClonesForEntity(entry.targetHandle, 0);
    if (cleared)
    {
        ++gTintDiagCleared;
        return true;
    }
    if (++entry.failedClearAttempts >= kMaxTintClearAttempts)
    {
        if (candidate && RestoreAnimalTintMaterialClonesForEntity(entry.targetHandle, 0))
        {
            ++gTintDiagCleared;
        }
        return true;
    }

    entry.nextClearAttemptMs = NextTintClearAttempt(nowMs);
    return false;
}

void ClearKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    gTintDiagnosticsEnabled = state.config.debugLogDiagnostics;
    if (gUnavailableTintGeneration != 0 || !gUnavailableTintHandles.empty())
    {
        gUnavailableTintHandles.clear();
        gUnavailableTintGeneration = 0;
    }

    if (state.characterTintEntries.empty() && gAnimalTintMaterialCloneEntries.empty())
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    std::vector<CharacterTintEntry>& entries = state.characterTintEntries;
    size_t retained = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        CharacterTintEntry& entry = entries[i];
        if (IsTintClearDue(entry, nowMs)
            && ClearTintEntry(entry, ResolveTintCharacterByHandle(entry.targetHandle), nowMs, pluginName))
        {
            continue;
        }
        if (retained != i)
        {
            entries[retained] = entry;
        }
        ++retained;
    }
    entries.erase(entries.begin() + retained, entries.end());
    EmitTintDiagLogIfDue(pluginName);
}

void SyncKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    gTintDiagnosticsEnabled = state.config.debugLogDiagnostics;
    ++gTintDiagSyncCalls;
    const bool tintConfigChanged = (state.lastTintConfigRevision != state.configRevision);
    if (gTintDiagnosticsEnabled && tintConfigChanged)
    {
        ++gTintDiagConfigRefreshes;
    }

    if (!state.config.enableCharacterTint || !ou)
    {
        ClearKoCharacterTint(state, pluginName);
        state.lastTintConfigRevision = state.configRevision;
        return;
    }

    if (IsTintWarmupActive())
    {
        ClearKoCharacterTint(state, pluginName);
        return;
    }

    const DWORD clearNowMs = GetTickCount();
    const unsigned int generation = state.probeRuntimeCaches.generation;
    if (gUnavailableTintGeneration != generation)
    {
        gUnavailableTintHandles.clear();
        gUnavailableTintGeneration = generation;
    }

    std::vector<CharacterTintEntry> rawDesiredEntries;
    BuildDesiredTintEntries(state, rawDesiredEntries);

    std::vector<ResolvedTintCandidate> desiredEntries;
    desiredEntries.reserve(rawDesiredEntries.size());
    for (size_t i = 0; i < rawDesiredEntries.size(); ++i)
    {
        const CharacterTintEntry& tint = rawDesiredEntries[i];
        if (!gUnavailableTintHandles.empty()
            && gUnavailableTintHandles.count(MakeProbeHandleKey(tint.targetHandle)) != 0)
        {
            continue;
        }

        Character* candidate = ResolveTintCharacterByHandle(tint.targetHandle);
        if (!candidate)
        {
            gUnavailableTintHandles.insert(MakeProbeHandleKey(tint.targetHandle));
            continue;
        }
        ResolvedTintCandidate desired = {tint, candidate};
        desiredEntries.push_back(desired);
    }

    std::sort(
        desiredEntries.begin(),
        desiredEntries.end(),
        [](const ResolvedTintCandidate& a, const ResolvedTintCandidate& b) -> bool
        {
            return ResolveTintApplyPriority(a.tintEntry.markerRelation)
                < ResolveTintApplyPriority(b.tintEntry.markerRelation);
        });

    if (gTintDiagnosticsEnabled)
    {
        gTintDiagDesiredEntries += static_cast<unsigned int>(desiredEntries.size());
    }

    std::map<ProbeHandleKey, size_t> desiredIndexByHandle;
    for (size_t i = 0; i < desiredEntries.size(); ++i)
    {
        desiredIndexByHandle[MakeProbeHandleKey(desiredEntries[i].tintEntry.targetHandle)] = i;
    }

    std::vector<CharacterTintEntry> retainedEntries;
    retainedEntries.reserve(state.characterTintEntries.size());
    std::map<ProbeHandleKey, bool> retainedByHandle;
    for (size_t i = 0; i < state.characterTintEntries.size(); ++i)
    {
        const CharacterTintEntry& oldEntry = state.characterTintEntries[i];
        std::map<ProbeHandleKey, size_t>::const_iterator desiredIt =
            desiredIndexByHandle.find(MakeProbeHandleKey(oldEntry.targetHandle));
        const bool hasDesiredEntry = desiredIt != desiredIndexByHandle.end();
        const size_t desiredIndex = hasDesiredEntry ? desiredIt->second : 0;
        if (gTintDiagnosticsEnabled)
        {
            if (hasDesiredEntry)
            {
                ++gTintDiagDesiredIndexHits;
            }
            else
            {
                ++gTintDiagDesiredIndexMisses;
            }
        }
        const bool keepCurrentTint = oldEntry.nextClearAttemptMs == 0
            && !tintConfigChanged
            && hasDesiredEntry
            && (desiredEntries[desiredIndex].tintEntry.markerRelation == oldEntry.markerRelation)
            && (desiredEntries[desiredIndex].tintEntry.totalBounty == oldEntry.totalBounty);
        if (keepCurrentTint)
        {
            // LEOPARD (16.10.2026): чужие (не свой отряд, не животные) красятся
            // через общий материал тела, а Character Highlight (пример
            // KenshiLib) переписывает его каждый кадр - наша окраска держалась
            // один кадр. Повторяем её каждый кадр (кадр мира уже прошёл -
            // см. Vital-Sense.cpp: пишем после него). Свои и животные - на
            // своих копиях материала, их никто не трогает.
            Character* const keptCandidate = desiredEntries[desiredIndex].character;
            if (keptCandidate
                && oldEntry.markerRelation != CachedKoTarget::RELATION_SQUAD
                && !vs_character_lookup::IsAnimalCharacterSafe(keptCandidate)
                && !vs_character_lookup::IsCharacterInPlayerSquadSafe(keptCandidate))
            {
                SetTintForCharacter(
                    keptCandidate,
                    DesiredTintColour(state, desiredEntries[desiredIndex].tintEntry),
                    state.config.characterTintForceDepthOverride,
                    pluginName);
            }
            retainedEntries.push_back(oldEntry);
            retainedByHandle[MakeProbeHandleKey(oldEntry.targetHandle)] = true;
            if (gTintDiagnosticsEnabled)
            {
                ++gTintDiagRetainedEntries;
                ++gTintDiagRetainedReused;
            }
            continue;
        }

        CharacterTintEntry retained = oldEntry;
        if (!IsTintClearDue(retained, clearNowMs)
            || !ClearTintEntry(
                retained,
                hasDesiredEntry
                    ? desiredEntries[desiredIndex].character
                    : (gUnavailableTintHandles.count(MakeProbeHandleKey(oldEntry.targetHandle)) != 0
                        ? 0 : ResolveTintCharacterByHandle(oldEntry.targetHandle)),
                clearNowMs,
                pluginName))
        {
            retainedEntries.push_back(retained);
            retainedByHandle[MakeProbeHandleKey(retained.targetHandle)] = true;
            if (gTintDiagnosticsEnabled)
            {
                ++gTintDiagRetainedEntries;
            }
        }
    }

    std::vector<CharacterTintEntry> nextEntries = retainedEntries;
    nextEntries.reserve(desiredEntries.size());
    for (size_t i = 0; i < desiredEntries.size(); ++i)
    {
        const ResolvedTintCandidate& desired = desiredEntries[i];
        if (retainedByHandle.find(MakeProbeHandleKey(desired.tintEntry.targetHandle)) != retainedByHandle.end())
        {
            if (gTintDiagnosticsEnabled)
            {
                ++gTintDiagDesiredSkippedRetained;
            }
            continue;
        }

        Ogre::ColourValue tintColour = DesiredTintColour(state, desired.tintEntry);
        Character* desiredCandidate = desired.character;
        if (gTintDiagnosticsEnabled && desiredCandidate)
        {
            const DWORD nowMs = GetTickCount();
            if (gTintRelationSampleLogWindowStartMs == 0
                || (nowMs - gTintRelationSampleLogWindowStartMs) >= 2000)
            {
                gTintRelationSampleLogWindowStartMs = nowMs;
                gTintRelationSampleLogCount = 0;
            }
            if (gTintRelationSampleLogCount < 8)
            {
                std::stringstream ss;
                ss << "tint relation sample relation=" << desired.tintEntry.markerRelation
                   << " colour_rgba=("
                   << tintColour.r << ","
                   << tintColour.g << ","
                   << tintColour.b << ","
                   << tintColour.a << ")"
                   << " handle_type=" << desired.tintEntry.targetHandle.type
                   << " handle_index=" << desired.tintEntry.targetHandle.index
                   << " handle_serial=" << desired.tintEntry.targetHandle.serial;
                vs_log::LogInfo(pluginName, ss.str());
                ++gTintRelationSampleLogCount;
            }
        }
        if (gTintDiagnosticsEnabled && desiredCandidate && vs_character_lookup::IsAnimalCharacterSafe(desiredCandidate))
        {
            const DWORD nowMs = GetTickCount();
            if (gTintAnimalSampleLogWindowStartMs == 0
                || (nowMs - gTintAnimalSampleLogWindowStartMs) >= 2000)
            {
                gTintAnimalSampleLogWindowStartMs = nowMs;
                gTintAnimalSampleLogCount = 0;
            }
            if (gTintAnimalSampleLogCount < 6)
            {
                std::stringstream ss;
                ss << "animal tint sample relation=" << desired.tintEntry.markerRelation
                   << " colour_rgba=("
                   << tintColour.r << ","
                   << tintColour.g << ","
                   << tintColour.b << ","
                   << tintColour.a << ")"
                   << " handle_type=" << desired.tintEntry.targetHandle.type
                   << " handle_index=" << desired.tintEntry.targetHandle.index
                   << " handle_serial=" << desired.tintEntry.targetHandle.serial;
                vs_log::LogInfo(pluginName, ss.str());
                ++gTintAnimalSampleLogCount;
            }
        }
        if (SetTintForCharacter(
                desiredCandidate,
                tintColour,
                state.config.characterTintForceDepthOverride,
                pluginName))
        {
            nextEntries.push_back(desired.tintEntry);
        }
        else if (FindAnimalTintCloneEntryByHandle(desired.tintEntry.targetHandle) >= 0)
        {
            // An interrupted attach can leave a private material that still needs cleanup.
            CharacterTintEntry pending = desired.tintEntry;
            pending.nextClearAttemptMs = NextTintClearAttempt(clearNowMs);
            nextEntries.push_back(pending);
        }
    }

    state.characterTintEntries.swap(nextEntries);
    state.lastTintConfigRevision = state.configRevision;
    EmitTintDiagLogIfDue(pluginName);
}

// LEOPARD (22.10.2026): портреты. Портрет игра рисует сразу внутри
// RenderToTextureotron::renderCharacter, копируя материалы модели персонажа
// (Entity::setMaterial) - а своих и животных мы красим клонами материалов на
// частях модели: оглушённый под зажатой клавишей попадал в портрет
// окрашенным. На время отрисовки портрета снимаем свою окраску с этого
// персонажа и сразу возвращаем (Character Highlight делает так же со своей).
bool SuspendCharacterTintForPortrait(
    RuntimeStateView& state,
    Character* character,
    Ogre::ColourValue& colourOut,
    bool& depthOut,
    const char* pluginName)
{
    if (!character || state.characterTintEntries.empty())
    {
        return false;
    }

    const int index = FindTintEntryByHandle(state.characterTintEntries, character->handle);
    if (index < 0)
    {
        return false;
    }

    colourOut = DesiredTintColour(state, state.characterTintEntries[index]);
    depthOut = state.config.characterTintForceDepthOverride;

    const CharacterTintEntry& entry = state.characterTintEntries[index];
    const bool resumeAfterDraw = state.highlightRuntimeActive
        && state.config.enableCharacterTint
        && entry.nextClearAttemptMs == 0;
    const bool hadClones = resumeAfterDraw && FindAnimalTintCloneEntryByHandle(entry.targetHandle) >= 0;
    bool hadException = false;
    const bool cleared = TryApplyTintToCharacterSeh(character, &kClearTintColour, false, pluginName, &hadException);
    // Restore the visible tint after drawing the portrait, including partly restored models.
    return resumeAfterDraw && (cleared || hadClones);
}

void ResumeCharacterTintAfterPortrait(
    Character* character,
    const Ogre::ColourValue& colour,
    bool depth,
    const char* pluginName)
{
    bool hadException = false;
    TryApplyTintToCharacterSeh(character, &colour, depth, pluginName, &hadException);
}

} // namespace vs_character_tint
