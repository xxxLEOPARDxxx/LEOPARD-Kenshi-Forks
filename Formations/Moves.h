// THE ORDERLY MOVE and THE SHAPES READOUT.
//
// ORDERLY MOVE (owner, boot 7): "squads run in a gaggle unless directed to
// follow a character... if the leader is fast he leaves his squad; the gaggle
// has a run-as-fast-as-the-slowest feature. I would like to combine the
// mechanics and be able to select a squad and send them somewhere in a nice
// orderly fashion." And: "the slowest person will be the one whose legs are
// broken - Usain Bolt couldn't outrun a dust bandit with broken legs."
//
// So: ALT + a plain right-CLICK (a drag shorter than 4 units) sends the
// selection to the point with the SLOWEST selected character leading: the
// leader gets the ground order and nobody can outrun a leader who is the
// slowest. The rest either ADVANCE in the current shape behind him (Advance.h,
// the default) or take the game's own follow order on him (Advance=0).
//
// "Slowest" is read from the engine's own movement ceiling, not from a stat:
//   Character::getMovement()               -> CharMovement* (= Character+0x640, read from the binary)
//   AbstractMovementBase::getMaxSpeed(mov) -> float at +0xB4, the speed the mover actually allows
// (getCurrentSpeed reads +0xB8 on the same object, and Character::getMovementSpeed
// reads [Character+0x640]+0xB8 - so CharMovement IS an AbstractMovementBase at
// offset 0, and the export applies). CharStats::getMaxRunSpeed (+0x17C) is
// logged beside it for the record; it is the stat, and the owner's point is
// that the stat is not the truth when a leg is broken. Boot 7 confirmed it:
// the crippled character read 11.00 against 35-83 and led every time.
//
// THE FOLLOW ORDER. Boot 7: playerMoveOrderDefault(null, leader, pos) walked the
// followers to the leader ONCE - read from the binary, that function always ends
// in addOrder(TaskType 29 MOVE_CUS_ORDERED), a ground move, whatever the object.
// The friendly right-click handler (PlayerInterface::characterSelected) gives
// its object orders through the exported Character::addOrder(building,
// TaskType, object, bool, true, pos) (loot = 26, first aid = 25, seen there),
// and the SDK enum (KenshiLib Enums.h, NULL_TASK = 0, sequential; 5/25/29/30/31
// measured) puts FOLLOW_PLAYER_ORDER at 44. Boot 8: 29 followers on 44 tracked
// the leader - the game's follow, which is a clump; that is what Advance.h
// replaces with the shape.
//
typedef void* (*CH_getMovement_t)(void* ch);
typedef float (*MV_float_t)(void* mov);
typedef void* (*CH_getStats_t)(void* ch);
typedef float (*CS_float_t)(void* stats);
static CH_getMovement_t CH_getMovement = 0;
static MV_float_t       MV_getMaxSpeed = 0;
static CH_getStats_t    CH_getStats = 0;
static CS_float_t       CS_getMaxRunSpeed = 0;
typedef void (*CH_addOrder_t)(void* ch, void* building, int task, void* obj, bool shift, bool b, const Vec3* pos);
static CH_addOrder_t CH_addOrder = 0;

typedef void (*PI_speedGroup_t)(void* pi, bool b);
static PI_speedGroup_t PI_assignSpeedGroup = 0;      // the game's own pace (see Advance.h)

// a heap-backed engine string for text longer than the 15 inline characters:
// the buffer is OURS and outlives the call; the engine reads a const reference
static bool StrMakeHeap(EngineString& out, char* backing, size_t backingCap, const char* text)
{
    size_t n = strlen(text);
    if (n <= 15) return StrMakeInline(out, text);
    if (n + 1 > backingCap) return false;
    memcpy(backing, text, n + 1);
    memset(&out, 0, sizeof(out));
    *(char**)out.buf = backing;
    out.size = n;
    out.cap  = backingCap - 1;
    return true;
}

