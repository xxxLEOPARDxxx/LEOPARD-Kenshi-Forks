#include "vs_character_tint.h"

#include "vs_appearance_extern.h"
#include "vs_log.h"

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
#include <ogre/OgrePass.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreTechnique.h>

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

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
size_t gTintLastObservedActiveCharacterCount = 0;

struct ResolvedCharacter
{
    hand targetHandle;
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

void ResetTintRuntimeTracking(RuntimeStateView& state)
{
    state.characterTintEntries.clear();
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
}

bool HandlesEqualByKey(const hand& a, const hand& b)
{
    return a.type == b.type
        && a.index == b.index
        && a.serial == b.serial;
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

bool ResolvedCharactersContainHandle(
    const std::vector<ResolvedCharacter>& resolvedCharacters,
    const hand& targetHandle)
{
    for (size_t i = 0; i < resolvedCharacters.size(); ++i)
    {
        if (HandlesEqualByKey(resolvedCharacters[i].targetHandle, targetHandle))
        {
            return true;
        }
    }

    return false;
}

bool TryReadCharacterHandleSafe(Character* candidate, hand* handleOut)
{
    if (!candidate || !handleOut)
    {
        return false;
    }

    __try
    {
        *handleOut = candidate->getHandle();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return !handleOut->isNull();
}

void AddResolvedCharacter(
    std::vector<ResolvedCharacter>& resolvedCharacters,
    const hand& targetHandle,
    Character* candidate)
{
    if (!candidate || targetHandle.isNull())
    {
        return;
    }

    if (ResolvedCharactersContainHandle(resolvedCharacters, targetHandle))
    {
        return;
    }

    ResolvedCharacter resolved = { targetHandle, candidate };
    resolvedCharacters.push_back(resolved);
}

void CollectResolvedCharacters(std::vector<ResolvedCharacter>& resolvedCharacters)
{
    resolvedCharacters.clear();
    if (!ou)
    {
        return;
    }

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    resolvedCharacters.reserve(activeCharacters.size());
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        hand targetHandle;
        if (!TryReadCharacterHandleSafe(candidate, &targetHandle))
        {
            continue;
        }

        AddResolvedCharacter(resolvedCharacters, targetHandle, candidate);
    }

    const ogre_unordered_map<hand, Character*>::type& deathParadeCharacters = ou->deathParade;
    for (ogre_unordered_map<hand, Character*>::type::const_iterator it = deathParadeCharacters.begin();
         it != deathParadeCharacters.end();
         ++it)
    {
        Character* candidate = 0;
        __try
        {
            candidate = ou->getFromDeathParade(it->first);
            if (!candidate)
            {
                candidate = it->second;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            candidate = 0;
        }

        if (!candidate)
        {
            continue;
        }

        AddResolvedCharacter(resolvedCharacters, it->first, candidate);
    }
}

Character* FindResolvedCharacterByHandle(
    const std::vector<ResolvedCharacter>& resolvedCharacters,
    const hand& targetHandle)
{
    for (size_t i = 0; i < resolvedCharacters.size(); ++i)
    {
        if (HandlesEqualByKey(resolvedCharacters[i].targetHandle, targetHandle))
        {
            return resolvedCharacters[i].character;
        }
    }

    return 0;
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
                    bool setOk = false;
                    try
                    {
                        fragmentParams->setNamedConstant(colourParamNames[i], colour);
                        setOk = true;
                    }
                    catch (...)
                    {
                        setOk = false;
                    }
                    if (setOk)
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
                    bool setOk = false;
                    try
                    {
                        vertexParams->setNamedConstant(depthParamNames[i], depthOverride ? 1 : 0);
                        setOk = true;
                    }
                    catch (...)
                    {
                        setOk = false;
                    }
                    if (setOk)
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
                    bool setOk = false;
                    try
                    {
                        fragmentParams->setNamedConstant(colourParamNames[i], colour);
                        setOk = true;
                    }
                    catch (...)
                    {
                        setOk = false;
                    }
                    if (setOk)
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
                    try
                    {
                        vertexParams->setNamedConstant(depthParamNames[i], depthOverride ? 1 : 0);
                    }
                    catch (...)
                    {
                    }
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
                try
                {
                    fragmentParams->setNamedConstant(kColorOverrideParam, colour);
                    applied = true;
                }
                catch (...)
                {
                }
                try
                {
                    fragmentParams->setNamedConstant(kColourOverrideParam, colour);
                    applied = true;
                }
                catch (...)
                {
                }
                try
                {
                    fragmentParams->setNamedConstant(kColorOverrideParamCamel, colour);
                    applied = true;
                }
                catch (...)
                {
                }
                try
                {
                    fragmentParams->setNamedConstant(kColourOverrideParamCamel, colour);
                    applied = true;
                }
                catch (...)
                {
                }
            }
        }

        if (pass->hasVertexProgram())
        {
            Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
            if (!vertexParams.isNull())
            {
                try
                {
                    vertexParams->setNamedConstant(kDepthOverrideParam, depthOverride ? 1 : 0);
                    applied = true;
                }
                catch (...)
                {
                }
                try
                {
                    vertexParams->setNamedConstant(kOverrideDepthLowerParam, depthOverride ? 1 : 0);
                    applied = true;
                }
                catch (...)
                {
                }
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

bool RestoreAnimalTintMaterialClonesForEntity(const hand& targetHandle, Ogre::Entity* characterEntity)
{
    const int entryIndex = FindAnimalTintCloneEntryByHandle(targetHandle);
    if (entryIndex < 0)
    {
        return false;
    }

    if (!characterEntity)
    {
        return false;
    }

    AnimalTintMaterialCloneEntry& entry = gAnimalTintMaterialCloneEntries[static_cast<size_t>(entryIndex)];

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    if (subEntityCount < entry.originalMaterials.size())
    {
        return false;
    }

    bool restoredAny = false;
    bool restoredAll = true;
    for (size_t i = 0; i < entry.originalMaterials.size(); ++i)
    {
        if (entry.originalMaterials[i].isNull())
        {
            continue;
        }

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
            restoredAll = false;
            continue;
        }

        bool setOk = false;
        try
        {
            subEntity->setMaterial(entry.originalMaterials[i]);
            setOk = true;
        }
        catch (...)
        {
            setOk = false;
        }
        if (setOk)
        {
            restoredAny = true;
        }
        else
        {
            restoredAll = false;
        }
    }

    if (restoredAll)
    {
        gAnimalTintMaterialCloneEntries.erase(gAnimalTintMaterialCloneEntries.begin() + entryIndex);
    }

    return restoredAny;
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
    if (entry.originalMaterials.size() != subEntityCount || entry.cloneMaterials.size() != subEntityCount)
    {
        entry.originalMaterials.clear();
        entry.cloneMaterials.clear();
        entry.originalMaterials.resize(subEntityCount);
        entry.cloneMaterials.resize(subEntityCount);
    }

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
            cloneMaterial.setNull();
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
            continue;
        }

        bool setOk = false;
        try
        {
            subEntity->setMaterial(cloneMaterial);
            setOk = true;
        }
        catch (...)
        {
            setOk = false;
        }
        if (setOk)
        {
            appliedAny = true;
        }
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
    if (!address || !valueOut)
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

bool IsAnimalCharacterSafe(Character* candidate)
{
    if (!candidate)
    {
        return false;
    }

    bool isAnimalCharacter = false;
    __try
    {
        isAnimalCharacter = (candidate->isAnimal() != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        isAnimalCharacter = false;
    }

    return isAnimalCharacter;
}

bool IsCharacterInPlayerSquadSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool inPlayerSquad = false;
    __try
    {
        const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
        if (playerCharacters.valid())
        {
            for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
            {
                if (*it == candidate)
                {
                    inPlayerSquad = true;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        inPlayerSquad = false;
    }

    return inPlayerSquad;
}

bool IsEnemyToAnyPlayerCharacterSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool isEnemy = false;
    __try
    {
        const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
        if (!playerCharacters.valid())
        {
            return false;
        }

        for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
        {
            Character* playerCharacter = *it;
            if (!playerCharacter || playerCharacter == candidate)
            {
                continue;
            }

            bool hostileToPlayerCharacter = false;
            __try
            {
                hostileToPlayerCharacter = candidate->isEnemy(playerCharacter, true);
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = candidate->shouldIScrewThisGuyOver(playerCharacter);
                }
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = candidate->areYouGonnaGetMe(playerCharacter);
                }
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = playerCharacter->areYouGonnaGetMe(candidate);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                hostileToPlayerCharacter = false;
            }

            if (hostileToPlayerCharacter)
            {
                isEnemy = true;
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        isEnemy = false;
    }

    return isEnemy;
}

bool IsSameFactionAsPlayerSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool sameFaction = false;
    __try
    {
        Faction* playerFaction = ou->player->participant;
        sameFaction = (playerFaction && candidate->owner == playerFaction);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sameFaction = false;
    }

    return sameFaction;
}

int ResolveEffectiveTintRelationSafe(Character* candidate, int cachedRelation)
{
    if (!candidate)
    {
        if (cachedRelation == CachedKoTarget::RELATION_SQUAD
            || cachedRelation == CachedKoTarget::RELATION_ALLY
            || cachedRelation == CachedKoTarget::RELATION_ENEMY)
        {
            return cachedRelation;
        }
        return CachedKoTarget::RELATION_ENEMY;
    }

    if (IsCharacterInPlayerSquadSafe(candidate))
    {
        return CachedKoTarget::RELATION_SQUAD;
    }

    // Keep tint relation aligned with probe/marker classification first.
    if (cachedRelation == CachedKoTarget::RELATION_ENEMY
        || cachedRelation == CachedKoTarget::RELATION_ALLY)
    {
        return cachedRelation;
    }

    if (IsEnemyToAnyPlayerCharacterSafe(candidate))
    {
        return CachedKoTarget::RELATION_ENEMY;
    }

    if (IsSameFactionAsPlayerSafe(candidate))
    {
        return CachedKoTarget::RELATION_ALLY;
    }

    // Default hostile unless we can prove squad/ally through direct runtime checks.
    return CachedKoTarget::RELATION_ENEMY;
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
    return 2;
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
    const bool isAnimalCharacter = IsAnimalCharacterSafe(candidate);
    const bool isSquadCharacter = IsCharacterInPlayerSquadSafe(candidate);
    hand targetHandle;
    const bool hasTargetHandle = TryReadCharacterHandleSafe(candidate, &targetHandle);
    const bool wantsBodyHighlight = colour.a > 0.0f;
    if (!wantsBodyHighlight)
    {
        bool restoredClone = false;
        if (hasTargetHandle)
        {
            restoredClone = RestoreAnimalTintMaterialClonesForEntity(targetHandle, characterEntity);
        }
        SetEntitySkeletonVisible(characterEntity, false);
        const bool clearedByConstants =
            ApplyTintToAppearanceMaterials(appearance, characterEntity, colour, depthOverride, pluginName)
            || ApplyTintToEntity(characterEntity, colour, depthOverride);
        return restoredClone || clearedByConstants;
    }

    if (hasTargetHandle)
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

    if (SetEntitySkeletonVisible(characterEntity, wantsBodyHighlight))
    {
        if (wantsBodyHighlight)
        {
            ++gTintDiagAppliedSkeletonFallback;
            if (!gTintSkeletonFallbackWarned)
            {
                vs_log::LogWarn(pluginName, "shader tint unavailable; using body skeleton highlight fallback");
                gTintSkeletonFallbackWarned = true;
            }
        }
        return true;
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
            continue;
        }

        CharacterTintEntry entry = {
            target.targetHandle,
            target.markerRelation
        };
        desiredEntries.push_back(entry);
    }
}

bool SetTintForHandle(
    const hand& targetHandle,
    const std::vector<ResolvedCharacter>& resolvedCharacters,
    const Ogre::ColourValue& colour,
    bool depthOverride,
    const char* pluginName)
{
    Character* candidate = FindResolvedCharacterByHandle(resolvedCharacters, targetHandle);
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
}

void ClearKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    gTintDiagnosticsEnabled = state.config.debugLogDiagnostics;

    if (state.characterTintEntries.empty() && gAnimalTintMaterialCloneEntries.empty())
    {
        return;
    }

    std::vector<ResolvedCharacter> resolvedCharacters;
    CollectResolvedCharacters(resolvedCharacters);

    std::vector<CharacterTintEntry> unclearedEntries;
    unclearedEntries.reserve(state.characterTintEntries.size());

    for (size_t i = 0; i < state.characterTintEntries.size(); ++i)
    {
        if (SetTintForHandle(
                state.characterTintEntries[i].targetHandle,
                resolvedCharacters,
                kClearTintColour,
                false,
                pluginName))
        {
            ++gTintDiagCleared;
        }
        else
        {
            // Keep unresolved entries so clear can be retried when the entity becomes resolvable.
            unclearedEntries.push_back(state.characterTintEntries[i]);
        }
    }

    state.characterTintEntries.swap(unclearedEntries);
    EmitTintDiagLogIfDue(pluginName);
}

void SyncKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    gTintDiagnosticsEnabled = state.config.debugLogDiagnostics;
    ++gTintDiagSyncCalls;

    if (!state.config.enableCharacterTint || !ou)
    {
        ClearKoCharacterTint(state, pluginName);
        return;
    }

    if (IsTintWarmupActive())
    {
        ClearKoCharacterTint(state, pluginName);
        return;
    }

    std::vector<CharacterTintEntry> desiredEntries;
    BuildDesiredTintEntries(state, desiredEntries);

    std::vector<ResolvedCharacter> resolvedCharacters;
    if (!desiredEntries.empty()
        || !state.characterTintEntries.empty()
        || !gAnimalTintMaterialCloneEntries.empty())
    {
        CollectResolvedCharacters(resolvedCharacters);
    }

    for (size_t i = 0; i < desiredEntries.size(); ++i)
    {
        Character* desiredCandidate = FindResolvedCharacterByHandle(
            resolvedCharacters,
            desiredEntries[i].targetHandle);
        desiredEntries[i].markerRelation = ResolveEffectiveTintRelationSafe(
            desiredCandidate,
            desiredEntries[i].markerRelation);
    }

    std::sort(
        desiredEntries.begin(),
        desiredEntries.end(),
        [](const CharacterTintEntry& a, const CharacterTintEntry& b) -> bool
        {
            return ResolveTintApplyPriority(a.markerRelation)
                < ResolveTintApplyPriority(b.markerRelation);
        });

    std::vector<CharacterTintEntry> retainedEntries;
    retainedEntries.reserve(state.characterTintEntries.size());
    for (size_t i = 0; i < state.characterTintEntries.size(); ++i)
    {
        const CharacterTintEntry& oldEntry = state.characterTintEntries[i];
        const int desiredIndex = FindTintEntryByHandle(desiredEntries, oldEntry.targetHandle);
        const bool keepCurrentTint = (desiredIndex >= 0)
            && (desiredEntries[static_cast<size_t>(desiredIndex)].markerRelation == oldEntry.markerRelation);
        if (keepCurrentTint)
        {
            retainedEntries.push_back(oldEntry);
            continue;
        }

        if (SetTintForHandle(oldEntry.targetHandle, resolvedCharacters, kClearTintColour, false, pluginName))
        {
            ++gTintDiagCleared;
        }
        else
        {
            // Keep unresolved handles to retry clear instead of dropping stale tint state.
            CharacterTintEntry retained = oldEntry;
            if (desiredIndex >= 0)
            {
                retained.markerRelation = desiredEntries[static_cast<size_t>(desiredIndex)].markerRelation;
            }
            retainedEntries.push_back(retained);
        }
    }

    std::vector<CharacterTintEntry> nextEntries = retainedEntries;
    nextEntries.reserve(desiredEntries.size());
    for (size_t i = 0; i < desiredEntries.size(); ++i)
    {
        const CharacterTintEntry& desired = desiredEntries[i];
        if (FindTintEntryByHandle(retainedEntries, desired.targetHandle) >= 0)
        {
            continue;
        }

        Ogre::ColourValue tintColour = ResolveTintColour(state, desired.markerRelation);
        Character* desiredCandidate = FindResolvedCharacterByHandle(resolvedCharacters, desired.targetHandle);
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
                const bool inPlayerSquad = IsCharacterInPlayerSquadSafe(desiredCandidate);
                const bool enemyToPlayer = IsEnemyToAnyPlayerCharacterSafe(desiredCandidate);
                const bool sameFaction = IsSameFactionAsPlayerSafe(desiredCandidate);
                std::stringstream ss;
                ss << "tint relation sample relation=" << desired.markerRelation
                   << " in_squad=" << (inPlayerSquad ? "true" : "false")
                   << " enemy=" << (enemyToPlayer ? "true" : "false")
                   << " same_faction=" << (sameFaction ? "true" : "false")
                   << " colour_rgba=("
                   << tintColour.r << ","
                   << tintColour.g << ","
                   << tintColour.b << ","
                   << tintColour.a << ")"
                   << " handle_type=" << desired.targetHandle.type
                   << " handle_index=" << desired.targetHandle.index
                   << " handle_serial=" << desired.targetHandle.serial;
                vs_log::LogInfo(pluginName, ss.str());
                ++gTintRelationSampleLogCount;
            }
        }
        if (gTintDiagnosticsEnabled && desiredCandidate && IsAnimalCharacterSafe(desiredCandidate))
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
                ss << "animal tint sample relation=" << desired.markerRelation
                   << " colour_rgba=("
                   << tintColour.r << ","
                   << tintColour.g << ","
                   << tintColour.b << ","
                   << tintColour.a << ")"
                   << " handle_type=" << desired.targetHandle.type
                   << " handle_index=" << desired.targetHandle.index
                   << " handle_serial=" << desired.targetHandle.serial;
                vs_log::LogInfo(pluginName, ss.str());
                ++gTintAnimalSampleLogCount;
            }
        }
        if (SetTintForHandle(
                desired.targetHandle,
                resolvedCharacters,
                tintColour,
                state.config.characterTintForceDepthOverride,
                pluginName))
        {
            nextEntries.push_back(desired);
        }
    }

    state.characterTintEntries.swap(nextEntries);
    EmitTintDiagLogIfDue(pluginName);
}

} // namespace vs_character_tint
