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
const Ogre::ColourValue kSquadTintColour(0.12f, 0.48f, 1.0f, 1.0f);
const DWORD kTintDiagLogIntervalMs = 2000;
const DWORD kTintNoShaderRetryIntervalMs = 2500;

int gAppearanceEntityOffsetBytes = -1;
int gAppearanceBodyMaterialOffsetBytes = -1;
std::vector<int> gAppearanceTintMaterialOffsets;
bool gTintEntityOffsetLogged = false;
bool gTintEntityOffsetFailureLogged = false;
bool gTintBodyMaterialOffsetLogged = false;
bool gTintBodyMaterialOffsetFailureLogged = false;
bool gTintNoShaderParamWarned = false;
DWORD gTintNoShaderRetryAfterMs = 0;
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
unsigned int gTintDiagAppliedMaterialOverride = 0;
bool gTintSkeletonFallbackWarned = false;
bool gTintMaterialOverrideWarned = false;
unsigned int gTintMaterialCloneSerial = 0;

struct ResolvedCharacter
{
    hand targetHandle;
    Character* character;
};

struct CharacterMaterialOverrideEntry
{
    hand targetHandle;
    std::vector<Ogre::MaterialPtr> originalMaterials;
    std::vector<Ogre::MaterialPtr> overrideMaterials;
};

std::vector<CharacterMaterialOverrideEntry> gCharacterMaterialOverrideEntries;

void EmitTintDiagLogIfDue(const char* pluginName)
{
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
       << " applied_material_override=" << gTintDiagAppliedMaterialOverride
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
    gTintDiagAppliedMaterialOverride = 0;
    gTintDiagAppliedSkeletonFallback = 0;
}

bool HasReachedTick(DWORD nowTick, DWORD targetTick)
{
    return static_cast<LONG>(nowTick - targetTick) >= 0;
}

