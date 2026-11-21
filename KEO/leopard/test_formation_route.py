"""Compile the real formation order handoff with stand-ins for the game and route search."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(file):
    return (ROOT / file).read_text(encoding="utf-8-sig")


def function(raw, signature):
    start = raw.index(signature)
    end = raw.index("{", start) + 1
    depth = 1
    while depth:
        depth += (raw[end] == "{") - (raw[end] == "}")
        end += 1
    return raw[start:end]


def segment(raw, first, last):
    start = raw.index(first)
    return raw[start:raw.index(last, start)]


capture = source("src/planner/planner_capture.cpp")
merge = source("src/planner/planner_merge.cpp")
formation = source("src/movement/formation.cpp")
code = r"""
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstddef>
@GROUP_TYPES@
#include "movement/formation_members.h"
#include "movement/formation_gather_policy.h"
#include "planner/plan_merge.h"
#include "planner/plan_store.h"
#include "planner/planner_config.h"
#include "planner/planner_merge.h"
#include "planner/planner_tick.h"
#include "planner/planner_tick_internal.h"
#include "planner/planner_water.h"

static double wall = 0.0;
static unsigned generation = 1;
static bool goalKnown = true;
static bool anchorRoute = true;
static unsigned attempts = 0;
static float nextGather = 0.0f;
static int written = 0;
static float lastTarget[3];
struct Actor { uintptr_t cm; float pos[3]; };
static double ElapsedSec() { return wall; }
static LONGLONG QpcNow() { return 0; }
static double QpcToMs(LONGLONG) { return 0.0; }

namespace planner {
using namespace planner_tick_detail;
PlannerConfig g_plannerCfg = {};
unsigned CgStoreGen() { return generation; }
const char* PlannerModeName(int) { return "fixture"; }
bool PlannerOrderRunTogether(const uintptr_t*, int) { return true; }
int PlannerOrderWater(const uintptr_t*, int, float*, float*) { return 0; }

namespace planner_tick_detail {
double BehaviorNow(double now) { return now; }
bool PlayersOverCap() { return false; }
void TakeSnapshot() {}
bool Locate(const float p[3], Located* out) { out->dir = 0; return goalKnown || p[0] < 10.0f; }
uintptr_t MovementOf(uintptr_t character) { return ((Actor*)character)->cm; }
void CharPos(uintptr_t character, float out[3]) { memcpy(out, ((Actor*)character)->pos, sizeof(float) * 3); }
void ClearMemo() {}
const Built* SearchAndBuild(const Located&, const Located&, const float*, const PlanSearchParams&)
{
    static const Built found = {};
    return &found;
}
int WritePlan(uintptr_t cm, const float*, const Located&, const float dest[3], const Built&,
              double now, int* verdict, int, const PlanSearchParams&, int outdoors)
{
    memcpy(lastTarget, dest, sizeof(lastTarget));
    ++written;
    PlanWrite w = {};
    w.cm = cm;
    w.now = now;
    w.orderOutdoors = outdoors;
    w.verdict = *verdict = PV_DIRECT;
    memcpy(w.finalDest, dest, sizeof(w.finalDest));
    return PlanStoreWrite(w);
}
int DropPlan(uintptr_t cm, PlanDropWhy) { return PlanStoreDrop(cm); }
}

@CAPTURE_STATE@
@MERGE_STATE@
static bool SearchJoins(const uintptr_t*, int n, int, const float*, const Located&, const float*, const PlanSearchParams&)
{
    ++attempts;
    for (int i = 0; i < n; ++i) s_join[i] = 1;
    return anchorRoute;
}
static void GatherPointAt(int, float p[3]) { p[0] = nextGather; }
static float GatherDistance(int, int, int, const float*) { return 100.0f; }
static void ReportMerge(int, int, int, float, const char*, int, int, const float*, double) {}
static void ReportOrderPlan(int, int, const Located&, const Located&, int, const Built&, int, bool, uintptr_t) {}
@MERGE@
@CAPTURE@
}
using namespace planner;

