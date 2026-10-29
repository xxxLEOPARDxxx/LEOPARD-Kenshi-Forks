#define KLOC_DOMAIN "kill_counter"
#include <Localization.h>
#include <PluginSettings.h>
#include <PluginDiagnostics.h>
#include <ModConfigMenu.h>

#include <Debug.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Character.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Damages.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/combat/CombatTechniqueData.h>
#include <kenshi/util/hand.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/CharacterStatsWindow.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/GameSaveState.h>
#include <kenshi/GameData.h>

#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Delegate.h>

#include <core/Functions.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <map>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Per-attacker kill stats, keyed by the attacker's Character pointer.
//
// NOTE: we deliberately key by Character* rather than by hand. During
// Character::serialise the game can re-issue/rotate handles, so hand(thisptr)
// at save time wouldn't match the hand captured during combat and g_stats would
// silently create a fresh zero entry (stats saved as 0). The Character object
// pointer is stable for the object's lifetime, so combat/save/load/UI all hit
// the same entry.
// ---------------------------------------------------------------------------

struct KillStats
{
    int kills;
    int kos;
    int limbs;
    KillStats() : kills(0), kos(0), limbs(0) {}
};

static std::map<Character*, KillStats> g_stats;
static bool g_enabled = true, g_showKills = true, g_showKOs = true, g_showLimbs = true;
static void LoadSettings()
{
    const std::string p = PluginSettings::Path();
    g_enabled = PluginSettings::Bool("KillCounter", "Enabled", true, p);
    g_showKills = PluginSettings::Bool("KillCounter", "ShowKills", true, p);
    g_showKOs = PluginSettings::Bool("KillCounter", "ShowKnockouts", true, p);
    g_showLimbs = PluginSettings::Bool("KillCounter", "ShowLimbs", true, p);
    PluginDiagnostics::Configure("KillCounter", p);
    PluginDiagnostics::Write(std::string("KillCounter: settings enabled=") + (g_enabled ? "1" : "0"));
}

// A victim tracked for kill attribution. Kenshi marks death asynchronously (a
// frame or two after the hit), so the kill is credited from the update loop
// once the victim actually dies rather than inside the hit. `attacker` is the
// LAST hitter, refreshed on every hit against the victim (standing or down),
// so whoever hits them last gets the kill even if the victim was never knocked
// out and simply dies later from unrelated causes. `koCreditor` is only set on
// the downing hit, so the KO undo on death only applies when the same
// character both knocked the victim out and killed them while still down. The
// entry lives while the victim stays loaded and alive - dropped only on death
// or unload - so slow blood-loss and delayed deaths still credit a kill.
struct PendingKill
{
    Character* attacker;    // last hitter (kill credit)
    Character* koCreditor;  // who earned the KO for the downing (KO credit)
    PendingKill() : attacker(NULL), koCreditor(NULL) {}
};

static std::map<hand, PendingKill> g_pendingKills; // victim -> pending

// victim -> stealth attacker, populated by Character::getStealthKOChance and
// consumed by MedicalSystem::knockout. Successful assassinations knock the
// victim out through knockout() directly (never hitByMeleeAttack), so we pair
// the two hooks to attribute the KO and last-hitter credit. Cleared on any
// normal hit, so a failed/stale assassination can't double-count.
// Запись живёт ASSASSIN_RECORD_MS: ИИ спрашивает шанс оглушения много раз,
// а бьёт - редко, и без срока записи копились всю игру, а адрес удалённого
// персонажа мог достаться новому вместе с чужим «убийцей».
struct AssassinRecord
{
    Character* assassin;
    DWORD at;
    AssassinRecord() : assassin(NULL), at(0) {}
};
static const DWORD ASSASSIN_RECORD_MS = 10000;
static std::map<Character*, AssassinRecord> g_assassinTargets; // victim -> assassin
// Conservative set of loaded characters whose raw pointer was stored in an
// attribution record. Ordinary NPC unloads never scan the pending-hit lists.
// Entries disappear at character destruction or when tracking is disabled.
static std::set<Character*> g_attributionParticipants;

