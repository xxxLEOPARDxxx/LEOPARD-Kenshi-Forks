# Changelog

All notable changes to Map-markers are documented in this file.

## [0.1.0-alpha.2] - 2026-03-12

- Added Mod Hub int-setting v2 support for `default_marker_type` on compatible Emkejs Mod Core builds.
- Reduced default probe logging noise while keeping targeted diagnostics available.
- Refreshed release and build documentation to match the current config defaults and subtree-tracked shared scripts.

## [0.1.0-alpha.1] - 2026-03-12

- Proved world-map feasibility through runtime MyGUI discovery and diagnostics hooks.
- Added map-space marker rendering on the Kenshi world map.
- Added middle-click marker placement and per-save JSON persistence.
- Expanded the single-marker spike into a multi-marker management MVP.
- Added marker selection, move, delete, and right-click deselect flow.
- Added marker types, labels, and a selected-marker editor panel.
- Added persistent UI settings in `mod-config.json`.
- Added a `Markers: On/Off` footer button on the map UI.
- Improved label editing with single-space input and `Ctrl+Left`, `Ctrl+Right`, and `Ctrl+Backspace`.
- Added overlay diagnostics for map/footer/editor troubleshooting.
- Fixed markers drawing over the editor by masking overlapping marker widgets.
- Added optional Emkejs Mod Hub integration for `enabled`, `close_editor_on_map_close`, `show_hover_labels`, and `default_marker_type`.

## [0.0.0] - 2026-02-24

- Reset repository to a clean Map-markers plugin base.
- Removed legacy feature implementation and related release artifacts.
