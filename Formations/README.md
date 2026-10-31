# Formations (LEOPARD fork)

Bannerlord-style squad formations for Kenshi - an RE_Kenshi plugin.

Original: **Formations - Squad Formations** by **Slinkysalmon03**
([Steam Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3795714050)), GPLv3 (see
`LICENSE`). The author ships the source with the mod and keeps no repository; the first commit of this
folder is that source unchanged.

## What the fork adds

- **Follow formations.** The squad keeps a shape while it follows its leader, not only where a drag
  ends. The game's "Follow Formation" list on the AI tab gets eleven shapes besides Random, Military and
  Caravan: Wedge, Line, Ring, Rhombus, Square, Column of two, Pincers, Diamond, Moth, Chevron (sorted,
  shown without scrolling). It is the default for every player squad; in Mod Config Menu each squad of
  the loaded game can have its own. Only the follower's offset from the leader is replaced
  (`Blackboard::getFormationPositionOffset`) - the game's own follow does the walking and pathfinding,
  no orders of our own. The choice travels with the save as `Formations.save` in the save folder; the
  game's own value stays 0-2, so a save played without the plugin falls back to the game's shapes.
- **The same shapes for the drag.** Square, Rhombus, Column of two, Pincers, Diamond, Moth and Chevron
  join Line, Circle, Rectangle and Wedge (default keys 6 7 8 9 0 - =).
- **Any modifier key.** Any key or combination, captured in Mod Config Menu (was ALT/CTRL/SHIFT only).
- **Hold is optional.** "Hold position after forming up" - off, and a formation no longer switches the
  game's Hold on.
- Settings page in Mod Config Menu (en/ru/zh), readout in the game's language.

## Version 2

- Fixed language selection for Steam Workshop installations. The MCM page and
  in-game readout use the game's settings, regardless of the DLL's location.
- Normal and nested RE_Kenshi executable paths are supported. No working-directory
  changes or polling were added; language is read when the UI is initialized.
- Personal settings and formations stored in saves remain unchanged.

Версия 2: исправлен выбор языка при установке через Steam Workshop. Страница
MCM и игровые подписи используют настройки игры независимо от расположения DLL,
включая вложенный EXE RE_Kenshi. Личные настройки и строй в сейвах не изменяются.

Validation: five checks compile the production language reader against local,
Workshop, nested executable and missing-settings fixtures. No frame-loop work,
world pointers or persistent file handles are added.

## Build

`D:\DEV\Kenshi\build_formations.ps1` (MSVC 2022, `/utf-8`). KenshiLib is not linked - everything is
reached through `GetProcAddress`. Shared headers (`ModConfigMenu.h`, `HoldKey.h`) are in
[LEOPARD-Kenshi-Plugins](https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Plugins) (`shared/`); the
per-squad page needs Mod Config Menu with API v6.
