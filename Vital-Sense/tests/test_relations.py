"""Exercise the production relation classifier with stubbed Kenshi faction and roster APIs."""
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOOKUP = (ROOT / "src/vs_character_lookup.cpp").read_text("utf-8-sig")
PROBE = (ROOT / "src/vs_probe.cpp").read_text("utf-8-sig")


def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def production_source():
    squad_check = function(LOOKUP, "bool IsCharacterInPlayerSquadSafe(")
    faction_read = function(LOOKUP, "static int ReadFactionRelation(")
    resolver = function(LOOKUP, "int ResolveMarkerRelationSafe(")
    relation_store = PROBE[
        PROBE.index("const int markerRelation =", PROBE.index("vs_probe_cache::ClearNotDownedCandidate(state, targetHandle);")):
        PROBE.index("if (vs_probe_cache::UpsertCachedKoTarget")
    ]
    entry = (
        "Character* candidate, bool isSpider = false) {\n"
        "const bool isPlayerControlledCharacter = vs_character_lookup::IsCharacterInPlayerSquadSafe(candidate);\n"
        "const bool speciesClassified = true;\n"
        "const struct { bool isLikelySpider; } speciesInfo = {isSpider};\n"
        "RuntimeStateView state = {};\n"
        "int targetHandle=0, candidatePos=0, nowMs=0, markerState=0, totalBounty=0;\n"
        + relation_store +
        "return cachedTarget.markerRelation;\n}"
    )
    return "\n".join([
        "namespace vs_character_lookup {", squad_check, faction_read, resolver, "}", "int Resolve(" + entry
    ])


