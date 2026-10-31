// Formations - an RE_Kenshi plugin for Kenshi.
// Copyright (C) 2026 the author (see the Steam Workshop page). Built with OpenKenshi, with AI assistance (Claude).
//
// This program is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// It is distributed WITHOUT ANY WARRANTY; see the LICENSE file shipped beside it for
// the full terms. RE_Kenshi, which this plugin loads into, is GPLv3 as well.

// FORMATIONS - Bannerlord-style squad formations for Kenshi (RE_Kenshi plugin).
//
// Owner (2026-09-04): "bannerlord allows the player to click and drag a
// formation as well as choose default formations... make them shapes and let the
// player decide what role a squad plays". So: hold the modifier (ALT by default)
// and drag with the right mouse button across the ground, and the SELECTED
// characters are laid out along that line in the current shape. ALT + 1..7
// picks the shape. Hold-formation keeps each character in its slot until the
// player gives a plain order, or the character is fighting.
//
// Everything is measured (notes/PLUGIN-formations.md) and everything is an
// exported call resolved by name at runtime - no engine library is linked, the
// MapProbe discipline: PODs across the boundary, SEH around every engine touch,
// objects from hook parameters, never members walked.
//
//   ground cast   Ogre::Camera::getCameraToViewportRay(x01, y01, Ray*) from the
//                 camera Camera::setPosition hands us, marched against
//                 UtilityT::getTerrainHeight(x, z) (static)
//   selection     PlayerInterface::getAllPlayerCharacters + isObjectSelected
//   orders        Character::addJob(MOVE_CUS_ORDERED 29, null, false, false, pos)
//   hold          Character::setStandingOrder(12 = the Hold button, on) once the character has ARRIVED in its slot
//   combat        Character::getAllAttackersCount / isDown / isUnconcious / isDead (the fallback regroup only)
//
// Build: build.bat (MSVC v143).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// ---------------------------------------------------------------- logging
static char    g_logPath[MAX_PATH] = {0};
static char    g_dir[MAX_PATH] = {0};
static HMODULE g_self = 0;