#define KEO_DETAIL(x) ((void)0)
#define KLIB_MEMBER(domain,base,name,offset) ((uintptr_t)(base)+(offset))
const size_t OFF_CHAR_MOVEMENT = offsetof(Actor, cm);
const size_t OFF_CHAR_POS_X = offsetof(Actor, pos);
const size_t OFF_CHAR_POS_Y = OFF_CHAR_POS_X + 4;
const size_t OFF_CHAR_POS_Z = OFF_CHAR_POS_X + 8;
FormationGroup formationGroups[MAX_FORMATION_GROUPS];
static unsigned nextFormationGroupId = 1;
@CLOCK@
static void FormationPaceResetGroup(int) {}
static bool CharacterIsHolding(uintptr_t) { return false; }
static bool CharacterIsInSomething(uintptr_t) { return false; }
static float GatherSpeedOf(uintptr_t, float) { return 50.0f; }
static bool ZonePauseIsPaused() { return false; }
static double OOT_ActiveNow(double t, bool) { return t; }
void FormationDetachCharacters(const uintptr_t*, int) { formationGroups[0].active = false; }
@CREATE@

static void Check(bool ok, const char* what)
{
    if (!ok) { fprintf(stderr, "FAIL %s\n", what); ExitProcess(2); }
}

static int Move(uintptr_t* chars, int count, const float* dest, int* order)
{
    return PlannerNoteOrder(chars, count, dest, NULL, false, false, true, order);
}

static void Create(uintptr_t* chars, const float* dest, int order, int leader)
{
    uintptr_t sorted[3] = { chars[leader], chars[0], chars[1] };
    int j = 1;
    for (int i = 0; i < 3; ++i)
        if (i != leader) sorted[j++] = chars[i];
    Check(CreateFormationGroup(dest, sorted, 3, order), "formation accepts the specified order");
}

