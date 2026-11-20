// THE RALLY FLAG - a flag planted at the rally point, seen only while the
// modifier is held. Owner (boot 15): "an invisible flag at the rally point,
// one that shows up when hitting alt." Two WhiteSkin widgets, a pole and an
// orange pennant, projected to the screen the way the ghost squares are.
// 1.4: the overlay is the FALLBACK. The real thing is THE BANNER below - a
// piece of the game's own furniture standing on the ground at the point,
// placed through Ogre's scene API (owner, 2026-09-09: "the little orange flag
// could be exchanged for a real world item"). While the banner stands, the
// overlay stays hidden; if the banner cannot be made this boot, the overlay
// serves as before.
static void* g_flagPole = 0; static void* g_flagPennant = 0;
static bool  g_flagDead = false, g_flagShown = false;

static void* FlagMake(const char* name, int w, int h, const MgColour& c)
{
    if (g_flagDead || !MG_getGui || !MG_guiCreate) return 0;
    EngineString type, skin, layer, nm;
    if (!StrMakeInline(type, "Widget") || !StrMakeInline(skin, "WhiteSkin") || !StrMakeInline(layer, "Back") || !StrMakeInline(nm, name)) { g_flagDead = true; return 0; }
    void* wd = 0;
    __try
    {
        void* gui = MG_getGui(); if (!PtrOk(gui)) { g_flagDead = true; return 0; }
        wd = MG_guiCreate(gui, &type, &skin, -100, -100, w, h, ALIGN_DEFAULT, &layer, &nm);
        if (!wd) { g_flagDead = true; ErrorLog("flag: createWidgetT returned null"); return 0; }
        if (MG_setColour) MG_setColour(wd, &c);
        if (MG_setAlpha) MG_setAlpha(wd, 0.95f);
        MG_setVisible(wd, false);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_flagDead = true; ErrorLog("flag: widget creation faulted"); return 0; }
    return wd;
}

