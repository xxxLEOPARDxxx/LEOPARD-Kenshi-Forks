// GHOST OUTLINES - while the drag is held, one small square per slot, on the
// ground, where each character will stand. Owner (boot 5): "for the new game
// start it would need the ghost outlines to work, that is a lot of people".
//
// Each slot is laid out by Layout() from the live drag, given the terrain's
// height, moved into Ogre's floating-origin space (world minus the shift the
// cast uses), and projected with the game's own cast camera:
//   clip = Frustum::getProjectionMatrix() * Camera::getViewMatrix(true) * p
// which is exactly the pair getCameraToViewportRay inverts, so a marker and a
// cast through the same pixel agree. Ogre::Matrix4 is sixteen floats, row
// major, column-vector convention (x' = m00 x + m01 y + m02 z + m03).
//
// A marker is a MyGUI "Widget" wearing "WhiteSkin" on the "Back" layer (the
// lowest shared layer, under every window), tinted with Widget::setColour.
// Widgets are created once, on the game thread, and hidden between drags.
// Everything is resolved by name; nothing is walked.
typedef const float* (*CAM_viewMat_t)(void* cam, bool ownFrustumOnly);
typedef const float* (*FR_projMat_t)(void* frustum);
typedef void* (*GuiGetPtr_t)();
typedef void* (*GuiCreate_t)(void* self, const void* type, const void* skin, int x, int y, int w, int h, int align, const void* layer, const void* name);
typedef void  (*W_setPos_t)(void* w, int x, int y);
typedef void  (*W_setVisible_t)(void* w, bool v);
typedef void  (*W_setAlpha_t)(void* w, float a);
struct MgColour { float r, g, b, a; };
typedef void  (*W_setColour_t)(void* w, const MgColour* c);
static CAM_viewMat_t  CAM_getViewMatrix = 0;
static FR_projMat_t   FR_getProjectionMatrix = 0;
static GuiGetPtr_t    MG_getGui = 0;
static GuiCreate_t    MG_guiCreate = 0;
static W_setPos_t     MG_setPosition = 0;
static W_setVisible_t MG_setVisible = 0;
static W_setAlpha_t   MG_setAlpha = 0;
static W_setColour_t  MG_setColour = 0;
static void* g_ghost[SQUAD_MAX];
static int   g_ghostMade = 0;                 // widgets created so far (never destroyed; hidden when unused)
static int   g_ghostShown = 0;                // how many are visible right now
static bool  g_ghostDead = false;             // creation faulted or returned null: never again this session
static const int GHOST_SIZE = 10;
static const int ALIGN_DEFAULT = 5;           // MyGUI::Align Left|Top; a one-int struct, passed as an int

static void GhostInit(HMODULE mygui, HMODULE ogre)
{
    if (ogre)
    {
        CAM_getViewMatrix = (CAM_viewMat_t)GetProcAddress(ogre, "?getViewMatrix@Camera@Ogre@@QEBAAEBVMatrix4@2@_N@Z");
        FR_getProjectionMatrix = (FR_projMat_t)GetProcAddress(ogre, "?getProjectionMatrix@Frustum@Ogre@@UEBAAEBVMatrix4@2@XZ");
    }
    if (mygui)
    {
        MG_getGui = (GuiGetPtr_t)GetProcAddress(mygui, "?getInstancePtr@?$Singleton@VGui@MyGUI@@@MyGUI@@SAPEAVGui@2@XZ");
        MG_guiCreate = (GuiCreate_t)GetProcAddress(mygui, "?createWidgetT@Gui@MyGUI@@QEAAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0HHHHUAlign@2@00@Z");
        MG_setPosition = (W_setPos_t)GetProcAddress(mygui, "?setPosition@Widget@MyGUI@@QEAAXHH@Z");
        MG_setVisible = (W_setVisible_t)GetProcAddress(mygui, "?setVisible@Widget@MyGUI@@UEAAX_N@Z");
        MG_setAlpha = (W_setAlpha_t)GetProcAddress(mygui, "?setAlpha@Widget@MyGUI@@QEAAXM@Z");
        MG_setColour = (W_setColour_t)GetProcAddress(mygui, "?setColour@Widget@MyGUI@@QEAAXAEBUColour@2@@Z");
    }
    bool ok = CAM_getViewMatrix && FR_getProjectionMatrix && MG_getGui && MG_guiCreate && MG_setPosition && MG_setVisible;
    LogAvailability(ok, "Formation preview", "the drag shows no placement outlines");
    if (!ok) g_ghostDead = true;
}

