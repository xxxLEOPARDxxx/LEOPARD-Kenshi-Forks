# Shared context-menu admission, 2026-11-18

Baseline: published KEO4, `8e18f0c19f78b8cd2d5727bfd90156edf6795866`.
The supplied paired logs record `native task contract refused` before the job
hooks are installed. KEP0.17.3 and KenshiLua load before KEO; Animal_Husbandry-Lua
is present. The files remain private in ignored build storage. No new player
capture, game run, diagnostic DLL or publication is part of this change.

## Reproduced compatibility defect

`Plugin::start` in KenshiLua loads active mods' `scripts/init` files before its
plugin entry returns. Animal Husbandry's startup registers the harvest events,
including `ContextMenuGUI::show`. KenshiLua consequently installs an ordinary
KenshiLib/MinHook hook before KEO's startup profile check. That check required
the original32-byte context-menu entry and therefore refused all custom jobs.
This reproduces the logged refusal; the logs do not contain the actual mismatched
window, so it is not proof that every reported refusal has this same cause.

Source reference, pinned to public commit53a7e4d1dc544a6b51872d03990f97af5dded94e:

- https://github.com/Genpretz/KenshiLua/blob/53a7e4d1dc544a6b51872d03990f97af5dded94e/src/Plugin.cpp
- https://github.com/Genpretz/KenshiLua/blob/53a7e4d1dc544a6b51872d03990f97af5dded94e/examples/example_mods/Animal_Husbandry-Lua/scripts/init/00_ah_startup.lua
- https://github.com/Genpretz/KenshiLua/blob/53a7e4d1dc544a6b51872d03990f97af5dded94e/examples/example_mods/Animal_Husbandry-Lua/scripts/ah/harvest.lua
- https://github.com/Genpretz/KenshiLua/blob/53a7e4d1dc544a6b51872d03990f97af5dded94e/src/Hooks/Hooks_Gui.cpp

## Fix and boundaries

Admit a five-byte E9 entry only for CONTEXT_SHOW when all remaining27 native
bytes match. KEO chains this UI hook through AddHook rather than calling it as
an unmodified native function. No factory/data/code/RTTI/vtable requirement is
relaxed. The established main shared-prologue policy is broader; this exception
is intentionally limited to the reproduced entry. Existing install-stage skips
for KEO's own caption/context hooks are unchanged. No private core ABI bypass,
module allowlist removal or foreign callback replacement is introduced.

Same-target chaining is supported by the SDK's unique hook IDs and its
m417z/minhook multihook dependency; the previous E9 is relocated into the new
trampoline. KenshiLua and KEO use the same Windows x64 arguments at this entry.

## Verification and limits

- Regression fails on old production code at shared-menu admission, then passes
  in Od and O2:148 checks each. Tests include every tail byte, unreadable input,
  arbitrary patched opcode, other native entries, all vtable slots and ASLR.
- Steam test windows and vtables checked against the actual EXE's existing
  SHA256504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e.
  GOG uses the previously reviewed frozen contract snapshot, not an actual GOG
  execution or a fresh GOG file comparison.
- Current-checkout native factory206 controls pass; extracted input/caption
  runtime321 controls pass in Od/O2. Existing eight negative controls fail as
  expected. Tests execute owned host fixtures, not the game.
- Full391-source DEV and PROD builds pass; x64/startPlugin and183 KenshiLib0.5.0
  imports checked on both DLLs. Independent review finds no blocking issue.
- Startup-only fixed-window comparison; no additional gameplay loops, locks,
  allocations, retained pointers or file I/O. No measured FPS claim.

Owner's installed DLL, personal settings, saves and published4 packages remain
untouched. This is an ordinary functional compatibility fix for the next release,
not a diagnostic build and not a confirmed in-game fix for every affected player.
