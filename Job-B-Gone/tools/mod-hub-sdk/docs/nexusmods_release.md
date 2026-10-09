Category: Utilities
Mod name: Emkejs Mod Core
Current version: 0.1.0-alpha.2

Brief overview
Emkejs Mod Core is the shared RE_Kenshi runtime behind my Mod Hub integrations. It gives supported mods one in-game Mod Hub menu and provides the shared SDK/helper path used by consumer mods.

Detailed description
This is primarily a dependency/runtime mod for my other mods and any plugin that integrates with the Mod Hub API.

[b][size=4]What it does:[/size][/b]
- Provides the shared Mod Hub UI and registration/commit pipeline.
- Ships the shared SDK/helper runtime and scaffold workflow used by supported consumer mods.
- Hardens deploy, attach, export, and fallback behavior with the reliability work added for Mod Hub v1.
- Adds configurable logging controls for troubleshooting while keeping release-default logging quieter.

[b][size=4]What it does not do:[/size][/b]
- It does not add standalone gameplay content by itself.
- It does not replace behavior owned by consumer mods; those mods still own their own features, settings, and compatibility.

[b][size=4]Install[/size][/b]
- Ensure RE_Kenshi is installed and enabled.
- Download and extract.
- Copy the `Emkejs-Mod-Core` folder into your Kenshi `mods` directory.
- Enable the mod in the Kenshi launcher.
- Recommended: place `Emkejs-Mod-Core` before mods that integrate with Mod Hub.

[b][size=4]How to Use[/size][/b]
- Install it alongside supported consumer mods.
- Open Kenshi options, and use the Mod Hub tab to manage settings exposed by supported mods.
- Use scoped search like `loot:enable` to search inside one mod only.
- In the search box, `Ctrl+Left`, `Ctrl+Right`, and `Ctrl+Backspace` work for faster keyboard editing.
- Search now stays in place when you close and reopen Options during the same Kenshi session, until you clear it or exit Kenshi.
- Use `Collapse all` / `Expand all` to fold or reopen every supported mod section in the current namespace.

[b][size=4]Config[/size][/b]
- `emkejs-mod-core.ini` stores the small set of core runtime settings owned by Emkejs Mod Core itself.
- `mod-config.json` also exposes optional debug logging switches:
- `debugLogging`
- `debugSearchLogging`
- `debugBindingLogging`
- The Mod Hub includes an `Emkejs Mod Core` section where you can toggle:
- `Persist search until cleared`
- `Persist collapse state until exit`
- `Auto focus search on open`
- End-user settings for supported mods are still exposed through those consumer mods inside Mod Hub.

[b][size=4]Current Features[/size][/b]
- Mod Hub v1 core runtime with bool, keybind, int, float, and action row support.
- Public consumer SDK/helper runtime plus scaffold generation for mod authors.
- Shared logging policy/config scaffolding with opt-in debug/search/binding logging controls.
- Improved Mod Hub search with scoped `mod:term` queries such as `loot:enable`, clear button, and keyboard word editing with `Ctrl+Left`, `Ctrl+Right`, and `Ctrl+Backspace`.
- Search and collapse-state persistence across Options reopen for the current Kenshi session, configurable from the `Emkejs Mod Core` section in Mod Hub.
- Optional automatic search-box focus when opening Mod Hub, configurable from the `Emkejs Mod Core` section in Mod Hub.
- `Ctrl+F` and `/` can now jump focus into the Mod Hub search box while the Mod Hub tab is active.
- Smoother Mod Hub list scrolling, clearer numeric helper text and hint placement, larger setting labels, and a quick `Collapse all` / `Expand all` control.

[b][size=4]Compatibility[/size][/b]
- Load order recommendation: place `Emkejs-Mod-Core` before mods that integrate with Mod Hub.
- No direct gameplay conflicts are expected from this mod alone because it acts as shared runtime infrastructure.
- Consumer mods still define their own gameplay compatibility.

[b][size=4]Troubleshooting[/size][/b]
- Ensure RE_Kenshi is installed and enabled.
- Confirm `RE_Kenshi.json` includes `Emkejs-Mod-Core.dll`.
- If an update/deploy fails because the DLL is locked, close Kenshi and retry after the file is no longer in use.

[b][size=4]Changelog[/size][/b]
- 0.1.0-alpha.2: Gives supported mods one shared in-game settings menu with better search, keyboard shortcuts, smoother scrolling, configurable debug logging defaults, search and collapse persistence controls, optional search auto-focus, quieter release diagnostics, clearer numeric helper text, and build/release workflow cleanup.

[b][size=4]Credits[/size][/b]
- Lo-Fi Games (Kenshi)
- RE_Kenshi team

[b][size=4]Support[/size][/b]
- Please report issues and feedback in the Nexus comments for this mod.

[b][size=4]Support the developer[/size][/b]
If you enjoy my mods and would like to support development:
https://ko-fi.com/emkej

[b][size=4]Requirements[/size][/b]
- [url=https://www.nexusmods.com/kenshi/mods/847]RE_Kenshi 0.3.0[/url]

[b][size=4]Tested with[/size][/b]
- Kenshi 1.0.65
- RE_Kenshi 0.3.0
- Windows 11

[b][size=4]My other mods[/size][/b]
[url=https://www.nexusmods.com/kenshi/mods/1863]Wall-B-Gone[/url] - Adds a hotkey that allows the player to quickly dismantle player-built walls
[url=https://www.nexusmods.com/kenshi/mods/1867]Auto-Pause on Load[/url] - Auto-pauses Kenshi right after a save finishes loading
[url=https://www.nexusmods.com/kenshi/mods/1870]Job-B-Gone[/url] - Adds an in-game above the squad UI so you can quickly remove queued jobs for one member, selected members, a whole squad, or all player squads.
[url=https://www.nexusmods.com/kenshi/mods/1871]Loot-Scoot-Execute[/url] - Adds an Execute action for downed enemies in the right-click context flow
[url=https://www.nexusmods.com/kenshi/mods/1873]Vital Sense[/url] - Highlights downed characters with state text/icons and shows configurable bounty symbol markers for downed and live bounty targets while ALT is held.
[url=https://www.nexusmods.com/kenshi/mods/1886]Organize the Trader[/url] - Adds a search bar to Kenshi trader windows so you can find items faster.