static void MovesInit(HMODULE kl)
{
    CH_getMovement = (CH_getMovement_t)Real(GetProcAddress(kl, "?getMovement@Character@@QEAAPEAVCharMovement@@XZ"));
    MV_getMaxSpeed = (MV_float_t)Real(GetProcAddress(kl, "?getMaxSpeed@AbstractMovementBase@@QEBAMXZ"));
    CH_getStats = (CH_getStats_t)Real(GetProcAddress(kl, "?getStats@Character@@QEAAPEAVCharStats@@XZ"));
    CS_getMaxRunSpeed = (CS_float_t)Real(GetProcAddress(kl, "?getMaxRunSpeed@CharStats@@QEAAMXZ"));
    CH_addOrder = (CH_addOrder_t)Real(GetProcAddress(kl, "?addOrder@Character@@QEAAXPEAVBuilding@@W4TaskType@@PEAVRootObject@@_N3AEBVVector3@Ogre@@@Z"));
    PI_assignSpeedGroup = (PI_speedGroup_t)Real(GetProcAddress(kl, "?assignSpeedGroup@PlayerInterface@@QEAAX_N@Z"));
    Log("  speed group (the game's own pace)  %s", PI_assignSpeedGroup ? "reachable (PlayerInterface::assignSpeedGroup)" : "NOT reachable - followers run up and stop");
    Log("  the slowest leads (movement ceiling) %s", (CH_getMovement && MV_getMaxSpeed) ? "reachable (Character::getMovement + AbstractMovementBase::getMaxSpeed)" : "NOT reachable - the first selected leads");
    Log("  the follow order (Character::addOrder) %s, task %d", CH_addOrder ? "reachable" : "NOT reachable - followers get a one-shot move", g_set.followTask);
}

// the engine's ceiling for this character right now; -1 when unreadable
static float CeilingOf(void* ch, float* runStat)
{
    float m = -1;
    __try
    {
        if (CH_getMovement && MV_getMaxSpeed) { void* mov = CH_getMovement(ch); if (PtrOk(mov)) m = MV_getMaxSpeed(mov); }
        if (runStat) { *runStat = -1; if (CH_getStats && CS_getMaxRunSpeed) { void* st = CH_getStats(ch); if (PtrOk(st)) *runStat = CS_getMaxRunSpeed(st); } }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    return m;
}

// the game's own click order for one character to a point (the shape's door)
static bool GroundOrder(void* ch, const Vec3* pos)
{
    __try
    {
        if (CH_playerMoveOrderDefault) CH_playerMoveOrderDefault(ch, 0, 0, pos);
        else if (CH_addJob) CH_addJob(ch, TASK_MOVE_CUS_ORDERED, 0, false, false, pos);
        else return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

#include "Advance.h"
#include "Retreat.h"

static void OrderlyMove(const Vec3& to)
{
    int n = ReadSelection();
    if (n == 0) { Log("move: nothing selected"); return; }
    if (!CH_playerMoveOrderDefault && !CH_addJob) { Log("move: no order door"); return; }
    AdvanceStop("a new move");
    int lead = -1; float slow = 1e30f; bool able[SQUAD_MAX];
    for (int i = 0; i < n; i++)
    {
        void* ch = g_members[i]; able[i] = false;
        const char* why = CharBusyWhy(ch);
        if (why && strcmp(why, "under attack") != 0) { Log("   %2d: skipped, %s", i, why); continue; }
        able[i] = true;
        float run = -1; float c = CeilingOf(ch, &run);
        Log("   %2d: ceiling %.2f  (run stat %.2f)", i, c, run);
        if (c >= 0 && c < slow) { slow = c; lead = i; }
    }
    if (lead < 0) { for (int i = 0; i < n && lead < 0; i++) if (able[i]) lead = i; }   // ceilings unreadable: the first able one leads
    if (lead < 0) { Log("move: nobody able to move"); return; }
    void* leader = g_members[lead];
    if (!GroundOrder(leader, &to)) { Log("move: the leader's order faulted"); return; }
    if (g_set.advance)
    {
        int k = AdvanceStart(leader, to, able, n);
        Log("move: the slowest leads - #%d (ceiling %.2f) to %.0f, %.0f; %d advance behind him in a %s", lead, slow < 1e29f ? slow : -1.0f, to.x, to.z, k, SHAPE_NAMES[g_set.shape]);
        return;
    }
    Vec3 lp = to;
    if (CH_getPosition) { __try { CH_getPosition(leader, &lp); } __except (EXCEPTION_EXECUTE_HANDLER) { lp = to; } }
    int followers = 0;
    for (int i = 0; i < n; i++)
    {
        if (i == lead || !able[i]) continue;
        __try
        {
            if (CH_addOrder) CH_addOrder(g_members[i], 0, g_set.followTask, leader, false, true, &lp);   // the follow ORDER on the leader (persistent, unlike a job)
            else if (CH_playerMoveOrderDefault) CH_playerMoveOrderDefault(g_members[i], 0, leader, &lp);
            else CH_addJob(g_members[i], TASK_MOVE_CUS_ORDERED, 0, false, false, &to);
            followers++;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("move: a follow order faulted (slot %d)", i); }
    }
    Log("move: the slowest leads - #%d (ceiling %.2f) to %.0f, %.0f; %d follow via %s", lead, slow < 1e29f ? slow : -1.0f, to.x, to.z, followers,
        CH_addOrder ? "Character::addOrder(FollowTask)" : CH_playerMoveOrderDefault ? "a one-shot move to the leader" : "addJob to the point");
}