int main()
{
    Actor actors[4] = { {1,{0,0,0}}, {2,{2,0,0}}, {3,{5,0,0}}, {4,{7,0,0}} };
    uintptr_t chars[3] = { (uintptr_t)&actors[0], (uintptr_t)&actors[1], (uintptr_t)&actors[2] };
    uintptr_t other[3] = { chars[0], chars[1], (uintptr_t)&actors[3] };
    uintptr_t subset[2] = { chars[1], chars[0] };
    uintptr_t reordered[3] = { chars[2], chars[0], chars[1] };
    float a[3] = {400,0,0}, b[3] = {800,0,0};
    int order = 0;
    PlanStoreArm(PLANNER_ON);
    g_plannerCfg.mergeBias = 3;
    anchorRoute = false;
    wall = 10.0;
    Check(Move(chars,3,a,&order) == PLANNER_SKIP_FORMATION && order == 0 && attempts == 1,
          "leader search fails, individual native movement retained");
    PlanStoreDrop(actors[1].cm);
    anchorRoute = true;
    wall = 10.1;
    Check(Move(chars,3,a,&order) == PLANNER_SKIP_FORMATION && order == 0 && attempts == 1,
          "same command preserves refusal despite a dropped individual plan");

    wall = 10.2;
    nextGather = 124.0f;
    int leader = Move(chars,3,b,&order);
    Check(leader == 1 && order > 0 && attempts == 2, "changed target runs a fresh search");
    const int firstOrder = order;
    Create(chars,b,order,leader);
    unsigned firstGroup = formationGroups[0].groupId;
    Check(formationGroups[0].startX == 124.0f && !formationGroups[0].gathered, "native gather uses the new route's point");
    formationGroups[0].members[1].gatherSent = true;
    PlannerDrop(chars[1], false);
    goalKnown = false;
    wall = 10.3;
    const int repeatLeader = Move(reordered,3,b,&order);
    goalKnown = true;
    Check(order == firstOrder && repeatLeader == 2 && attempts == 2, "accepted exact repeat survives internal Follow and an unavailable individual refresh");
    Create(reordered,b,order,repeatLeader);
    Check(formationGroups[0].groupId == firstGroup && !formationGroups[0].members[1].gatherSent,
          "same gather continues after the native repeat replaces member moves");

    wall = 10.4;
    nextGather = 270.0f;
    leader = Move(chars,3,a,&order);
    Check(order != firstOrder && attempts == 3 && leader == 1, "A to B to A considers only the latest command");
    Create(chars,a,order,leader);
    Check(formationGroups[0].groupId != firstGroup && formationGroups[0].startX == 270.0f,
          "new destination for same roster replaces the old gather");
    const int latestOrder = order;

    wall = 10.5;
    anchorRoute = false;
    Check(Move(subset,2,a,&order) == PLANNER_SKIP_FORMATION && order == 0 && attempts == 4,
          "new subset still needs its own search even when its individual destinations match");

    wall = 10.6;
    PlanStoreDrop(actors[0].cm);
    written = 0;
    PlannerResumeFromGather(latestOrder,&chars[0],1,wall);
    Check(written == 1 && lastTarget[0] == a[0], "resume uses exact latest order even after the leader leaves");
    PlanStoreDrop(actors[0].cm);
    written = 0;
    PlannerResumeFromGather(0,&chars[0],1,wall);
    Check(written == 0, "unmerged formation cannot resume from any overlapping record");
    ++generation;
    PlannerResumeFromGather(firstOrder,&chars[0],1,wall);
    Check(written == 0, "world change fences previous gather records");
    Check(!CreateFormationGroup(b,reordered,3,firstOrder), "stale identity cannot fall back to an unverified gather");

    goalKnown = false;
    wall = 11.0;
    Check(Move(other,3,b,&order) == PLANNER_SKIP_FORMATION && attempts == 4,
          "unlocated goal also forbids an unverified gather");
    goalKnown = true;
    anchorRoute = true;
    wall = 11.1;
    Check(Move(other,3,b,&order) == PLANNER_SKIP_FORMATION && attempts == 4,
          "same click cannot promote an unlocated goal refusal");
    PlannerDrop(other[0]);
    Check(Move(other,3,b,&order) == 1 && attempts == 5, "intervening independent order invalidates reuse");
    printf("PASS formation capture, repeat identity, debounce and exact resume\n");
}
"""

sections = {
    "CAPTURE_STATE": segment(capture, "static int            s_orderSeq", "// ---- The order capture"),
    "MERGE_STATE": segment(merge, "namespace planner_merge_detail", "// Member k's own optimum"),
    "MERGE": "\n".join(function(merge, function_name) for function_name in (
        "static void Record(", "int planner_tick_detail::MergeOrder(", "static MergeRecord* FindMergeRecord(",
        "static int RecordMember(", "bool PlannerMergeGather(", "void PlannerResumeFromGather(",
    )),
    "CAPTURE": "\n".join(function(capture, function_name) for function_name in (
        "static bool IsRepeatOrder(", "static bool WasRepeat(", "static void DropOrderPlans(",
        "static int MarkRepeats(", "int PlannerNoteOrder(", "void PlannerDrop(",
    )),
    "GROUP_TYPES": "const int MAX_FORMATION_MEMBERS_LIMIT = 64;\n" + segment(
        source("src/movement/formation.h"), "const int MAX_FORMATION_GROUPS", "extern bool scatterPatchApplied;"
    ),
    "CLOCK": segment(formation, "namespace formation_detail {", "// ========================================================================="),
    "CREATE": function(formation, "bool CreateFormationGroup("),
}
for key, contents in sections.items():
    code = code.replace(f"@{key}@", contents)

vc = Path("C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC")
sdk = Path("C:/Program Files/Microsoft SDKs/Windows/v7.1")
env = dict(os.environ, INCLUDE=f'{vc / "include"};{sdk / "Include"}', LIB=f'{vc / "lib/amd64"};{sdk / "Lib/x64"}')
env["PATH"] = str(vc / "bin/amd64") + ";" + env["PATH"]

with tempfile.TemporaryDirectory(prefix="keo-formation-route-") as tmp:
    folder = Path(tmp)
    fixture = folder / "formation_route.cpp"
    fixture.write_text(code, encoding="utf-8")
    exe = folder / "formation_route.exe"
    sources = [
        "src/planner/plan_store.cpp", "src/planner/plan_policy.cpp", "src/planner/plan_build.cpp",
        "src/planner/plan_merge.cpp", "src/movement/formation_members.cpp", "src/movement/formation_gather_policy.cpp",
    ]
    for opt in ("Od", "O2"):
        command = [str(vc / "bin/amd64/cl.exe"), "/nologo", "/EHsc", "/MT", "/" + opt,
                   "/I" + str(ROOT / "src"), str(fixture)]
        command.extend(str(ROOT / p) for p in sources)
        result = subprocess.run(command + ["/Fe" + str(exe)], env=env, cwd=folder, capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        result = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)
        print(opt, result.stdout + result.stderr, end="")
        if result.returncode:
            raise SystemExit(result.returncode)
