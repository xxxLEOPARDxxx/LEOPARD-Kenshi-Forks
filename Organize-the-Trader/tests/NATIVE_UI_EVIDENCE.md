# Trader UI evidence - 2026-10-30

All checks are offline. No DLL was loaded into the agent, no game process was opened, and no native function was called in the game. Visual acceptance and GOG runtime acceptance remain outstanding.

## GOG activation after selecting an animal

The previous `ResolveInventoryLayoutCreateGUIHookAddress` selected Steam RVA `0x14EEA0` and GOG RVA `0x14F450`. This is an asymmetric pair of different functions, not the same function on two platforms.

`D:/DEV/Kenshi/tools/slots.py` reads the real installed KenshiLib export stubs and their function-pointer slots; it does not use the newer SDK symbol numbering. The installed official Steam/GOG1.0.65 RVA tables resolve those slots as follows:

| Installed export | Slot | Steam RVA | GOG RVA |
| --- | ---: | ---: | ---: |
| CharacterInventoryLayout_setupSections_PLACEHOLDER | 2019 | 0x14EEA0 | 0x14EDC0 |
| AnimalInventoryLayout_setupSections_PLACEHOLDER | 600 | 0x14F530 | 0x14F450 |
| TraderInventoryLayout_setupSections_PLACEHOLDER | 6959 | 0x150530 | 0x150450 |

The reported animal trigger matches the function actually hooked on GOG. The repair resolves the Character export with `GetProcAddress` and `KenshiLib::GetRealAddress`, uses the active platform's table, and also binds the actual trader setup path at startup. It never substitutes a nearby vtable function on an address-distance guess. The original setup function runs before section/inventory binding.

The SDK declarations and Steam native prologues agree on the four arguments: layout `this`, InventoryGUI*, section map reference, Inventory*. Existing native inventory binding receives this committed section map; it does not construct a fake trader inventory.

Official table SHA256:

- Steam1.0.65: `573c395997ed8d93dd837e27f89a440323089edd1ba93f68291631b448f9cbd0`
- GOG1.0.65: `a2a6394e95312a89e3d3f6842fbfda9c16e7d79749f1ee796fbd6168600bf347`

Run `CheckNativeContract.py` with the bundled Python to reproduce the installed-export/table comparison and verify every KenshiLib/MyGUI/Ogre DLL import against the installed binaries.

## Wallet and native panel updates

Character::getMoney resolves through installed slot1604 to Steam `0x790AC0`. The unpacked Steam function tests the character's player-side data and then reads the player's participant faction through PlayerInterface+0x2A0, faction ownerships+0x80, and money+0x88. Non-player branches use the character's own ownership. The earlier three-label candidate resolved a fresh player Character to display that wallet; the current two-label UI removes that getter and fallback entirely. No version writes a wallet.

The merchant amount comes from the game's live numeric data-panel caption. Currency prefix and separators follow that caption. The deal is the opening amount minus the current amount. Parsing accepts the full non-negative signed-int range, distinguishes zero from missing data, and rejects overflow.

Independent offline review resolved InventoryGUI::updateDatapanel to Steam `0x70C1F0` through `0x70C242`: it checks native data-panel/inventory pointers and calls the inventory getter and owner fill routine, with no alpha/render gate in this function. The current skin correction keeps the native plate opaque and makes only the old data-panel contents transparent, preserving the update path. A sale/reopen runtime check is still required.

## Header ownership and layout boundaries

The installed default and Dark UI Advanced `Kenshi_InventoryTraderWindow.layout` both put `datapanel` in a dedicated small money plate beneath Content. The adapter rejects a plate equal to the injection parent, window owner, or a widget named Content; it also rejects an untrusted plate exceeding one third of parent height. A previously acquired dedicated plate remains trusted during vertical shrinking. Rejection after acquisition restores the header instead of leaving Content transparent.

Both current custom header widgets must be created and have a valid opaque XML style before the old data-panel contents are faded or `LEOPARD_OTT_MoneyHeader=1` is set. The native plate itself retains its XML background and original opacity. UITweaks skips only this marked native data panel; the marker is removed and content alpha restored on teardown. Native overflow tooltips are resolved afresh, hidden/cleared for the live owned widget before its destruction, and are never cached as world-owned pointers.

The viewport adapter's production helper tracks a native baseline and its last written rectangle independently for each viewport. Normal MyGUI alignment projects both through parent-size changes; a distinct native absolute setCoord is adopted as a new baseline. Reservation clamps height to zero, temporarily hides a fully collapsed viewport, and restores the latest baseline and prior visibility on teardown. The helper is exercised directly in AdaptiveLayoutTests, including mixed viewport heights, repeated updates, collapse, ordinary resizing, absolute geometry reset, and restoration.

## Verification and remaining acceptance

Run `tests/run_offline_checks.ps1` after building both production DLLs. It compiles tests with VC10x64, checks the installed layouts, validates native packing and six rollback cases, checks Cyrillic normalization, runs the actual adaptive arithmetic/baseline/width/floor helpers, and verifies the produced DLL imports and entry points.

