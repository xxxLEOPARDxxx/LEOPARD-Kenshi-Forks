// RALLY POINT, MEDICS and RETREAT.
//
// Owner (boot 11): "set a rally point that everyone retreats to and medics
// could be assigned and while in a battle naturally take the wounded to the
// point." Boot 12: "medic is a default job in the game... piggy back off that
// and just add the carry function." Boot 13: "the heal command needs to come
// after the transport command."
//
//   ALT + V   the rally point: the ground under the cursor
//   ALT + X   retreat: everyone standing runs to the rally point (or, with no
//             rally point, away from the fight / back the way the last march
//             came), and whoever is standing picks up the wounded on the way
//   medics    whoever holds the game's own Medic job (the orders panel button,
//             NUM7): OrdersReceiver::hasPermajob(JOB_MEDIC 58) through
//             Character::getOrdersReciever - nothing of the plugin's to assign
//
// THE CARRY, in three steps, because of what boot 13 measured: the lift order
// (LIFT_PERSON 68) is taken by the game every time ("no problem... IS in his
// orders"), but the move to the rally point queued behind it never ran - a
// queued move is a JOB (Character::addOrder with shift routes to addJob), and
// the Medic job is a job too, so the game's own healing won and the medic
// healed where the wounded lay. So now:
//   1. the lift order alone; every two seconds the wounded is asked
//      Character::isBeingCarried() - the lift has happened when it says yes
//   2. THEN the move to the rally point as a player ORDER (the click order,
//      which outranks any job), carried person and all
//   3. at the rally point PUT_DOWN_OBJECT 69, and the medic is free - the
//      Medic job heals from there, after the transport
// The same three steps serve the retreat's carriers, who join the roster for
// that one job.
static const int TASK_LIFT_PERSON = 68, TASK_PUT_DOWN = 69, TASK_JOB_MEDIC = 58;
static Vec3  g_rally = { 0, 0, 0 }; static bool g_haveRally = false;
// 1.4 (owner, 2026-09-09, the Workshop's ask "a way to hide or clear the rally point flag"; his rule: "no new keys for clearing just make
// it the same controls and if the player clicks in the same spot as the first flag it clears it... a small radius"): ALT+V within
// RALLY_CLEAR_RADIUS of the standing point CLEARS it - the medics' "already at the point" radius, so the two agree on what "at" means.
static const float RALLY_CLEAR_RADIUS = 25.0f;
static void FlagHide(); static bool BannerShow(const Vec3& at); static void BannerDestroy();   // Flag.h, included at the end of this file
static void* g_medic[SQUAD_MAX]; static int g_medics = 0;
static void* g_medicJob[SQUAD_MAX];                        // the wounded this one is fetching (0 = free)
static int   g_medicPhase[SQUAD_MAX];                      // 0 free, 1 lift ordered, 2 carrying to the point, 3 put down
static bool  g_medicTemp[SQUAD_MAX];                       // a retreat's carrier: on the roster only for this job
static Vec3  g_medicTo[SQUAD_MAX];                         // where this job carries to
static unsigned long g_medicSince[SQUAD_MAX];
static unsigned long g_medicTick = 0;
// the delivered: a wounded put down at the point is not fetched again while he lies there (boot 14: lift-carry-drop-lift loop)
static void* g_delivered[SQUAD_MAX]; static unsigned long g_deliveredAt[SQUAD_MAX]; static int g_deliveredN = 0;
static bool Delivered(void* w, unsigned long now) { for (int i = 0; i < g_deliveredN; i++) if (g_delivered[i] == w) return now - g_deliveredAt[i] < 180000; return false; }
static void MarkDelivered(void* w, unsigned long now) { for (int i = 0; i < g_deliveredN; i++) if (g_delivered[i] == w) { g_deliveredAt[i] = now; return; } if (g_deliveredN < SQUAD_MAX) { g_delivered[g_deliveredN] = w; g_deliveredAt[g_deliveredN] = now; g_deliveredN++; } }
typedef void* (*CH_getRecv_t)(void* ch);
typedef bool  (*OR_hasTask_t)(void* recv, int task);
typedef bool  (*CH_checkOrder_t)(void* ch, int task, void* obj);
typedef bool  (*CH_boolq_t)(void* ch);
static CH_getRecv_t    CH_getOrdersReciever = 0;
static OR_hasTask_t    OR_hasPermajob = 0, OR_hasPlayerOrder = 0;
static CH_checkOrder_t CH_checkPlayerOrder = 0;
static CH_boolq_t      CH_isBeingCarried = 0;
static bool g_retreatInit = false;