FIXTURE = r"""
#include <cstdio>
#include <map>
#include <vector>
#include <windows.h>

template <class T> struct lektor : std::vector<T> {
    bool available;
    lektor() : available(true) {}
    bool valid() const { return available; }
};

struct Faction;
struct FactionRelations {
    struct RelationData {
        bool alliance;
        float relation;
        RelationData() : alliance(false), relation(0.0f) {}
    };
    std::map<Faction*, RelationData> _factionRelations;
    float defaultFactionRelation;
    Faction* me;
    explicit FactionRelations(float standing) : defaultFactionRelation(standing), me(0) {}
    const RelationData& getRelationData(Faction* faction) {
        if (_factionRelations.find(faction) == _factionRelations.end()) {
            _factionRelations[faction].relation = defaultFactionRelation;
        }
        return _factionRelations[faction];
    }
    bool isEnemy(Faction* faction) { return getRelationData(faction).relation <= -30.0f; }
    bool isAlly(Faction* faction) {
        const RelationData& data = getRelationData(faction);
        return data.alliance || data.relation >= 50.0f;
    }
};
struct Faction { FactionRelations* relations; };
struct Character {
    Faction* owner;
    bool enemy, ally, animal, adverseAI;
    bool isEnemy(Character*, bool) { return enemy; }
    bool isAlly(Character*, bool) { return ally; }
    bool isAnimal() { return animal; }
    bool shouldIScrewThisGuyOver(Character*) { return adverseAI; }
    bool areYouGonnaGetMe(Character*) { return adverseAI; }
};
struct PlayerInterface { Faction* participant; lektor<Character*> playerCharacters; };
struct World { PlayerInterface* player; };
World* ou = 0;
struct RuntimeStateView { struct { unsigned generation; } probeRuntimeCaches; };
struct CachedKoTarget {
    enum { RELATION_SQUAD, RELATION_ALLY, RELATION_ENEMY, RELATION_NEUTRAL };
    int handle, position, time, state, markerRelation, bounty;
    unsigned generation;
};

// @PRODUCTION@

int checks = 0;
int failures = 0;
void Check(bool passed, const char* name) {
    ++checks;
    if (!passed) {
        ++failures;
        std::printf("FAIL: %s\n", name);
    }
}

int main() {
    FactionRelations playerRelations(0.0f);
    FactionRelations neutralRelations(0.0f);
    FactionRelations enemyRelations(-30.0f);
    FactionRelations alliedRelations(50.0f);
    Faction own = {&playerRelations}, neutral = {&neutralRelations};
    Faction enemy = {&enemyRelations}, ally = {&alliedRelations}, unknown = {0};
    Character rosterMember = {&own, false, false, false, false};
    Character deceasedMember = {&own, false, false, false, true};
    Character neutralNpc = {&neutral, false, false, false, true};
    Character hostileCorpse = {&enemy, false, false, false, false};
    Character alliedNpc = {&ally, false, false, false, false};
    Character unknownNpc = {&unknown, false, false, false, false};
    Character noOwner = {0, false, false, false, false};
    Character spider = {&neutral, false, false, true, false};

    PlayerInterface player;
    player.participant = &own;
    player.playerCharacters.push_back(&rosterMember);
    World world = {&player};
    ou = &world;

    Check(Resolve(&rosterMember) == CachedKoTarget::RELATION_SQUAD, "roster member uses squad colour");
    Check(Resolve(&deceasedMember) == CachedKoTarget::RELATION_SQUAD, "player-owned body stays squad outside roster");
    Check(vs_character_lookup::IsCharacterInPlayerSquadSafe(&deceasedMember), "body tint honours squad membership for off-roster own body");
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_NEUTRAL, "temporary AI reaction does not turn neutral faction red");
    Check(Resolve(&hostileCorpse) == CachedKoTarget::RELATION_ENEMY, "hostile corpse uses faction even without combat hostility");
    Check(Resolve(&alliedNpc) == CachedKoTarget::RELATION_ALLY, "allied faction does not depend on each squad member");
    Check(Resolve(&spider, true) == CachedKoTarget::RELATION_NEUTRAL, "neutral spider uses its faction");
    spider.owner = &enemy;
    Check(Resolve(&spider, true) == CachedKoTarget::RELATION_ENEMY, "enemy spider uses its faction");
    spider.owner = &own;
    Check(Resolve(&spider, true) == CachedKoTarget::RELATION_SQUAD, "owned spider uses squad colour outside roster");

    rosterMember.owner = &neutral;
    Check(Resolve(&rosterMember) == CachedKoTarget::RELATION_SQUAD, "an explicit roster member remains squad with atypical ownership");
    player.playerCharacters.available = false;
    Check(Resolve(&hostileCorpse) == CachedKoTarget::RELATION_ENEMY, "hostile faction persists when roster is unavailable");
    Check(Resolve(&unknownNpc) == CachedKoTarget::RELATION_NEUTRAL, "unavailable relation data does not invent an enemy");
    Check(Resolve(&noOwner) == CachedKoTarget::RELATION_NEUTRAL, "missing owner does not invent an enemy");
    Check(playerRelations._factionRelations.empty() && neutralRelations._factionRelations.empty()
        && enemyRelations._factionRelations.empty() && alliedRelations._factionRelations.empty(),
        "reading missing relation entries leaves both diplomacy tables untouched");
    playerRelations._factionRelations[&unknown].relation = -30.0f;
    Check(Resolve(&unknownNpc) == CachedKoTarget::RELATION_ENEMY, "hostility declared by player faction is visible");
    neutralRelations._factionRelations[&own].relation = -29.99f;
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_NEUTRAL, "standing just above enemy threshold stays neutral");
    neutralRelations._factionRelations[&own].relation = 49.99f;
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_NEUTRAL, "standing just below ally threshold stays neutral");
    neutralRelations._factionRelations[&own].alliance = true;
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_ALLY, "native alliance overrides the ally threshold");
    playerRelations._factionRelations[&neutral].relation = -30.0f;
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_ENEMY, "hostility in one direction takes priority over alliance in the other");
    Check(playerRelations._factionRelations.size() == 2 && neutralRelations._factionRelations.size() == 1,
        "reading tracked factions keeps their relation tables the same size");

    ou = 0;
    Check(Resolve(&neutralNpc) == CachedKoTarget::RELATION_NEUTRAL, "unavailable player context does not invent an enemy");
    Check(Resolve(0) == CachedKoTarget::RELATION_NEUTRAL, "null candidate does not invent an enemy");

    std::printf("Vital Sense relations: %d checks, %d failed\n", checks, failures);
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
    with tempfile.TemporaryDirectory(prefix="vital-sense-relations-") as path:
        work = Path(path)
        source = work / "relations.cpp"
        source.write_text(FIXTURE.replace("// @PRODUCTION@", production_source()), "utf-8")
        exe = work / "relations.exe"
        subprocess.run(
            [str(vc / "bin/amd64/cl.exe"), "/nologo", "/EHsc", "/MD", "/O2", str(source), "/Fe" + str(exe)],
            env=env, cwd=work, check=True,
        )
        raise SystemExit(subprocess.run([str(exe)], check=False).returncode)