The ordinary screenshot window widths use native font sizes. Narrow layouts wrap groups first, omit the redundant Sort label if necessary, then limit font reduction to at least75% of the original size and at least12px (without enlarging an originally smaller font). A pathological window still too small uses bounded widgets with a full preset popup/native tooltip; this is not a claim that physically impossible full captions fit inline.

Still required in game: default/Dark UI visual checks, small and large trader windows, a sale and refreshed wallet captions, all sort presets/directions, human/animal switches, focused search and no matches, close/reopen, MCM disable/re-enable, resolution change, world load/new game, and actual GOG1.0.65 testing. No offline test substitutes for these.

## Actual Native failure and corrective contract (30.10.2026, PID29460 snapshot)

The saved snapshot at `game/_codex_work/mod-updates-20261030/ott-adaptive-debug/RUNTIME-29460-OTT-READONLY.json` consumed579794 bytes with no read errors, under a Root-verified PID/start and exact candidate .text hash. No further process reads were used to implement this correction.

All three custom cash labels had correct captions, visible=true, own and parent real-alpha1, but **fontHeight0 and base Colour(0,0,0,0)**. Native cash EditBox mainText was null. The combo root mainText was null too; its public Client was a valid text widget with Medium21px. Closed combo outer width171 provided actual Client text width137. Cached root font heights were0 for both combo and search EditBox, and the longest list width was only171. The probe's initial type mapper returned unknown, so it did **not** capture money's private mClientText; no such capture is claimed.

Installed MyGUIEngine disassembly establishes the missing contract:

- TextBox::getFontHeight0x1A9460, getFontName0x1A93A0 and getTextColour0x1A92E0 are inherited nonvirtual getters consulting root SkinItem text; missing text returns empty/zero defaults.
- EditBox::initialiseOverride0xBFE00 first stores the root text at0xC028F, then overrides it with Client skin text at0xC02B3 when present. The production adapter matches this Client-first behaviour through **public Widget::getClientWidget and SkinItem::getSubWidgetText**, bounded to four live widget references; it never uses the private offsets.
- ISubWidgetText getters are virtual and return the actual rendered style. Using the inherited TextBox getter for a composite EditBox is insufficient.
- EditBox::getTextSize0xBB660 consults its actual client text. EditText::getTextSize0xC5900 refreshes the text view when its caption is dirty and returns Native layout width independently of widget visibility. Cursor/shadow variants can add up to3px in that getter; the adapter adds4px after measuring the actual font and non-text chrome.
- Root alpha propagation was not the cause: EditText::getTextColour0xC38F0 returns the base colour, and setAlpha0xC3900 writes separate alpha/packed colour fields. The zero base colour copied from a root without text and zero font height directly explain the invisible custom labels.

`RenderedTextTests.cpp` first failed with the old root-only resolver on an EditBox with null mainText/zero nonvirtual getters and a Client at21px with opaque colour. The corrected Client-first resolver passes plain, composed, missing and cyclic cases. The actual production measurement-cache helper is exercised for1000 unchanged frames/layout-only requests: no additional creates, Begin/font setup, SetCaption or width measurements; actual height or locale-content changes cause one bounded seven-caption refresh.

The hidden native measuring widget and its caches are owned by the plugin. Popup row pointers are obtained via public ListBox::getWidgetByIndex, used only during that call, and never retained. Actual popup text-region chrome is calculated separately from the closed combo. Missing money render text or a zero/empty/transparent source style restores/leaves the native header visible rather than hiding it.

A matching candidate2 MAP, compiler-measured cache layout and fresh read-only probe are prepared before the next user launch. That future capture must show all three custom cash styles with positive height/alpha, correct Client widths and stable measurement counters. This section records a source/Native-contract correction; visual acceptance of candidate2 is still pending.

## Accepted candidate and removal of player wallet

The user accepted candidate7c629088: sums and preset names are visible. Root's controlled snapshot consumed613220 bytes with no read errors: the original three labels used opaque21px text, closed combo191/client157 with Native text153, popup217 and rendered text157, and stable measurement counters.

The next user request removes the player wallet entirely. The current header owns exactly two labels, OTT_TraderMoney and OTT_TradeBalance, with the same accepted rendered-text style. No OTT_PlayerMoney widget, player-money cache, Character::getMoney or getAnyPlayerCharacter call remains in this UI translation unit. The PE audit rejects those player-wallet getter imports. The layout helper receives playerNeed0; no hidden third row is reserved for an absent player label. Initial headerCaptionSets is2 instead of3; counters remain cumulative across reopen and unchanged frames add no writes. New unit cases cover the two-label layout, narrow two-label wrap, merchant-only mode, and both opaque21px style copies. Font/footer behaviour and MO/config files are unchanged.

Candidate3 has a separate matching MAP/compiler layout/probe; candidate2 artifacts and its accepted snapshot are preserved. The two-label simplification still awaits the short final visual check after Root installs it; GOG runtime acceptance remains outstanding.

## Native plate reset regression