static void RetreatInit()
{
    if (g_retreatInit) return; g_retreatInit = true;
    HMODULE kl = GetModuleHandleA("KenshiLib.dll"); if (!kl) return;
    CH_getOrdersReciever = (CH_getRecv_t)Real(GetProcAddress(kl, "?getOrdersReciever@Character@@QEBAPEAVOrdersReceiver@@XZ"));
    OR_hasPermajob = (OR_hasTask_t)Real(GetProcAddress(kl, "?hasPermajob@OrdersReceiver@@QEBA_NW4TaskType@@@Z"));
    OR_hasPlayerOrder = (OR_hasTask_t)Real(GetProcAddress(kl, "?hasPlayerOrder@OrdersReceiver@@QEBA_NW4TaskType@@@Z"));
    CH_checkPlayerOrder = (CH_checkOrder_t)Real(GetProcAddress(kl, "?checkPlayerOrderForProblems@Character@@QEAA_NW4TaskType@@PEAVRootObject@@@Z"));
    CH_isBeingCarried = (CH_boolq_t)Real(GetProcAddress(kl, "?isBeingCarried@Character@@QEBA_NXZ"));
    LogAvailability(CH_getOrdersReciever && OR_hasPermajob, "Medic job detection", "automatic rally rescues are unavailable");
    LogAvailability(CH_isBeingCarried != nullptr, "Carried character detection", "movement follows the lift after a delay");
}

static void ClearRally(const char* why)
{
    g_haveRally = false; FlagHide(); BannerDestroy();
    Log("rally: CLEARED (%s) - the retreat runs away from the fight again, the medics heal where the wounded lie", why);
}
static void SetRally()
{
    Vec3 p;
    if (!CursorToGround(p)) { Log("rally: no ground under the cursor"); return; }
    if (g_haveRally)
    {   // the same key on the standing point clears it (owner, 1.4)
        float dx = p.x - g_rally.x, dz = p.z - g_rally.z;
        if (dx * dx + dz * dz <= RALLY_CLEAR_RADIUS * RALLY_CLEAR_RADIUS) { ClearRally("ALT+V on the flag"); return; }
    }
    // 1.4.1: the point is the SURFACE the cursor is on - a roof, a wall top, the ground - from the ghost building's own trace; the terrain
    // march above is the fallback and the check on space (a hit far from the march's x,z is in the game's shifted space: the shift is added)
    Vec3 s; const char* what = 0; const char* placed = "the terrain march";
    if (CursorToSurface(s, DebugEnabled() ? &what : nullptr))
    {
        float dx = s.x - p.x, dz = s.z - p.z;
        if (dx * dx + dz * dz > 500.0f * 500.0f) { s.x += g_lastShift.x; s.y += g_lastShift.y; s.z += g_lastShift.z; dx = s.x - p.x; dz = s.z - p.z; placed = "the game's trace, shifted into world space"; }
        else placed = "the game's trace";
        if (dx * dx + dz * dz <= 500.0f * 500.0f) p = s; else { placed = "the terrain march (the trace's point was nowhere near, even shifted)"; }
    }
    g_rally = p; g_haveRally = true;
    bool marker = BannerShow(p);
    Log("rally: set at %.0f, %.0f, %.0f on %s (%s; %d hold the Medic job)%s", p.x, p.y, p.z, what ? what : "the ground", placed, g_medics, marker ? " - the marker stands there while ALT is held" : " - no marker (the overlay flag while ALT is held)");
}

