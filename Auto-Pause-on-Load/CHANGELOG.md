# Changelog

All notable changes to Auto-Pause-on-Load will be documented in this file.

## [Unreleased]
- Fixed load-pause false positives during long-distance travel and similar non-save streaming transitions by requiring real save-load intent before arming the pause.
- Fixed the first save loaded from the main menu sometimes missing the pause because the initial load transition was observed too late.
- Hardened inventory pause detection to scan the current selection instead of the full player roster and to use direct inventory widget visibility checks, removing the repeated inventory-visibility exception flood from APOL logs.
- Fixed trade and inventory pause toggles so trader windows use the trade setting and normal inventory uses the inventory setting.
- Fixed normal inventory pause being suppressed after closing trade.
- Reduced background trade and inventory pause checks so UI detection no longer runs every frame.
- Reduced trader-window fallback scanning while no trade window is open.
- Renamed the public debug config key to `debug_logging` while continuing to accept `debug_log_transitions` as a legacy alias on read.

### Nexus
- Fixed the game pausing by mistake during long-distance travel and other similar transitions where no actual save was being loaded.
- Inventory pause detection is quieter and no longer floods RE_Kenshi logs with inventory-visibility warnings.
- Trade and inventory pause toggles now affect the correct UI windows independently.
- Background trade and inventory pause checks now do less repeated work during normal gameplay.
- Debug config now uses `debug_logging`; legacy `debug_log_transitions` configs still load and are rewritten automatically.

### Steam
- Load pause now ignores distant-travel false positives and catches the first real save load again.
- Inventory pause detection is quieter and no longer floods RE_Kenshi logs with inventory-visibility warnings.
- Trade and inventory pause toggles now affect the correct UI windows independently.
- Background trade and inventory pause checks now do less repeated work during normal gameplay.
- Debug config now uses `debug_logging`; legacy `debug_log_transitions` configs still load and are rewritten automatically.

## [0.2.0-alpha.1] - 2026-03-17
- **Added Mod Hub integration** with lazy hook installation via late lifecycle host
- **Runtime improvements**: Removed dead load-host investigation scaffolding and unreachable deferred SaveManager hook path
- **Build system**: Migrated build-scripts from submodule to subtree consumption
- Hooks now install after game reaches safe state (deferred from startup) to avoid crashes

### Nexus
- Added Mod Hub integration
- Code cleanup

### Steam
- Added Mod Hub integration
- Code cleanup

## [0.2.0-alpha.0] - 2026-02-10
- Added pause/resume behavior for trading.
- Added pause/resume behavior for inventory.
- Added dedicated config toggles for trade/inventory pause and resume.

## [0.1.0-alpha.1] - 2026-02-10
- Initial release of Auto Pause on Load.
- Auto-pauses once after save loads complete.
- Uses `SaveManager::load(...)` hooks to arm pause-on-load behavior.
- Detects load transitions via `GameWorld::isLoadingFromASaveGame()`.
- Forces paused state with `GameWorld::userPause(true)`.
- Includes configurable settings in `mod-config.json`:
  - `enabled`
  - `pause_on_save_load`
  - `pause_debounce_ms`
  - `debug_logging` (`debug_log_transitions` legacy alias accepted)
