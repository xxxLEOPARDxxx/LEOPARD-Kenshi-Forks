# Character Highlight hover and intensity

Goal: independent hover enable toggle; beige contour by default, optional current relationship colors. Add 0-100% tint intensity that preserves clothing textures. Existing hold-key coloring, portraits and Vital Sense precedence stay intact. No restart required for settings. Debug last and off by default.

Implementation: use existing SDK cursor target with cursorAction and GUI gates. Keep target by full handle. User expanded scope to optional contours for ALT as well as hover. Clone per-subentity materials with an owned inverted-hull pass, share original shader parameters, and restore only live matching subentities. Public appearance scene nodes supply body/equipment candidates; equipment coverage requires game validation. Bound actors/materials and new attachments per frame, cache unsupported materials, guard native traversal/cleanup, and suspend only the portrait actor's contour. Programs are bounded shared resources. Global coloring keeps its existing refresh budget. Track effective applied RGBA/depth for portrait restoration; do not restore raw cached relation colors.

Steps:
- Add production policy tests for opacity clamp, hover/global priority and disabled mode; verify initial failure.
- Implement policy/settings, bounded contour renderer and cursor target lifecycle. Check native cursor semantics before claiming hover works.
- Update ru_RU/en_GB/zh_CN, defaults and README. Prepare one version4 candidate against published3; do not publish or add personal released changelog yet.
- Build VC10 x64 /GL /LTCG, verify imports, run policy/shader checks and independent perf/lifecycle review. Install only if process closed; preserve personal INI.
- User/game validation: move between two characters and ground/UI, beige vs relationship mode, faint intensity, portrait generation, hover disabled, original hold key and Vital Sense.

Risks to review: material replacement during zone transitions, stale target while menus open, shader auto constants and hidden parts, non-human skeletons, applying a custom pass to shared material, portrait capture, no source-package disclosure of unrelated projects. No world traversal added when hold-key coloring is inactive.

## Verification and local installation

2026-11-06: VC10 x64 /GL /LTCG native build passed. Nine HLSL variants compiled with D3DCompile_47. Intensity boundaries and 12 explicit player-facing mode scenarios passed under /Od and /O2. All 53 MCM strings resolve in each of en_GB, ru_RU and zh_CN; all three compiled MO catalogues were parsed independently by gettext. x64/plugin exports and all 70 imported KenshiLib/Ogre/MyGUI functions match the installed runtime. Native read-only evidence confirms ordinary cursor updates reset cursorAction and clear both targets; action-target overload sets them again. Independent renderer review completed and its cleanup/cache/portrait/budget findings addressed.

Candidate SHA256 F48716D722C59B206AFEC4637A19E5E69CC344E674B50068054F1FABB540C744 installed locally with previous DLL/settings backup; personal INI and RE_Kenshi.json hashes unchanged at installation. Verified candidate module path in running process PID30568 and its MCM registration. User visual test requested; contour appearance, complete equipment coverage, FPS and long-run memory still pending. No public publication, package upload or personal released changelog entry.
