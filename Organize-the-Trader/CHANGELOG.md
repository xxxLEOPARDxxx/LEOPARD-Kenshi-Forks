# Changelog

All notable changes to Organize-the-Trader will be documented in this file.

## [0.1.0-alpha.2] - TBD
- Add a separate draggable sort panel to trader windows.
- Add trader sorting by name, unit price, stack value, weight, and value/weight with an ascending or descending toggle.
- Sorting now respects mixed item sizes, keeps matching search results packed at the top, and stays usable after Kenshi's Arrange action.
- Add persisted sort panel size and position settings alongside the existing search controls.
- Add blueprint-aware trader search using blueprint target item names.
- Add the `b:` blueprint-only search prefix and pack matching search results to the top of the trader grid.
- Add configurable trader search autofocus in Mod Hub and `mod-config.json`.
- Make `Tab` blur the trader search input when it is focused.

## [0.1.0-alpha.1] - 2026-03-12
- Restrict supported Kenshi runtime builds to `1.0.65`.
- Add a Mod Hub runtime smoke-test script for live attach/registration verification.
- Align Mod Hub integration to the helper-owned retry flow documented by `Emkejs-Mod-Core`.
- Align shared build tools to the `git subtree` consumer flow.

## [0.0.0] - 2026-02-24
- Reset repository to a clean Organize the Trader plugin base.
- Removed legacy feature implementation and related release artifacts.
