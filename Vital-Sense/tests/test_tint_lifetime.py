"""Run production material ownership and tint eviction paths against an Ogre test double."""
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/vs_character_tint.cpp").read_text("utf-8-sig")


def function(signature):
    start = SOURCE.index(signature)
    end = SOURCE.index("{", start) + 1
    depth = 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[start:end]


names = (
    "void RetireAnimalTintClone(",
    "bool HasAnimalTintClones(",
    "bool RestoreAnimalTintMaterialClonesForEntity(",
    "bool ApplyTintToEntityUsingAnimalMaterialClones(",
    "static bool IsTintClearDue(",
    "static DWORD NextTintClearAttempt(",
    "static bool ClearTintEntry(",
    "void ClearKoCharacterTint(",
)

fixture = r"""
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

struct hand { int index; };
struct Character {};
namespace Ogre {
    struct ColourValue { int tint; };
    struct Material;
    struct MaterialPtr {
        Material* ptr;
        MaterialPtr(Material* p = 0) : ptr(p) {}
        bool isNull() const { return ptr == 0; }
        void setNull() { ptr = 0; }
        Material* operator->() const { return ptr; }
        Material* getPointer() const { return ptr; }
    };
    static std::vector<Material*> created;
    struct Material {
        bool supportsTint;
        int tint;
        Material(bool support = true) : supportsTint(support), tint(0) {}
        MaterialPtr clone(const std::string&) {
            Material* value = new Material(*this);
            created.push_back(value);
            return value;
        }
    };
    struct SubEntity {
        MaterialPtr material;
        bool fail;
        SubEntity(Material* m = 0) : material(m), fail(false) {}
        MaterialPtr getMaterial() const { return material; }
        void setMaterial(const MaterialPtr& value) {
            if (fail) throw std::runtime_error("setMaterial");
            material = value;
        }
    };
    struct Entity {
        std::vector<SubEntity> parts;
        size_t getNumSubEntities() const { return parts.size(); }
        SubEntity* getSubEntity(size_t i) { return &parts.at(i); }
    };
}

struct AnimalTintMaterialCloneEntry {
    hand targetHandle;
    std::vector<Ogre::MaterialPtr> originalMaterials;
    std::vector<Ogre::MaterialPtr> cloneMaterials;
};
std::vector<AnimalTintMaterialCloneEntry> gAnimalTintMaterialCloneEntries;
std::vector<Ogre::MaterialPtr> released;
const Ogre::ColourValue kClearTintColour = {0};
const DWORD kFailedTintClearRetryMs = 250;
const unsigned int kMaxTintClearAttempts = 3;
std::set<int> gUnavailableTintHandles;
unsigned int gUnavailableTintGeneration = 0;
bool gTintDiagnosticsEnabled = false;
unsigned int gTintDiagCleared = 0;
DWORD clockMs = 100;
int resolveCalls = 0;
bool knownCharacter = false, clearWorks = false;
Character live;
struct CharacterTintEntry {
    hand targetHandle;
    int markerRelation;
    int totalBounty;
    DWORD nextClearAttemptMs;
    unsigned int failedClearAttempts;
};
struct RuntimeStateView {
    struct { bool debugLogDiagnostics; } config;
    std::vector<CharacterTintEntry> characterTintEntries;
};
int FindAnimalTintCloneEntryByHandle(const hand& handle) {
    for (size_t i = 0; i < gAnimalTintMaterialCloneEntries.size(); ++i)
        if (gAnimalTintMaterialCloneEntries[i].targetHandle.index == handle.index)
            return static_cast<int>(i);
    return -1;
}
std::string BuildAnimalTintCloneName(const hand&, size_t) { return "clone"; }
bool MaterialPtrsReferSameObject(const Ogre::MaterialPtr& a, const Ogre::MaterialPtr& b) {
    return !a.isNull() && a.ptr == b.ptr;
}
void ReleaseAnimalTintCloneLater(Ogre::MaterialPtr& value) {
    if (!value.isNull()) released.push_back(value);
    value.setNull();
}
void ReleaseAnimalTintClonesLater(std::vector<Ogre::MaterialPtr>& values) {
    for (size_t i = 0; i < values.size(); ++i) ReleaseAnimalTintCloneLater(values[i]);
    values.clear();
}
bool ApplyTintToMaterialColourRequired(Ogre::Material* m, const Ogre::ColourValue& c, bool) {
    if (!m || !m->supportsTint) return false;
    m->tint = c.tint;
    return true;
}
bool ApplyTintToMaterialPrimaryPassLikeExampleColourRequired(
    const Ogre::MaterialPtr& m, const Ogre::ColourValue& c, bool depth) {
    return ApplyTintToMaterialColourRequired(m.ptr, c, depth);
}
Character* ResolveTintCharacterByHandle(const hand&) {
    ++resolveCalls;
    return knownCharacter ? &live : 0;
}
bool SetTintForCharacter(Character* c, const Ogre::ColourValue&, bool, const char*) {
    return c && clearWorks;
}
void EmitTintDiagLogIfDue(const char*) {}

#define GetTickCount ReadFixtureClock
DWORD ReadFixtureClock() { return clockMs; }

// @PRODUCTION@

int failures = 0, checks = 0;
void Check(bool value, const char* name) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", name); }
}

int main() {
    const char* plugin = "test";
    const Ogre::ColourValue red = {1};
    const hand id = {1};
    Ogre::Material plain, unsupported(false), replacement;
    Ogre::Entity entity;
    entity.parts.push_back(Ogre::SubEntity(&unsupported));
    Check(!ApplyTintToEntityUsingAnimalMaterialClones(id, &entity, red, false),
        "unsupported shader skips tint");
    Check(gAnimalTintMaterialCloneEntries.empty() && released.size() == 1,
        "failed shader retires an unattached clone");
    gAnimalTintMaterialCloneEntries.clear();
    released.clear();

    entity.parts[0].material = &plain;
    entity.parts[0].fail = true;
    Check(!ApplyTintToEntityUsingAnimalMaterialClones(id, &entity, red, false),
        "failed attach reports no tint");
    Check(gAnimalTintMaterialCloneEntries.empty() && released.size() == 1,
        "failed attach retires an unbound clone");
    gAnimalTintMaterialCloneEntries.clear();
    released.clear();

    entity.parts[0].fail = false;
    entity.parts.push_back(Ogre::SubEntity(&plain));
    Check(ApplyTintToEntityUsingAnimalMaterialClones(id, &entity, red, true),
        "valid parts can be tinted");
    Ogre::Material* secondClone = entity.parts[1].material.ptr;
    entity.parts[0].fail = true;
    Check(!RestoreAnimalTintMaterialClonesForEntity(id, &entity),
        "partial restore remains pending");
    Check(gAnimalTintMaterialCloneEntries.size() == 1 && released.size() == 1,
        "successful slot retires while the failing slot stays tracked");
    entity.parts[0].fail = false;
    entity.parts[0].material = &replacement;
    entity.parts.pop_back();
    Check(RestoreAnimalTintMaterialClonesForEntity(id, &entity),
        "new model can retire obsolete records");
    Check(entity.parts[0].material.ptr == &replacement && gAnimalTintMaterialCloneEntries.empty(),
        "restore cannot overwrite a replacement model");
    Check(secondClone != 0, "second clone existed");
    gAnimalTintMaterialCloneEntries.clear();
    released.clear();

    entity.parts[0].material = &plain;
    ApplyTintToEntityUsingAnimalMaterialClones(id, &entity, red, true);
    Ogre::Material* vanished = entity.parts[0].material.ptr;
    RuntimeStateView state = {};
    CharacterTintEntry entry = {id, 2, 0, 0, 0};
    state.characterTintEntries.push_back(entry);
    ClearKoCharacterTint(state, plugin);
    ClearKoCharacterTint(state, plugin);
    Check(resolveCalls == 1 && state.characterTintEntries.empty(),
        "vanished handle leaves the cleanup list on its first miss");
    Check(gAnimalTintMaterialCloneEntries.empty() && released.size() == 1 && vanished->tint == 0,
        "vanished actor's clone is neutralized and released even if an entity reference survives");

    knownCharacter = true;
    resolveCalls = 0;
    state.characterTintEntries.clear();
    state.characterTintEntries.push_back(entry);
    ClearKoCharacterTint(state, plugin);
    ClearKoCharacterTint(state, plugin);
    Check(resolveCalls == 1 && state.characterTintEntries.size() == 1,
        "temporary clear failure on a present actor is not retried in the next frame");
    clearWorks = true;
    clockMs += kFailedTintClearRetryMs;
    ClearKoCharacterTint(state, plugin);
    Check(resolveCalls == 2 && state.characterTintEntries.empty(),
        "present actor retries after the holdoff and is removed on success");

    clearWorks = false;
    resolveCalls = 0;
    state.characterTintEntries.push_back(entry);
    ApplyTintToEntityUsingAnimalMaterialClones(id, &entity, red, true);
    Ogre::Material* permanentlyUnavailable = entity.parts[0].material.ptr;
    for (unsigned int i = 0; i < kMaxTintClearAttempts + 2; ++i) {
        ClearKoCharacterTint(state, plugin);
        clockMs += kFailedTintClearRetryMs;
    }
    Check(resolveCalls == kMaxTintClearAttempts && state.characterTintEntries.empty(),
        "present character with permanently inaccessible model has bounded retries");
    Check(permanentlyUnavailable->tint == 0 && gAnimalTintMaterialCloneEntries.empty(),
        "permanently inaccessible model keeps no MaterialPtr in tint cache");

    resolveCalls = 0;
    knownCharacter = false;
    state.characterTintEntries.push_back(entry);
    ClearKoCharacterTint(state, plugin);
    Check(resolveCalls == 1 && state.characterTintEntries.size() == 1,
        "shared material survives a transient miss for cleanup");
    knownCharacter = true;
    clearWorks = true;
    clockMs += kFailedTintClearRetryMs;
    ClearKoCharacterTint(state, plugin);
    Check(resolveCalls == 2 && state.characterTintEntries.empty(),
        "shared material is cleared when the target returns");

    for (size_t i = 0; i < Ogre::created.size(); ++i) delete Ogre::created[i];
    std::printf("Vital Sense tint lifetime: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
"""

if __name__ == "__main__":
    vc = Path("C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC")
    sdk = Path("C:/Program Files/Microsoft SDKs/Windows/v7.1")
    env = os.environ | {
        "PATH": str(vc / "bin/amd64") + ";" + os.environ["PATH"],
        "INCLUDE": f"{vc / 'include'};{sdk / 'Include'}",
        "LIB": f"{vc / 'lib/amd64'};{sdk / 'Lib/x64'}",
    }
    with tempfile.TemporaryDirectory(prefix="vital-sense-tint-lifetime-") as temp:
        work = Path(temp)
        cpp = work / "lifetime.cpp"
        source = fixture.replace("// @PRODUCTION@", "\n".join(function(name) for name in names))
        cpp.write_text(source, encoding="utf-8")
        exe = work / "lifetime.exe"
        subprocess.run(
            [str(vc / "bin/amd64/cl.exe"), "/nologo", "/EHsc", "/MD", "/O2", str(cpp), "/Fe" + str(exe)],
            env=env, cwd=work, check=True,
        )
        raise SystemExit(subprocess.run([str(exe)], check=False).returncode)
