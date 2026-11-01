# Organize the Trader language fallback audit, 2026-11-01

The Nexus commenter reported that both the original and this fork did not work
and asked whether German could be responsible. The comment supplies no precise
symptom, log, dependency versions, or reproduction steps. It does not establish a
crash or a localization cause.

No unsupported-language fallback defect was reproduced. No production code or
translation was changed; German and other unsupported languages use English.

## Executable evidence

`CheckLanguageFallback.py` builds `LanguageFallbackTests.cpp` with VC10 against
the unmodified shared `Localization.h`. Only the game's LocaleManager service and
log sink are adapters; Windows filesystem functions, catalog discovery, MO parsing,
candidate selection, and Tr are production code. Each locale runs in a fresh
process, preserving the one-load-per-DLL lifetime contract.

Two loader snapshots were tested:

| Loader | SHA256 |
| --- | --- |
| Current `D:/DEV/Kenshi/ours/shared/Localization.h` | `00e8951c6a1bfbb0ade74483ca6021f55002239c15c97755904ec1596f532449` |
| v7-era `b08772c:shared/Localization.h` in the Plugins repository | `c1b806adb952c892f8f1e85e7b8d5c738f248bad22c231948a9acf4da0f9b05b` |

**19 locale states per loader, 38 passing process runs:** `de_DE`, `de`, `german`,
`German`, `de-DE`, `de_DE.UTF-8`, `fr_FR`, `ja_JP`, `ko_KR`, `es_ES`, `it_IT`,
`pl_PL`, `xx_XX`, `en_GB`, `en_US`, `English`, empty ID, unavailable manager,
and unavailable current locale.

All 39 English catalog keys retain their packaged English text; a missing key
returns its English source. All 12 direct literal UI Tr keys are covered.
10,000 repeated calls per run preserve returned pointers and do not requery the
game locale. The current loader keeps successful loading quiet with Debug off
and reports its saved summary once when Debug is enabled afterwards.

The input was `D:/DEV/Kenshi/workshop/nexus/Organize-the-Trader-7.zip`, SHA256
`0ad54d6a76b9ca371aeb791e7ec0d39498f9a472bd36fb9261413fa31b260047`.
CRC passes; it contains exactly three MO catalogs and no PO files. Each catalog
matches the installed and Workshop staging copy. English MO: 3,263 bytes, SHA256
`aeb27308e2600b6154ce0f052a62a0c104b0bbc88a6b978f4cf147f75c032cc7`.

## UI recognition and complete offline checks

The trader layout is recognized by internal names (`ArrangeButton`,
`scrollview_backpack_content`, `backpack_content`, `datapanel`) before caption
heuristics. These names exist in both installed vanilla and Dark UI Advanced
layouts. Production recognition succeeds with English, German, empty and
merchant-name captions; no German caption table is needed.

`tests/run_offline_checks.ps1` completed with exit 0 using bundled Python after
installing its missing cached `pefile` dependency. Results include 200 skin and
lifecycle checks, 27,709 layout assertions, 16 rendered-text checks, seven
installed layout fixtures, packing rollback cases, and native binary contracts.
The OTT DLL has 153 validated game imports and the player-wallet virtual slot
`0x1b8`; Steam and GOG layout-slot checks pass.

Installed/build DLL: 669,696 bytes, SHA256
`934b97ac2a6d904bceb5a2f3f33f370afe95b95364bb388e985da34b05cc1cc8`.
Published v7 package/staging DLL: 669,184 bytes, SHA256
`b8d38620aba81d82e32464be5c3f6da46544923ddfc4cdd9e69e2b82100a4346`.
This audit did not rebuild or install either DLL.

## Limits and missing player evidence

Injecting `de_DE` into the locale adapter proves this loader's fallback behavior;
it does not reproduce a German game session or the commenter's setup. No game
was launched, focused, or modified. Offline pointer/polling checks do not measure
live FPS or long-term memory growth. These test-only changes add no runtime work.

Further investigation needs the exact failure (missing controls, ineffective
search/sort, failure to load, hang, or another symptom), game/store and mod
versions, RE_Kenshi/KenshiLib versions, installed path and UI mods, repeatable
steps, and `RE_Kenshi_log.txt` from a session with this plugin's detailed logging
enabled. Until then, the reported failure remains unidentified.

Machine-readable run evidence is written to
`D:/DEV/Kenshi/build/Organize-the-TraderLanguageFallbackTests/report.json`.
The runner also accepts `--archive <path>` for a specific release package.
