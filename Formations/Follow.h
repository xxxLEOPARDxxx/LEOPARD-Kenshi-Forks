// FOLLOW FORMATIONS (17.10.2026) - the shapes a squad keeps while it follows its leader.
//
// HOW THE GAME DOES IT. Blackboard::buildFormation (one Blackboard per squad) lays the followers out:
// each gets an offset from the leader, kept in Blackboard::characterFormationPosition, and the movement
// asks for it through Blackboard::getFormationPositionOffset(Character*). The shape is the faction's
// idata["squad formation"]: 0 Random, 1 Military, 2 Caravan (anything else - buildFormation lays nobody
// out). The AI tab's "Follow Formation" drop-down edits that int in place (ManagementScreen::refreshAI ->
// DatapanelGUI::setLineDropBox(label, 0, &idata[...]), then addAValue x3).
//
// MEASURED (stage 1 log, the owner's squad of 30): offsets are in the LEADER'S frame - x = distance
// behind him, z = to the side - and do not change as he turns. Military: rows of six, 22 apart, the
// first row 31 behind (31/53/75/97/119; -55..55 across). Caravan: one file, 43..780 behind, z = 0. The
// leader asks for no offset.
//
// WHAT WE DO - the owner: "very carefully, so it does not touch the AI logic, pathfinding and the
// rest". Only that one number changes: for a player squad with one of our shapes the hook hands out our
// offset instead of the game's. No orders, no movement of our own - the game's follow does the walking,
// turning and pathfinding exactly as for Military.
//
//  - The offsets are computed on the main thread (the main-loop hook, twice a second) into a table; the
//    hook (which the movement may call from anywhere) only reads the table, never game data.
//  - Our choice - the default shape and a shape per squad name - lives in our memory and travels with
//    the save as Formations.save in the save's folder (SaveFileSystem: written into the save's temp
//    folder before the copy is queued, read back after a load, cleared on a new game - KEO's sidecar
//    recipe). First kept in the faction's idata: the owner, 17.10.2026 - "I loaded another save and
//    the same squad there got the formation": the game saves only picked fields of the faction (the
//    shape itself as "formation" in the player record), while its data lives on in memory between
//    loads, so our keys leaked from one save into the next and were never written.
//    The game's own "squad formation" stays 0..2 (our shape -> 1, Military) - a save played without
//    the plugin falls back to the game's shapes instead of laying nobody out.
//  - The AI tab's drop-down is pointed at our default and gets our shapes after the game's three.

// ------------------------------------------------------------------ the shapes
enum { FS_RANDOM = 0, FS_MILITARY, FS_CARAVAN, FS_WEDGE, FS_LINE, FS_RING, FS_RHOMBUS, FS_SQUARE,
       FS_COLUMN2, FS_ECHELON, FS_PINCERS, FS_DIAMOND, FS_MOTH, FS_CHEVRON, FS_COUNT };
// FS_ECHELON stays a number (a save may hold it) but is no longer offered: the owner, 17.10.2026 -
// "Echelon: remove, not needed". Ids are never renumbered - Formations.save keeps them.
static bool FollowOffered(int s) { return s != FS_ECHELON; }
static const float FOLLOW_STEP = 22.0f;   // the game's own spacing in Military

// A shape drawn as a picture (the owner's screenshots of Moth and Chevron): the points of one layer,
// (side, behind) in steps, the leader at 0,0. More followers than points - the next layer is the same
// picture drawn wider around the last.
struct FollowPt { float z, x; };
static Vec3 FollowTemplate(const FollowPt* pts, int count, int i)
{
    int layer = i / count, j = i % count;
    float k = (1.0f + 0.9f * layer) * FOLLOW_STEP;
    Vec3 o = { pts[j].x * k, 0, pts[j].z * k };
    return o;
}
// Moth: two fore wings swept back from beside his head, two smaller hind wings close behind him
static const FollowPt MOTH_PTS[] = { { -1.15f, -0.9f }, { 1.15f, -0.9f }, { -2.1f, 0.0f }, { 2.1f, 0.0f },
                                     { -1.0f, 1.3f }, { 1.0f, 1.3f }, { -0.5f, 1.75f }, { 0.5f, 1.75f } };
// Chevron: a wide arch over and around him (no point at its apex), a small arrowhead trailing behind
static const FollowPt CHEVRON_PTS[] = { { -1.1f, -1.2f }, { 1.1f, -1.2f }, { -2.0f, -0.4f }, { 2.0f, -0.4f },
                                        { -2.4f, 0.7f }, { 2.4f, 0.7f }, { 0.0f, 1.7f }, { -0.6f, 2.4f }, { 0.6f, 2.4f } };