Independent actual-production review reproduced a native reset to the original money-plate rectangle while captions and semantic cache keys were unchanged: base width191, owned width468, reset left the plate at191 with owned text reaching447. The original independent case failed4 checks/3 failures. The correction compares live geometry with the last owned rectangle projected through ordinary parent alignment before the baseline helper adopts any native reset. A mismatch invalidates header layout even when the semantic text cache is unchanged.

The exact independently supplied case now passes4/0. The production header suite adds NativeReset -> Update ->100 stable calls for all three XML/style fixtures:93 checks/0, with no additional caption writes, widget allocations or geometry writes during the stable calls. Native baseline restoration, theme inheritance, two amounts and existing font/measurement caches remain covered. These are offline source/model checks, not visual or sale acceptance.


## Missing header in both themes: real layout parent (2026-10-30)

The direct-parent guard introduced with native background preservation rejected
both installed layouts. `ResolveInjectionParent(Window)` returns the Window skin
Client. `Kenshi_InventoryTraderWindow.layout` places the native money plate inside
its separate Content widget. The already captured RUNTIME-35788 candidate2 snapshot
independently records plate -> generated *_Content -> Client -> Window; its combo
controls instead live directly under Client. This explains a functioning footer
with a completely native cash header and no creation/style error in the log.

The previous production-function test flattened the plate directly under the
injection parent. Adding the actual nested Content, with distinct position and
size, failed five assertions before the fix (102 checks / 5 failures). The fix
accepts only a bounded chain of six widget nodes ending at the exact injection
parent. A sibling host, foreign host, cycle or excessive nesting is rejected.
The plate's immediate parent remains its coordinate/resize/restore reference;
no reparenting or skin/colour substitution is performed. Existing native text
insets, original background, native rendered font and XML text role are retained.
The footer and inventory viewport reservation still belong to the injection host.

The new tests cover offset nested Content, its own resize, native baseline
restoration, repeated stable frames without allocations/coordinate writes, foreign
and sibling hosts, a cycle and the explicit nesting bound. This is offline source
and historical snapshot evidence. The corrected DLL still requires fresh in-game
visual acceptance in vanilla and Dark UI; no process was opened for this work.


## Independent review: combined header and viewport reservation

The first Content-parent candidate (21e24a23) is rejected. Independent review
extracted both real production functions and reproduced another direct-parent
assumption in ReserveAdaptiveRows: seven checks, two failures. The header wrapped
by41px, but both Content-owned inventory viewports remained at their original top.
The previous header suite replaced that function with a no-op and missed overlap.

The permanent suite now extracts the complete production viewport binding,
reservation and teardown helpers alongside UpdateTradeBalance. Its first nested
combined case failed five checks (117 checks /5 failures) before this correction.
A viewport now retains weak references to its actual parent and exact injection
host. Alignment projection and reservation restoration use that immediate parent's
dimensions. Reparent/replacement clears the old identity and baseline; an exact
owned rectangle is undone on an in-scope reparent, while a supplied new Native
rectangle becomes the new baseline. Foreign roots and nested Windows are excluded.
Coordinate callbacks are followed by weak-reference/parent revalidation before
further GUI access. Destruction invalidates all viewport/parent/host references.

Native resource prefixes are accepted with an exact reserved suffix. The longer
scrollview_backpack_content role is checked before backpack_content, preventing
one ScrollView from being used for both. Binding uses a fixed128-entry breadth
first queue, at most six ancestry nodes and six search levels; inventory icon
children are never traversed. Duplicate roles fail closed. A missing role is
retried after60 reservation calls; normal stable frames perform no tree search,
allocation or geometry write. These are bounded source operations, not an FPS
or memory benchmark.

The combined cases cover separate Content offsets/dimensions, own-parent resize,
1000 stable frames, disable/reopen restoration, in-scope reparent with retained
owned geometry, an absolute Native baseline reset, foreign-root replacement,
Native delete/recreate, clipping with independent initial visibility, missing
raw viewport versus ScrollView role aliasing, nested foreign Windows, late
creation,300 icon descendants, synchronous coordinate-callback destruction and
window close. Together with the existing XML/style scenarios the suite passes
136 checks. In-game visual acceptance of the new candidate remains pending.


## 2026-11-01: restored player wallet (prepared release 7)

The user's newer request supersedes the two-label decision above. A third native-skinned label, OTT_PlayerMoney, is restored inside the current real money plate and right-aligned within its native content insets. The existing Header allocator receives the measured right-label width; narrow-window wrapping still reserves viewport space. The wallet source always uses getAnyPlayerCharacter, not selectedCharacter, so inspecting an NPC cannot select that NPC's purse. Actual Steam getAnyPlayerCharacter is O(1), returning the first entry of the player's member list. No Character/ownership pointer survives the update.

The VC10 production DLL emits the imported player getter followed by a null check and virtual Character getMoney dispatch at vtable+0x1b8; getMoney itself therefore need not appear in the import table. CheckNativeContract now verifies this compiled call sequence and all installed native exports. Three-widget creation/teardown, failure at each allocation, three installed XML role fixtures, player money change, missing-player fallback and unchanged-frame counters are covered by extracted production tests. No new per-frame widget allocations or repeated caption/geometry writes occur in those fixtures. This does not establish in-game FPS or long-term leak freedom.
