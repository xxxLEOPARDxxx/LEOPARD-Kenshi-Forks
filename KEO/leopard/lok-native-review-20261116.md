# LoK native read-only review - 2026-11-16

Scope: existing files and retained 2026-11-15 trace summary only. No game launch, recording, profile, runtime collection, mod mutation, source mutation, or installation performed.

## Confirmed files

Installed `mods/Legendary of Kenshi/RE_Kenshi.json` lists `lok_npcs.dll`. LoK therefore includes an executable native plugin in addition to FCS records/assets.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| Current installed lok_npcs.dll | 318976 | 5d76a0fea6c83b6a4b7711d86fb1e70db221c9343d7f63b5e83949bdbe9b25bd |
| Backup before 2026-11-13 update | 250368 | 397ffa4bfa91dce81ae91416eed3a3d5e93a3e18e7ebbe483652b1f11aaf7976 |
| Backup before 2026-11-05 update | 247296 | 1c363e9299b20dae5034af5052166c7364e834d3563023f9e90850e36f06a6e1 |

Backup roots: `D:/DEV/Kenshi/backup/LoK-before-update-20261113-163311/mods/Legendary of Kenshi` and `D:/DEV/Kenshi/backup/LoK before update 2026-11-05`. `ours/translations/LegendaryOfKenshiRU/verification-20261113.json` lines 163-165 record the same before/after DLL hashes. Thus the last installation was not merely a data update. The receipt separately records 5 added/38 changed FCS records; it does not enumerate everything the executable implements.

## Actual retained stack and native work

`etw_tools/captures/game-20261115-040423-dd6772a7/stack-summary.json` lines 569 onward has one retained 14-sample stack containing:

`kernelbase -> lok_npcs +107B -> lok_npcs +1B6D -> lok_npcs +1FA3 -> native sub_6BA82B -> Faction::update -> faction-manager/main-loop`.

Mapping those offsets against the current installed image (base 0x180000000):

- RVA 1F90..1FB3 is a wrapper: calls 1AD0 at 1F9E, then tailcalls its saved original through global 4E150 at 1FAC. The 1FA3 return address matches exactly.
- RVA 1AD0..1F81 first checks a static successful-install flag at 4E168. For valid/non-null/non-wrapping input it calls helper 1000 on `self+0x78`, size 0x79, at 1B68. No timer or per-object memoization precedes that call. The return address is exactly 1B6D.
- RVA 1000..10FC validates the requested memory range via `VirtualQuery` (IAT 46530), with call at 1078 and return at 107B, looping over covered regions. It then obtains the current-process handle and calls `ReadProcessMemory` at 10D3. This is OS memory validation/copy, not disk I/O.
- Only after that first safe read can the callback reject based on the loaded/active layout fields. Further reads inspect parent, GameData, identifiers and ownership. One branch loops over 2..512 members, with safe reads for pointers and character identifiers. This is bounded but can contain many OS calls if that specialized branch is reached.
- IDs in the examined callback include 5133117, 5133118, 5133138 from Legendary of Kenshi.mod and 40631-Newwworld.mod. The expensive member branch is specialized; do not describe it as a full-world scan for every ordinary squad.
- Native metadata table at 37B00 names `leader_update` as `?_NV_update@ActivePlatoon@@QEAA_NXZ`; the installer compares the `leader_update` tag at 8049/8050, resolves/validates it, and installs wrapper 1F90 at 86FD..870E. This is an ActivePlatoon update hook that runs below Faction::update, rather than a direct hook of Faction::update itself.

Compact relevant instructions:

```
1F9E call 1AD0
1FA3 mov rcx,rsi
1FAC jmp [4E150]        ; saved native original
1AEB movzx eax,byte[4E168]
1AF2 cmp al,1
1B54 lea rax,[rcx+78]
1B5C mov r8d,79
1B68 call 1000
1072 mov rcx,r15
1078 call r13           ; loaded from VirtualQuery IAT at 104F
107B cmp rax,30
10B8 call GetCurrentProcess
10D3 call ReadProcessMemory
```

## Critical before/after limit

The pre-13-Nov DLL already contains the same examined algorithm. Offline Capstone comparison of instruction streams, normalizing relocated RIP references, mapped callees and local branch destinations, found equal instruction sequences:

| Current RVA | Pre-13-Nov RVA | Instructions |
| --- | --- | ---: |
| 1000 | 1000 | 80 |
| 1100 | 1100 | 54 |
| 1980 | 1930 | 65 |
| 1A70 | 1A20 | 28 |
| 1AD0 | 1A80 | 257 |
| 1F90 | 1F40 | 11 |
| B6E0 | 6DE0 | 62 |

The relevant record identifiers are also identical. Normalization intentionally abstracts global addresses and is not proof of the same entire DLL, startup environment or runtime data. It proves the inspected branch bodies are structurally unchanged, and directly contradicts attributing this algorithm's introduction to the latest update. The same helper 1000 also exists in the pre-05-Nov DLL.

## User's paired comparison, 2026-11-16

After this review, the owner reports importing the same base save with LoK and
without LoK: FPS is substantially higher without it. Treat this as user-provided
evidence that enabling LoK contributes substantial load in that scenario. It
was not measured by this review and no numeric result was supplied. The mod-level
comparison strengthens the LoK attribution; it does not isolate the DLL callback
from FCS records/assets or establish which changed native feature causes it.
No additional run or collection was requested.

## What this establishes and does not establish

Established: LoK executes recurring native code in normal squad updates; the last update really changed the DLL; the retained trace observed its memory-validation path under faction updates. A small FCS file can also change population, squad contents, AI workload or which DLL branches are reached; download size does not measure recurring simulation cost.

Not established by the native review: the size of this specific callback cost, a 13-Nov regression attributable to this particular callback, a new DLL hot path, a memory leak, repeated INI reads, or repeated disk logging. No such claim follows from import names alone. The inspected callback/helper chain contains no INI or disk-log calls. Dynamic native calls at its successful specialized tail were not timed. The 14 retained samples are not a controlled before/after comparison, nor a whole-session percentage.

Author-facing concrete question: can the repeated VirtualQuery/ReadProcessMemory validation before the specialized squad-ID filter be avoided or safely limited for ordinary squads? Preserve object lifetime and world/save transitions; blindly caching raw game pointers or deleting checks would be unsafe. Compare the changed native features separately from FCS changes before claiming the new version is the cause. This report does not request or authorize patching/turning off the third-party plugin.
