Emkejs Mod Core is the shared RE_Kenshi runtime behind my Mod Hub-integrated mods. Install it before supported consumer mods to get one in-game Mod Hub menu and the shared core they depend on.

[h3]Requirements[/h3]
- RE_Kenshi

[h3]What This Mod Does[/h3]
- Provides the shared Mod Hub UI/runtime used by supported mods
- Ships the shared SDK/helper path used by supported consumer mods
- Hardens attach, export, deploy, and fallback behavior for Mod Hub consumers
- Adds configurable logging controls for troubleshooting while keeping default release logging quieter
- Improves Mod Hub search with scoped `mod:term` queries, a clear button, keyboard word editing, smoother list browsing, session search persistence, and configurable session collapse persistence

[h3]How to Use[/h3]
- Install RE_Kenshi and this mod.
- Place `Emkejs-Mod-Core` before mods that integrate with Mod Hub.
- Open Options, then Mods, then Mod Hub to manage settings exposed by supported mods.
- Search stays in place when you close and reopen Options during the same Kenshi session, until you clear it or exit Kenshi.
- You can also keep mod sections collapsed/expanded across Options reopen, optionally auto-focus the search box on open, and use `Ctrl+F` or `/` to jump back into search while Mod Hub is active.
- Use `Collapse all` / `Expand all` to fold or reopen all mod sections in the current namespace.
- By itself this mod is mostly shared infrastructure, not a standalone gameplay feature.
[u]Configuration:[/u]
- `emkejs-mod-core.ini` stores the small set of core settings owned by Emkejs Mod Core.
- `mod-config.json` also includes optional `debugLogging`, `debugSearchLogging`, and `debugBindingLogging` switches for troubleshooting.
- Mod Hub now includes an `Emkejs Mod Core` section with:
- `Persist search until cleared`
- `Persist collapse state until exit`
- `Auto focus search on open`
- Most user-facing settings still come from supported consumer mods through Mod Hub.

[h3]Compatibility[/h3]
- Load this before supported Mod Hub consumer mods.
- No direct gameplay changes on its own.
- Consumer mods still own their own behavior and compatibility.

[h3]Troubleshooting[/h3]
- Confirm RE_Kenshi is installed and enabled.
- Confirm `RE_Kenshi.json` includes `Emkejs-Mod-Core.dll`.
- If an update fails because the DLL is locked, close Kenshi and retry.

Version: 0.1.0-alpha.2

[h3]Patch Notes[/h3]
- Logging policy/config scaffolding with opt-in debug/search/binding logging and quieter default release diagnostics
- Logging-policy smoke coverage plus build stability cleanup around startup version detection
- README/build docs now clarify the shared build-scripts subtree workflow and local wrapper-script boundary
- Initial Mod Hub core release with shared runtime, consumer SDK, and scaffold workflow
- Reliability hardening for attach/fallback, export compatibility, and deploy preflight
- Improved Mod Hub search with scoped queries, clear button, `Ctrl` word-editing shortcuts, session persistence controls, optional search auto-focus, and `Ctrl+F`/`/` search focus shortcuts
- Smoother Mod Hub scrolling, clearer numeric helper text, and quick collapse/expand controls
- Shared build scripts now use the subtree workflow and emit one final timestamp on completion

[h3]Support[/h3]
- Use the Steam Workshop comments/discussions and the mod release pages for updates and feedback.
