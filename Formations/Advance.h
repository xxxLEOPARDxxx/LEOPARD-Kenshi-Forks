// ADVANCE - the orderly move marches in a military column behind the leader.
//
// Owner (boot 8): the game's own follow order tracked the leader but "they are
// still gaggled up" - vanilla follow is a clump. Boot 9: the shape while
// walking worked, and "make it a military column like the base game has, a
// walking circle looks funny". So the march is always a column of two files
// behind the leader, whatever shape is chosen for standing.
//
// The engine has a formation mover of its own (FormationMover::setFormationMode
// (hand), Character::getFormation = [[Character+0x640]+0x310]) but reaching it
// means walking an unmeasured member of CharMovement, which this plugin does
// not do. So the column is kept the plugin's way, with the game's own click
// order and nothing else:
//
//   1. Each follower gets a fixed place in the column (file left/right, rank),
//      nearest-first from where they stand at the start, and never swaps.
//   2. Once a second each place is re-computed from where the leader stands now
//      and which way the destination lies, and a follower out of his place by
//      more than 0.6 spacing gets the ground order to it. One already there is
//      left alone.
//   3. When the leader arrives (within 6 units) or has stood still for 4 s, a
//      final pass puts everyone in place and the advance ends.
//
// THE RUN-UP-STOP (boot 9, owner: "if he's creeping along they run up, stop,
// run up, stop"): the game's own answer is the SPEED GROUP - MoveSpeed has
// WALK, JOG, RUN, GROUPED (the "as fast as the slowest" the gaggle uses), and
// every right-click ends in PlayerInterface::assignSpeedGroup(false), which
// groups the SELECTION. The advance makes that same call after its orders,
// while the leader and his followers are still the selection, so the group's
// slowest - the leader - is everyone's pace. Unverified until boot.
//
// A plain right-click, a new formation drag or a new move ends the advance.
static bool  g_adv = false;
static void* g_advLeader = 0;
static Vec3  g_advTo = { 0, 0, 0 };
static int   g_advN = 0;                                   // followers
static void* g_advCh[SQUAD_MAX];
static float g_advAlong[SQUAD_MAX], g_advAcross[SQUAD_MAX];   // the follower's place relative to the leader, in the column's frame
static Vec3  g_advLastOrder[SQUAD_MAX];
static float g_advHx = 0, g_advHz = -1;                    // the heading (unit, xz)
static unsigned long g_advTick = 0, g_advStillSince = 0;
static Vec3  g_advLastLeader = { 0, 0, 0 };

static void AdvanceStop(const char* why)
{
    if (!g_adv) return;
    g_adv = false;
    Log("advance: ended - %s", why);
}