static float FollowHash(int i, int salt)
{
    unsigned int h = (unsigned int)i * 2654435761u ^ (unsigned int)salt * 40503u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (h & 0xFFFF) / 65535.0f;
}

// offset of follower i of n in shape s: x = behind the leader, z = to the side
static Vec3 FollowOffset(int s, int i, int n)
{
    const float S = FOLLOW_STEP;
    Vec3 o = { 0, 0, 0 };
    switch (s)
    {
    case FS_RANDOM:                                         // a loose crowd behind him
        o.x = 25.0f + FollowHash(i, 1) * 90.0f; o.z = (FollowHash(i, 2) - 0.5f) * 130.0f; break;
    case FS_MILITARY:                                       // the game's: rows of six from 31 behind
    {
        int row = i / 6, col = i % 6, inRow = n - row * 6; if (inRow > 6) inRow = 6;
        o.x = 31.0f + row * S; o.z = (col - (inRow - 1) * 0.5f) * S; break;
    }
    case FS_CARAVAN:                                        // the game's: one file
        o.x = 43.0f + i * 25.0f; break;
    case FS_WEDGE:                                          // a filled triangle, the leader at the tip
    {
        int r = 1, first = 0;
        while (first + r + 1 <= i) { first += r + 1; r++; }
        int c = i - first, inRow = r + 1, left = n - first; if (left < inRow) inRow = left;
        o.x = r * S; o.z = (c - (inRow - 1) * 0.5f) * S; break;
    }
    case FS_LINE:                                           // abreast with him
    {
        int k = i / 2 + 1; o.x = 0; o.z = (i % 2 ? k : -k) * S; break;
    }
    case FS_RING:                                           // a ring around him
    {
        float R = n * S / 6.2832f; if (R < 2.0f * S) R = 2.0f * S;
        float a = 6.2832f * i / (n > 0 ? n : 1);
        o.x = R * cosf(a); o.z = R * sinf(a); break;
    }
    case FS_RHOMBUS:                                        // a filled rhombus, the leader at the front tip
    {
        int k = 1; while (2 * k * k + 2 * k + 1 < n + 1) k++;
        // the lattice points |dx|+|dz| <= k, front to back, centre outwards; the tip is the leader's
        int idx = 0;
        for (int dx = -k; dx <= k; dx++)
        {
            int w = k - (dx < 0 ? -dx : dx);
            for (int a = 0; a <= w; a++)
                for (int sgn = 0; sgn < (a ? 2 : 1); sgn++)
                {
                    if (dx == -k) continue;                 // the tip
                    if (idx == i) { o.x = (dx + k) * S; o.z = (sgn ? -a : a) * S; return o; }
                    idx++;
                }
        }
        break;
    }
    case FS_SQUARE:                                         // a hollow square around him
    {
        int m = 3; while (4 * (m - 1) < n) m++;
        int per = 4 * (m - 1), p = (int)((float)i * per / (n > 0 ? n : 1));
        float h = (m - 1) * 0.5f, side = (float)(m - 1);
        int edge = p / (m - 1); float t = (float)(p % (m - 1));
        float x, z;
        if (edge == 0) { x = -h; z = -h + t; }              // front edge
        else if (edge == 1) { x = -h + t; z = h; }
        else if (edge == 2) { x = h; z = h - t; }           // back edge
        else { x = h - t; z = -h; }
        (void)side;
        o.x = x * S; o.z = z * S; break;
    }
    case FS_COLUMN2:                                        // two files behind him
        o.x = (i / 2 + 1) * S; o.z = (i % 2 ? 0.5f : -0.5f) * S; break;
    case FS_ECHELON:                                        // a diagonal back and to the side
        o.x = (i + 1) * S * 0.7f; o.z = (i + 1) * S * 0.7f; break;
    case FS_PINCERS:                                        // a hollow V opening backwards
    {
        int r = i / 2 + 1; o.x = r * S; o.z = (i % 2 ? r : -r) * S * 0.9f; break;
    }
    case FS_MOTH:
        return FollowTemplate(MOTH_PTS, sizeof(MOTH_PTS) / sizeof(MOTH_PTS[0]), i);
    case FS_CHEVRON:
        return FollowTemplate(CHEVRON_PTS, sizeof(CHEVRON_PTS) / sizeof(CHEVRON_PTS[0]), i);
    case FS_DIAMOND:                                        // the leader in the middle: a ring of eight, four tips, wider rings
    {
        if (i < 8) { float a = 0.3927f + 0.7854f * i; o.x = 1.6f * S * cosf(a); o.z = 1.6f * S * sinf(a); }
        else if (i < 12) { float a = 1.5708f * (i - 8); o.x = 3.2f * S * cosf(a); o.z = 3.2f * S * sinf(a); }
        else
        {
            int j = i - 12, ring = 0, cap = 12;
            while (j >= cap) { j -= cap; ring++; cap += 4; }
            float R = (4.5f + 1.4f * ring) * S, a = 6.2832f * j / cap + 0.2618f;
            o.x = R * cosf(a); o.z = R * sinf(a);
        }
        break;
    }
    }
    return o;
}