// Forget entries at the object's actual lifetime boundary, before its address
// can be reused. Zero-stat characters need no persistent map entry at all.
static void ForgetCharacter(Character* character)
{
    g_stats.erase(character);
    g_assassinTargets.erase(character);
    g_pendingKills.erase(hand(character));
    if (g_attributionParticipants.erase(character) == 0) return;
    for (std::map<Character*, AssassinRecord>::iterator it = g_assassinTargets.begin();
         it != g_assassinTargets.end();)
        if (it->second.assassin == character) it = g_assassinTargets.erase(it); else ++it;
    for (std::map<hand, PendingKill>::iterator it = g_pendingKills.begin(); it != g_pendingKills.end();)
    {
        if (it->second.attacker == character) it->second.attacker = NULL;
        if (it->second.koCreditor == character) it->second.koCreditor = NULL;
        if (it->first == character || (it->second.attacker == NULL && it->second.koCreditor == NULL))
            it = g_pendingKills.erase(it);
        else ++it;
    }
}

static void (*Character_destroy_orig)(Character*) = NULL;
static void Character_destroy_hook(Character* character)
{
    ForgetCharacter(character);
    Character_destroy_orig(character);
}

// Count how many of the 4 limbs are severed/crushed, using the limb state
// slots (LEFT_ARM/RIGHT_ARM/LEFT_LEG/RIGHT_LEG). This only ever changes when
// a limb is actually lost - unlike body-part health, it does NOT flip when the
// character simply dies.
static int CountSeveredLimbs(MedicalSystem* med)
{
    if (med == NULL || med->robotLimbs == NULL)
        return 0;

    int n = 0;
    for (int i = RobotLimbs::LEFT_ARM; i <= RobotLimbs::RIGHT_LEG; ++i)
    {
        LimbState st = med->robotLimbs->states[i];
        if (st == LIMB_STUMP || st == LIMB_CRUSHED)
            ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Shared hit attribution: after a hit lands, diff the victim's state against
// the pre-hit snapshot and attribute the KO / limb loss / downing to whoever
// dealt the hit. Both melee (hitByMeleeAttack) and ranged (iShotYou) hits use
// this. Kills are credited later in the update loop once the victim is
// confirmed dead (death flags set asynchronously).
// ---------------------------------------------------------------------------

static void AttributeHit(Character* victim, Character* attacker, bool wasKO, int severedBefore)
{
    if (!g_enabled || attacker == NULL || victim == NULL || attacker == victim)
        return;

    MedicalSystem* med = victim->getMedical();
    g_attributionParticipants.insert(attacker);

    // KO: the hit transitioned the victim from standing to unconscious.
    bool nowKO = (med != NULL) && med->isUnconcious();
    if (nowKO && !wasKO)
    {
        // This hit caused the KO - credit it, and remember who earned it so
        // the death-time KO undo only hits the same guy.
        ++g_stats[attacker].kos;
        if (PluginDiagnostics::Enabled()) PluginDiagnostics::Write("KillCounter: knockout by " + attacker->displayName);
        g_pendingKills[hand(victim)].koCreditor = attacker;
    }

    // Last-hitter attribution: remember whoever hit this victim last so the
    // kill goes to them once the victim dies - even if the victim is never
    // knocked out and drops dead later from unrelated causes. Refresh on every
    // hit, so someone hitting after us takes the credit. Only track living
    // victims (death is flagged asynchronously, so a lethal hit still records
    // before the flag lands).
    if (med != NULL && !med->isDead())
        g_pendingKills[hand(victim)].attacker = attacker;

    // Limbs: diff the severed-limb count. Only arms/legs, only real
    // sever/crush state changes - never triggered by plain death.
    int severedAfter = CountSeveredLimbs(med);
    int lost = severedAfter - severedBefore;
    if (lost > 0)
    {
        g_stats[attacker].limbs += lost;
        if (PluginDiagnostics::Enabled()) PluginDiagnostics::Write("KillCounter: severed limb(s) by " + attacker->displayName);
    }
}

// ---------------------------------------------------------------------------
// Combat hook: Character::hitByMeleeAttack (RVA 0x438D70)
//
// this   = victim
// damage = the hit's damage values
// who    = the attacker
//
// The detour runs BEFORE the original, so we snapshot the victim's state,
// let the hit apply, then diff to see if THIS hit caused the KO / limb loss.
// ---------------------------------------------------------------------------

typedef HitMaterialType (*HitByMeleeAttackFn)(Character* thisptr, CutDirection dir, Damages& damage, Character* who, CombatTechniqueData* attack, int comboID);
HitByMeleeAttackFn hitByMeleeAttack_orig = NULL;

HitMaterialType hitByMeleeAttack_hook(Character* thisptr, CutDirection dir, Damages& damage, Character* who, CombatTechniqueData* attack, int comboID)
{
    if (!g_enabled) return hitByMeleeAttack_orig(thisptr, dir, damage, who, attack, comboID);
    // Snapshot the victim before the hit lands.
    bool wasKO = false;
    int severedBefore = 0;
    if (thisptr != NULL)
    {
        MedicalSystem* med = thisptr->getMedical();
        if (med != NULL)
            wasKO = med->isUnconcious();
        severedBefore = CountSeveredLimbs(thisptr->getMedical());
    }

    // A normal hit means no assassination is pending for this victim - clear
    // any stale stealth record BEFORE the hit applies, so a KO applied inside
    // the original (e.g. a failed assassination that turned into a regular
    // attack) can't be double-counted against the wrong attacker.
    if (thisptr != NULL)
        g_assassinTargets.erase(thisptr);

    HitMaterialType result = hitByMeleeAttack_orig(thisptr, dir, damage, who, attack, comboID);

    AttributeHit(thisptr, who, wasKO, severedBefore);

    return result;
}

// ---------------------------------------------------------------------------
// Ranged hook: Character::iShotYou (RVA 0x439510)
//
// Called on the VICTIM the moment a harpoon lands - this is the ranged
// equivalent of hitByMeleeAttack and covers BOTH crossbow bolts and turret
// harpoons. The attacker is the character who fired (for a turret that is the
// operator, since GunClass::shoot passes the operator as the shooter).
//
// this     = victim
// attacker = who shot them
// poon     = the harpoon that hit
// onPurpose= whether the shot was intentional
//
// Same snapshot/diff pattern as the melee hook.
// ---------------------------------------------------------------------------

typedef bool (*IShotYouFn)(Character* thisptr, Character* attacker, Harpoon* poon, bool onPurpose);
IShotYouFn iShotYou_orig = NULL;

bool iShotYou_hook(Character* thisptr, Character* attacker, Harpoon* poon, bool onPurpose)
{
    if (!g_enabled) return iShotYou_orig(thisptr, attacker, poon, onPurpose);
    // Snapshot the victim before the harpoon applies its damage.
    bool wasKO = false;
    int severedBefore = 0;
    if (thisptr != NULL)
    {
        MedicalSystem* med = thisptr->getMedical();
        if (med != NULL)
            wasKO = med->isUnconcious();
        severedBefore = CountSeveredLimbs(thisptr->getMedical());
    }

    // See hitByMeleeAttack_hook - clear any stale stealth record first.
    if (thisptr != NULL)
        g_assassinTargets.erase(thisptr);

    bool result = iShotYou_orig(thisptr, attacker, poon, onPurpose);

    AttributeHit(thisptr, attacker, wasKO, severedBefore);

    return result;
}

// ---------------------------------------------------------------------------
// Stealth KO (assassination) attribution.
//
// A successful assassination knocks the victim out by calling
// MedicalSystem::knockout() DIRECTLY on the victim - it never goes through
// hitByMeleeAttack/iShotYou, so the normal hit-diff path can't see it. We pair
// two hooks:
//
//   Character::getStealthKOChance(thisptr = assassin, victim, factors)
//     - called by the assassination AI right before the knockout roll; record
//       the intended attacker for that victim.
//   MedicalSystem::knockout(thisptr = victim's medical, skill01)
//     - fires when the assassination lands; if we have an assassin recorded
//       for this victim, credit them the KO and the last-hitter (kill)
//       credit, exactly like a melee downing. The record is consumed here.
//
// The record is also cleared on any normal hit against the victim (in the
// melee/ranged hooks, BEFORE the original runs, so a KO applied inside the hit
// can't see a stale record), which prevents a failed assassination from ever
// double-counting or mis-attributing a later combat KO.
// ---------------------------------------------------------------------------

typedef float (*GetStealthKOChanceFn)(Character* thisptr, Character* victim, bool factors);
GetStealthKOChanceFn getStealthKOChance_orig = NULL;

float getStealthKOChance_hook(Character* thisptr, Character* victim, bool factors)
{
    float result = getStealthKOChance_orig(thisptr, victim, factors);

    if (g_enabled && thisptr != NULL && victim != NULL && thisptr != victim)
    {
        g_attributionParticipants.insert(thisptr);
        AssassinRecord& record = g_assassinTargets[victim];
        record.assassin = thisptr;
        record.at = GetTickCount();
    }

    return result;
}

typedef void (*KnockoutFn)(MedicalSystem* thisptr, float skill01);
KnockoutFn knockout_orig = NULL;

void knockout_hook(MedicalSystem* thisptr, float skill01)
{
    if (knockout_orig != NULL)
        knockout_orig(thisptr, skill01);

    if (!g_enabled || thisptr == NULL || thisptr->me == NULL)
        return;

    Character* victim = thisptr->me;
    std::map<Character*, AssassinRecord>::iterator it = g_assassinTargets.find(victim);
    if (it == g_assassinTargets.end())
        return;

    const bool fresh = GetTickCount() - it->second.at < ASSASSIN_RECORD_MS;
    Character* attacker = fresh ? it->second.assassin : NULL;
    g_assassinTargets.erase(it);

    if (attacker == NULL || attacker == victim)
        return;

    // Credit the KO and register the assassin as the last hitter, so if the
    // victim dies while down the assassin gets the kill (and the death-time
    // KO undo applies, since the same guy both downed and killed them).
    ++g_stats[attacker].kos;
    if (PluginDiagnostics::Enabled()) PluginDiagnostics::Write("KillCounter: stealth knockout by " + attacker->displayName);
    PendingKill& pk = g_pendingKills[hand(victim)];
    pk.attacker = attacker;
    pk.koCreditor = attacker;
}

// ---------------------------------------------------------------------------
// Death attribution: called every frame from the game loop. For every victim
// we ever hit, if they are now dead we credit the kill to the last hitter and
// stop tracking. Entries persist while the victim stays loaded and alive, so
// a victim who was only lightly touched and dies later from unrelated causes
// still credits whoever hit them last.
// ---------------------------------------------------------------------------

static void UpdatePendingKills()
{
    for (std::map<hand, PendingKill>::iterator it = g_pendingKills.begin(); it != g_pendingKills.end();)
    {
        // The victim may have been unloaded from the world between frames.
        // hand::getCharacter() returns NULL for an unloaded/invalid handle, so
        // we simply drop the entry - never dereference a stale pointer.
        Character* victim = it->first.getCharacter();
        if (victim == NULL)
        {
            it = g_pendingKills.erase(it);
            continue;
        }

        MedicalSystem* med = victim->getMedical();
        if (med == NULL)
        {
            it = g_pendingKills.erase(it);
            continue;
        }

        if (med->isDead())
        {
            Character* killer = it->second.attacker;
            if (killer != NULL)
            {
                ++g_stats[killer].kills;
                if (PluginDiagnostics::Enabled()) {
                    char note[160]; sprintf_s(note,"KillCounter: kill credited attacker=%p total=%d",killer,g_stats[killer].kills);
                    DebugLog(note);
                }
                // The downing hit already counted this victim as a KO; if they
                // died while still down it's the same event, so undo the KO to
                // avoid double counting - but only when the same character both
                // knocked the victim out and killed them. A finisher who never
                // earned the KO shouldn't eat one, and a victim who recovered
                // before dying keeps their KO as a separate survived event.
                if (killer == it->second.koCreditor && med->isUnconcious())
                {
                    KillStats& ks = g_stats[killer];
                    if (ks.kos > 0)
                        --ks.kos;
                }
            }
            it = g_pendingKills.erase(it);
            continue;
        }

        // Victim still alive (standing or down) - keep tracking so the last
        // hitter keeps the credit until they die or leave the world.
        ++it;
    }
}

// ---------------------------------------------------------------------------
// Save / load: ride inside the character's own stats record.
//
// A character's stats live in the typecode-25 (STATS) GameData record, which
// the save wires to the character via the squad's instance states[]. We add
// our 3 floats to that existing record's floats section (GameData::fdata), so
// they're stored as part of the character itself - readable in FCS, survives
// zone reload/import, and needs no name keying or custom record type.
//
// Save side hooks Character::serialise, which returns the character's
// GameSaveState by value. On MSVC x64 a by-value non-trivial return uses the
// hidden sret slot, but the exact register order of that slot vs `this` has
// differed across game/RE_Kenshi builds (e.g. this binary passes `this` in
// RCX and the hidden slot in RDX). Rather than hard-code an order, we forward
// the first two registers to the original untouched - so it always runs with
// the layout it expects - and recover the Character as the one of the two that
// is NOT the GameSaveState slot that serialise returns.
// ---------------------------------------------------------------------------

typedef GameSaveState* (*SerialiseFn)(void* arg1, void* arg2,
                                      GameDataContainer* container, GameData* refList,
                                      PosRotPair* offsetPosToSubtract);
SerialiseFn serialise_orig = NULL;

GameSaveState* serialise_hook(void* arg1, void* arg2,
                              GameDataContainer* container, GameData* refList,
                              PosRotPair* offsetPosToSubtract)
{
    GameSaveState* result = serialise_orig(arg1, arg2, container, refList, offsetPosToSubtract);
    if (result == NULL)
        return result;

    // `result` is the hidden return slot the original filled in; the Character
    // is the other of the two forwarded pointers. The save-time pointer then
    // matches the one g_stats is keyed by during combat, so the write lands in
    // the right entry instead of a fresh zero one.
    Character* thisptr = (Character*)(result == (GameSaveState*)arg1 ? arg2 : arg1);
    if (thisptr == NULL)
        return result;

    // The stats record already exists in this character's save state - write
    // our floats into it (appends to the record's floats section).
    GameData* gd = result->getState((itemType)STATS);
    if (gd != NULL)
    {
        const std::map<Character*, KillStats>::const_iterator found = g_stats.find(thisptr);
        const KillStats s = found != g_stats.end() ? found->second : KillStats();
        gd->fdata["kills"] = (float)s.kills;
        gd->fdata["kos"] = (float)s.kos;
        gd->fdata["limbs"] = (float)s.limbs;
    }

    return result;
}

void (*loadFromSerialise_orig)(Character* thisptr, GameSaveState* state) = NULL;
void loadFromSerialise_hook(Character* thisptr, GameSaveState* state)
{
    loadFromSerialise_orig(thisptr, state);

    if (thisptr == NULL || state == NULL)
        return;

    GameData* gd = state->getState((itemType)STATS);
    if (gd == NULL)
        return;

    KillStats s;
    auto value = gd->fdata.find("kills");
    if (value != gd->fdata.end()) s.kills = (int)value->second;
    value = gd->fdata.find("kos");
    if (value != gd->fdata.end()) s.kos = (int)value->second;
    value = gd->fdata.find("limbs");
    if (value != gd->fdata.end()) s.limbs = (int)value->second;
    if (s.kills || s.kos || s.limbs) g_stats[thisptr] = s;
    else g_stats.erase(thisptr);
    if (PluginDiagnostics::Enabled())
    {
        char totals[160]; sprintf_s(totals, " loaded kills=%d knockouts=%d limbs=%d", s.kills, s.kos, s.limbs);
        DebugLog("KillCounter: " + thisptr->displayName + totals);
    }
}

// ---------------------------------------------------------------------------
// UI: inject the 3 kill counters into the open STA (character stats) window.
//
// We reach every open CharacterStatsWindow through gui->characterStatsWindows
// and add keyed lines to its attributes datapanel. setLine(key, ...) is
// idempotent by key - calling it every frame updates the line in place rather
// than stacking duplicates, so the numbers stay live while the window is open
// and are based on the character the STA window is showing (not the selection).
// ---------------------------------------------------------------------------

static void SetStatsLine(DatapanelGUI* panel, int category,
                         const std::string& key, const std::string& label, int value,
                         const std::string& skinA, const std::string& skinB)
{
    if (panel == NULL)
        return;

    // Цвет не назначаем вовсе. Автор ставил RGB (100, 95, 90) и писал в
    // комментарии, что это цвет остальной панели качеств, - но «Сила» и
    // соседи рисуются цветом панели по умолчанию и заметно светлее.
    // Вдобавок этот цвет каждый кадр перебивал раскраску StatColours.

    // Line already exists from a previous frame - update its value column.
    if (panel->lineExists(key, category))
    {
        DataPanelLine* line = panel->getLine(key, category);
        if (line != NULL && line->w2 != NULL)
        {
            const std::string text = std::to_string((long long)value);
            if (line->w2->getCaption().asUTF8() != text) line->w2->setCaption(text);
        }
        return;
    }

    // Первый раз в этом окне - создаём строку. Скины пришли от соседней
    // строки, которую нарисовала сама игра: иначе шрифт выходит тоньше.
    if (!skinA.empty() && !skinB.empty())
        panel->setLine(key, label, std::to_string((long long)value), skinA, skinB, category);
    else
        panel->setLine(key, label, std::to_string((long long)value), category, false, true);
}

static void UpdateStatsWindows()
{
    if (gui == NULL)
        return;

    Ogre::vector<CharacterStatsWindow*>::type& windows = gui->characterStatsWindows;
    for (size_t i = 0; i < windows.size(); ++i)
    {
        CharacterStatsWindow* win = windows[i];
        if (win == NULL)
            continue;

        DatapanelGUI* panel = win->attributesDatapanel;
        Character* c = win->character;
        if (panel == NULL || c == NULL)
            continue;

        int category = panel->getCurrentCategory();

        // Первая строка панели - «Сила», её создаёт игра. Списываем с неё
        // скины, чтобы наши три выглядели ровно так же.
        std::string skinA, skinB;
        if (panel->getNumLines(category) > 0)
        {
            DataPanelLine* first = panel->getLineByNum(category, 0);
            if (first != NULL)
            {
                skinA = first->skinW1;
                skinB = first->skinW2;
            }
        }

        const std::map<Character*, KillStats>::const_iterator found = g_stats.find(c);
        const KillStats s = found != g_stats.end() ? found->second : KillStats();
        const char* keys[] = { "KillCounterKills", "KillCounterKOs", "KillCounterLimbs" };
        const bool show[] = { g_showKills, g_showKOs, g_showLimbs };
        const char* labels[] = { Tr("Kills"), Tr("Knockouts"), Tr("Limbs removed") };
        const int values[] = { s.kills, s.kos, s.limbs };
        for (int j = 0; j < 3; ++j)
            if (g_enabled && show[j]) SetStatsLine(panel, category, keys[j], labels[j], values[j], skinA, skinB);
            else if (panel->lineExists(keys[j], category)) panel->removeLine(keys[j], category);
    }
}

// Hook the game loop: run death attribution and keep the STA lines live.
void (*GameWorld_update_orig)(GameWorld* thisptr, float time) = NULL;
void GameWorld_update_hook(GameWorld* thisptr, float time)
{
    if (GameWorld_update_orig != NULL)
        GameWorld_update_orig(thisptr, time);

    if (g_enabled) UpdatePendingKills();
    else { g_pendingKills.clear(); g_assassinTargets.clear(); g_attributionParticipants.clear(); }
    UpdateStatsWindows();

    // Устаревшие записи о попытках оглушения - раз в несколько секунд.
    static DWORD lastPrune = 0;
    const DWORD now = GetTickCount();
    if (now - lastPrune > 5000)
    {
        lastPrune = now;
        for (std::map<Character*, AssassinRecord>::iterator it = g_assassinTargets.begin();
             it != g_assassinTargets.end();)
        {
            if (now - it->second.at >= ASSASSIN_RECORD_MS)
                it = g_assassinTargets.erase(it);
            else
                ++it;
        }
    }
}

// ---------------------------------------------------------------------------

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string p = PluginSettings::Path();
    api->beginMod(api, "KillCounter", "Kill Counter", p.c_str(), LoadSettings);
    if (api->version >= 2) api->info(api, Tr("Kills, knockouts and severed limbs in the stats window. Existing save statistics are preserved."));
    api->toggle(api, "KillCounter", "Enabled", Tr("Enabled"), Tr("Count new combat events and show counters. Saved statistics are kept when disabled."), 1, 0);
    api->toggle(api, "KillCounter", "ShowKills", Tr("Show kills"), Tr("Show the kill total in the character stats window."), 1, 0);
    api->toggle(api, "KillCounter", "ShowKnockouts", Tr("Show knockouts"), Tr("Show the knockout total in the character stats window."), 1, 0);
    api->toggle(api, "KillCounter", "ShowLimbs", Tr("Show severed limbs"), Tr("Show how many limbs this character removed."), 1, 0);
    api->toggle(api, "KillCounter", "Debug", Tr("Detailed log"), Tr("Details in RE_Kenshi_log.txt: kills, knockouts, lost limbs and loaded counters."), 0, 0);
}

__declspec(dllexport) void startPlugin()
{
    LoadSettings();
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::_DESTRUCTOR),
            (void*)&Character_destroy_hook, (void**)&Character_destroy_orig))
        ErrorLog("KillCounter: could not hook Character destructor");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::_NV_hitByMeleeAttack),
            (void*)&hitByMeleeAttack_hook,
            (void**)&hitByMeleeAttack_orig))
        ErrorLog("KillCounter: could not hook Character::hitByMeleeAttack");
    else
        PluginDiagnostics::Write("KillCounter: hooked Character::hitByMeleeAttack");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::iShotYou),
            (void*)&iShotYou_hook,
            (void**)&iShotYou_orig))
        ErrorLog("KillCounter: could not hook Character::iShotYou");
    else
        PluginDiagnostics::Write("KillCounter: hooked Character::iShotYou");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::getStealthKOChance),
            (void*)&getStealthKOChance_hook,
            (void**)&getStealthKOChance_orig))
        ErrorLog("KillCounter: could not hook Character::getStealthKOChance");
    else
        PluginDiagnostics::Write("KillCounter: hooked Character::getStealthKOChance");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&MedicalSystem::knockout),
            (void*)&knockout_hook,
            (void**)&knockout_orig))
        ErrorLog("KillCounter: could not hook MedicalSystem::knockout");
    else
        PluginDiagnostics::Write("KillCounter: hooked MedicalSystem::knockout");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
            (void*)&GameWorld_update_hook,
            (void**)&GameWorld_update_orig))
        ErrorLog("KillCounter: could not hook GameWorld update");
    else
        PluginDiagnostics::Write("KillCounter: hooked GameWorld update");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::_NV_serialise),
            (void*)&serialise_hook,
            (void**)&serialise_orig))
        ErrorLog("KillCounter: could not hook Character::serialise");
    else
        PluginDiagnostics::Write("KillCounter: hooked Character::serialise");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&Character::_NV_loadFromSerialise),
            (void*)&loadFromSerialise_hook,
            (void**)&loadFromSerialise_orig))
        ErrorLog("KillCounter: could not hook Character::loadFromSerialise");
    else
        PluginDiagnostics::Write("KillCounter: hooked Character::loadFromSerialise");
}