static void LogInit()
{
    char dir[MAX_PATH];
    if (GetModuleFileNameA(g_self, dir, MAX_PATH)) { char* s = strrchr(dir, '\\'); if (s) *(s + 1) = 0; }
    else strcpy_s(dir, MAX_PATH, ".\\");
    strcpy_s(g_dir, MAX_PATH, dir);
    _snprintf_s(g_logPath, MAX_PATH, _TRUNCATE, "%sFormations_log.txt", dir);
    FILE* f = 0;
    if (fopen_s(&f, g_logPath, "w") == 0 && f)
    {
        fputs("Formations - squad formations for Kenshi\n", f);
        fputs("Hold ALT, drag with the RIGHT mouse button across the ground: the selected characters take the shape along that line.\n", f);
        fputs("While the drag is held, a small square marks where each character will stand (Ghost=0 in the settings turns them off).\n", f);
        fputs("ALT + a plain right-CLICK (no drag) sends the selection to the point in order: the slowest character leads, and the rest keep the current shape behind him (Advance=0 in the settings gives the game's own follow instead).\n", f);
        fputs("While ALT is held a readout lists the shapes.\n", f);
        fputs("ALT+1 line  ALT+2 circle  ALT+3 rectangle  ALT+4 wedge  ALT+5 tight/loose.  (Every key and the modifier are yours to change in Formations_settings.txt.)\n", f);
        fputs("ALT+X retreat: everyone standing runs to the rally point (or away from the fight) and picks up the wounded on the way.\n", f);
        fputs("ALT+V sets the rally point under the cursor - a marker stands there while ALT is held (the editor's own town statue, 25 world units tall; MarkerHeight in the settings); ALT+V again on the marker clears it. Characters holding the game's own Medic job (the orders panel button, NUM7) carry the wounded to it on their own while a rally point exists, put them down there, and the Medic job heals them.\n", f);
        fputs("With Hold=1 (the default) the drag also switches the game's own Hold on for them (the orders panel button / NUM1 releases it); Hold=0 leaves it alone. While ALT is held the game's plain right-click move does not fire.\n", f);
        fputs("ALT above is the default modifier - any key or combination can be set (Modifier in the settings, or Mod Config Menu).\n\n", f);
        fclose(f);
    }
}
static void Log(const char* fmt, ...)
{
    FILE* f = 0;
    if (fopen_s(&f, g_logPath, "a") != 0 || !f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

// ------------------------------------------------------- guarded reading
static bool Readable(const void* p, size_t n)
{
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return (const unsigned char*)p + n <= (const unsigned char*)mbi.BaseAddress + mbi.RegionSize;
}
static bool CopyGuarded(void* dst, const void* src, size_t n)
{
    if (!Readable(src, n)) return false;
    __try { memcpy(dst, src, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool PtrOk(const void* p) { return p && (unsigned __int64)p > 0x10000; }

// the engine's std::string, measured (MapProbe step 1): inline while cap <= 15
struct EngineString { char buf[16]; unsigned __int64 size, cap; };
static bool StrMakeInline(EngineString& out, const char* text)
{
    size_t n = strlen(text); if (n > 15) return false;
    memset(&out, 0, sizeof(out)); memcpy(out.buf, text, n); out.size = n; out.cap = 15; return true;
}

// --------------------------------------------------- export table walking
static void* FindExportByPrefix(HMODULE mod, const char* prefix)
{
    if (!mod || !prefix) return 0;
    unsigned char* base = (unsigned char*)mod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return 0;
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
    DWORD* names = (DWORD*)(base + exp->AddressOfNames);
    WORD*  ords  = (WORD*)(base + exp->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)(base + exp->AddressOfFunctions);
    size_t plen = strlen(prefix);
    for (DWORD i = 0; i < exp->NumberOfNames; i++)
        if (strncmp((const char*)(base + names[i]), prefix, plen) == 0) return (void*)(base + funcs[ords[i]]);
    return 0;
}

// ------------------------------------------------------------ KenshiLib
typedef int     (*AddHook_t)(void* target, void* detour, void** original);
typedef __int64 (*GetRealAddress_t)(void* fn);
static AddHook_t        KL_AddHook = 0;
static GetRealAddress_t KL_GetRealAddress = 0;
// GetRealAddress only for KenshiLib's own export stubs: for an Ogre function (Camera::setPosition) KenshiLib logs an
// "Unnecessary call ... Error" pair at every start.
static bool InKenshiLib(void* fn)
{
    HMODULE owner = 0;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)fn, &owner)
        && owner == GetModuleHandleA("KenshiLib.dll");
}
static void* Real(void* fn) { if (fn && KL_GetRealAddress && InKenshiLib(fn)) { __int64 r = KL_GetRealAddress(fn); if (r) return (void*)r; } return fn; }
static bool Hook(const char* label, void* target, void* detour, void** orig)
{
    if (!target) { Log("  %-34s NOT FOUND", label); return false; }
    int rc = KL_AddHook(Real(target), detour, orig);
    Log("  %-34s %s", label, rc == 0 ? "hooked" : "FAILED");
    return rc == 0;
}

// ------------------------------------------------------------- settings
// 1.4.3 (the owner: "make the keybinds configurable"): the seven keys live in the settings as names - a letter or digit, F1-F12, or
// space, tab, enter, home, end, pageup, pagedown, insert, delete, numpad0-9, plus, minus, comma, period. KEY_ order: line, circle,
// rectangle, wedge, tight/loose, retreat, rally. An unknown name disables that key (logged at load), never crashes.
// 17.10.2026: seven more shapes - the follow shapes (Follow.h) for the drag too; their keys go after the
// old ones so the settings file and its order stay as they were.
enum { KEY_LINE = 0, KEY_CIRCLE, KEY_RECT, KEY_WEDGE, KEY_LOOSE, KEY_RETREAT, KEY_RALLY,
       KEY_SQUARE, KEY_RHOMBUS, KEY_COLUMN2, KEY_PINCERS, KEY_DIAMOND, KEY_MOTH, KEY_CHEVRON, KEY_COUNT };
static const char* KEY_SETTING[KEY_COUNT] = { "KeyLine", "KeyCircle", "KeyRectangle", "KeyWedge", "KeyLoose", "KeyRetreat", "KeyRally",
                                             "KeySquare", "KeyRhombus", "KeyColumn", "KeyPincers", "KeyDiamond", "KeyMoth", "KeyChevron" };
// modifier (16.10.2026): any key or combination, written the way Mod Config Menu captures it -
// "ALT", "SHIFT", "F", "CTRL+B", "NUM5" (shared/HoldKey.h reads it). The old alt/ctrl/shift still read.
// hold (16.10.2026): 1 = a formation switches the game's own Hold on for everyone it places. The owner:
// "it sets Hold by itself, that is infuriating" - a SHIFT modifier caught the game's own shift-click.
struct Settings { char modifier[32]; float spacing; int shape; int ghost; int followTask; int advance; int loose; float retreat; float markerHeight; char keys[KEY_COUNT][12]; int hold; };
static Settings g_set = { "ALT", 10.0f, 0, 1, 44, 1, 1, 300.0f, 25.0f, { "1", "2", "3", "4", "5", "X", "V", "6", "7", "8", "9", "0", "minus", "plus" }, 1 };
static int KeyVK(const char* name)
{   // a key name -> virtual-key code; 0 = unknown (the key is off)
    if (!name || !name[0]) return 0;
    if (!name[1]) { unsigned char c = (unsigned char)toupper((unsigned char)name[0]); return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? (int)c : 0; }
    if ((name[0] == 'F' || name[0] == 'f') && isdigit((unsigned char)name[1])) { int n = atoi(name + 1); return n >= 1 && n <= 12 ? VK_F1 + n - 1 : 0; }
    if (_strnicmp(name, "numpad", 6) == 0 && isdigit((unsigned char)name[6]) && !name[7]) return VK_NUMPAD0 + (name[6] - '0');
    struct { const char* n; int vk; } T[] = { { "space", VK_SPACE }, { "tab", VK_TAB }, { "enter", VK_RETURN }, { "home", VK_HOME }, { "end", VK_END }, { "pageup", VK_PRIOR }, { "pagedown", VK_NEXT }, { "insert", VK_INSERT }, { "delete", VK_DELETE }, { "plus", VK_OEM_PLUS }, { "minus", VK_OEM_MINUS }, { "comma", VK_OEM_COMMA }, { "period", VK_OEM_PERIOD }, { "backspace", VK_BACK } };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) if (_stricmp(name, T[i].n) == 0) return T[i].vk;
    return 0;
}
static int KeyOf(int which) { return KeyVK(g_set.keys[which]); }   // markerHeight (1.4.2): the rally marker's height in world units - the editor's marker mesh is a unit cube, so a raw scale (1.4.1's MarkerScale 0.25) stood it a quarter of a unit high, invisible; 25 is the wall banner's height   // spacing in world units: a mouse drag across the screen is hundreds of them (measured 280-768), so 10, not 2
static char g_settingsPath[MAX_PATH] = {0};
static const char* SHAPE_NAMES[] = { "Line", "Circle", "Rectangle", "Wedge", "Square", "Rhombus", "Column of two", "Pincers", "Diamond", "Moth", "Chevron" };
// Экранные имена отдельно от журнальных: в лог пусть идёт английский,
// чтобы отчёт об ошибке остался понятен автору мода. Язык - как у игры (g_uiLang: 0 en, 1 ru, 2 zh).
static const char* SHAPE_NAMES_UI[][3] = { { "Line", "Линия", "横队" }, { "Circle", "Круг", "圆阵" }, { "Rectangle", "Прямоугольник", "方阵" },
    { "Wedge", "Клин", "楔形" }, { "Square", "Каре", "空心方阵" }, { "Rhombus", "Ромб", "菱形" }, { "Column of two", "Колонна по двое", "双列纵队" },
    { "Pincers", "Клещи", "钳形" }, { "Diamond", "Алмаз", "钻石阵" }, { "Moth", "Мотылёк", "飞蛾阵" }, { "Chevron", "Шеврон", "人字阵" } };
static const int SHAPES = 11;
static const int SHAPE_KEY[11] = { KEY_LINE, KEY_CIRCLE, KEY_RECT, KEY_WEDGE, KEY_SQUARE, KEY_RHOMBUS, KEY_COLUMN2, KEY_PINCERS, KEY_DIAMOND, KEY_MOTH, KEY_CHEVRON };
static int g_uiLang = 0;
// Bannerlord's tight/loose toggle: the spacing every shape and the march use
static float Spacing() { return g_set.spacing * (g_set.loose ? 2.0f : 1.0f); }

#include "HoldKey.h"
static HoldKey::Key g_modKey;
static bool ParseModifier() { g_modKey = HoldKey::Parse(g_set.modifier); return g_modKey.vk != 0; }

static void SaveSettings()
{
    FILE* f = 0;
    if (!g_settingsPath[0] || fopen_s(&f, g_settingsPath, "w") != 0 || !f) return;
    fputs("# Formations - настройки\n", f);
    fputs("# Modifier: клавиша, которую держат при перетаскивании правой кнопкой и с клавишами фигур - ALT, CTRL, SHIFT, буква, F1-F12, NUM0-9, сочетание вроде CTRL+B\n", f);
    fprintf(f, "Modifier=%s\n", g_set.modifier);
    fputs("# Spacing: расстояние между бойцами в единицах мира. Перетаскивание через экран - несколько сотен единиц; 10 - плотный строй.\n", f);
    fprintf(f, "Spacing=%.1f\n", g_set.spacing);
    fputs("# Shape: фигура по умолчанию. 0 линия, 1 круг, 2 прямоугольник, 3 клин\n", f);
    fprintf(f, "Shape=%d\n", g_set.shape);
    fputs("# Ghost: 1 = при перетаскивании бледный квадрат показывает, где встанет каждый боец\n", f);
    fprintf(f, "Ghost=%d\n", g_set.ghost);
    fputs("# FollowTask: задача, которую марш выдаёт идущим за ведущим (44 = FOLLOW_PLAYER_ORDER в перечислении SDK). Без нужды не трогать.\n", f);
    fprintf(f, "FollowTask=%d\n", g_set.followTask);
    fputs("# Advance: 1 = на марше отряд держит строй за ведущим; 0 = обычное следование игры, то есть толпой\n", f);
    fprintf(f, "Advance=%d\n", g_set.advance);
    fputs("# Loose: 1 = двойной интервал (свободный строй), 0 = плотный. Переключается модификатором и 5.\n", f);
    fprintf(f, "Loose=%d\n", g_set.loose);
    fputs("# Hold: 1 = построенным включается кнопка игры \"Держать\" (стоят на месте); 0 = встают в строй, но \"Держать\" не трогается\n", f);
    fprintf(f, "Hold=%d\n", g_set.hold);
    fputs("# MarkerHeight: высота метки точки сбора в единицах мира (это статуя из редактора, data/meshes/athene.mesh; настенное знамя - примерно 25). Метка видна, только пока держишь модификатор.\n", f);
    fprintf(f, "MarkerHeight=%.0f\n", g_set.markerHeight);
    fputs("# Клавиши (нажимаются вместе с модификатором): буква или цифра, F1-F12, а также space, tab, enter, home, end, pageup, pagedown, insert, delete, numpad0-9, plus, minus, comma, period. Пусто или непонятное значение = клавиша выключена.\n", f);
    for (int i = 0; i < KEY_COUNT; i++) fprintf(f, "%s=%s\n", KEY_SETTING[i], g_set.keys[i]);
    fclose(f);
}
static void LoadSettings()
{
    _snprintf_s(g_settingsPath, MAX_PATH, _TRUNCATE, "%sFormations_settings.txt", g_dir);
    FILE* f = 0;
    if (fopen_s(&f, g_settingsPath, "r") != 0 || !f) { SaveSettings(); return; }
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        size_t n = strlen(line); while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = 0;
        if (!n || line[0] == '#') continue;
        char* eq = strchr(line, '='); if (!eq) continue; *eq = 0;
        const char* k = line; const char* v = eq + 1;
        if (_stricmp(k, "Modifier") == 0) strcpy_s(g_set.modifier, sizeof(g_set.modifier), v);
        else if (_stricmp(k, "Spacing") == 0) { g_set.spacing = (float)atof(v); if (g_set.spacing < 1.0f) g_set.spacing = 1.0f; if (g_set.spacing > 100) g_set.spacing = 100; }
        else if (_stricmp(k, "Shape") == 0) { g_set.shape = atoi(v); if (g_set.shape < 0 || g_set.shape >= SHAPES) g_set.shape = 0; }
        else if (_stricmp(k, "Ghost") == 0) g_set.ghost = atoi(v) != 0;
        else if (_stricmp(k, "FollowTask") == 0) g_set.followTask = atoi(v);
        else if (_stricmp(k, "Advance") == 0) g_set.advance = atoi(v) != 0;
        else if (_stricmp(k, "Loose") == 0) g_set.loose = atoi(v) != 0;
        else if (_stricmp(k, "Hold") == 0) g_set.hold = atoi(v) != 0;
        else if (_stricmp(k, "Retreat") == 0) g_set.retreat = (float)atof(v);
        else if (_stricmp(k, "MarkerHeight") == 0) { g_set.markerHeight = (float)atof(v); if (g_set.markerHeight < 2.0f) g_set.markerHeight = 2.0f; if (g_set.markerHeight > 300.0f) g_set.markerHeight = 300.0f; }
        else if (_stricmp(k, "MarkerScale") == 0) Log("settings: MarkerScale (1.4.1) is retired - the marker is sized by MarkerHeight now (world units, default 25); the line is ignored");
        else { bool hit = false; for (int i = 0; i < KEY_COUNT && !hit; i++) if (_stricmp(k, KEY_SETTING[i]) == 0 || (i == KEY_LOOSE && _stricmp(k, "KeyTightLoose") == 0)) { strncpy_s(g_set.keys[i], sizeof(g_set.keys[i]), v, _TRUNCATE); hit = true; } }
    }
    fclose(f);
    if (!ParseModifier()) { Log("settings: Modifier=%s is not a key - ALT is used", g_set.modifier); strcpy_s(g_set.modifier, sizeof(g_set.modifier), "ALT"); ParseModifier(); }
    for (int i = 0; i < KEY_COUNT; i++) if (!KeyOf(i)) Log("settings: %s=%s is not a key name - that key is OFF (a letter or digit, F1-F12, space, tab, enter, home, end, pageup, pagedown, insert, delete, numpad0-9, plus, minus, comma, period)", KEY_SETTING[i], g_set.keys[i]);
    Log("keys: %s+%s line, +%s circle, +%s rectangle, +%s wedge, +%s tight/loose, +%s retreat, +%s rally (Formations_settings.txt)", g_set.modifier, g_set.keys[0], g_set.keys[1], g_set.keys[2], g_set.keys[3], g_set.keys[4], g_set.keys[5], g_set.keys[6]);
    SaveSettings();   // the file always carries every key, so a player sees what to edit
}
// A TEXT BOX HAS THE KEYBOARD (1.4.2, 2026-09-09): the game's own hotkeys stand down while a MyGUI widget holds key focus (the
// core's ENGINE-mygui-input-7.md); a plugin that polls GetAsyncKeyState asks MyGUI::InputManager::isFocusKey() first or its keys
// fire inside every text field - the Cuneiform chat, the game's save-name and rename boxes.
typedef void* (*IM_instance_t)(); typedef bool (*IM_bool_t)(void* im);
static IM_instance_t MG_inputManager = 0; static IM_bool_t MG_isFocusKey = 0;
static bool GuiHasKeyboard()
{
    if (!MG_inputManager || !MG_isFocusKey) return false;
    __try { void* im = MG_inputManager(); return PtrOk(im) && MG_isFocusKey(im); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool ModifierHeld() { return g_modKey.vk != 0 && HoldKey::Held(g_modKey); }

// ------------------------------------------------------- the engine doors
struct Vec3 { float x, y, z; };
struct Ray  { Vec3 origin, dir; };
typedef void  (*mainLoop_t)(void* self, float dt);
typedef void  (*piUpdate_t)(void* self);
typedef void  (*camSetPos_t)(void* self, const Vec3* p);
typedef void  (*camRay_t)(void* cam, float x01, float y01, Ray* out);
typedef float (*terrainH_t)(float x, float z);
typedef const void* (*PI_getAllChars_t)(void* pi);            // const lektor<Character*>&
typedef bool  (*PI_isSelected_t)(void* pi, void* obj);
typedef void  (*CH_addJob_t)(void* ch, int task, void* subject, bool shift, bool addDontClear, const Vec3* loc);
typedef Vec3* (*CH_getPos_t)(void* ch, Vec3* out);
typedef int   (*CH_attackers_t)(void* ch);
typedef bool  (*CH_bool_t)(void* ch);

static mainLoop_t  orig_mainLoop = 0;
static piUpdate_t  orig_piUpdate = 0;
static camSetPos_t orig_camSetPos = 0;
static camRay_t    CAM_getRay = 0;
static terrainH_t  UT_terrainHeight = 0;
static PI_getAllChars_t PI_getAllPlayerCharacters = 0;
static PI_isSelected_t  PI_isObjectSelected = 0;
static CH_addJob_t   CH_addJob = 0;
static CH_getPos_t   CH_getPosition = 0;
static CH_attackers_t CH_attackers = 0;
static CH_bool_t     CH_isDown = 0, CH_isUnconcious = 0, CH_isDead = 0;
typedef bool (*CH_combatMode_t)(void* ch, bool a, bool b);   // Character::isInCombatMode(bool, bool) const - weapon out / engaging
static CH_combatMode_t CH_isInCombatMode = 0;
// WHAT THE HOLD BUTTON DOES - read from the binary (Heart --dis, boot 4):
//   OrdersPanel::holdButtonCallback -> PlayerInterface::setOrderSelectedCharacters(StandingOrder 12)
//   -> per selected character: Character::setStandingOrder(12, !getStandingOrder(12))
//   setStandingOrder(12, true):  CharStats+0x12B = 1 (the byte holdPositionMode reads),
//                                the character's CURRENT position is copied into
//                                CharStats+0x130 (the hold location), and if the
//                                OrdersReceiver has no goal of TaskType 16 one is
//                                added - THAT goal is the stance: stand, engage what
//                                comes in range, come back.
//   setStandingOrder(12, false): flag = 0, removeGoal(16).
// Boot 3: setHoldLocation alone set a location and nothing else - no flag, no
// goal - so nobody fought. Boot 4: the button's call PLUS setHoldLocation(slot)
// held, but everyone flickered idle/move inside the circle: the slot's y was the
// terrain height, a few units under their feet, and the hold goal measures its
// distance to a spot it can never quite reach. Owner: "I wonder if just clicking
// hold and then using the formation would work?" - so now the plugin does the
// micro by hand: MOVE to the slot, and the moment a character has ARRIVED and
// stopped, make the button's own call, which snapshots the character's own
// position. No location of the plugin's making exists any more.
//   StandingOrder values seen: 11 (+0x128), 12 hold (+0x12B), 13 passive (+0x12C), 14 (+0x12A), 17 (+0x129)
typedef void (*CH_setStanding_t)(void* ch, int order, bool on);
typedef bool (*CH_getStanding_t)(void* ch, int order);
static CH_setStanding_t CH_setStandingOrder = 0;
static CH_getStanding_t CH_getStandingOrder = 0;
static const int STANDING_HOLD = 12;

static void* g_playerIface = 0;
static const int TASK_MOVE_CUS_ORDERED = 29;

// THE CAMERA: several cameras call setPosition (shadow and reflection passes
// too). Each is remembered with its last position; the one to cast from is the
// one whose position matches the game's own camera position, read from ou.
typedef Vec3* (*GW_getCameraPos_t)(void* gw, Vec3* out);
static GW_getCameraPos_t GW_getCameraPos = 0;
typedef const Vec3* (*camDerivedPos_t)(void* cam);          // const Vector3& - a pointer into the camera
static camDerivedPos_t CAM_getDerivedPos = 0;
static void** KL_ou = 0;
static const int CAMS = 8;
static void* g_camPtr[CAMS]; static Vec3 g_camPos[CAMS]; static int g_camCount = 0;

static void hook_camSetPos(void* self, const Vec3* p)
{
    if (orig_camSetPos) orig_camSetPos(self, p);
    Vec3 v; if (!p || !CopyGuarded(&v, p, sizeof(v))) return;
    for (int i = 0; i < g_camCount; i++) if (g_camPtr[i] == self) { g_camPos[i] = v; return; }
    if (g_camCount < CAMS) { g_camPtr[g_camCount] = self; g_camPos[g_camCount] = v; g_camCount++; }
}
static bool g_camDiag = false;            // one detailed log per drag start

// THE REAL CAMERA (boot 1 of this plugin): the camera the player looks through
// is moved by its scene node and never calls Camera::setPosition - the one that
// did was a helper camera parked at -3.7, 14.3, 9.0, and a ray from there hit
// the terrain at once. The game itself casts the ground under the mouse with
// Camera::getCameraToViewportRay, so BOTH overloads are hooked and the `this`
// the game passes is the camera to cast from - an object from a hook
// parameter, the recipe that has never failed here.
typedef Ray* (*camRayVal_t)(void* cam, Ray* ret, float x01, float y01);      // Ray by value = hidden return pointer after `this`
typedef void (*camRayOut_t)(void* cam, float x01, float y01, Ray* out);
static camRayVal_t orig_camRayVal = 0;
static camRayOut_t orig_camRayOut = 0;
static void* g_castCam = 0;
static unsigned long g_castCamSeen = 0;
static void NoteCastCamera(void* cam)
{
    if (!PtrOk(cam)) return;
    if (cam != g_castCam) { g_castCam = cam; g_castCamSeen = 0; }
    g_castCamSeen++;
}
static Ray* hook_camRayVal(void* cam, Ray* ret, float x01, float y01) { NoteCastCamera(cam); return orig_camRayVal ? orig_camRayVal(cam, ret, x01, y01) : ret; }
static void hook_camRayOut(void* cam, float x01, float y01, Ray* out) { NoteCastCamera(cam); if (orig_camRayOut) orig_camRayOut(cam, x01, y01, out); }

static void* MainCamera()
{
    if (g_castCam)
    {
        if (g_camDiag) Log("camera: the game's own viewport-ray camera %p (seen %lu times)", g_castCam, g_castCamSeen);
        return g_castCam;
    }
    if (g_camCount == 0) return 0;
    Vec3 gp = { 0, 0, 0 }; bool haveGp = false;
    if (KL_ou && GW_getCameraPos && PtrOk(*KL_ou))
    {
        __try { GW_getCameraPos(*KL_ou, &gp); haveGp = true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { haveGp = false; }
    }
    int best = -1; float bd = 1e30f;
    if (haveGp)
        for (int i = 0; i < g_camCount; i++)
        {
            float dx = g_camPos[i].x - gp.x, dy = g_camPos[i].y - gp.y, dz = g_camPos[i].z - gp.z, d = dx * dx + dy * dy + dz * dz;
            if (d < bd) { bd = d; best = i; }
        }
    if (g_camDiag)
    {
        Log("camera: game says %.1f, %.1f, %.1f (%s); %d camera(s) seen:", gp.x, gp.y, gp.z, haveGp ? "read" : "NOT read", g_camCount);
        for (int i = 0; i < g_camCount; i++) Log("   cam %d %p at %.1f, %.1f, %.1f%s", i, g_camPtr[i], g_camPos[i].x, g_camPos[i].y, g_camPos[i].z, i == best ? (bd < 100.0f * 100.0f ? "  <- match" : "  <- nearest, but far") : "");
    }
    if (best >= 0 && bd < 100.0f * 100.0f) return g_camPtr[best];
    return g_camPtr[0];
}
static void hook_piUpdate(void* self) { if (orig_piUpdate) orig_piUpdate(self); if (PtrOk(self)) g_playerIface = self; }

// THE GAME'S OWN MOVE ORDER (read from the binary, Heart --dis): a right-click on
// the ground is PlayerInterface::playerMove(pos, building) -> addOrderSelected
// Characters(building, TaskType 29, null, false, false, pos), which snaps the
// point to the navmesh and then calls, per selected character, vtable+0x318 =
// Character::playerMoveOrderDefault(building, object, pos). The plugin calls
// that per slot - the same order the player's click gives - and hooks
// playerMove so that while the modifier is held (our drag's press) the base
// game's move does not fire and nobody takes off for the drag's first point.
typedef void (*CH_playerMove_t)(void* ch, void* building, void* obj, const Vec3* pos);
static CH_playerMove_t CH_playerMoveOrderDefault = 0;
typedef void (*PI_playerMove_t)(void* pi, const Vec3* pos, void* building);
static PI_playerMove_t orig_playerMove = 0;
static int g_swallowed = 0;
static void AdvanceStop(const char* why);                  // Advance.h, included later
static void hook_playerMove(void* pi, const Vec3* pos, void* building)
{
    if (ModifierHeld()) { if (g_swallowed++ < 5) Log("move: the game's right-click move swallowed (modifier held)"); return; }
    AdvanceStop("the player's own order");
    if (orig_playerMove) orig_playerMove(pi, pos, building);
}

// ----------------------------------------------------------- the ground cast
//
// THE SURFACE UNDER THE CURSOR (1.4.1; owner: the banner "sticks to the ground despite placing it on buildings and walls. you would need
// to see how the ghost buildings place when opening the build menu because they work great"). The ghost is placed from a physHit
// (PreviewBuilding::buildingPlacementUpdate(physHit&, ...)) that PlayerInterface::mouseScan fills with UtilityT::getMouseRay +
// UtilityT::rayTrace(hit, ray, 0x7879e00) - the game's own physics trace, buildings included. All three are KenshiLib exports, and so is
// physHit's constructor: physHit(bool, bool, float maxDistance) - the corpus at 0x140168d30: +0 max distance, +4 aborted, +7 hit, +8 the
// point (Vector3), +0x14 the normal, +0x30 a hand (vtable set by the ctor), +0x50 the group, +0x58 the object; 0x60 bytes. The terrain
// march below stays as the fallback and as the check on SPACE: the trace answers in the game's own space; if its x,z sit far from the
// terrain march's (the floating-origin shift), the last shift is added. Unread until the boot: which space, and the two bools' meaning
// (mouseScan passes a global and 0; getLastGroundTraceResultMT passes 0 and 1).
typedef void* (*PH_ctor_t)(void* hit, bool a, bool b, float maxDist);
typedef void* (*UT_getMouseRay_t)(Ray* out);                          // Ray by value: through the hidden pointer
typedef void  (*UT_rayTrace_t)(void* hit, const Ray* ray, unsigned mask);
typedef unsigned short (*PH_group_t)(void* hit);
typedef void* (*PH_getBuilding_t)(void* hit);
static PH_ctor_t PH_ctor = 0; static UT_getMouseRay_t UT_getMouseRay = 0; static UT_rayTrace_t UT_rayTrace = 0; static PH_group_t PH_group = 0; static PH_getBuilding_t PH_getBuilding = 0;
static const unsigned MOUSE_SCAN_MASK = 0x7879e00u;                   // mouseScan's own mask
static Vec3 g_lastShift = { 0, 0, 0 };                                // the world-minus-Ogre shift the terrain march last measured
static bool CursorToSurface(Vec3& out, const char** what)
{
    if (!PH_ctor || !UT_getMouseRay || !UT_rayTrace) return false;
    unsigned char hit[0x60]; memset(hit, 0, sizeof(hit)); Ray ray = { { 0, 0, 0 }, { 0, 0, 0 } };
    float px = 0, py = 0, pz = 0; bool flag = false; unsigned short group = 0; void* bld = 0;
    __try
    {
        PH_ctor(hit, false, false, 100000.0f);
        UT_getMouseRay(&ray);
        UT_rayTrace(hit, &ray, MOUSE_SCAN_MASK);
        flag = hit[7] != 0;
        memcpy(&px, hit + 8, 4); memcpy(&py, hit + 12, 4); memcpy(&pz, hit + 16, 4);
        if (PH_group) group = PH_group(hit);
        if (PH_getBuilding) bld = PH_getBuilding(hit);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("surface: the game's trace faulted - DISARMED; the terrain march serves"); PH_ctor = 0; return false; }
    if (!flag) return false;
    out.x = px; out.y = py; out.z = pz;
    static char whatBuf[64]; _snprintf_s(whatBuf, sizeof(whatBuf), _TRUNCATE, "%s (group %u)", bld ? "a building" : "the ground", (unsigned)group);
    if (what) *what = whatBuf;
    return true;
}
// The cursor in the game window, 0..1 across the client area; the camera's ray
// through it; the ray marched until it dips under the terrain, then bisected.
static bool CursorToGround(Vec3& out, int* dbgX = 0, int* dbgY = 0)
{
    void* cam = MainCamera();
    if (!cam || !CAM_getRay || !UT_terrainHeight) return false;
    HWND hw = GetForegroundWindow(); POINT p; RECT rc;
    if (!hw || !GetCursorPos(&p) || !ScreenToClient(hw, &p) || !GetClientRect(hw, &rc) || rc.right <= 0 || rc.bottom <= 0) return false;
    if (dbgX) *dbgX = (int)p.x; if (dbgY) *dbgY = (int)p.y;
    float x01 = (float)p.x / (float)rc.right, y01 = (float)p.y / (float)rc.bottom;
    if (x01 < 0 || x01 > 1 || y01 < 0 || y01 > 1) return false;
    Ray ray = { { 0, 0, 0 }, { 0, 0, 0 } };
    // through the ORIGINAL of the out-pointer overload when it is hooked (the hook
    // would otherwise count our own call as the game's), else the export itself
    __try { if (orig_camRayOut) orig_camRayOut(cam, x01, y01, &ray); else CAM_getRay(cam, x01, y01, &ray); }
    __except (EXCEPTION_EXECUTE_HANDLER) { if (g_camDiag) Log("cast: getCameraToViewportRay faulted"); return false; }
    float dl = sqrtf(ray.dir.x * ray.dir.x + ray.dir.y * ray.dir.y + ray.dir.z * ray.dir.z);
    if (dl < 1e-6f) return false;
    ray.dir.x /= dl; ray.dir.y /= dl; ray.dir.z /= dl;
    // OGRE'S SPACE IS NOT THE WORLD'S (boot 2 of this plugin): the ray came from
    // -310, 952, -383 while the game's camera stood at 91625, 834, 24353 - Kenshi
    // renders around a floating origin near the player. The shift is the game's
    // world camera position minus the Ogre camera's derived position, both
    // exported; the ray is moved into world space by it before the march, so
    // the terrain (which answers in world coordinates) and the order agree.
    Vec3 shift = { 0, 0, 0 }; bool haveShift = false;
    if (CAM_getDerivedPos && KL_ou && GW_getCameraPos && PtrOk(*KL_ou))
    {
        Vec3 wc = { 0, 0, 0 }, lc = { 0, 0, 0 };
        __try
        {
            GW_getCameraPos(*KL_ou, &wc);
            const Vec3* lp = CAM_getDerivedPos(cam);
            if (CopyGuarded(&lc, lp, sizeof(lc))) { shift.x = wc.x - lc.x; shift.y = wc.y - lc.y; shift.z = wc.z - lc.z; haveShift = true; }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { haveShift = false; }
        if (g_camDiag) Log("cast: game camera %.1f, %.1f, %.1f; ogre camera %.1f, %.1f, %.1f; shift %.1f, %.1f, %.1f%s", wc.x, wc.y, wc.z, lc.x, lc.y, lc.z, shift.x, shift.y, shift.z, haveShift ? "" : " (NOT read)");
    }
    if (!haveShift) { if (g_camDiag) Log("cast: no world shift - refusing rather than ordering into the wrong space"); return false; }
    g_lastShift = shift;                                    // remembered for the surface trace's space check (1.4.1)
    ray.origin.x += shift.x; ray.origin.y += shift.y; ray.origin.z += shift.z;
    if (g_camDiag)
    {
        float th = 0; __try { th = UT_terrainHeight(ray.origin.x, ray.origin.z); } __except (EXCEPTION_EXECUTE_HANDLER) { th = -9999; }
        Log("cast: cursor %.3f, %.3f -> world ray origin %.1f, %.1f, %.1f dir %.3f, %.3f, %.3f; terrain under the origin %.1f", x01, y01, ray.origin.x, ray.origin.y, ray.origin.z, ray.dir.x, ray.dir.y, ray.dir.z, th);
    }
    __try
    {
        float lo = 0, hi = -1;
        for (float t = 0; t < 40000.0f; t += t < 400.0f ? 8.0f : 40.0f)
        {
            float x = ray.origin.x + ray.dir.x * t, y = ray.origin.y + ray.dir.y * t, z = ray.origin.z + ray.dir.z * t;
            if (y <= UT_terrainHeight(x, z)) { hi = t; break; }
            lo = t;
        }
        if (hi < 0) return false;
        if (hi < 2.0f) { if (g_camDiag) Log("cast: the ray is under the terrain at its origin (t=%.1f) - not a ground point; refused", hi); return false; }   // a hit AT the camera is garbage, and nobody gets ordered to it
        for (int i = 0; i < 18; i++)
        {
            float t = (lo + hi) * 0.5f;
            float x = ray.origin.x + ray.dir.x * t, y = ray.origin.y + ray.dir.y * t, z = ray.origin.z + ray.dir.z * t;
            if (y <= UT_terrainHeight(x, z)) hi = t; else lo = t;
        }
        out.x = ray.origin.x + ray.dir.x * hi; out.z = ray.origin.z + ray.dir.z * hi; out.y = UT_terrainHeight(out.x, out.z);
        if (g_camDiag) Log("cast: hit at t=%.1f -> %.1f, %.1f, %.1f", hi, out.x, out.y, out.z);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ------------------------------------------------------------- the squad
static const int SQUAD_MAX = 64;
static void* g_members[SQUAD_MAX]; static int g_memberCount = 0;   // the selected characters, in selection-list order

static const char* CharBusyWhy(void* ch)  // fighting, down, out or dead: leave it alone; the reason, or null
{
    __try
    {
        if (CH_isDead && CH_isDead(ch)) return "dead";
        if (CH_isUnconcious && CH_isUnconcious(ch)) return "unconscious";
        if (CH_isDown && CH_isDown(ch)) return "down";
        if (CH_attackers && CH_attackers(ch) > 0) return "under attack";
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return "unreadable"; }
}
static bool CharBusy(void* ch) { return CharBusyWhy(ch) != 0; }
static int ReadSelection()
{
    g_memberCount = 0;
    if (!g_playerIface || !PI_getAllPlayerCharacters || !PI_isObjectSelected) return 0;
    __try
    {
        const unsigned char* lek = (const unsigned char*)PI_getAllPlayerCharacters(g_playerIface);
        int count = 0; void** stuff = 0;
        if (!CopyGuarded(&count, lek + 8, sizeof(count)) || !CopyGuarded(&stuff, lek + 16, sizeof(stuff))) return 0;
        if (count < 0 || count > 4096 || !stuff) return 0;
        for (int i = 0; i < count && g_memberCount < SQUAD_MAX; i++)
        {
            void* ch = 0;
            if (!CopyGuarded(&ch, stuff + i, sizeof(ch)) || !PtrOk(ch)) continue;
            if (PI_isObjectSelected(g_playerIface, ch)) g_members[g_memberCount++] = ch;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_memberCount = 0; return 0; }
    return g_memberCount;
}

// --------------------------------------------------------------- the shapes
static Vec3 FollowOffset(int s, int i, int n);   // Follow.h - the follow shapes, also laid by the drag (Layout.h 4..10)
#include "Layout.h"

// nearest-first assignment: each slot in turn takes the closest unassigned
// character, so nobody crosses the whole formation to reach a far slot
static void* g_slotChar[SQUAD_MAX]; static Vec3 g_slotPos[SQUAD_MAX]; static int g_slotCount = 0;
static void Assign(int n, Vec3* slots)
{
    bool used[SQUAD_MAX] = { false };
    Vec3 pos[SQUAD_MAX];
    for (int i = 0; i < n; i++) { pos[i].x = pos[i].y = pos[i].z = 0; if (CH_getPosition) { __try { CH_getPosition(g_members[i], &pos[i]); } __except (EXCEPTION_EXECUTE_HANDLER) { } } }
    g_slotCount = 0;
    for (int s = 0; s < n; s++)
    {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < n; i++)
        {
            if (used[i]) continue;
            float dx = pos[i].x - slots[s].x, dz = pos[i].z - slots[s].z, d = dx * dx + dz * dz;
            if (d < bd) { bd = d; best = i; }
        }
        if (best < 0) break;
        used[best] = true;
        g_slotChar[g_slotCount] = g_members[best]; g_slotPos[g_slotCount] = slots[s];
        if (UT_terrainHeight) { __try { g_slotPos[g_slotCount].y = UT_terrainHeight(slots[s].x, slots[s].z); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        g_slotCount++;
    }
}
// THE ORDER, per slot: the game's Hold on (Character::setStandingOrder 12, the
// Hold button's own call), then the game's own per-character right-click move.
// Owner (boot 6): "make holding alt and dragging simply toggle hold and send
// them to a shape" - the recipe done by hand ("toggled hold and right clicked")
// is the one that readied up, so it is done exactly that way and nothing else:
// no location of the plugin's making, no loop afterwards, the game holds.
static int OrderToSlots()
{
    int sent = 0, held = 0;
    for (int i = 0; i < g_slotCount; i++)
    {
        void* ch = g_slotChar[i]; if (!PtrOk(ch)) continue;
        const char* why = CharBusyWhy(ch);
        if (why && strcmp(why, "under attack") != 0) { Log("   slot %d skipped: %s", i, why); continue; }
        if (CH_setStandingOrder && g_set.hold)
        {
            __try { CH_setStandingOrder(ch, STANDING_HOLD, true); if (CH_getStandingOrder && CH_getStandingOrder(ch, STANDING_HOLD)) held++; }
            __except (EXCEPTION_EXECUTE_HANDLER) { Log("order: setStandingOrder faulted - disarming"); CH_setStandingOrder = 0; }
        }
        __try
        {
            if (CH_playerMoveOrderDefault) CH_playerMoveOrderDefault(ch, 0, 0, &g_slotPos[i]);
            else if (CH_addJob) CH_addJob(ch, TASK_MOVE_CUS_ORDERED, 0, false, false, &g_slotPos[i]);
            else continue;
            sent++;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("order: the move order faulted - disarming"); CH_playerMoveOrderDefault = 0; CH_addJob = 0; return sent; }
    }
    Log("   hold %s for %d of %d; moved by %s", g_set.hold ? "on" : "left alone (Hold=0)", held, g_slotCount, CH_playerMoveOrderDefault ? "Character::playerMoveOrderDefault (the game's own click)" : "Character::addJob");
    return sent;
}

// -------------------------------------------------------------- the drag
static bool  g_dragging = false;
static Vec3  g_dragA = { 0, 0, 0 }, g_dragB = { 0, 0, 0 };
static bool  g_rbDown = false, g_modWasDown = false;
static bool  g_keyDown[10] = { false };
static unsigned long g_lastHoldTick = 0, g_lastHUD = 0;

static void ApplyFormation()
{
    int n = ReadSelection();
    AdvanceStop("a formation");
    if (n == 0) { Log("formation: nothing selected"); return; }
    // a drag is made near the squad: a line more than 3,000 units from the first
    // selected character is a cast gone wrong, and nobody is sent there
    if (CH_getPosition)
    {
        Vec3 p = { 0, 0, 0 }; bool okp = false;
        __try { CH_getPosition(g_members[0], &p); okp = true; } __except (EXCEPTION_EXECUTE_HANDLER) { }
        float dx = g_dragA.x - p.x, dz = g_dragA.z - p.z;
        if (okp && dx * dx + dz * dz > 3000.0f * 3000.0f) { Log("formation: REFUSED - the drag (%.0f, %.0f) is %.0f units from the squad (%.0f, %.0f); the cast is wrong", g_dragA.x, g_dragA.z, sqrtf(dx * dx + dz * dz), p.x, p.z); return; }
    }
    Vec3 slots[SQUAD_MAX];
    int k = Layout(g_set.shape, n, g_dragA, g_dragB, Spacing(), slots);
    Assign(k, slots);
    int sent = OrderToSlots();
    Log("formation: %s for %d selected, %d ordered (drag %.0f,%.0f -> %.0f,%.0f, %.0f long)%s",
        SHAPE_NAMES[g_set.shape], n, sent, g_dragA.x, g_dragA.z, g_dragB.x, g_dragB.z,
        sqrtf((g_dragB.x - g_dragA.x) * (g_dragB.x - g_dragA.x) + (g_dragB.z - g_dragA.z) * (g_dragB.z - g_dragA.z)), "; hold on");
}

#include "Ghost.h"
#include "Moves.h"
static void FollowTick();   // Follow.h (17.10.2026): shapes for following the leader

static float DragLen() { float dx = g_dragB.x - g_dragA.x, dz = g_dragB.z - g_dragA.z; return sqrtf(dx * dx + dz * dz); }
static void hook_mainLoop(void* self, float dt)
{
    if (orig_mainLoop) orig_mainLoop(self, dt);
    bool mod = ModifierHeld() && !GuiHasKeyboard();   // no hotkey while a text box types (1.4.2; the Cuneiform chat's boot 7)
    bool rb  = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    ReadoutUpdate(mod);
    RallyFlagUpdate(mod);
    AdvanceTick(false);
    FollowTick();
    MedicTick();

    // modifier + the shape keys; + the tight/loose key (1.4.3: every key from the settings; 0 = off)
    static bool shapeKeyDown[16] = { false };
    for (int s = 0; s < SHAPES; s++)
    {
        int vk = KeyOf(SHAPE_KEY[s]); bool d = mod && vk && (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (d && !shapeKeyDown[s]) { g_set.shape = s; SaveSettings(); Log("shape: %s", SHAPE_NAMES[g_set.shape]); }
        shapeKeyDown[s] = d;
    }
    {
        int vk = KeyOf(KEY_LOOSE); bool d = mod && vk && (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (d && !g_keyDown[5]) { g_set.loose = !g_set.loose; SaveSettings(); Log("spacing: %s (%.0f units)", g_set.loose ? "loose" : "tight", Spacing()); }
        g_keyDown[5] = d;
    }
    {   // modifier + the retreat key: retreat, carrying the wounded
        int vk = KeyOf(KEY_RETREAT); bool d = mod && vk && (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (d && !g_keyDown[7]) Retreat();
        g_keyDown[7] = d;
    }
    {   // modifier + the rally key: the rally point under the cursor
        int vk = KeyOf(KEY_RALLY); bool d = mod && vk && (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (d && !g_keyDown[8]) SetRally();
        g_keyDown[8] = d;
    }

    // the drag: modifier + right button down starts it, release lays the formation
    if (rb && !g_rbDown && mod)
    {
        int cx = 0, cy = 0;
        g_camDiag = true;                                  // one detailed trace per drag start, for the record
        if (ReadSelection() > 0 && CH_getPosition) { Vec3 p = { 0, 0, 0 }; __try { CH_getPosition(g_members[0], &p); } __except (EXCEPTION_EXECUTE_HANDLER) { } Log("drag: first selected character at %.1f, %.1f, %.1f", p.x, p.y, p.z); }
        if (CursorToGround(g_dragA, &cx, &cy)) { g_dragging = true; g_dragB = g_dragA; }
        else Log("drag: no ground under the cursor (client %d,%d; cameras seen %d; the game's cast camera %s)", cx, cy, g_camCount, g_castCam ? "known" : "NOT seen yet - move the mouse over the world first");
        g_camDiag = false;
    }
    if (rb && g_dragging)
    {
        Vec3 b; if (CursorToGround(b)) g_dragB = b;
        if (g_set.ghost && DragLen() >= 4.0f) GhostUpdate(g_memberCount, g_dragA, g_dragB); else GhostHide();
    }
    // release: a drag is the shape; a click (under 4 units of drag) is the orderly move
    if (!rb && g_rbDown && g_dragging) { g_dragging = false; GhostHide(); if (DragLen() < 4.0f) OrderlyMove(g_dragA); else ApplyFormation(); }
    g_rbDown = rb;
}

// ---------------------------------------------------------------- startup
// ------------------------------------------------- ModConfigMenu page
// A page in the MCM tab of the game's Options window. Without MCM nothing changes: the settings live in
// Formations_settings.txt as before. The rows call straight into g_set + SaveSettings(), so a change applies at once.
// Formations links no KenshiLib (and so no Localization.h): the texts sit here in three languages and the language is
// the game's own choice, read from settings.cfg (language=ru_RU).
#include "ModConfigMenu.h"

namespace formations_mcm
{
    enum Lang { EN = 0, RU = 1, ZH = 2 };

    static int CurrentLang()
    {
        FILE* f = 0;
        // Workshop DLLs need not be on the game's drive or at mods/<name>.
        // Locate settings relative to the executable, including RE_Kenshi's
        // nested executable, without changing the process working directory.
        wchar_t path[MAX_PATH];
        const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
        if (!length || length >= MAX_PATH) return EN;
        for (int depth=0; depth<4; ++depth)
        {
            wchar_t* slash=wcsrchr(path,L'\\');
            if (!slash) break;
            *(slash+1)=0;
            wchar_t candidate[MAX_PATH];
            if (_snwprintf_s(candidate,MAX_PATH,_TRUNCATE,L"%ssettings.cfg",path)<0) return EN;
            if (_wfopen_s(&f,candidate,L"r")==0 && f) break;
            *slash=0;
        }
        if (!f) return EN;
        char line[256];
        int lang = EN;
        while (fgets(line, sizeof(line), f))
        {
            if (_strnicmp(line, "language=", 9) != 0) continue;
            if (_strnicmp(line + 9, "ru", 2) == 0) lang = RU;
            else if (_strnicmp(line + 9, "zh", 2) == 0) lang = ZH;
            break;
        }
        fclose(f);
        return lang;
    }

    struct Text { const char* t[3]; };
    static int g_lang = EN;
    static const char* T(const Text& x) { return x.t[g_lang]; }

    static const Text kInfo = {{ "Bannerlord-style formations: hold the modifier and drag with the right mouse button to line up the selected squad",
                                 "Строй как в Bannerlord: держите модификатор и тяните правой кнопкой - выделенные встанут вдоль линии",
                                 "类似《骑马与砍杀》的阵型：按住修饰键并用右键拖动，所选角色会沿线排列" }};
    static const Text kSecMain = {{ "Formation", "Строй", "阵型" }};
    static const Text kSecKeys = {{ "Keys (pressed together with the modifier)", "Клавиши (вместе с модификатором)", "按键（与修饰键同时按下）" }};
    static const Text kModifier = {{ "Modifier", "Модификатор", "修饰键" }};
    static const Text kModifierTip = {{ "Key held while dragging with the right mouse button and with the shape keys. Click and press any key or combination. SHIFT clashes with the game's own shift-click: a slightly dragged click lines the squad up",
                                       "Клавиша, которую держат при перетаскивании правой кнопкой и с клавишами фигур. Нажмите на поле и затем любую клавишу или сочетание. SHIFT мешает очереди приказов игры: чуть смазанный щелчок строит отряд",
                                       "右键拖动及按阵型键时需按住的键。点击后按下任意键或组合键。SHIFT 会与游戏自带的 Shift 点击冲突" }};
    static const Text kHold = {{ "Hold position after forming up", "Держать позицию после построения", "列阵后原地坚守" }};
    static const Text kHoldTip = {{ "On: everyone placed in a formation gets the game's Hold button on, so they stay in their places and fight from there; the Hold button or NUM1 releases it. Off: they form up, but Hold is not touched",
                                   "Включено: всем, кого поставили в строй, включается кнопка игры \"Держать\" - стоят на своих местах и бьются оттуда; снимается той же кнопкой или NUM1. Выключено: встают в строй, но \"Держать\" не трогается",
                                   "开启：列阵的角色会打开游戏的“坚守”按钮，留在原位作战；用该按钮或 NUM1 取消。关闭：只列阵，不改动“坚守”" }};
    static const Text kSpacing = {{ "Spacing", "Интервал", "间距" }};
    static const Text kSpacingTip = {{ "Distance between fighters in world units; 10 is a tight line. Loose formation doubles it",
                                      "Расстояние между бойцами в единицах мира; 10 - плотный строй. Свободный строй удваивает его",
                                      "队员之间的距离（世界单位）；10 为紧密阵型。松散阵型时加倍" }};
    static const Text kShape = {{ "Default shape", "Фигура по умолчанию", "默认阵型" }};
    static const Text kShapeTip = {{ "Shape used when you drag without picking one first", "Фигура, в которую встаёт отряд, если её не выбрать заранее", "未预先选择时拖动所用的阵型" }};
    static const Text kShapeNames[11] = {{{ "Line", "Линия", "横队" }}, {{ "Circle", "Круг", "圆阵" }},
                                        {{ "Rectangle", "Прямоугольник", "方阵" }}, {{ "Wedge", "Клин", "楔形" }},
                                        {{ "Square", "Каре", "空心方阵" }}, {{ "Rhombus", "Ромб", "菱形" }},
                                        {{ "Column of two", "Колонна по двое", "双列纵队" }}, {{ "Pincers", "Клещи", "钳形" }},
                                        {{ "Diamond", "Алмаз", "钻石阵" }}, {{ "Moth", "Мотылёк", "飞蛾阵" }}, {{ "Chevron", "Шеврон", "人字阵" }}};
    static const Text kGhost = {{ "Show places while dragging", "Показывать места при перетаскивании", "拖动时显示站位" }};
    static const Text kGhostTip = {{ "A pale square marks where each fighter will stand", "Бледный квадрат показывает, где встанет каждый боец", "用淡色方块标出每名队员的站位" }};
    static const Text kAdvance = {{ "Keep formation on the march", "Держать строй на марше", "行军时保持阵型" }};
    static const Text kAdvanceTip = {{ "A march is the modifier plus a plain right-click (no drag) on the ground. On: the slowest fighter leads to that point, the rest walk behind him in the chosen shape, and at the point they stand up in it. Off: the game's usual follow - everyone walks there as a crowd",
                                      "Марш - это модификатор и простой щелчок правой кнопкой по земле, без перетаскивания. Включено: к точке ведёт самый медленный боец, остальные идут за ним выбранной фигурой и на месте в неё встают. Выключено: обычное следование игры - все идут туда толпой",
                                      "行军：按住修饰键并在地面上右键单击（不拖动）。开启：最慢的队员带队前往该点，其余人按所选阵型跟随并在终点列阵。关闭：原版跟随，成群前往" }};
    static const Text kLoose = {{ "Loose formation", "Свободный строй", "松散阵型" }};
    static const Text kLooseTip = {{ "On: fighters stand twice the Spacing apart (loose ranks, less crowding in a fight). Off: exactly the Spacing (tight ranks). Also switched in game by the modifier and the tight/loose key",
                                    "Включено: бойцы стоят на двойном интервале (редкий строй, меньше толкотни в бою). Выключено: ровно на интервале (плотный строй). В игре переключается модификатором и клавишей \"Плотный / свободный\"",
                                    "开启：队员间距为设定间距的两倍（松散）。关闭：按设定间距（紧密）。游戏中也可用修饰键加紧密/松散键切换" }};
    static const Text kMarker = {{ "Rally marker height", "Высота метки сбора", "集结标记高度" }};
    static const Text kMarkerTip = {{ "Height of the rally point marker in world units; a wall banner is about 25. Seen only while the modifier is held",
                                     "Высота метки точки сбора в единицах мира; настенное знамя - около 25. Видна, только пока держишь модификатор",
                                     "集结点标记的高度（世界单位）；墙上旗帜约为 25。仅在按住修饰键时可见" }};
    static const Text kKeyNames[KEY_COUNT] = {
        {{ "Line", "Линия", "横队" }}, {{ "Circle", "Круг", "圆阵" }}, {{ "Rectangle", "Прямоугольник", "方阵" }},
        {{ "Wedge", "Клин", "楔形" }}, {{ "Tight / loose", "Плотный / свободный", "紧密 / 松散" }},
        {{ "Retreat", "Отступление", "撤退" }}, {{ "Rally", "Сбор", "集结" }},
        {{ "Square", "Каре", "空心方阵" }}, {{ "Rhombus", "Ромб", "菱形" }}, {{ "Column of two", "Колонна по двое", "双列纵队" }},
        {{ "Pincers", "Клещи", "钳形" }}, {{ "Diamond", "Алмаз", "钻石阵" }}, {{ "Moth", "Мотылёк", "飞蛾阵" }}, {{ "Chevron", "Шеврон", "人字阵" }} };
    static const Text kKeyTip = {{ "A letter or digit, F1-F12, space, tab, enter, home, end, pageup, pagedown, insert, delete, numpad0-9, plus, minus, comma, period. Empty - key off",
                                   "Буква или цифра, F1-F12, space, tab, enter, home, end, pageup, pagedown, insert, delete, numpad0-9, plus, minus, comma, period. Пусто - клавиша выключена",
                                   "字母或数字、F1-F12、space、tab、enter、home、end、pageup、pagedown、insert、delete、numpad0-9、plus、minus、comma、period。留空则禁用该键" }};

    static void Err(char* err, unsigned size, const char* text) { if (err && size) strncpy_s(err, size, text, _TRUNCATE); }

    // MCM hands over OIS key codes (DirectInput scan codes); the settings keep the text MCM itself
    // prints for them, which is also what HoldKey reads.
    struct KeyName { int code; const char* name; };
    static const KeyName kOis[] = {
        { 0x38, "ALT" }, { 0x1D, "CTRL" }, { 0x2A, "SHIFT" },
        { 0x1E, "A" }, { 0x30, "B" }, { 0x2E, "C" }, { 0x20, "D" }, { 0x12, "E" }, { 0x21, "F" }, { 0x22, "G" },
        { 0x23, "H" }, { 0x17, "I" }, { 0x24, "J" }, { 0x25, "K" }, { 0x26, "L" }, { 0x32, "M" }, { 0x31, "N" },
        { 0x18, "O" }, { 0x19, "P" }, { 0x10, "Q" }, { 0x13, "R" }, { 0x1F, "S" }, { 0x14, "T" }, { 0x16, "U" },
        { 0x2F, "V" }, { 0x11, "W" }, { 0x2D, "X" }, { 0x15, "Y" }, { 0x2C, "Z" },
        { 0x0B, "0" }, { 0x02, "1" }, { 0x03, "2" }, { 0x04, "3" }, { 0x05, "4" }, { 0x06, "5" }, { 0x07, "6" },
        { 0x08, "7" }, { 0x09, "8" }, { 0x0A, "9" },
        { 0x3B, "F1" }, { 0x3C, "F2" }, { 0x3D, "F3" }, { 0x3E, "F4" }, { 0x3F, "F5" }, { 0x40, "F6" },
        { 0x41, "F7" }, { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" }, { 0x57, "F11" }, { 0x58, "F12" },
        { 0x39, "SPACE" }, { 0x0F, "TAB" }, { 0x1C, "ENTER" }, { 0xD2, "INSERT" }, { 0xD3, "DELETE" },
        { 0xC7, "HOME" }, { 0xCF, "END" }, { 0xC9, "PAGEUP" }, { 0xD1, "PAGEDOWN" }, { 0xC8, "UP" },
        { 0xD0, "DOWN" }, { 0xCB, "LEFT" }, { 0xCD, "RIGHT" }, { 0x29, "`" }, { 0x0C, "-" }, { 0x0D, "=" },
        { 0x1A, "[" }, { 0x1B, "]" }, { 0x27, ";" }, { 0x28, "'" }, { 0x33, "," }, { 0x34, "." }, { 0x35, "/" },
        { 0x2B, "\\" },
        { 0x52, "NUM0" }, { 0x4F, "NUM1" }, { 0x50, "NUM2" }, { 0x51, "NUM3" }, { 0x4B, "NUM4" }, { 0x4C, "NUM5" },
        { 0x4D, "NUM6" }, { 0x47, "NUM7" }, { 0x48, "NUM8" }, { 0x49, "NUM9" },
        { 0x1D, "LCTRL" }, { 0x9D, "RCTRL" }, { 0x2A, "LSHIFT" }, { 0x36, "RSHIFT" }, { 0x38, "LALT" }, { 0xB8, "RALT" },
    };
    static const int kOisCount = sizeof(kOis) / sizeof(kOis[0]);
    static int __cdecl GetModifier(void*, MCM_Key* out)
    {
        out->keycode = 0x38; out->modifiers = 0;
        char t[32]; size_t n = 0;
        for (const char* p = g_set.modifier; *p && n + 1 < sizeof(t); ++p) if (*p != ' ') t[n++] = (char)toupper((unsigned char)*p);
        t[n] = 0;
        const char* key = t;
        for (;;)
        {
            const char* plus = strchr(key, '+');
            if (!plus || !plus[1]) break;
            if (_strnicmp(key, "CTRL+", 5) == 0) out->modifiers |= MCM_KEY_CTRL;
            else if (_strnicmp(key, "SHIFT+", 6) == 0) out->modifiers |= MCM_KEY_SHIFT;
            else if (_strnicmp(key, "ALT+", 4) == 0) out->modifiers |= MCM_KEY_ALT;
            key = plus + 1;
        }
        for (int i = 0; i < kOisCount; i++) if (strcmp(kOis[i].name, key) == 0) { out->keycode = kOis[i].code; return 0; }
        out->modifiers = 0;
        return 0;
    }
    static int __cdecl SetModifier(void*, MCM_Key v, char* err, unsigned size)
    {
        // Without a modifier every right-button drag would line the squad up - a key is required.
        if (v.keycode < 0) { Err(err, size, "the modifier cannot be empty"); return 1; }
        const char* name = 0;
        for (int i = 0; i < kOisCount && !name; i++) if (kOis[i].code == v.keycode) name = kOis[i].name;
        if (!name) { Err(err, size, "this key cannot be the modifier"); return 1; }
        char t[32];
        _snprintf_s(t, sizeof(t), _TRUNCATE, "%s%s%s%s", (v.modifiers & MCM_KEY_CTRL) ? "CTRL+" : "",
                    (v.modifiers & MCM_KEY_SHIFT) ? "SHIFT+" : "", (v.modifiers & MCM_KEY_ALT) ? "ALT+" : "", name);
        char old[sizeof(g_set.modifier)]; strcpy_s(old, sizeof(old), g_set.modifier);
        strcpy_s(g_set.modifier, sizeof(g_set.modifier), t);
        if (!ParseModifier()) { strcpy_s(g_set.modifier, sizeof(g_set.modifier), old); ParseModifier(); Err(err, size, "this key cannot be the modifier"); return 1; }
        SaveSettings(); return 0;
    }
    static int __cdecl GetSpacing(void*, float* out) { *out = g_set.spacing; return 0; }
    static int __cdecl SetSpacing(void*, float v, char*, unsigned)
    {
        if (v < 1.0f) v = 1.0f; if (v > 100.0f) v = 100.0f;
        g_set.spacing = v; SaveSettings(); return 0;
    }
    static int __cdecl GetShape(void*, int* out) { *out = g_set.shape; return 0; }
    static int __cdecl SetShape(void*, int v, char* err, unsigned size)
    {
        if (v < 0 || v >= SHAPES) { Err(err, size, "bad shape"); return 1; }
        g_set.shape = v; SaveSettings(); return 0;
    }
    static int __cdecl GetFlag(void* ud, int* out) { *out = *static_cast<int*>(ud) ? 1 : 0; return 0; }
    static int __cdecl SetFlag(void* ud, int v, char*, unsigned) { *static_cast<int*>(ud) = v ? 1 : 0; SaveSettings(); return 0; }
    static int __cdecl GetMarker(void*, int* out) { *out = (int)(g_set.markerHeight + 0.5f); return 0; }
    static int __cdecl SetMarker(void*, int v, char*, unsigned)
    {
        if (v < 2) v = 2; if (v > 300) v = 300;
        g_set.markerHeight = (float)v; SaveSettings(); return 0;
    }
    static int g_keyIndex[KEY_COUNT] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
    static int __cdecl GetKey(void* ud, char* out, unsigned size)
    {
        strncpy_s(out, size, g_set.keys[*static_cast<int*>(ud)], _TRUNCATE); return 0;
    }
    static int __cdecl SetKey(void* ud, const char* v, char* err, unsigned size)
    {
        const int i = *static_cast<int*>(ud);
        if (!v) v = "";
        while (*v == ' ') v++;
        if (strlen(v) >= sizeof(g_set.keys[i])) { Err(err, size, "key name too long"); return 1; }
        if (v[0] && !KeyVK(v)) { Err(err, size, "unknown key name"); return 1; }
        strcpy_s(g_set.keys[i], sizeof(g_set.keys[i]), v); SaveSettings(); return 0;
    }
}

#include "Follow.h"

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    using namespace formations_mcm;
    if (api->version < 3) return;
    g_lang = CurrentLang();
    const bool defs = api->version >= 5;

    api->beginMod(api, "Formations", "Formations", NULL, NULL);
    if (api->version >= 2) api->info(api, T(kInfo));

    api->section(api, T(kSecMain));
    api->hotkeyFn(api, T(kModifier), T(kModifierTip), &GetModifier, &SetModifier, NULL, 0);
    if (defs) api->defaultValue(api, "ALT");
    api->numberFn(api, T(kSpacing), T(kSpacingTip), &GetSpacing, &SetSpacing, NULL, 1.0f, 100.0f, 1, 0);
    if (defs) api->defaultValue(api, "10.0");
    static const int shapeValues[11] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    const char* shapeLabels[11];
    for (int s = 0; s < 11; s++) shapeLabels[s] = T(kShapeNames[s]);
    api->choiceFn(api, T(kShape), T(kShapeTip), &GetShape, &SetShape, NULL, shapeValues, shapeLabels, 11, 0);
    if (defs) api->defaultValue(api, "0");
    api->toggleFn(api, T(kLoose), T(kLooseTip), &GetFlag, &SetFlag, &g_set.loose, 0);
    if (defs) api->defaultValue(api, "1");
    api->toggleFn(api, T(kHold), T(kHoldTip), &GetFlag, &SetFlag, &g_set.hold, 0);
    if (defs) api->defaultValue(api, "1");
    api->toggleFn(api, T(kAdvance), T(kAdvanceTip), &GetFlag, &SetFlag, &g_set.advance, 0);
    if (defs) api->defaultValue(api, "1");
    api->toggleFn(api, T(kGhost), T(kGhostTip), &GetFlag, &SetFlag, &g_set.ghost, 0);
    if (defs) api->defaultValue(api, "1");
    api->integerFn(api, T(kMarker), T(kMarkerTip), &GetMarker, &SetMarker, NULL, 2, 300, 0);
    if (defs) api->defaultValue(api, "25");

    api->section(api, T(kSecKeys));
    static const char* const keyDefaults[KEY_COUNT] = { "1", "2", "3", "4", "5", "X", "V", "6", "7", "8", "9", "0", "minus", "plus" };
    for (int i = 0; i < KEY_COUNT; i++)
    {
        api->textFn(api, T(kKeyNames[i]), T(kKeyTip), &GetKey, &SetKey, &g_keyIndex[i], 0);
        if (defs) api->defaultValue(api, keyDefaults[i]);
    }

    // 17.10.2026: a follow shape for each squad of the loaded game (Follow.h; MCM v6 builds the rows on every show)
    if (api->version >= 6 && GD_idataIndex)
    {
        static const Text kSecSquads = {{ "Squads - follow formation", "Отряды - строй при следовании", "小队 - 跟随阵型" }};
        api->section(api, T(kSecSquads));
        api->rows(api, &FollowRows, NULL);
    }
}

__declspec(dllexport) void startPlugin()
{
    LogInit();
    LoadSettings();
    g_uiLang = formations_mcm::CurrentLang();
    HMODULE kl = GetModuleHandleA("KenshiLib.dll"); if (!kl) kl = LoadLibraryA("KenshiLib.dll");
    if (!kl) { Log("KenshiLib.dll not loaded - nothing to do"); return; }
    KL_AddHook = (AddHook_t)GetProcAddress(kl, "?AddHook@KenshiLib@@YA?AW4HookStatus@1@PEAX0PEAPEAX@Z");
    KL_GetRealAddress = (GetRealAddress_t)GetProcAddress(kl, "?GetRealAddress@KenshiLib@@YA_JPEAX@Z");
    if (!KL_AddHook) { Log("KenshiLib::AddHook not found - aborting"); return; }
    HMODULE ogre = GetModuleHandleA("OgreMain_x64.dll"); if (!ogre) ogre = LoadLibraryA("OgreMain_x64.dll");
    BannerResolve(ogre);   // 1.4: the rally banner's scene API (Flag.h)

    Hook("GameWorld main loop", GetProcAddress(kl, "?_NV_mainLoop_GPUSensitiveStuff@GameWorld@@QEAAXM@Z"), (void*)&hook_mainLoop, (void**)&orig_mainLoop);
    Hook("PlayerInterface::update", GetProcAddress(kl, "?update@PlayerInterface@@QEAAXXZ"), (void*)&hook_piUpdate, (void**)&orig_piUpdate);
    FollowInstall(kl);
    if (ogre) Hook("Camera::setPosition", GetProcAddress(ogre, "?setPosition@Camera@Ogre@@QEAAXAEBVVector3@2@@Z"), (void*)&hook_camSetPos, (void**)&orig_camSetPos);

    CAM_getRay = ogre ? (camRay_t)GetProcAddress(ogre, "?getCameraToViewportRay@Camera@Ogre@@QEBAXMMPEAVRay@2@@Z") : 0;
    PH_ctor = (PH_ctor_t)Real(GetProcAddress(kl, "??0physHit@@QEAA@_N0M@Z"));                                   // 1.4.1: the surface under the cursor, the ghost building's own trace
    UT_getMouseRay = (UT_getMouseRay_t)Real(GetProcAddress(kl, "?getMouseRay@UtilityT@@SA?AVRay@Ogre@@XZ"));
    UT_rayTrace = (UT_rayTrace_t)Real(GetProcAddress(kl, "?rayTrace@UtilityT@@SAXAEAVphysHit@@AEBVRay@Ogre@@I@Z"));
    PH_group = (PH_group_t)Real(GetProcAddress(kl, "?group@physHit@@QEBAGXZ"));
    PH_getBuilding = (PH_getBuilding_t)Real(GetProcAddress(kl, "?getBuilding@physHit@@QEAAPEAVBuilding@@XZ"));
    Log("  the surface under the cursor       %s", (PH_ctor && UT_getMouseRay && UT_rayTrace) ? "reachable (UtilityT::getMouseRay + rayTrace into a physHit - the ghost building's own)" : "NOT reachable - the terrain march serves");
    GhostInit(GetModuleHandleA("MyGUIEngine_x64.dll"), ogre);
    MovesInit(kl, GetModuleHandleA("MyGUIEngine_x64.dll"));
    {
        HMODULE mg = GetModuleHandleA("MyGUIEngine_x64.dll");
        if (mg)
        {
            MG_inputManager = (IM_instance_t)GetProcAddress(mg, "?getInstancePtr@?$Singleton@VInputManager@MyGUI@@@MyGUI@@SAPEAVInputManager@2@XZ");
            MG_isFocusKey = (IM_bool_t)GetProcAddress(mg, "?isFocusKey@InputManager@MyGUI@@QEBA_NXZ");
        }
        Log("  hotkeys stand down while a text box types   %s", (MG_inputManager && MG_isFocusKey) ? "yes (InputManager::isFocusKey)" : "NO - the keys fire inside text fields");
    }
    CH_playerMoveOrderDefault = (CH_playerMove_t)Real(GetProcAddress(kl, "?playerMoveOrderDefault@Character@@UEAAXPEAVBuilding@@PEAVRootObject@@AEBVVector3@Ogre@@@Z"));
    Log("  the game's own move order          %s", CH_playerMoveOrderDefault ? "reachable (Character::playerMoveOrderDefault)" : "NOT reachable - addJob instead");
    Hook("PlayerInterface::playerMove", GetProcAddress(kl, "?playerMove@PlayerInterface@@QEAAXAEBVVector3@Ogre@@PEAVBuilding@@@Z"), (void*)&hook_playerMove, (void**)&orig_playerMove);
    CAM_getDerivedPos = ogre ? (camDerivedPos_t)GetProcAddress(ogre, "?getDerivedPosition@Camera@Ogre@@QEBAAEBVVector3@2@XZ") : 0;
    Log("  camera derived position            %s", CAM_getDerivedPos ? "reachable" : "NOT reachable - no world shift, casts refused");
    if (ogre)
    {
        Hook("Camera::getCameraToViewportRay (val)", GetProcAddress(ogre, "?getCameraToViewportRay@Camera@Ogre@@QEBA?AVRay@2@MM@Z"), (void*)&hook_camRayVal, (void**)&orig_camRayVal);
        Hook("Camera::getCameraToViewportRay (out)", GetProcAddress(ogre, "?getCameraToViewportRay@Camera@Ogre@@QEBAXMMPEAVRay@2@@Z"), (void*)&hook_camRayOut, (void**)&orig_camRayOut);
    }
    UT_terrainHeight = (terrainH_t)Real(GetProcAddress(kl, "?getTerrainHeight@UtilityT@@SAMMM@Z"));
    GW_getCameraPos = (GW_getCameraPos_t)Real(GetProcAddress(kl, "?getCameraPos@GameWorld@@QEBA?BVVector3@Ogre@@XZ"));
    KL_ou = (void**)GetProcAddress(kl, "?ou@@3PEAVGameWorld@@EA");
    PI_getAllPlayerCharacters = (PI_getAllChars_t)Real(GetProcAddress(kl, "?getAllPlayerCharacters@PlayerInterface@@QEBAAEBV?$lektor@PEAVCharacter@@@@XZ"));
    PI_isObjectSelected = (PI_isSelected_t)Real(FindExportByPrefix(kl, "?isObjectSelected@PlayerInterface@@"));
    CH_addJob = (CH_addJob_t)Real(GetProcAddress(kl, "?addJob@Character@@QEAAXW4TaskType@@PEAVRootObject@@_N2AEBVVector3@Ogre@@@Z"));
    CH_getPosition = (CH_getPos_t)Real(GetProcAddress(kl, "?getPosition@Character@@UEAA?AVVector3@Ogre@@XZ"));
    CH_attackers = (CH_attackers_t)Real(GetProcAddress(kl, "?getAllAttackersCount@Character@@QEAAHXZ"));
    CH_isDown = (CH_bool_t)Real(GetProcAddress(kl, "?isDown@Character@@QEAA_NXZ"));
    CH_isUnconcious = (CH_bool_t)Real(FindExportByPrefix(kl, "?isUnconcious@Character@@"));
    CH_isDead = (CH_bool_t)Real(FindExportByPrefix(kl, "?isDead@Character@@"));
    CH_isInCombatMode = (CH_combatMode_t)Real(GetProcAddress(kl, "?isInCombatMode@Character@@QEBA_N_N0@Z"));
    Log("  combat mode (weapon out)           %s", CH_isInCombatMode ? "reachable" : "NOT reachable - hold pauses on attackers only");
    CH_setStandingOrder = (CH_setStanding_t)Real(GetProcAddress(kl, "?setStandingOrder@Character@@UEAAXW4StandingOrder@MessageForB@@_N@Z"));
    CH_getStandingOrder = (CH_getStanding_t)Real(GetProcAddress(kl, "?getStandingOrder@Character@@QEBA_NW4StandingOrder@MessageForB@@@Z"));
    Log("  the game's hold stance             %s", CH_setStandingOrder ? "reachable (Character::setStandingOrder 12, the Hold button's call)" : "NOT reachable - the plugin's own regroup only");

    Log("  camera ray                         %s", CAM_getRay ? "reachable" : "NOT reachable");
    Log("  terrain height (static)            %s", UT_terrainHeight ? "reachable" : "NOT reachable");
    Log("  game camera position / ou          %s", (GW_getCameraPos && KL_ou) ? "reachable" : "NOT reachable (first camera seen is used)");
    Log("  selection (players + isSelected)   %s", (PI_getAllPlayerCharacters && PI_isObjectSelected) ? "reachable" : "NOT reachable");
    Log("  orders (Character::addJob)         %s", CH_addJob ? "reachable" : "NOT reachable");
    Log("  combat/down/dead                   %s", (CH_attackers && CH_isDown && CH_isUnconcious && CH_isDead) ? "reachable" : "partly reachable");
    Log("\narmed - hold %s and drag with the right mouse button (the shape, game Hold on if Hold=1); %s + right-click = march, slowest leads; %s+1..4 shape (now: %s); %s+5 tight/loose (now: %s)\n",
        g_set.modifier, g_set.modifier, g_set.modifier, SHAPE_NAMES[g_set.shape], g_set.modifier, g_set.loose ? "loose" : "tight");
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { g_self = mod; DisableThreadLibraryCalls(mod); }
    return TRUE;
}
