# Wall Dismantle Hotkey — Pre-Alpha Test Checklist (Selection-Based)

**Assumption (locked):**
- The wall **must be selected** (via normal Kenshi selection)
- **Hovering is irrelevant**
- Hotkey acts only on the **currently selected object**

This is safer and more predictable than hover-based logic.

---

## 1. Core Functionality (must pass)

### 1.1 Hotkey detection
Test:
- Select a wall → press hotkey → dismantle triggers
- Press hotkey with **no selection** → nothing happens
- Press hotkey with **multiple selections** → see 1.3

Verify:
- GOOD No repeat firing from a single press
- GOOD Holding key does **not** spam dismantle
- DUNNO WHERE ANY FIELDS ARE Hotkey does nothing while typing in UI fields

❌ Fail = do not publish

---

### 1.2 Selection validation (CRITICAL)
Test:
- Select **player-owned wall** → dismantle works
- Select **non-wall building** → nothing happens
- Select **terrain / character / item** → nothing happens

Verify:
- No crash
- No partial dismantle
- Clear rejection path internally

ALL GOOD

❌ Fail = do not publish

---

### 1.3 Multi-selection behavior (must be explicit)
Decide and test **ONE** of these behaviors:

Options:
1) **Only first selected object is processed** (recommended for alpha)
2) **Abort if multiple objects selected**
3) **Dismantle all selected walls** (higher risk)

**Recommendation:** **Option 2 (abort)** or **Option 1** for alpha.

Test:
- Select multiple walls
- Press hotkey

Verify:
- Behavior matches your documented choice
- No accidental mass dismantle

❌ Fail = do not publish

---

## 2. Ownership & Permission Safety

### 2.1 Player ownership check
Test:
- Select wall in your base → dismantle works
- Select wall in NPC town → nothing happens
- Select wall owned by another faction → nothing happens

Verify:
- No crime flags
- No faction relation changes
- No AI reaction

❌ Fail = do not publish

---

## 3. Structural Safety (very important)

### 3.1 Connected structures
Test:
- Wall connected to:
  - Other walls
  - Gate
  - Watchtower

Verify:
- Only the **selected wall segment** is dismantled
- No chain reaction
- No dangling invisible collision

❌ Fail = do not publish

---

### 3.2 Collision & pathing
Test:
- Dismantle selected wall
- Order characters to walk through the former location

Verify:
- No invisible wall
- Pathing updates correctly

❌ Fail = do not publish

---

## 4. Save / Load Integrity (NON-NEGOTIABLE)

### 4.1 Persistence test
Test:
1) Select wall → dismantle
2) Save game
3) Reload save

Verify:
- Wall remains dismantled
- No ghost mesh
- No crash on load

❌ Fail = **do not publish under any circumstances**

---

## 5. Edge-Case Interaction Tests

### 5.1 Game state
Test:
- Game paused
- Game unpaused
- Game at 3× speed

Verify:
- Either works consistently
- Or cleanly refuses (but never crashes)

---

### 5.2 Repeated use stress
Test:
- Dismantle:
  - 5 walls
  - 20 walls
  - 50+ walls (large base)

Verify:
- No FPS degradation
- No growing delay
- No memory increase
- No crash

---

## 6. Performance & Idle Cost

### 6.1 Idle behavior
Test:
- Plugin enabled
- Do nothing for 10 minutes

Verify:
- No CPU usage spikes
- No log spam
- No FPS loss

---

### 6.2 Busy scenes
Test:
- Town with many NPCs
- Large player base
- Camera panning + zooming

Verify:
- Hotkey responsiveness unchanged
- No hitch when pressing hotkey

---

## 7. Logging & Diagnostics (Alpha-appropriate)

Verify:
- Logs appear only on:
  - Plugin init
  - Invalid selection attempt
  - Actual dismantle
- No per-frame logging

Good log examples:
- `WallDismantle: no valid selection, ignoring`
- `WallDismantle: selected object is not a wall`
- `WallDismantle: dismantled wall id=XYZ`

---

## 8. Disable & Safety Switch

### 8.1 Hard disable
Test:
- Set `enabled=false`
- Reload game

Verify:
- Hotkey does nothing
- No hooks perform work

---

## 9. UX Expectations (Alpha level)

Verify:
- If selection is valid → wall disappears cleanly
- If selection is invalid → nothing happens
- No half-states (no broken meshes, no stuck ghost walls)

Optional:
- One-time on-screen message (not required for alpha)

---

## 10. NexusMods Alpha Readiness Gate

You are **safe to publish alpha** if:
- ✅ Selection validation is strict
- ✅ No non-player walls can be dismantled
- ✅ Save/load is clean
- ✅ Feature is disableable
- ✅ No crashes in stress tests

### Recommended Alpha Disclaimer
> “Alpha version. Requires selecting a player-owned wall before pressing the hotkey.  
> Use on backup saves. Windows only. RE_Kenshi plugin.”

---

## Final Verdict
**Selection-based activation is GOOD design for alpha.**  
It dramatically reduces accidental damage and user error.

If you want next, I can:
- Rewrite this as a **YES/NO pre-publish checklist**
- Help you decide the **multi-selection policy**
- Review your **NexusMods description** so users don’t misunderstand the selection requirement