// ------------------------------------------------------------------ game access
typedef const Vec3* (*BB_offset_t)(void* bb, void* ch);
static BB_offset_t orig_bbOffset = 0;
typedef void* (*CH_getPlatoon_t)(void* ch);
static CH_getPlatoon_t CH_getPlatoon = 0;
typedef const void* (*AP_getName_t)(void* ap);
static AP_getName_t AP_getName = 0;
typedef unsigned char* (*IdataIndex_t)(void* map, const void* key);    // boost map operator[]: the node, value at +0x28
static IdataIndex_t GD_idataIndex = 0;
typedef void* (*DP_setLineDropBox_t)(void* panel, const void* label, int cat, int* value, bool b, float f);
static DP_setLineDropBox_t orig_setLineDropBox = 0;
typedef void (*DB_addAValue_t)(void* line, const void* text, int value);
static DB_addAValue_t orig_addAValue = 0;

// a std::string the way the game (VC10, release) lays it out: 16-byte buffer or pointer, size, capacity
struct GStr { union { char buf[16]; char* ptr; }; size_t size; size_t cap; };
static void GStrSet(GStr& s, const char* text, char* store, size_t storeSize)
{
    size_t n = strlen(text);
    if (n < 16) { memcpy(s.buf, text, n + 1); s.cap = 15; }
    else { strncpy_s(store, storeSize, text, _TRUNCATE); n = strlen(store); s.ptr = store; s.cap = n; }
    s.size = n;
}
static bool GStrRead(const void* p, char* out, size_t outSize)
{
    GStr s; if (!CopyGuarded(&s, p, sizeof(s)) || s.size > 200) return false;
    const char* src = s.cap >= 16 ? s.ptr : (const char*)p;
    if (s.cap >= 16 && !PtrOk(src)) return false;
    if (s.size + 1 > outSize) return false;
    if (!CopyGuarded(out, src, s.size)) return false;
    out[s.size] = 0;
    return true;
}

