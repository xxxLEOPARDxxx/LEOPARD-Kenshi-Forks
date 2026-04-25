# Changelog

All notable changes to Vital-Sense will be documented in this file.

## [0.2.0-alpha.2] - 2026-04-25
### Added
- Expanded Emkejs Mod Core Mod Hub coverage to the full config surface, including grouped sections, color rows, bounded text rows, and hover hints on supported core builds.

### Changed
- Reduced highlight overhead in dense scenes by caching tint sync work, marker visuals, and repeated not-downed probe results instead of rebuilding them every active frame.

## [0.2.0-alpha.1] - 2026-03-26
### Added
- Configurable per-character body tint highlighting for downed targets, including squad and bounty-only controls.
- Full Emkejs Mod Core Mod Hub coverage for the config surface, including grouped bool, int, keybind, select, text, and color rows with hover hints on supported core builds.
- `highlight_key` plus optional `CTRL` / `SHIFT` / `ALT` modifiers, with `UNBOUND` support and migration from deprecated `only_when_alt_held`.

### Changed
- Shared build scripts and Mod Hub SDK consumption now follow the current shared consumer workflow.

### Fixed
- Tint stability across reloads and animal material restore paths.

## [0.1.0-alpha.2] - 2026-03-02
- Added configurable bounty marker support for downed targets and live on-screen targets with bounty.
- Added live bounty-only display mode (`$`) in addition to downed bounty markers, with stable anchor control via `bounty_symbol_live_anchor_y_offset_cm`.
- Added bounty symbol configuration: custom symbol text, symbol placement before icon/text, and show bounty symbol on all characters.
- Added configurable bounty icon glow support (`show_bounty_glow`).

## [0.1.0-alpha.1] - 2026-02-24
- Reset repository to a clean Vital Sense plugin base.
- Removed legacy feature implementation and related release artifacts.
