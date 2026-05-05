# Changelog

All notable changes to Vital Read will be documented in this file.

## [0.1.2] - 2026-05-05
### Added
- Added Mod Hub dropdown controls for portrait icon and text corner settings.
- Added a default playing-dead portrait icon using the same built-in Kenshi UI atlas pip used by Vital Sense.

### Changed
- Moved `debugLogging` into the Mod Hub Advanced section and removed the obsolete search and binding debug toggles from Vital Read configuration.
- Aligned the Mod Hub SDK integration with current upstream section support so Vital Read settings appear correctly in the in-game Mod Hub menu.

### Fixed
- Fixed portrait overlay cache validation so partial or hidden portrait bars no longer trigger repeated full GUI scans.
- Reduced avoidable portrait overlay refresh and texture work during normal runtime updates.

## [0.1.0-alpha.1] - 2026-03-28
- Initial alpha release.