// idata[key] of a GameData - main thread only (it inserts a missing key, as the game's own AI tab does)
static int* IdataRef(void* gameData, const char* key)
{
    if (!GD_idataIndex || !PtrOk(gameData)) return 0;
    GStr k; char store[160];
    GStrSet(k, key, store, sizeof(store));
    unsigned char* node = 0;
    __try { node = GD_idataIndex((unsigned char*)gameData + 0x178, &k); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return PtrOk(node) ? (int*)(node + 0x28) : 0;
}

// Platoon -> its faction's GameData: the same virtual (Platoon vtable +0x58) buildFormation calls
static void* PlatoonFactionData(void* platoon)
{
    void* data = 0;
    __try
    {
        if (!PtrOk(platoon)) return 0;
        void** vt = *(void***)platoon; if (!PtrOk(vt)) return 0;
        typedef void* (*fac_t)(void*);
        void* fac = ((fac_t)vt[0x58 / 8])(platoon);
        if (!PtrOk(fac)) return 0;
        data = *(void**)((unsigned char*)fac + 0x240);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return PtrOk(data) ? data : 0;
}

// ------------------------------------------------------------------ the table the hook reads
struct FollowSlot { void* ch; Vec3 off; };
struct FollowTable { int n; FollowSlot slot[SQUAD_MAX * 4]; };
static FollowTable g_followTab[2];
static volatile LONG g_followActive = 0;            // which table the hook reads
static void* g_playerFactionData = 0;               // main thread

// Our choices (main thread: the tick, the AI tab, MCM and the save/load hooks all run there).
static int g_followDefault = -1;                    // -1: not chosen yet - the game's own value is taken; the AI tab's drop-down points here
struct FollowPick { char name[160]; int shape; };
static FollowPick g_followPicks[128];
static int g_followPickN = 0;

static int FollowDefault() { return (g_followDefault >= 0 && g_followDefault < FS_COUNT) ? g_followDefault : FS_MILITARY; }
static int FollowPickGet(const char* name)
{
    for (int i = 0; i < g_followPickN; i++) if (strcmp(g_followPicks[i].name, name) == 0) return g_followPicks[i].shape;
    return -1;
}
static void FollowPickSet(const char* name, int shape)     // -1 - back to the default
{
    int i = 0; while (i < g_followPickN && strcmp(g_followPicks[i].name, name) != 0) i++;
    if (shape < 0 || shape >= FS_COUNT)
    {
        if (i < g_followPickN) { g_followPicks[i] = g_followPicks[g_followPickN - 1]; g_followPickN--; }
        return;
    }
    if (i == g_followPickN) { if (g_followPickN >= 128) return; g_followPickN++; strncpy_s(g_followPicks[i].name, sizeof(g_followPicks[i].name), name, _TRUNCATE); }
    g_followPicks[i].shape = shape;
}

// the follower list of an ActivePlatoon: lektor at +0x50 (count +0x58, data +0x60), leader at +0xA0
static int SquadFollowers(void* ap, void** out, int max)
{
    int n = 0, count = 0; void** data = 0; void* leader = 0;
    if (!CopyGuarded(&count, (unsigned char*)ap + 0x58, sizeof(count)) || !CopyGuarded(&data, (unsigned char*)ap + 0x60, sizeof(data))) return 0;
    CopyGuarded(&leader, (unsigned char*)ap + 0xA0, sizeof(leader));
    if (count <= 0 || count > 512 || !PtrOk(data)) return 0;
    for (int i = 0; i < count && n < max; i++)
    {
        void* c = 0;
        if (!CopyGuarded(&c, data + i, sizeof(c)) || !PtrOk(c) || c == leader) continue;
        out[n++] = c;
    }
    return n;
}

// the player's squads, from the player's characters (main thread)
static int FollowSquads(void** squads, int max)
{
    int ns = 0;
    if (!g_playerIface || !PI_getAllPlayerCharacters || !CH_getPlatoon) return 0;
    __try
    {
        const unsigned char* lek = (const unsigned char*)PI_getAllPlayerCharacters(g_playerIface);
        int count = 0; void** stuff = 0;
        if (!CopyGuarded(&count, lek + 8, sizeof(count)) || !CopyGuarded(&stuff, lek + 16, sizeof(stuff)) || count <= 0 || count > 4096 || !stuff) return 0;
        for (int i = 0; i < count; i++)
        {
            void* c = 0; if (!CopyGuarded(&c, stuff + i, sizeof(c)) || !PtrOk(c)) continue;
            void* ap = CH_getPlatoon(c); if (!PtrOk(ap)) continue;
            int k = 0; while (k < ns && squads[k] != ap) k++;
            if (k == ns && ns < max) squads[ns++] = ap;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return ns;
}

static unsigned long g_followTickAt = 0;
static int g_followLastDefault = -1;
static void FollowTick()
{
    unsigned long now = GetTickCount();
    if (now - g_followTickAt < 500) return;
    g_followTickAt = now;
    // Review 17.10.2026: nothing of ours in use (the game's shape for all, no squad picks) - no walk over
    // the player's characters at all; the table is emptied once so the hook passes everything through.
    if (g_followDefault >= 0 && g_followDefault <= FS_CARAVAN && g_followPickN == 0 && g_followLastDefault == g_followDefault)
    {
        if (g_followTab[g_followActive].n) { LONG back = 1 - g_followActive; g_followTab[back].n = 0; InterlockedExchange(&g_followActive, back); }
        return;
    }
    void* squads[64]; int ns = FollowSquads(squads, 64);
    if (!ns) return;

    // the player faction's data and our keys in it
    void* platoon = 0; CopyGuarded(&platoon, (unsigned char*)squads[0] + 0x78, sizeof(platoon));
    void* fd = PlatoonFactionData(platoon);
    if (!fd) return;
    if (fd != g_playerFactionData) { g_playerFactionData = fd; g_followLastDefault = -1; }
    if (g_followDefault < 0)
    {
        // nothing chosen in this save yet (no Formations.save): the game's own shape is the default
        int* game = IdataRef(fd, "squad formation");
        g_followDefault = (game && *game >= 0 && *game <= FS_CARAVAN) ? *game : FS_MILITARY;
        Log("follow: default shape taken from the game - %d", g_followDefault);
        g_followLastDefault = -1;
    }
    int def = FollowDefault();
    if (def != g_followLastDefault)
    {
        // the game's own value follows ours where it can; our shape -> Military, the game's fallback
        int* game = IdataRef(fd, "squad formation");
        if (game) { int want = def <= FS_CARAVAN ? def : FS_MILITARY; if (*game != want) { *game = want; } }
        Log("follow: default shape %d", def);
        g_followLastDefault = def;
    }

    // build the inactive table
    LONG back = 1 - g_followActive;
    FollowTable& t = g_followTab[back];
    t.n = 0;
    for (int k = 0; k < ns; k++)
    {
        char name[160]; int shape = def; bool own = false;
        __try { if (AP_getName && GStrRead(AP_getName(squads[k]), name, sizeof(name))) { int p = FollowPickGet(name); if (p >= 0) { shape = p; own = true; } } }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
        // the game's shape with nothing of ours: the game's own offsets stand
        if (!own && shape <= FS_CARAVAN) continue;
        void* f[SQUAD_MAX * 4]; int n = SquadFollowers(squads[k], f, SQUAD_MAX * 4);
        for (int i = 0; i < n && t.n < SQUAD_MAX * 4; i++) { t.slot[t.n].ch = f[i]; t.slot[t.n].off = FollowOffset(shape, i, n); t.n++; }
    }
    // sorted by character, so the hook (called for every follower in the world, NPCs included) finds
    // one in a few steps instead of reading the whole table
    for (int a = 1; a < t.n; a++)
        for (int b = a; b > 0 && (UINT_PTR)t.slot[b].ch < (UINT_PTR)t.slot[b - 1].ch; b--)
        { FollowSlot x = t.slot[b]; t.slot[b] = t.slot[b - 1]; t.slot[b - 1] = x; }
    InterlockedExchange(&g_followActive, back);
}

static const Vec3* hook_bbOffset(void* bb, void* ch)
{
    const FollowTable& t = g_followTab[g_followActive];
    int lo = 0, hi = t.n - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) >> 1;
        UINT_PTR m = (UINT_PTR)t.slot[mid].ch, c = (UINT_PTR)ch;
        if (m == c) return &t.slot[mid].off;
        if (m < c) lo = mid + 1; else hi = mid - 1;
    }
    return orig_bbOffset ? orig_bbOffset(bb, ch) : 0;
}

// ------------------------------------------------------------------ the AI tab's drop-down
static void* g_followDropBox = 0;
static int g_followLang = 0;
static const char* const FOLLOW_NAMES[FS_COUNT][3] = {
    { "Random", "Случайным", "随机" }, { "Military", "Военным", "军队" }, { "Caravan", "Караванным", "商队" },
    { "Wedge", "Клином", "楔形" }, { "Line", "Линией", "横队" }, { "Ring", "Кольцом", "环形" },
    { "Rhombus", "Ромбом", "菱形" }, { "Square", "Каре", "方阵" }, { "Column of two", "Колонной по двое", "双列纵队" },
    { "Echelon", "Уступом", "梯队" }, { "Pincers", "Клещами", "钳形" }, { "Diamond", "Алмазом", "钻石阵" },
    { "Moth", "Мотыльком", "飞蛾阵" }, { "Chevron", "Шевроном", "人字阵" } };

static void* hook_setLineDropBox(void* panel, const void* label, int cat, int* value, bool b, float f)
{
    // the AI tab's "Follow Formation": its value is the player faction's idata["squad formation"]
    __try
    {
        if (g_playerFactionData && value && value == IdataRef(g_playerFactionData, "squad formation"))
        {
            if (g_followDefault < 0) g_followDefault = (*value >= 0 && *value <= FS_CARAVAN) ? *value : FS_MILITARY;
            void* line = orig_setLineDropBox(panel, label, cat, &g_followDefault, b, f);
            g_followDropBox = line;
            return line;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    g_followDropBox = 0;
    return orig_setLineDropBox(panel, label, cat, value, b, f);
}

// The owner (17.10.2026): "the list in alphabetical order, and all of it visible at once, without
// scrolling". So the game's own three adds to this drop-down are held back, and after its last one
// (Caravan) all the shapes go in sorted by their shown name - the values stay the same, only the order
// changes. Chinese stays in the shapes' own order (a byte sort means nothing there). Then the list is
// allowed to open tall enough for all of them (MyGUI::ComboBox::setMaxListLength, the line's listBox).
typedef void (*CB_setMaxListLength_t)(void* combo, int px);
static CB_setMaxListLength_t CB_setMaxListLength = 0;

static void hook_addAValue(void* line, const void* text, int value)
{
    if (line != g_followDropBox) { orig_addAValue(line, text, value); return; }
    if (value != FS_CARAVAN) return;                        // the game's Random and Military: added below, in order
    __try
    {
        int order[FS_COUNT];
        for (int s = 0; s < FS_COUNT; s++) order[s] = s;
        if (g_followLang != 2)
            for (int a = 1; a < FS_COUNT; a++)
                for (int b = a; b > 0 && strcmp(FOLLOW_NAMES[order[b]][g_followLang], FOLLOW_NAMES[order[b - 1]][g_followLang]) < 0; b--)
                { int x = order[b]; order[b] = order[b - 1]; order[b - 1] = x; }
        for (int k = 0; k < FS_COUNT; k++)
        {
            if (!FollowOffered(order[k])) continue;
            GStr s; char store[96];
            GStrSet(s, FOLLOW_NAMES[order[k]][g_followLang], store, sizeof(store));
            orig_addAValue(line, &s, order[k]);
        }
        void* combo = 0;
        if (CB_setMaxListLength && CopyGuarded(&combo, (unsigned char*)line + 0x120, sizeof(combo)) && PtrOk(combo))
            CB_setMaxListLength(combo, 1000);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("follow: filling the AI tab's list faulted"); }
}

// the boost map operator[] the AI tab uses on idata: "add rcx, 0x178" (GameData::idata) and the call after it
static void* FindIdataIndex(void* refreshAI)
{
    if (!refreshAI) return 0;
    const unsigned char* p = (const unsigned char*)refreshAI;
    static const unsigned char pat[] = { 0x48, 0x81, 0xC1, 0x78, 0x01, 0x00, 0x00 };
    __try
    {
        for (int i = 0; i < 0x1400; i++)
        {
            if (memcmp(p + i, pat, sizeof(pat)) != 0) continue;
            for (int j = i + 7; j < i + 24; j++)
            {
                if (p[j] != 0xE8) continue;
                const unsigned char* t = p + j + 5 + *(const int*)(p + j + 1);
                for (int k = 0; k < 3 && t[0] == 0xE9; k++) t = t + 5 + *(const int*)(t + 1);   // jump stubs
                return (void*)t;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    return 0;
}

// ------------------------------------------------------------------ Formations.save in the save folder
// SaveFileSystem::writeFile/readFile(name) give the path of a file inside the save's folder (writeFile:
// its temp folder, before the copy is queued). They return a std::string through a hidden pointer; the
// game builds it, and a path longer than 15 characters sits in the game's heap - we never free it (a
// path per save or load), freeing it with our own runtime would be a crash.
typedef bool  (*SFS_saveGame_t)(void* sfs, const void* path);
typedef void  (*SFS_loadGame_t)(void* sfs, const void* path);
typedef void  (*SFS_newGame_t)(void* sfs);
typedef void* (*SFS_pathOf_t)(void* sfs, void* ret, const void* name);
static SFS_saveGame_t orig_sfsSaveGame = 0;
static SFS_loadGame_t orig_sfsLoadGame = 0;
static SFS_newGame_t  orig_sfsNewGame = 0;
static SFS_pathOf_t   SFS_writeFile = 0, SFS_readFile = 0;
static const char FOLLOW_FILE[] = "Formations.save";   // 15 characters - fits the string's own buffer

static bool FollowPathOf(SFS_pathOf_t fn, void* sfs, char* out, size_t outSize)
{
    GStr name; char store[32]; GStrSet(name, FOLLOW_FILE, store, sizeof(store));
    GStr path; memset(&path, 0, sizeof(path)); path.cap = 15;
    __try { fn(sfs, &path, &name); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return GStrRead(&path, out, outSize) && out[0];
}

static void FollowClear()
{
    g_followDefault = -1; g_followPickN = 0;
    g_followLastDefault = -1; g_followTickAt = 0;
}

static void FollowWrite(void* sfs)
{
    char path[MAX_PATH * 2];
    if (!SFS_writeFile || !FollowPathOf(SFS_writeFile, sfs, path, sizeof(path))) { ErrorLog("follow: no path for %s - not saved", FOLLOW_FILE); return; }
    FILE* f = 0;
    if (fopen_s(&f, path, "wb") != 0 || !f) { ErrorLog("follow: cannot write %s", path); return; }
    fprintf(f, "Formations 1\ndefault\t%d\n", g_followDefault);
    for (int i = 0; i < g_followPickN; i++) fprintf(f, "squad\t%d\t%s\n", g_followPicks[i].shape, g_followPicks[i].name);
    fclose(f);
    Log("follow: saved default %d and %d squad shape(s) to %s", g_followDefault, g_followPickN, path);
}

static void FollowRead(void* sfs)
{
    char path[MAX_PATH * 2];
    if (!SFS_readFile) { ErrorLog("follow: save path API unavailable; cannot read %s", FOLLOW_FILE); return; }
    if (!FollowPathOf(SFS_readFile, sfs, path, sizeof(path))) { Log("follow: this save has no %s - the game's shape is the default", FOLLOW_FILE); return; }
    FILE* f = 0;
    const errno_t openError = fopen_s(&f, path, "rb");
    if (openError == ENOENT) { Log("follow: this save has no %s - the game's shape is the default", FOLLOW_FILE); return; }
    if (openError || !f) { ErrorLog("follow: cannot read %s", path); return; }
    char line[256]; int n = 0;
    if (fgets(line, sizeof(line), f) && strncmp(line, "Formations 1", 12) == 0)
        while (fgets(line, sizeof(line), f))
        {
            size_t len = strlen(line); while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
            if (strncmp(line, "default\t", 8) == 0) { int v = atoi(line + 8); g_followDefault = (v >= 0 && v < FS_COUNT) ? v : -1; }
            else if (strncmp(line, "squad\t", 6) == 0)
            {
                char* tab = strchr(line + 6, '\t'); if (!tab) continue;
                int v = atoi(line + 6);
                if (tab[1] && v >= 0 && v < FS_COUNT) { FollowPickSet(tab + 1, v); n++; }
            }
        }
    fclose(f);
    Log("follow: loaded default %d and %d squad shape(s) from %s", g_followDefault, n, path);
}

static bool hook_sfsSaveGame(void* sfs, const void* path)
{
    __try { FollowWrite(sfs); } __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("follow: writing %s faulted", FOLLOW_FILE); }
    return orig_sfsSaveGame(sfs, path);
}
static void hook_sfsLoadGame(void* sfs, const void* path)
{
    orig_sfsLoadGame(sfs, path);
    FollowClear();
    __try { FollowRead(sfs); } __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("follow: reading %s faulted", FOLLOW_FILE); }
}
static void hook_sfsNewGame(void* sfs)
{
    orig_sfsNewGame(sfs);
    FollowClear();
}

static void FollowInstall(HMODULE kl)
{
    CH_getPlatoon = (CH_getPlatoon_t)Real(GetProcAddress(kl, "?getPlatoon@Character@@QEBAPEAVActivePlatoon@@XZ"));
    AP_getName = (AP_getName_t)Real(GetProcAddress(kl, "?getName@ActivePlatoon@@QEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ"));
    GD_idataIndex = (IdataIndex_t)FindIdataIndex(Real(GetProcAddress(kl, "?refreshAI@ManagementScreen@@QEAAXXZ")));
    g_followLang = formations_mcm::CurrentLang();
    HMODULE mygui = GetModuleHandleA("MyGUIEngine_x64.dll");
    if (mygui) CB_setMaxListLength = (CB_setMaxListLength_t)GetProcAddress(mygui, "?setMaxListLength@ComboBox@MyGUI@@QEAAXH@Z");
    LogAvailability(CH_getPlatoon && GD_idataIndex, "Follow formations", "squad or AI tab lookup failed");
    LogAvailability(AP_getName != nullptr, "Squad names", "per-squad choices are unavailable");
    if (!CH_getPlatoon || !GD_idataIndex) return;
    Hook("Blackboard::getFormationPositionOffset",
         GetProcAddress(kl, "?getFormationPositionOffset@Blackboard@@QEAAAEBVVector3@Ogre@@PEAVCharacter@@@Z"),
         (void*)&hook_bbOffset, (void**)&orig_bbOffset);
    Hook("DatapanelGUI::setLineDropBox",
         GetProcAddress(kl, "?setLineDropBox@DatapanelGUI@@QEAAPEAVDataPanelLine_DropBox@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@HPEAH_NM@Z"),
         (void*)&hook_setLineDropBox, (void**)&orig_setLineDropBox);
    SFS_writeFile = (SFS_pathOf_t)Real(GetProcAddress(kl, "?writeFile@SaveFileSystem@@QEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z"));
    SFS_readFile = (SFS_pathOf_t)Real(GetProcAddress(kl, "?readFile@SaveFileSystem@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z"));
    Hook("SaveFileSystem::saveGame",
         GetProcAddress(kl, "?saveGame@SaveFileSystem@@QEAA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z"),
         (void*)&hook_sfsSaveGame, (void**)&orig_sfsSaveGame);
    Hook("SaveFileSystem::loadGame",
         GetProcAddress(kl, "?loadGame@SaveFileSystem@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z"),
         (void*)&hook_sfsLoadGame, (void**)&orig_sfsLoadGame);
    Hook("SaveFileSystem::newGame", GetProcAddress(kl, "?newGame@SaveFileSystem@@QEAAXXZ"),
         (void*)&hook_sfsNewGame, (void**)&orig_sfsNewGame);
    Hook("DataPanelLine_DropBox::addAValue",
         GetProcAddress(kl, "?addAValue@DataPanelLine_DropBox@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@H@Z"),
         (void*)&hook_addAValue, (void**)&orig_addAValue);
}

// ------------------------------------------------------------------ MCM: a shape for each squad
// The owner (17.10.2026): "after a save is loaded, the squads appear in MCM, and each gets its own
// formation there". MCM v6 builds these rows every time the page is shown (api->rows), so the list is
// the loaded save's squads. Value 0 - the squad follows the AI tab's default; k+1 - shape k. Kept by
// squad name (Formations.save; a renamed squad starts over).
struct FollowSquadRow { char name[160]; };
static FollowSquadRow g_followRows[64];

static const char* const FOLLOW_DEFAULT_LABEL[3] = { "Default (AI tab)", "По умолчанию (вкладка ИИ)", "默认（AI 页）" };
static const char* const FOLLOW_ROW_TIP[3] = {
    "The shape this squad keeps while it follows its leader. Default - the one chosen on the AI tab of the map.",
    "Строй, которым отряд идёт за ведущим. По умолчанию - тот, что выбран на вкладке ИИ в окне карты.",
    "该小队跟随领队时的阵型。默认：地图窗口 AI 页中所选的阵型。" };
static const char* const FOLLOW_NO_SQUADS[3] = { "Squads appear here once a game is loaded", "Отряды появятся здесь, когда загрузится игра", "载入存档后此处会列出小队" };

static int __cdecl FollowRowGet(void* ud, int* out)
{
    const FollowSquadRow* r = (const FollowSquadRow*)ud;
    int p = r->name[0] ? FollowPickGet(r->name) : -1;
    *out = p >= 0 ? p + 1 : 0;
    return 0;
}
static int __cdecl FollowRowSet(void* ud, int value, char* err, unsigned size)
{
    const FollowSquadRow* r = (const FollowSquadRow*)ud;
    if (!r->name[0]) { if (err && size) strncpy_s(err, size, "no squad", _TRUNCATE); return 1; }
    FollowPickSet(r->name, value - 1);
    g_followTickAt = 0;                                     // the table follows at once
    return 0;
}

static void __cdecl FollowRows(MCM_Api* api, void*)
{
    void* squads[64]; int ns = FollowSquads(squads, 64);
    if (!ns) { api->section(api, FOLLOW_NO_SQUADS[g_followLang]); return; }

    // the choice: default first, then the offered shapes in the AI tab's (alphabetical) order
    int values[FS_COUNT + 1]; const char* labels[FS_COUNT + 1]; int nv = 0;
    values[nv] = 0; labels[nv] = FOLLOW_DEFAULT_LABEL[g_followLang]; nv++;
    int order[FS_COUNT]; for (int s = 0; s < FS_COUNT; s++) order[s] = s;
    if (g_followLang != 2)
        for (int a = 1; a < FS_COUNT; a++)
            for (int b = a; b > 0 && strcmp(FOLLOW_NAMES[order[b]][g_followLang], FOLLOW_NAMES[order[b - 1]][g_followLang]) < 0; b--)
            { int x = order[b]; order[b] = order[b - 1]; order[b - 1] = x; }
    for (int k = 0; k < FS_COUNT; k++) if (FollowOffered(order[k])) { values[nv] = order[k] + 1; labels[nv] = FOLLOW_NAMES[order[k]][g_followLang]; nv++; }

    for (int k = 0; k < ns && k < 64; k++)
    {
        FollowSquadRow& r = g_followRows[k];
        r.name[0] = 0;
        __try { if (AP_getName) GStrRead(AP_getName(squads[k]), r.name, sizeof(r.name)); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        if (!r.name[0]) continue;
        // the options panel keys lines by their caption - two squads of one name would merge
        char label[200]; strcpy_s(label, sizeof(label), r.name);
        for (int j = 0, dup = 1; j < k; j++) if (strcmp(g_followRows[j].name, r.name) == 0) _snprintf_s(label, sizeof(label), _TRUNCATE, "%s (%d)", r.name, ++dup);
        api->choiceFn(api, label, FOLLOW_ROW_TIP[g_followLang], &FollowRowGet, &FollowRowSet, &r, values, labels, nv, 0);
        if (api->version >= 5) api->defaultValue(api, "0");
    }
}