// every player character, not only the selected ones
static int ReadAllPlayers(void** out, int cap)
{
    int k = 0;
    if (!g_playerIface || !PI_getAllPlayerCharacters) return 0;
    __try
    {
        const unsigned char* lek = (const unsigned char*)PI_getAllPlayerCharacters(g_playerIface);
        int count = 0; void** stuff = 0;
        if (!CopyGuarded(&count, lek + 8, sizeof(count)) || !CopyGuarded(&stuff, lek + 16, sizeof(stuff))) return 0;
        if (count < 0 || count > 4096 || !stuff) return 0;
        for (int i = 0; i < count && k < cap; i++)
        {
            void* ch = 0;
            if (!CopyGuarded(&ch, stuff + i, sizeof(ch)) || !PtrOk(ch)) continue;
            out[k++] = ch;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return k;
}

static bool HasTask(void* ch, OR_hasTask_t q, int task)
{
    if (!q || !CH_getOrdersReciever) return false;
    __try { void* r = CH_getOrdersReciever(ch); return PtrOk(r) && q(r, task); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool Carried(void* wounded)
{
    if (!CH_isBeingCarried || !PtrOk(wounded)) return false;
    __try { return CH_isBeingCarried(wounded); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static int MedicIndex(void* ch) { for (int i = 0; i < g_medics; i++) if (g_medic[i] == ch) return i; return -1; }
static void RosterDrop(int m) { g_medics--; g_medic[m] = g_medic[g_medics]; g_medicJob[m] = g_medicJob[g_medics]; g_medicPhase[m] = g_medicPhase[g_medics]; g_medicTemp[m] = g_medicTemp[g_medics]; g_medicTo[m] = g_medicTo[g_medics]; g_medicSince[m] = g_medicSince[g_medics]; }
static int RosterAdd(void* ch, bool temp)
{
    if (g_medics >= SQUAD_MAX) return -1;
    int m = g_medics++; g_medic[m] = ch; g_medicJob[m] = 0; g_medicPhase[m] = 0; g_medicTemp[m] = temp; g_medicTo[m] = g_rally; g_medicSince[m] = 0;
    return m;
}

// the roster follows the game's Medic job: new holders join, lapsed ones leave; a retreat's carrier leaves when his job is done
static void SyncMedics(void** all, int n)
{
    for (int i = 0; i < n; i++)
    {
        bool is = HasTask(all[i], OR_hasPermajob, TASK_JOB_MEDIC);
        int m = MedicIndex(all[i]);
        if (is && m < 0) { if (RosterAdd(all[i], false) >= 0) Log("medic: a character holds the Medic job - %d on the roster", g_medics); }
        else if (is && m >= 0 && g_medicTemp[m]) g_medicTemp[m] = false;
        else if (!is && m >= 0 && !g_medicTemp[m]) { RosterDrop(m); Log("medic: a Medic job was dropped - %d on the roster", g_medics); }
    }
    for (int m = g_medics - 1; m >= 0; m--) if (g_medicTemp[m] && g_medicPhase[m] == 0) RosterDrop(m);
}

static bool IsWounded(void* ch) { const char* w = CharBusyWhy(ch); return w && (strcmp(w, "down") == 0 || strcmp(w, "unconscious") == 0); }
static bool BeingFetched(void* wounded) { for (int i = 0; i < g_medics; i++) if (g_medicJob[i] == wounded) return true; return false; }

static bool ObjectOrder(void* ch, int task, void* obj, bool queue, const Vec3* pos)
{
    if (!CH_addOrder) return false;
    __try { CH_addOrder(ch, 0, task, obj, queue, true, pos); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
static void ReleaseHold(void* ch)
{
    if (!CH_setStandingOrder) return;
    __try { CH_setStandingOrder(ch, STANDING_HOLD, false); } __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// step 1 for one carrier: the lift order on one wounded, to be carried to `to`
static bool StartCarry(int m, void* wounded, const Vec3& wpos, const Vec3& to, const char* who)
{
    void* med = g_medic[m];
    ReleaseHold(med);
    bool problem = false;
    if (CH_checkPlayerOrder) { __try { problem = CH_checkPlayerOrder(med, TASK_LIFT_PERSON, wounded); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    if (!ObjectOrder(med, TASK_LIFT_PERSON, wounded, false, &wpos)) { ErrorLog("%s %d: the lift order could not be placed", who, m); return false; }
    const bool debug = DebugEnabled();
    const bool took = debug && HasTask(med, OR_hasPlayerOrder, TASK_LIFT_PERSON);
    g_medicJob[m] = wounded; g_medicPhase[m] = 1; g_medicTo[m] = to; g_medicSince[m] = GetTickCount();
    if (debug) Log("%s %d: lift ordered%s%s", who, m, problem ? " - the game reports a problem" : "", took ? "" : " - NOT in his orders");
    return true;
}

// steps 2 and 3 for everyone on the roster with a job
static void CarryTick(unsigned long now)
{
    for (int m = 0; m < g_medics; m++)
    {
        if (g_medicPhase[m] == 0) continue;
        void* med = g_medic[m]; void* w = g_medicJob[m];
        if (!PtrOk(med)) { g_medicPhase[m] = 0; g_medicJob[m] = 0; continue; }
        const char* why = CharBusyWhy(med);
        if (why && strcmp(why, "under attack") != 0) { Log("medic %d: %s - his job is dropped", m, why); g_medicPhase[m] = 0; g_medicJob[m] = 0; continue; }
        if (g_medicPhase[m] == 1)
        {
            if (!PtrOk(w) || (!IsWounded(w) && !Carried(w))) { Log("medic %d: the wounded is up (or gone) - free", m); g_medicPhase[m] = 0; g_medicJob[m] = 0; continue; }
            bool lifted = Carried(w) || (!CH_isBeingCarried && now - g_medicSince[m] > 6000);
            if (lifted)
            {
                GroundOrder(med, &g_medicTo[m]);                 // the move as a player ORDER: it outranks the Medic job
                g_medicPhase[m] = 2; g_medicSince[m] = now;
                Log("medic %d: lifted - carrying to %.0f, %.0f (a player order, ahead of the Medic job)", m, g_medicTo[m].x, g_medicTo[m].z);
            }
            else if (now - g_medicSince[m] > 45000) { Log("medic %d: 45 s and no lift - giving up this one", m); g_medicPhase[m] = 0; g_medicJob[m] = 0; }
            continue;
        }
        if (g_medicPhase[m] == 2)
        {
            Vec3 mp; if (!PosOf(med, mp)) continue;
            float rx = mp.x - g_medicTo[m].x, rz = mp.z - g_medicTo[m].z;
            if (rx * rx + rz * rz < 8.0f * 8.0f)
            {
                ObjectOrder(med, TASK_PUT_DOWN, w, false, &g_medicTo[m]);
                MarkDelivered(w, now);
                g_medicPhase[m] = 3; g_medicSince[m] = now;
                Log("medic %d: at the point - putting the wounded down; the Medic job heals from here", m);
            }
            else if (!Carried(w) && CH_isBeingCarried && now - g_medicSince[m] > 4000) { Log("medic %d: the wounded is no longer carried on the way - free", m); g_medicPhase[m] = 0; g_medicJob[m] = 0; }
            else if (now - g_medicSince[m] > 120000) { Log("medic %d: 120 s carrying and not there - giving up", m); g_medicPhase[m] = 0; g_medicJob[m] = 0; }
            continue;
        }
        if (g_medicPhase[m] == 3 && now - g_medicSince[m] > 4000) { g_medicPhase[m] = 0; g_medicJob[m] = 0; }
    }
}

static void MedicTick()
{
    RetreatInit();
    unsigned long now = GetTickCount();
    if (now - g_medicTick < 2000) return;
    g_medicTick = now;
    void* all[256]; int n = ReadAllPlayers(all, 256);
    SyncMedics(all, n);
    CarryTick(now);
    if (!g_haveRally) return;
    // free medics fetch the nearest wounded not already being fetched and not already at the point
    for (int m = 0; m < g_medics; m++)
    {
        if (g_medicPhase[m] || g_medicTemp[m]) continue;
        void* med = g_medic[m]; if (!PtrOk(med) || CharBusyWhy(med)) continue;
        Vec3 mp; if (!PosOf(med, mp)) continue;
        void* best = 0; float bd = 1e30f; Vec3 bp = { 0, 0, 0 };
        for (int i = 0; i < n; i++)
        {
            void* ch = all[i]; if (ch == med || !IsWounded(ch) || BeingFetched(ch) || Carried(ch) || Delivered(ch, now)) continue;
            Vec3 p; if (!PosOf(ch, p)) continue;
            float wx = p.x - g_rally.x, wz = p.z - g_rally.z; if (wx * wx + wz * wz < 25.0f * 25.0f) continue;   // already at the point
            float dx = p.x - mp.x, dz = p.z - mp.z, d = dx * dx + dz * dz;
            if (d < bd) { bd = d; best = ch; bp = p; }
        }
        if (best) StartCarry(m, best, bp, g_rally, "medic");
    }
}

static void Retreat()
{
    RetreatInit();
    int n = ReadSelection();
    if (n == 0) { Log("retreat: nothing selected"); return; }
    AdvanceStop("a retreat");
    int kind[SQUAD_MAX]; Vec3 pos[SQUAD_MAX];             // 0 up, 1 under attack (up), 2 wounded, 3 dead/unreadable, 4 being carried / fetched
    Vec3 cAll = { 0, 0, 0 }, cAtt = { 0, 0, 0 }; int nAll = 0, nAtt = 0, nWounded = 0;
    for (int i = 0; i < n; i++)
    {
        const char* why = CharBusyWhy(g_members[i]);
        kind[i] = !why ? 0 : strcmp(why, "under attack") == 0 ? 1 : (strcmp(why, "down") == 0 || strcmp(why, "unconscious") == 0) ? 2 : 3;
        if (!PosOf(g_members[i], pos[i])) { kind[i] = 3; continue; }
        if (kind[i] == 2) { nWounded++; if (BeingFetched(g_members[i]) || Carried(g_members[i])) kind[i] = 4; continue; }
        if (kind[i] == 3) continue;
        cAll.x += pos[i].x; cAll.z += pos[i].z; nAll++;
        if (kind[i] == 1) { cAtt.x += pos[i].x; cAtt.z += pos[i].z; nAtt++; }
    }
    if (nAll == 0) { Log("retreat: nobody standing"); return; }
    cAll.x /= nAll; cAll.z /= nAll;
    Vec3 to; const char* how;
    if (g_haveRally) { to = g_rally; how = "to the rally point"; }
    else
    {
        float dx = 0, dz = 0;
        if (nAtt > 0 && nAtt < nAll) { cAtt.x /= nAtt; cAtt.z /= nAtt; dx = cAll.x - cAtt.x; dz = cAll.z - cAtt.z; }
        float dl = sqrtf(dx * dx + dz * dz);
        how = "away from the fight";
        if (dl < 1.0f)
        {
            if (!g_haveMarch) { Log("retreat: no rally point (ALT+V), no enemy direction (%d under attack of %d) and no march to fall back along", nAtt, nAll); return; }
            dx = -g_lastMarchHx; dz = -g_lastMarchHz; dl = 1.0f; how = "back the way the march came";
        }
        dx /= dl; dz /= dl;
        to.x = cAll.x + dx * g_set.retreat; to.y = 0; to.z = cAll.z + dz * g_set.retreat;
        if (UT_terrainHeight) { __try { to.y = UT_terrainHeight(to.x, to.z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    }
    // Hold off for everyone standing: a retreat is the one order a held character must not come back from
    for (int i = 0; i < n; i++) if (kind[i] == 0 || kind[i] == 1) ReleaseHold(g_members[i]);
    // carriers: each wounded not already being fetched gets the nearest free standing member (those not under attack first);
    // they join the roster for this one job and go through the same lift -> carry -> put down
    bool taken[SQUAD_MAX] = { false }; int carriers = 0, runners = 0;
    for (int i = 0; i < n; i++) { int m = MedicIndex(g_members[i]); if (m >= 0 && g_medicPhase[m]) taken[i] = true; }
    for (int pass = 0; pass < 2; pass++)
        for (int w = 0; w < n; w++)
        {
            if (kind[w] != 2) continue;
            int best = -1; float bd = 1e30f;
            for (int i = 0; i < n; i++)
            {
                if (taken[i] || kind[i] != pass) continue;
                float ex = pos[i].x - pos[w].x, ez = pos[i].z - pos[w].z, d = ex * ex + ez * ez;
                if (d < bd) { bd = d; best = i; }
            }
            if (best < 0) continue;
            taken[best] = true; kind[w] = 4;
            int m = MedicIndex(g_members[best]); if (m < 0) m = RosterAdd(g_members[best], true);
            if (m >= 0 && StartCarry(m, g_members[w], pos[w], to, "carrier")) carriers++;
            else if (GroundOrder(g_members[best], &to)) runners++;
        }
    for (int i = 0; i < n; i++)
    {
        if (taken[i] || kind[i] >= 2) continue;
        if (GroundOrder(g_members[i], &to)) runners++;
    }
    int left = 0; for (int w = 0; w < n; w++) if (kind[w] == 2) left++;
    Log("retreat: %s %.0f, %.0f - %d run, %d carry the wounded, %d wounded, %d left: nobody free", how, to.x, to.z, runners, carriers, nWounded, left);
}

#include "Flag.h"
