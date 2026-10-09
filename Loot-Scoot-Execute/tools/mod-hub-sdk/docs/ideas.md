# RE_Kenshi QoL Ideas — What Native Mods Can Improve (Multiple Options)

Below are **QoL features that are hard or impossible with FCS**, but **well-suited for RE_Kenshi native mods**.  
They’re grouped by category, each with **what’s missing**, **what RE_Kenshi enables**, and **risk level**.

---

## 1. Awareness & Visibility (High value, low–medium risk)

### 1.1 Unconscious / Downed Character Highlight (what you’re building)
**Missing**
- Hard to spot unconscious bodies, especially in towns / foliage / night.

**RE_Kenshi enables**
- State-based highlighting (HUD markers, glow, outlines, icons).
- Conditional (radius / key-held / faction).

**Risk**
- Low (HUD), High (shader outline).

---

### 1.2 “Why can’t I do this?” Action Feedback
**Missing**
- Kenshi often silently fails actions (build, heal, pick up, trade).

**RE_Kenshi enables**
- Intercept failed action attempts.
- Display short contextual reason:
  - “Too far”
  - “Missing skill”
  - “Inventory full”
  - “Blocked by AI state”

**Risk**
- Medium (needs good hook points).
**Value**
- Very high UX improvement.

---

### 1.3 Combat State Indicators
**Missing**
- No clear indication of:
  - Being targeted
  - Being flanked
  - Being staggered / animation-locked

**RE_Kenshi enables**
- HUD icons when:
  - Enemy targets your unit
  - Character is animation-locked
  - Stun/bleed thresholds crossed

**Risk**
- Medium.
**Value**
- High for squad control.

---

## 2. Control & Input QoL (Very high value, medium risk)

### 2.1 Smart Context Actions
**Missing**
- Same click does different things, but not always the *right* thing.

**RE_Kenshi enables**
- Context-aware right-click:
  - Downed ally → Heal
  - Downed enemy → Loot / Kidnap
  - Storage → Auto-sort best match
- Optional modifier keys.

**Risk**
- Medium–high (input hooks).
**Value**
- Extremely high.

---

### 2.2 Hold-to-Preview Systems (Pattern you already use)
**Missing**
- Too many toggles, no “peek” actions.

**RE_Kenshi enables**
- Hold key to:
  - Show stealth visibility
  - Show enemy aggro range
  - Show unconscious markers
  - Show encumbrance penalties

**Risk**
- Low.
**Value**
- High, very clean UX.

---

### 2.3 Squad Command Quality
**Missing**
- Limited fine control over formations and priorities.

**RE_Kenshi enables**
- Temporary “command modes”:
  - Hold key → everyone guards leader
  - Hold key → passive retreat
  - Hold key → stop chasing

**Risk**
- Medium (AI state manipulation).
**Value**
- High.

---

## 3. Information Surfacing (Low risk, high payoff)

### 3.1 Real-Time Skill Gain Feedback
**Missing**
- You don’t know *why* a skill is (or isn’t) increasing.

**RE_Kenshi enables**
- HUD text like:
  - “Strength not increasing (encumbrance too low)”
  - “Dexterity capped (weapon too slow)”
  - “Toughness optimal (enemy stronger)”

**Risk**
- Low–medium.
**Value**
- Very high for learning the game.

---

### 3.2 Health & Damage Warnings
**Missing**
- No early warning for critical limb damage.

**RE_Kenshi enables**
- Configurable alerts:
  - “Left leg critical — retreat advised”
  - “Bleed rate exceeds recovery”
- Visual limb highlights.

**Risk**
- Low.
**Value**
- High, especially in ironman runs.

---

### 3.3 AI Intent Debug Overlay (Optional / Dev Mode)
**Missing**
- AI decisions are opaque.

**RE_Kenshi enables**
- Toggle overlay showing:
  - Current AI goal
  - Target
  - Reason for current state

**Risk**
- Medium.
**Value**
- Very high for modders / advanced players.

---

## 4. Performance & Simulation QoL (Native-only power)

### 4.1 Smarter Update Throttling
**Missing**
- Engine treats many distant/irrelevant entities equally.

**RE_Kenshi enables**
- Dynamically throttle:
  - Far-away AI updates
  - Off-screen behavior
- Especially useful for big bases.

**Risk**
- High (must be careful).
**Value**
- Massive performance gains if done right.

---

### 4.2 Selective Simulation Pausing
**Missing**
- No way to say “don’t simulate this until I’m near”.

**RE_Kenshi enables**
- Pause simulation for:
  - Distant squads
  - Finished bases
  - Idle caravans

**Risk**
- High.
**Value**
- Huge late-game QoL.

---

## 5. Inventory & Economy QoL (Medium risk, high value)

### 5.1 Smart Inventory Sorting (Native)
**Missing**
- Sorting is basic and manual.

**RE_Kenshi enables**
- One-key:
  - Auto-stack
  - Auto-sort by weight/value/type
  - Auto-distribute food evenly

**Risk**
- Medium.
**Value**
- Very high.

---

### 5.2 Trade Comparison Overlay
**Missing**
- Hard to know if a deal is good.

**RE_Kenshi enables**
- Show:
  - Average price
  - Region modifier
  - “Good / bad deal” hint

**Risk**
- Medium.
**Value**
- High.

---

## 6. Camera & UX Polish (Low risk, high delight)

### 6.1 Smart Camera Snapping
**Missing**
- Camera sometimes loses context in chaos.

**RE_Kenshi enables**
- Snap camera to:
  - Recently downed squad member
  - Character taking heavy damage
  - Selected group center

**Risk**
- Low.
**Value**
- High.

---

### 6.2 Visual Noise Reduction
**Missing**
- Combat scenes get cluttered.

**RE_Kenshi enables**
- Fade unimportant UI elements when zoomed out.
- Highlight only selected / relevant entities.

**Risk**
- Low.
**Value**
- High.

---

## 7. “Small but Powerful” Native QoL Ideas

- Optional “safe speed-up” (disable 3x during combat spikes)
- Smart auto-save triggers (before big fights, city entry)
- Warning when pathing crosses hostile biome
- Squad hunger forecast (“Food runs out in ~8h”)

---

## Recommendation: Best First RE_Kenshi QoL Mods
If you want **maximum impact with minimal risk**, start with:

1) **Awareness overlays** (KO, targeting, danger)
2) **Hold-to-preview systems**
3) **Failure reason feedback**
4) **Inventory smart helpers**

They:
- Don’t fight the simulation
- Are easy to disable
- Scale well with performance
- Teach the player *why* things happen

If you want, next I can:
- Rank these by **implementation difficulty**
- Propose a **“QoL mod pack roadmap”**
- Or deep-dive one idea into a task-sized plan like we did for KO highlighting

## Drop items from inventory hotkey