static bool PosOf(void* ch, Vec3& p)
{
    if (!CH_getPosition || !PtrOk(ch)) return false;
    __try { CH_getPosition(ch, &p); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

// a place in the column re-placed from the leader's position and the heading:
// along = left/right of the line of march, across = along the heading (negative = behind)
static Vec3 AdvancePlace(float along, float across, const Vec3& lp)
{
    float ux = g_advHz, uz = -g_advHx;                     // left/right of the march
    float rx = g_advHx, rz = g_advHz;                      // the heading
    Vec3 t = { lp.x + ux * along + rx * across, lp.y, lp.z + uz * along + rz * across };
    if (UT_terrainHeight) { __try { t.y = UT_terrainHeight(t.x, t.z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    return t;
}

static void AdvanceTick(bool force);

static int AdvanceStart(void* leader, const Vec3& to, const bool* able, int n)
{
    g_adv = false; g_advN = 0;
    Vec3 lp; if (!PosOf(leader, lp)) return 0;
    float hx = to.x - lp.x, hz = to.z - lp.z, hl = sqrtf(hx * hx + hz * hz);
    if (hl < 1e-3f) { hx = 0; hz = -1; } else { hx /= hl; hz /= hl; }
    g_advHx = hx; g_advHz = hz;
    // the column's places: two files, half a spacing left and right of the line
    // of march, ranks 1.2 spacings apart behind the leader
    float sp = Spacing();
    int total = 0; for (int i = 0; i < n; i++) if (able[i] && g_members[i] != leader) total++;
    if (total < 1) return 0;
    float pa[SQUAD_MAX], pc[SQUAD_MAX]; Vec3 pw[SQUAD_MAX];
    for (int j = 0; j < total; j++)
    {
        pa[j] = (j % 2 ? 0.5f : -0.5f) * sp;
        pc[j] = -(float)(j / 2 + 1) * sp * 1.2f;
        pw[j] = AdvancePlace(pa[j], pc[j], lp);
    }
    bool used[SQUAD_MAX] = { false };
    for (int i = 0; i < n; i++)
    {
        void* ch = g_members[i];
        if (!able[i] || ch == leader) continue;
        Vec3 p; if (!PosOf(ch, p)) continue;
        int bs = -1; float bd = 1e30f;
        for (int s = 0; s < total; s++) { if (used[s]) continue; float dx = pw[s].x - p.x, dz = pw[s].z - p.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; bs = s; } }
        if (bs < 0) break;
        used[bs] = true;
        g_advCh[g_advN] = ch; g_advAlong[g_advN] = pa[bs]; g_advAcross[g_advN] = pc[bs];
        g_advLastOrder[g_advN] = lp; g_advLastOrder[g_advN].x += 1e6f;
        g_advN++;
    }
    g_advLeader = leader; g_advTo = to; g_advLastLeader = lp; g_advStillSince = 0; g_advTick = 0;
    g_adv = g_advN > 0;
    AdvanceTick(true);
    // the game's own pace: the selection (leader + followers) becomes one speed group
    if (PI_assignSpeedGroup && PtrOk(g_playerIface))
    {
        __try { PI_assignSpeedGroup(g_playerIface, false); Log("advance: speed group assigned (the game's own 'as fast as the slowest')"); }
        __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("advance: assignSpeedGroup faulted - disarmed"); PI_assignSpeedGroup = 0; }
    }
    return g_advN;
}

// THE MARCH ENDS AS THE STANDING SHAPE (owner, boot 11: "number 1 is a go"):
// at the destination the chosen shape is laid facing the way the march came,
// with Hold on, so one click finishes as a wedge or a line. The wedge's drag
// runs along the march with the tip at the leader; every other shape's drag
// runs across it, so the front rank faces forward and ranks fill behind.
static float g_lastMarchHx = 0, g_lastMarchHz = 0; static bool g_haveMarch = false;   // for Retreat.h
static void AdvanceLayShape(const Vec3& lp)
{
    void* mem[SQUAD_MAX]; int m = 0; mem[m++] = g_advLeader;
    for (int i = 0; i < g_advN; i++) if (PtrOk(g_advCh[i]) && !CharBusyWhy(g_advCh[i]) && m < SQUAD_MAX) mem[m++] = g_advCh[i];
    float hx = g_advHx, hz = g_advHz, sp = Spacing();
    Vec3 A, B; float L;
    if (g_set.shape == 3) { L = sp * m * 0.5f; A.x = lp.x - hx * L; A.y = 0; A.z = lp.z - hz * L; B.x = lp.x; B.y = 0; B.z = lp.z; }
    else
    {
        L = g_set.shape == 1 ? sp * m / 3.14159f : g_set.shape == 2 ? sp * (ceilf(sqrtf((float)m)) - 1) : sp * (m - 1);
        float ux = hz, uz = -hx;
        A.x = lp.x - ux * L * 0.5f; A.y = 0; A.z = lp.z - uz * L * 0.5f; B.x = lp.x + ux * L * 0.5f; B.y = 0; B.z = lp.z + uz * L * 0.5f;
    }
    Vec3 slots[SQUAD_MAX]; int k = Layout(g_set.shape, m, A, B, sp, slots);
    bool used[SQUAD_MAX] = { false }; int placed = 0, held = 0;
    for (int i = 0; i < m; i++)
    {
        Vec3 p; if (!PosOf(mem[i], p)) continue;
        int bs = -1; float bd = 1e30f;
        for (int s = 0; s < k; s++) { if (used[s]) continue; float dx = slots[s].x - p.x, dz = slots[s].z - p.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; bs = s; } }
        if (bs < 0) break; used[bs] = true;
        if (UT_terrainHeight) { __try { slots[bs].y = UT_terrainHeight(slots[bs].x, slots[bs].z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        if (CH_setStandingOrder && g_set.hold) { __try { CH_setStandingOrder(mem[i], STANDING_HOLD, true); held++; } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        if (GroundOrder(mem[i], &slots[bs])) placed++;
    }
    Log("advance: %s laid at the destination facing the march - %d placed, hold on for %d", SHAPE_NAMES[g_set.shape], placed, held);
}

static void AdvanceTick(bool force)
{
    if (!g_adv) return;
    unsigned long now = GetTickCount();
    if (!force && now - g_advTick < 1000) return;
    g_advTick = now;
    Vec3 lp; if (!PosOf(g_advLeader, lp)) { AdvanceStop("the leader cannot be read"); return; }
    const char* lw = CharBusyWhy(g_advLeader);
    if (lw && strcmp(lw, "under attack") != 0) { AdvanceStop("the leader is down"); return; }
    float hx = g_advTo.x - lp.x, hz = g_advTo.z - lp.z, hl = sqrtf(hx * hx + hz * hz);
    bool arrived = hl < 6.0f;
    if (hl > 2.0f) { g_advHx = hx / hl; g_advHz = hz / hl; }
    // the leader standing still for 4 s (blocked, or there) ends it too
    float mx = lp.x - g_advLastLeader.x, mz = lp.z - g_advLastLeader.z;
    if (mx * mx + mz * mz < 0.25f) { if (!g_advStillSince) g_advStillSince = now; } else g_advStillSince = 0;
    g_advLastLeader = lp;
    bool finalPass = arrived || (g_advStillSince && now - g_advStillSince > 4000 && !force);
    if (finalPass)
    {
        g_lastMarchHx = g_advHx; g_lastMarchHz = g_advHz; g_haveMarch = true; g_adv = false;
        Log("advance: %s", arrived ? "the leader arrived" : "the leader stopped");
        AdvanceLayShape(lp);
        return;
    }
    float sp = Spacing();
    int sent = 0, inPlace = 0, busy = 0;
    for (int i = 0; i < g_advN; i++)
    {
        void* ch = g_advCh[i]; if (!PtrOk(ch)) continue;
        if (CharBusyWhy(ch)) { busy++; continue; }
        Vec3 t = AdvancePlace(g_advAlong[i], g_advAcross[i], lp);
        Vec3 p; if (!PosOf(ch, p)) continue;
        float dx = p.x - t.x, dz = p.z - t.z, d = sqrtf(dx * dx + dz * dz);
        float ox = t.x - g_advLastOrder[i].x, oz = t.z - g_advLastOrder[i].z, moved = sqrtf(ox * ox + oz * oz);
        if (d <= sp * 0.6f && !finalPass) { inPlace++; continue; }
        if (finalPass && d <= 1.5f) { inPlace++; continue; }
        if (force || finalPass || moved > 1.0f) { if (GroundOrder(ch, &t)) { g_advLastOrder[i] = t; sent++; } }
    }
    if (force) Log("advance: column of %d behind the leader, first orders sent to %d (%d busy)", g_advN, sent, busy);
    if (finalPass) { Log("advance: %s - final places to %d, %d already there, %d busy", arrived ? "the leader arrived" : "the leader stopped", sent, inPlace, busy); g_adv = false; }
}