// world -> Ogre space shift, the same one the cast uses
static bool GhostShift(void* cam, Vec3& shift)
{
    if (!(CAM_getDerivedPos && KL_ou && GW_getCameraPos && PtrOk(*KL_ou))) return false;
    Vec3 wc = { 0, 0, 0 }, lc = { 0, 0, 0 };
    __try
    {
        GW_getCameraPos(*KL_ou, &wc);
        const Vec3* lp = CAM_getDerivedPos(cam);
        if (!CopyGuarded(&lc, lp, sizeof(lc))) return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    shift.x = wc.x - lc.x; shift.y = wc.y - lc.y; shift.z = wc.z - lc.z;
    return true;
}

static bool ProjectToScreen(const float* V, const float* P, const Vec3& world, const Vec3& shift, int W, int H, int& sx, int& sy)
{
    float x = world.x - shift.x, y = world.y - shift.y, z = world.z - shift.z;
    float vx = V[0] * x + V[1] * y + V[2] * z + V[3];
    float vy = V[4] * x + V[5] * y + V[6] * z + V[7];
    float vz = V[8] * x + V[9] * y + V[10] * z + V[11];
    float vw = V[12] * x + V[13] * y + V[14] * z + V[15];
    float cx = P[0] * vx + P[1] * vy + P[2] * vz + P[3] * vw;
    float cy = P[4] * vx + P[5] * vy + P[6] * vz + P[7] * vw;
    float cw = P[12] * vx + P[13] * vy + P[14] * vz + P[15] * vw;
    if (cw <= 1e-4f) return false;                                  // behind the camera
    float nx = cx / cw, ny = cy / cw;
    if (nx < -1.2f || nx > 1.2f || ny < -1.2f || ny > 1.2f) return false;
    sx = (int)((nx * 0.5f + 0.5f) * W);
    sy = (int)((0.5f - ny * 0.5f) * H);
    return true;
}

static void* GhostMake(int i)
{
    if (g_ghostDead || !MG_getGui || !MG_guiCreate) return 0;
    EngineString type, skin, layer, name; char nm[16]; sprintf_s(nm, "OK_Ghost%d", i);
    if (!StrMakeInline(type, "Widget") || !StrMakeInline(skin, "WhiteSkin") || !StrMakeInline(layer, "Back") || !StrMakeInline(name, nm)) { g_ghostDead = true; return 0; }
    void* w = 0;
    __try
    {
        void* gui = MG_getGui(); if (!PtrOk(gui)) { g_ghostDead = true; return 0; }
        w = MG_guiCreate(gui, &type, &skin, -100, -100, GHOST_SIZE, GHOST_SIZE, ALIGN_DEFAULT, &layer, &name);
        if (!w) { g_ghostDead = true; ErrorLog("ghost: createWidgetT returned null - outlines off for this session"); return 0; }
        MgColour c = { 0.55f, 0.95f, 1.0f, 1.0f };
        if (MG_setColour) MG_setColour(w, &c);
        if (MG_setAlpha) MG_setAlpha(w, 0.85f);
        MG_setVisible(w, false);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_ghostDead = true; ErrorLog("ghost: widget creation faulted - outlines off for this session"); return 0; }
    return w;
}

static void GhostHide()
{
    for (int i = 0; i < g_ghostMade; i++) if (g_ghost[i]) { __try { MG_setVisible(g_ghost[i], false); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    g_ghostShown = 0;
}

// lay the live drag out for the selection and put a marker on every slot
static void GhostUpdate(int n, const Vec3& A, const Vec3& B)
{
    if (g_ghostDead || n <= 0) return;
    void* cam = MainCamera(); if (!cam) return;
    HWND hw = GetForegroundWindow(); RECT rc;
    if (!hw || !GetClientRect(hw, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    Vec3 shift; if (!GhostShift(cam, shift)) return;
    float V[16], P[16];
    __try
    {
        const float* pv = CAM_getViewMatrix(cam, true); const float* pp = FR_getProjectionMatrix(cam);
        if (!CopyGuarded(V, pv, sizeof(V)) || !CopyGuarded(P, pp, sizeof(P))) return;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    Vec3 slots[SQUAD_MAX];
    int k = Layout(g_set.shape, n > SQUAD_MAX ? SQUAD_MAX : n, A, B, Spacing(), slots);
    int shown = 0;
    for (int i = 0; i < k; i++)
    {
        if (i >= g_ghostMade) { g_ghost[g_ghostMade] = GhostMake(g_ghostMade); g_ghostMade++; if (g_ghostDead) break; }
        void* w = g_ghost[i]; if (!w) continue;
        if (UT_terrainHeight) { __try { slots[i].y = UT_terrainHeight(slots[i].x, slots[i].z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        int sx, sy; bool on = ProjectToScreen(V, P, slots[i], shift, rc.right, rc.bottom, sx, sy);
        __try
        {
            if (on) { MG_setPosition(w, sx - GHOST_SIZE / 2, sy - GHOST_SIZE / 2); MG_setVisible(w, true); shown++; }
            else MG_setVisible(w, false);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { g_ghostDead = true; break; }
    }
    for (int i = k; i < g_ghostMade; i++) if (g_ghost[i]) { __try { MG_setVisible(g_ghost[i], false); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    g_ghostShown = shown;
}