bool IsTintAttemptSuppressed()
{
    if (gTintNoShaderRetryAfterMs == 0)
    {
        return false;
    }

    const DWORD nowTick = GetTickCount();
    return !HasReachedTick(nowTick, gTintNoShaderRetryAfterMs);
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

int FindMaterialOverrideEntryByHandle(const hand& targetHandle)
{
    for (size_t i = 0; i < gCharacterMaterialOverrideEntries.size(); ++i)
    {
        if (HandlesEqualByKey(gCharacterMaterialOverrideEntries[i].targetHandle, targetHandle))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

std::string BuildMaterialCloneName(const hand& targetHandle, size_t subEntityIndex)
{
    std::stringstream ss;
    ss << "VitalSenseTint_"
       << targetHandle.type << "_"
       << targetHandle.index << "_"
       << targetHandle.serial << "_"
       << subEntityIndex << "_"
       << gTintMaterialCloneSerial++;
    return ss.str();
}

void ConfigureHighlightMaterialClone(Ogre::MaterialPtr& materialClone, const Ogre::ColourValue& colour)
{
    if (materialClone.isNull())
    {
        return;
    }

    try
    {
        materialClone->setAmbient(
            colour.r * 0.35f + 0.10f,
            colour.g * 0.35f + 0.10f,
            colour.b * 0.35f + 0.10f);
        materialClone->setDiffuse(
            colour.r,
            colour.g,
            colour.b,
            0.85f);
        materialClone->setSelfIllumination(colour);
        materialClone->setSceneBlending(Ogre::SBT_ADD);
        materialClone->setDepthCheckEnabled(false);
        materialClone->setDepthWriteEnabled(false);
        materialClone->setCullingMode(Ogre::CULL_NONE);
        materialClone->compile();
        materialClone->load();
    }
    catch (...)
    {
    }
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

bool ApplyTintToMaterialField(
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
            applied = ApplyTintToMaterial(materialField->getPointer(), colour, depthOverride);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        applied = false;
    }

    return applied;
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

bool ApplyMaterialOverrideToEntity(
    const hand& targetHandle,
    Ogre::Entity* characterEntity,
    const Ogre::ColourValue& colour)
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

    int entryIndex = FindMaterialOverrideEntryByHandle(targetHandle);
    if (entryIndex < 0)
    {
        CharacterMaterialOverrideEntry created;
        created.targetHandle = targetHandle;
        created.originalMaterials.reserve(subEntityCount);
        created.overrideMaterials.reserve(subEntityCount);
        gCharacterMaterialOverrideEntries.push_back(created);
        entryIndex = static_cast<int>(gCharacterMaterialOverrideEntries.size() - 1);
    }

    CharacterMaterialOverrideEntry& entry = gCharacterMaterialOverrideEntries[static_cast<size_t>(entryIndex)];
    if (entry.originalMaterials.size() != subEntityCount || entry.overrideMaterials.size() != subEntityCount)
    {
        entry.originalMaterials.clear();
        entry.overrideMaterials.clear();
        entry.originalMaterials.resize(subEntityCount);
        entry.overrideMaterials.resize(subEntityCount);
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

        if (entry.originalMaterials[i].isNull())
        {
            entry.originalMaterials[i] = currentMaterial;
        }

        Ogre::MaterialPtr& overrideMaterial = entry.overrideMaterials[i];
        if (overrideMaterial.isNull())
        {
            try
            {
                overrideMaterial = entry.originalMaterials[i]->clone(BuildMaterialCloneName(targetHandle, i));
            }
            catch (...)
            {
                overrideMaterial.setNull();
            }
        }
        if (overrideMaterial.isNull())
        {
            continue;
        }

        ConfigureHighlightMaterialClone(overrideMaterial, colour);

        bool setOk = false;
        try
        {
            subEntity->setMaterial(overrideMaterial);
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

bool RestoreMaterialOverrideForEntity(const hand& targetHandle, Ogre::Entity* characterEntity)
{
    const int entryIndex = FindMaterialOverrideEntryByHandle(targetHandle);
    if (entryIndex < 0)
    {
        return false;
    }

    CharacterMaterialOverrideEntry entry = gCharacterMaterialOverrideEntries[static_cast<size_t>(entryIndex)];
    gCharacterMaterialOverrideEntries.erase(gCharacterMaterialOverrideEntries.begin() + entryIndex);

    if (!characterEntity)
    {
        return false;
    }

    bool restoredAny = false;
    size_t subEntityCount = 0;
    try
    {
        subEntityCount = characterEntity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    const size_t restoreCount = (subEntityCount < entry.originalMaterials.size()) ? subEntityCount : entry.originalMaterials.size();
    for (size_t i = 0; i < restoreCount; ++i)
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
    }

    return restoredAny;
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
    const char* pluginName)
{
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
        if (!ApplyTintToMaterialField(materialField, kClearTintColour, false))
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
        gAppearanceTintMaterialOffsets.swap(resolvedOffsets);
        gAppearanceBodyMaterialOffsetBytes = gAppearanceTintMaterialOffsets[0];
        if (!gTintBodyMaterialOffsetLogged)
        {
            std::stringstream ss;
            ss << "tint appearance material offsets resolved count=" << gAppearanceTintMaterialOffsets.size()
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

    bool applied = false;
    if (!gAppearanceTintMaterialOffsets.empty())
    {
        for (size_t i = 0; i < gAppearanceTintMaterialOffsets.size(); ++i)
        {
            Ogre::MaterialPtr* materialField = 0;
            if (!TryReadAppearanceMaterialField(appearance, gAppearanceTintMaterialOffsets[i], &materialField))
            {
                continue;
            }
            if (ApplyTintToMaterialField(materialField, colour, depthOverride))
            {
                applied = true;
            }
        }
        if (applied)
        {
            return true;
        }

        gAppearanceTintMaterialOffsets.clear();
        gAppearanceBodyMaterialOffsetBytes = -1;
        gTintBodyMaterialOffsetLogged = false;
    }

    if (!ResolveTintMaterialOffsetsFromAppearance(appearance, characterEntity, pluginName))
    {
        return false;
    }

    for (size_t i = 0; i < gAppearanceTintMaterialOffsets.size(); ++i)
    {
        Ogre::MaterialPtr* materialField = 0;
        if (!TryReadAppearanceMaterialField(appearance, gAppearanceTintMaterialOffsets[i], &materialField))
        {
            continue;
        }
        if (ApplyTintToMaterialField(materialField, colour, depthOverride))
        {
            applied = true;
        }
    }

    return applied;
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
        if (!gTintEntityOffsetLogged)
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
    const bool wantsBodyHighlight = colour.a > 0.0f;
    if (!wantsBodyHighlight)
    {
        const bool restoredMaterial = RestoreMaterialOverrideForEntity(candidate->getHandle(), characterEntity);
        SetEntitySkeletonVisible(characterEntity, false);
        return restoredMaterial
            || ApplyTintToAppearanceMaterials(appearance, characterEntity, colour, depthOverride, pluginName)
            || ApplyTintToEntity(characterEntity, colour, depthOverride);
    }

    if (ApplyTintToAppearanceMaterials(appearance, characterEntity, colour, depthOverride, pluginName))
    {
        RestoreMaterialOverrideForEntity(candidate->getHandle(), characterEntity);
        SetEntitySkeletonVisible(characterEntity, false);
        ++gTintDiagAppliedBodyMaterial;
        return true;
    }

    if (ApplyTintToEntity(characterEntity, colour, depthOverride))
    {
        RestoreMaterialOverrideForEntity(candidate->getHandle(), characterEntity);
        SetEntitySkeletonVisible(characterEntity, false);
        ++gTintDiagAppliedEntityMaterial;
        return true;
    }

    if (ApplyMaterialOverrideToEntity(candidate->getHandle(), characterEntity, colour))
    {
        SetEntitySkeletonVisible(characterEntity, false);
        ++gTintDiagAppliedMaterialOverride;
        if (!gTintMaterialOverrideWarned)
        {
            vs_log::LogWarn(pluginName, "shader tint unavailable; using body material override fallback");
            gTintMaterialOverrideWarned = true;
        }
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

Ogre::ColourValue ResolveTintColour(RuntimeStateView& state, int markerRelation)
{
    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return kSquadTintColour;
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
    bool allowNoShaderSuppression,
    const char* pluginName)
{
    Character* candidate = FindResolvedCharacterByHandle(resolvedCharacters, targetHandle);
    if (!candidate)
    {
        ++gTintDiagNoEntity;
        return false;
    }

    ++gTintDiagApplyAttempts;
    const bool tinted = ApplyTintToCharacter(candidate, colour, depthOverride, pluginName);
    if (tinted)
    {
        ++gTintDiagApplied;
        gTintNoShaderRetryAfterMs = 0;
    }
    else
    {
        ++gTintDiagNoShader;
        if (allowNoShaderSuppression)
        {
            gTintNoShaderRetryAfterMs = GetTickCount() + kTintNoShaderRetryIntervalMs;
        }
        if (!gTintNoShaderParamWarned)
        {
            vs_log::LogWarn(pluginName, "tint shader constants unavailable on resolved appearance/entity materials; no visual tint applied");
            gTintNoShaderParamWarned = true;
        }
    }
    return tinted;
}
} // namespace

void ClearKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    if (state.characterTintEntries.empty())
    {
        return;
    }

    std::vector<ResolvedCharacter> resolvedCharacters;
    CollectResolvedCharacters(resolvedCharacters);

    for (size_t i = 0; i < state.characterTintEntries.size(); ++i)
    {
        if (SetTintForHandle(
                state.characterTintEntries[i].targetHandle,
                resolvedCharacters,
                kClearTintColour,
                false,
                false,
                pluginName))
        {
            ++gTintDiagCleared;
        }
    }
    state.characterTintEntries.clear();
    EmitTintDiagLogIfDue(pluginName);
}

void SyncKoCharacterTint(RuntimeStateView& state, const char* pluginName)
{
    ++gTintDiagSyncCalls;

    if (!state.config.enableCharacterTint || !ou)
    {
        ClearKoCharacterTint(state, pluginName);
        return;
    }

    std::vector<CharacterTintEntry> desiredEntries;
    BuildDesiredTintEntries(state, desiredEntries);

    std::vector<ResolvedCharacter> resolvedCharacters;
    if (!desiredEntries.empty() || !state.characterTintEntries.empty())
    {
        CollectResolvedCharacters(resolvedCharacters);
    }

    std::vector<CharacterTintEntry> unchangedEntries;
    unchangedEntries.reserve(state.characterTintEntries.size());
    for (size_t i = 0; i < state.characterTintEntries.size(); ++i)
    {
        const CharacterTintEntry& oldEntry = state.characterTintEntries[i];
        const int desiredIndex = FindTintEntryByHandle(desiredEntries, oldEntry.targetHandle);
        const bool keepCurrentTint = (desiredIndex >= 0)
            && (desiredEntries[static_cast<size_t>(desiredIndex)].markerRelation == oldEntry.markerRelation);
        if (keepCurrentTint)
        {
            unchangedEntries.push_back(oldEntry);
            continue;
        }

        if (SetTintForHandle(oldEntry.targetHandle, resolvedCharacters, kClearTintColour, false, false, pluginName))
        {
            ++gTintDiagCleared;
        }
    }

    std::vector<CharacterTintEntry> nextEntries = unchangedEntries;
    nextEntries.reserve(desiredEntries.size());
    for (size_t i = 0; i < desiredEntries.size(); ++i)
    {
        const CharacterTintEntry& desired = desiredEntries[i];
        if (TintEntriesContainExact(unchangedEntries, desired.targetHandle, desired.markerRelation))
        {
            continue;
        }

        const Ogre::ColourValue tintColour = ResolveTintColour(state, desired.markerRelation);
        if (SetTintForHandle(
                desired.targetHandle,
                resolvedCharacters,
                tintColour,
                state.config.characterTintForceDepthOverride,
                true,
                pluginName))
        {
            nextEntries.push_back(desired);
        }
    }

    state.characterTintEntries.swap(nextEntries);
    EmitTintDiagLogIfDue(pluginName);
}

} // namespace vs_character_tint
