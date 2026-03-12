# Changelog

All notable changes to Vital-Sense will be documented in this file.

## [0.2.0-alpha.1] - 2026-03-12
- Added configurable per-character body tint highlighting for downed targets, including squad and bounty-only controls.
- Added optional Emkejs Mod Core Mod Hub integration for supported bool/int/keybind settings.
- Added `highlight_key` plus optional `CTRL` / `SHIFT` / `ALT` modifiers, with `UNBOUND` support and migration from deprecated `only_when_alt_held`.
- Improved tint stability across reloads and animal material restore paths.
- Aligned shared build scripts and pinned Mod Hub SDK consumption with the current shared consumer workflow.

## [0.1.0-alpha.2] - 2026-03-02
- Added configurable bounty marker support for downed targets and live on-screen targets with bounty.
- Added live bounty-only display mode (`$`) in addition to downed bounty markers, with stable anchor control via `bounty_symbol_live_anchor_y_offset_cm`.
- Added bounty symbol configuration: custom symbol text, symbol placement before icon/text, and show bounty symbol on all characters.
- Added configurable bounty icon glow support (`show_bounty_glow`).

## [0.1.0-alpha.1] - 2026-02-24
- Reset repository to a clean Vital Sense plugin base.
- Removed legacy feature implementation and related release artifacts.
