# Optional automatic exterior navmesh recovery - verification, 2026-11-17

Unreleased change intended for KEO4 (published baseline KEO3). Installed locally.
No game launch, gameplay run, profile, capture or new gameplay logs.

- Policy runtime RED: compiled old stub, 4108 CHECK failures, exit1.
- Final host run: 128 suites, all passed (includes actual config/MCM live/defaultOFF/off/on).
- Actual runtime translation unit with fake native boundaries: /Od and /O2 PASS.
- Six compiled production mutants all fail CHECK with exit1: ownership, live off, adoption, queue, stale cooldown clock, world reset.
- Full 391-unit DEV/PROD /O2 /GL /LTCG builds pass.
- Both binaries: x64, startPlugin export and 183 KenshiLib imports admitted against0.5.0.
- Three157-key MO catalogs compile and gettext roundtrip; Russian ASCII-hyphen check passes.
- Independent review /root/auto_nav_native_design: static native target/timers/active-set ownership, queue/done lock order, world lifetime and same-frame adoption reviewed.
- Review fixes: schedule after optional handoff/preload, remove stale adoption-cycle latch, add nonwaiting processJob probe, preserve Debug-last, correct legacy navmesh alias, fresh completion clock for five-second spacing.
- Installed INI and mods.cfg hashes unchanged; prior DLL/catalogs/README preserved under build/auto-nav-verification/installed-before.

## Scope and limits

The once-per-world table records accepted requests, not verified repaired paths.
Queue/PJ/physics idle gates are advisory snapshots; normal work may arrive afterward.
Full exterior generation remains asynchronous through the existing workers, with
normal cache invalidation and queue lifetimes. No new activation detour, furniture
repair or character teleportation. No game-level path/FPS/memory improvement claim.

## Installed files

- `KEO.dll`: 2138112 bytes, SHA256 `87bd8e71face18cf6900df58e8004159519b9dbfabffaa60c0442a35b54eabb9`.
- `locale/en_GB/LC_MESSAGES/keo.mo`: 28827 bytes, SHA256 `e07f1ecc9e9455c5b30da7894787354bf0cbc0ec81ddf3346e5eee78eee5ffaf`.
- `locale/ru_RU/LC_MESSAGES/keo.mo`: 40670 bytes, SHA256 `5bcdf55a4d292f2dadab6aa90276f79e8133a3191b41488a8670199a74b97bd5`.
- `locale/zh_CN/LC_MESSAGES/keo.mo`: 27982 bytes, SHA256 `65b03945b5811ebbd88f553d90fb2cfbef75afddb246e6fd5646044e4c011ddc`.
- `README.txt`: 74680 bytes, SHA256 `d0a249cb2ba24ea919c58b1daf6fb3a272ef625edff63900fa2f0d9fce40d0f1`.

## Protected preferences

- `D:\SteamLibrary\steamapps\common\Kenshi\mods\KEO\KEO.ini`: `6387c7e46e4fd253409da14f479312726ba7929d3c43a8dca108da15564a1d66` before and after.
- `D:\SteamLibrary\steamapps\common\Kenshi\data\mods.cfg`: `57446d44e5ccf23cd16b06063e3706d011fa558f322c6c6464db82f7e395e93f` before and after.