static void FlagHide()
{
    if (!g_flagShown) return;
    __try { if (g_flagPole) MG_setVisible(g_flagPole, false); if (g_flagPennant) MG_setVisible(g_flagPennant, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    g_flagShown = false;
}

// ---------------------------------------------------------------------------
// THE BANNER (1.4). The game ships a banner as bar furniture -
// data/buildings/furniture/new_furniture01/banner.mesh with its stand
// banner_support.mesh - and OgreMain_x64.dll exports the scene API that
// places a mesh: the camera's SceneManager makes an Entity from the mesh by
// name, a child of the root scene node carries it, Node::setPosition puts it
// at terrain height, and destroyEntity / destroySceneNode take it down when
// the point is cleared or moved. Ogre 2.0 names: SceneMemoryMgrTypes 0 =
// SCENE_DYNAMIC; the resource group "Autodetect" searches every group.
// Visual only: nothing in the save; gone on reload, like the rally point.
// UNREAD, and the boot tells: whether that furniture's resource group is
// indexed while no bar is loaded. If createEntity throws (an Ogre exception
// is an SEH exception 0xE06D7363 under /EHa - the __except takes it) the
// banner is DISARMED for the boot, the log says so, and the overlay serves.
typedef void* (*CAM_getSceneManager_t)(void* cam);
typedef void* (*SM_createEntity_t)(void* sm, const EngineString* meshName, const EngineString* groupName, int memType);
typedef void* (*SM_getRootSceneNode_t)(void* sm, int memType);
typedef void* (*SN_createChild_t)(void* node, int memType, const float* translate3, const float* rotate4wxyz);
typedef void  (*SN_attachObject_t)(void* node, void* movable);
typedef void  (*ND_setPosition3_t)(void* node, float x, float y, float z);
typedef void  (*SN_setVisible_t)(void* node, bool visible, bool cascade);
typedef void  (*SM_destroyEntity_t)(void* sm, void* entity);
typedef void  (*SM_destroySceneNode_t)(void* sm, void* node);
static CAM_getSceneManager_t CAM_getSceneManager = 0; static SM_createEntity_t SM_createEntity = 0; static SM_getRootSceneNode_t SM_getRootSceneNode = 0;
static SN_createChild_t SN_createChild = 0; static SN_attachObject_t SN_attachObject = 0; static ND_setPosition3_t ND_setPosition3 = 0;
static SN_setVisible_t SN_setVisible = 0; static SM_destroyEntity_t SM_destroyEntity = 0; static SM_destroySceneNode_t SM_destroySceneNode = 0;
// 1.4.1 (owner, the boot of 04:57: the bar banner "not so much... take a look at the statue towns use as their markers, the ones that
// are invisible that require shift f12 to see and move. what we need is a much smaller version of that and leave it invisible when not
// holding alt"): THE MARKER is data/meshes/marker.mesh - the editor's own town marker (PlayerInterface.c builds the name "marker.mesh";
// data/meshes is a group the game indexes at start, unlike the bar furniture) - sized by MarkerHeight (settings, 25 world units; the mesh is a unit cube) and shown ONLY
// while the modifier is held. Its local bounds are read once (MovableObject::getLocalAabb) and logged, so the scale can be judged.
typedef void  (*ND_setScale3_t)(void* node, float x, float y, float z);
typedef float* (*MO_getLocalAabb_t)(void* movable, float* outAabb6);   // Aabb {center xyz, halfSize xyz} returned through the hidden pointer (24 bytes, x64)
static ND_setScale3_t ND_setScale3 = 0; static MO_getLocalAabb_t MO_getLocalAabb = 0;
// 1.4.3 (the owner, 2026-09-14: "The statue is fine just know you tried it once and didnt get it for some reason. I think it was
// invisible ... figure out what causes it to be invisible in normal game and visible in f12"): THE STATUE IS athene.mesh, and the
// editor never trusts the mesh's own material - it FORCES the material "debug_marker" onto the entity (PlayerInterface.c:7652-7654
// Entity::setMaterialName("debug_marker", Autodetect); TownBase.c:3755 passes the same name in its load request). athene.mesh is an old
// Ogre sample model whose own materials are not in the game's resources, so without that call the entity exists and draws nothing -
// which is what happened the first time. debug_marker is a shipped script (data/materials/debug.material). marker.mesh (1.4.1-1.4.2)
// was a plain unit cube: "the 3d object that it currently uses is bad".
typedef void  (*ENT_setMaterialName_t)(void* entity, const EngineString* material, const EngineString* group);
static ENT_setMaterialName_t ENT_setMaterialName = 0;
static void* g_bannerSm = 0; static void* g_bannerNode = 0; static void* g_bannerEnt[2] = { 0, 0 }; static bool g_bannerDead = false, g_bannerVisible = false;
static const int BANNER_MESH_COUNT = 1; static const char* BANNER_MESHES[2] = { "athene.mesh", "" };
static const char* BANNER_MATERIAL = "debug_marker";

static void BannerResolve(HMODULE ogre)
{
    if (!ogre) { g_bannerDead = true; ErrorLog("Rally marker unavailable: OgreMain not loaded; using the overlay flag"); return; }
    CAM_getSceneManager = (CAM_getSceneManager_t)GetProcAddress(ogre, "?getSceneManager@Camera@Ogre@@QEBAPEAVSceneManager@2@XZ");
    SM_createEntity = (SM_createEntity_t)GetProcAddress(ogre, "?createEntity@SceneManager@Ogre@@UEAAPEAVEntity@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0W4SceneMemoryMgrTypes@2@@Z");
    SM_getRootSceneNode = (SM_getRootSceneNode_t)GetProcAddress(ogre, "?getRootSceneNode@SceneManager@Ogre@@QEAAPEAVSceneNode@2@W4SceneMemoryMgrTypes@2@@Z");
    SN_createChild = (SN_createChild_t)GetProcAddress(ogre, "?createChildSceneNode@SceneNode@Ogre@@UEAAPEAV12@W4SceneMemoryMgrTypes@2@AEBVVector3@2@AEBVQuaternion@2@@Z");
    SN_attachObject = (SN_attachObject_t)GetProcAddress(ogre, "?attachObject@SceneNode@Ogre@@QEAAXPEAVMovableObject@2@@Z");
    ND_setPosition3 = (ND_setPosition3_t)GetProcAddress(ogre, "?setPosition@Node@Ogre@@QEAAXMMM@Z");
    SN_setVisible = (SN_setVisible_t)GetProcAddress(ogre, "?setVisible@SceneNode@Ogre@@UEAAX_N0@Z");
    SM_destroyEntity = (SM_destroyEntity_t)GetProcAddress(ogre, "?destroyEntity@SceneManager@Ogre@@UEAAXPEAVEntity@2@@Z");
    SM_destroySceneNode = (SM_destroySceneNode_t)GetProcAddress(ogre, "?destroySceneNode@SceneManager@Ogre@@UEAAXPEAVSceneNode@2@@Z");
    ND_setScale3 = (ND_setScale3_t)GetProcAddress(ogre, "?setScale@Node@Ogre@@QEAAXMMM@Z");
    MO_getLocalAabb = (MO_getLocalAabb_t)GetProcAddress(ogre, "?getLocalAabb@MovableObject@Ogre@@QEBA?AUAabb@2@XZ");
    ENT_setMaterialName = (ENT_setMaterialName_t)GetProcAddress(ogre, "?setMaterialName@Entity@Ogre@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z");
    bool ok = CAM_getSceneManager && SM_createEntity && SM_getRootSceneNode && SN_createChild && SN_attachObject && ND_setPosition3 && SM_destroyEntity && SM_destroySceneNode && SN_setVisible && ENT_setMaterialName;
    if (!ok) g_bannerDead = true;
    LogAvailability(ok, "Rally marker", "an Ogre scene or material export is missing; using the overlay flag");
}

static void BannerDestroy()
{
    if (!g_bannerNode && !g_bannerEnt[0] && !g_bannerEnt[1]) return;
    __try
    {
        for (int i = 0; i < 2; i++) if (g_bannerEnt[i] && g_bannerSm && SM_destroyEntity) SM_destroyEntity(g_bannerSm, g_bannerEnt[i]);
        if (g_bannerNode && g_bannerSm && SM_destroySceneNode) SM_destroySceneNode(g_bannerSm, g_bannerNode);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_bannerDead = true; ErrorLog("banner: taking it down faulted - the banner is disarmed for this boot"); }
    g_bannerNode = 0; g_bannerEnt[0] = g_bannerEnt[1] = 0;
}

static void* BannerEntity(void* sm, const char* meshName, const EngineString* group)
{
    EngineString mesh; char mb[64]; if (!StrMakeHeap(mesh, mb, sizeof(mb), meshName)) return 0;
    void* e = 0;
    __try { e = SM_createEntity(sm, &mesh, group, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { e = 0; ErrorLog("banner: createEntity(\"%s\") failed", meshName); }
    if (!PtrOk(e)) return 0;
    // 1.4.3: the editor's own step - the material the statue draws with. Without it athene.mesh is an invisible entity (its own
    // materials are not in the game). An Ogre exception here is the SEH 0xE06D7363 the __except takes; the entity is then dropped.
    EngineString mat; if (!StrMakeInline(mat, BANNER_MATERIAL)) return e;
    bool matOk = false;
    __try { ENT_setMaterialName(e, &mat, group); matOk = true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { ErrorLog("marker: setMaterialName(\"%s\") failed", BANNER_MATERIAL); }
    if (!matOk) { __try { if (SM_destroyEntity) SM_destroyEntity(sm, e); } __except (EXCEPTION_EXECUTE_HANDLER) { } return 0; }
    return e;
}

static bool BannerShow(const Vec3& at)
{
    if (g_bannerDead) return false;
    BannerDestroy();                                          // a new point: a fresh banner (a node may not survive a load; never reuse one)
    void* cam = MainCamera(); if (!cam) { Log("banner: no camera yet"); return false; }
    void* sm = 0;
    __try { sm = CAM_getSceneManager(cam); } __except (EXCEPTION_EXECUTE_HANDLER) { sm = 0; }
    if (!PtrOk(sm)) { ErrorLog("banner: the camera has no scene manager"); return false; }
    Vec3 p = at;                                              // 1.4.1: the point as given - the surface the cursor hit, roof or ground - no terrain override
    EngineString group; if (!StrMakeInline(group, "Autodetect")) return false;
    void* node = 0; void* ents[2] = { 0, 0 }; int made = 0; float aabb[6] = { 0, 0, 0, 0, 0, 0 }; bool haveAabb = false;
    float scale = 1.0f;
    __try
    {
        void* root = SM_getRootSceneNode(sm, 0);
        if (!PtrOk(root)) { ErrorLog("marker: no root scene node"); return false; }
        float zero[3] = { 0, 0, 0 }, ident[4] = { 1, 0, 0, 0 };   // Quaternion w, x, y, z
        node = SN_createChild(root, 0, zero, ident);
        if (!PtrOk(node)) { ErrorLog("marker: createChildSceneNode returned null"); return false; }
        for (int i = 0; i < BANNER_MESH_COUNT; i++)
        {
            void* e = BannerEntity(sm, BANNER_MESHES[i], &group);
            if (!e) continue;
            if (!haveAabb && MO_getLocalAabb) { __try { MO_getLocalAabb(e, aabb); haveAabb = true; } __except (EXCEPTION_EXECUTE_HANDLER) { haveAabb = false; } }
            SN_attachObject(node, e); ents[made++] = e;
        }
        if (made == 0)
        {
            if (SM_destroySceneNode) SM_destroySceneNode(sm, node);
            g_bannerDead = true; ErrorLog("marker: the mesh could not be made - DISARMED for this boot; the overlay flag serves while ALT is held");
            return false;
        }
        // 1.4.2 (the owner, 2026-09-09: "the rally point item is invisible - it still works, just can't be seen"): the editor's marker
        // mesh is a UNIT CUBE (its stored bounds run -0.5..0.5, measured from the file; the wall banner that showed is 25 units tall),
        // so a raw scale of 0.25 stood it a quarter of a unit high. The size is now a HEIGHT in world units (MarkerHeight, default 25 -
        // the banner's size) divided by the mesh's own box, read here; a mesh whose box cannot be read is taken as one unit tall.
        float meshH = (haveAabb && aabb[4] > 0.001f) ? aabb[4] * 2.0f : 1.0f;
        scale = g_set.markerHeight / meshH;
        if (ND_setScale3) ND_setScale3(node, scale, scale, scale);
        // 1.4.3 (the owner: "the z offset has the statue half way in the map"): athene.mesh's origin is its MIDDLE (box centre 0, half-height
        // 81.6), so the node must stand the box's bottom on the surface: lift by (halfHeight - centreY) x scale. The editor lifts its town
        // statue by a constant (TownBase.c:3768); reading the box makes this right for any mesh.
        float lift = haveAabb ? (aabb[4] - aabb[1]) * scale : 0.0f;
        ND_setPosition3(node, p.x, p.y + lift, p.z);
        SN_setVisible(node, false, true);                     // invisible until the modifier is held (RallyFlagUpdate)
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_bannerDead = true; g_bannerNode = 0; ErrorLog("marker: placing it faulted - DISARMED for this boot; the overlay flag serves"); return false; }
    g_bannerSm = sm; g_bannerNode = node; g_bannerEnt[0] = ents[0]; g_bannerEnt[1] = ents[1]; g_bannerVisible = false;
    if (haveAabb) Log("marker: %s at %.0f, %.0f, %.0f, scale %.2f, lifted %.1f so its box stands on the surface - the mesh's own box: centre %.1f, %.1f, %.1f, half-size %.1f, %.1f, %.1f (so %.0f units tall as placed)", BANNER_MESHES[0], p.x, p.y, p.z, scale, (aabb[4] - aabb[1]) * scale, aabb[0], aabb[1], aabb[2], aabb[3], aabb[4], aabb[5], aabb[4] * 2.0f * scale);
    else Log("marker: %s at %.0f, %.0f, %.0f, scale %.2f (its box unread)", BANNER_MESHES[0], p.x, p.y, p.z, scale);
    return true;
}

static void BannerSetVisible(bool on)
{
    if (!g_bannerNode || g_bannerVisible == on) return;
    __try { SN_setVisible(g_bannerNode, on, true); g_bannerVisible = on; }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_bannerDead = true; g_bannerNode = 0; ErrorLog("marker: showing/hiding it faulted - DISARMED for this boot"); }
}

static void RallyFlagUpdate(bool show)
{
    if (g_bannerNode) { BannerSetVisible(show && g_haveRally); FlagHide(); return; }   // the marker in the world, shown only while the modifier is held; the overlay is not needed
    if (g_flagDead) return;
    if (!show || !g_haveRally) { FlagHide(); return; }
    if (!g_flagPole)
    {
        MgColour pole = { 0.95f, 0.95f, 0.95f, 1.0f }, pennant = { 1.0f, 0.55f, 0.10f, 1.0f };
        g_flagPole = FlagMake("OK_FlagPole", 3, 30, pole);
        g_flagPennant = FlagMake("OK_FlagTop", 16, 10, pennant);
        if (!g_flagPole || !g_flagPennant) return;
    }
    void* cam = MainCamera(); if (!cam) { FlagHide(); return; }
    HWND hw = GetForegroundWindow(); RECT rc;
    if (!hw || !GetClientRect(hw, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    Vec3 shift; if (!GhostShift(cam, shift)) { FlagHide(); return; }
    float V[16], P[16];
    __try
    {
        const float* pv = CAM_getViewMatrix(cam, true); const float* pp = FR_getProjectionMatrix(cam);
        if (!CopyGuarded(V, pv, sizeof(V)) || !CopyGuarded(P, pp, sizeof(P))) return;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    Vec3 p = g_rally;
    if (UT_terrainHeight) { __try { p.y = UT_terrainHeight(p.x, p.z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    int sx, sy;
    if (!ProjectToScreen(V, P, p, shift, rc.right, rc.bottom, sx, sy)) { FlagHide(); return; }
    __try
    {
        MG_setPosition(g_flagPole, sx - 1, sy - 30);
        MG_setPosition(g_flagPennant, sx + 2, sy - 30);
        MG_setVisible(g_flagPole, true); MG_setVisible(g_flagPennant, true);
        g_flagShown = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_flagDead = true; }
}
