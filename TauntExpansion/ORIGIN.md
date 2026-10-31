# Taunt Expansion fork provenance

- Original mod: [Taunt Expansion](https://steamcommunity.com/workshop/filedetails/?id=3771499164).
- Original author: tata / Tata-Gari. Thank you for the Taunt mechanic and its public source code.
- Original source: [Tata-Gari/RE_Kenshi_Plugins_Source](https://github.com/Tata-Gari/RE_Kenshi_Plugins_Source).
- Upstream HEAD verified on 2026-10-31 using `git ls-remote ... HEAD`: `b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8`.
- Source subtree: `03_Taunt_Expansion_Source` at that exact commit.
- `upstream/TauntExpansion.upstream.cpp`, README, changelog, limitations and license are unmodified pinned upstream files, fetched as bytes.
- Packaged `TauntExpansion.mod` and `RE_Kenshi.json` are byte-exact real assets from that upstream commit. The installed original's 46-byte MOD differs only in the 32-bit metadata value at offset `0x1C` (16 rather than the upstream package's 1); this fork uses the authentic pinned upstream MOD. That field is not used to infer the fork's release version. The loader JSON matches both copies. No dummy assets or screenshots are generated.
- GPL-3.0: preserve `LICENSE.upstream.md` and `upstream/LICENSE.md`; `LICENSE` contains the complete GPL v3 text.
- Fork release: **1**. No Nexus or Steam publication has been made or authorized before the user's Nexus link.

The original Taunt bonus, original controls and shipped multiplier defaults belong to the original author. Our changes add optional MCM, three interface languages, safe live setting changes, bounded optional diagnostics, nested/concurrent bonus restoration and defensive patch validation. See README for details and verification limits.
