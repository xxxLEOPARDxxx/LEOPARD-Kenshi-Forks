# Vital Sense: dark unconscious icon, 2026-11-02

## Diagnosis and intended artwork

The user's screenshot `D:/YandexDisk/Скриншоты/2026-11-02 00-51-10  Безымянный - Paint.NET версия 5.1.12.png`
shows a dark silhouette immediately before green `ZZ` labels. The installed
personal config has an empty unconscious image path, which intentionally selects
the stock atlas. The installed DLL and the pre-fix canonical build both had
SHA256 `3ebc420bd504b8f6782aae4cdb7d7f638d5565adb447cd5f6d4f2d5b597500bb`.

The native `data/gui/images/kenshi_images.xml` identifies this exact crop as
`ResourceImageSet pic_PointerKnockout`, texture `Kenshi_UI.png`, point `45 122`,
size `32 32`. The shape must not be identified from its small appearance alone:
it is the game's Knockout pointer, not a missing picture or an arbitrary new
sleep icon. The installed Dark UI Advanced overrides the atlas and inherits
the native image definitions. Original Vital Sense screenshots retained under
`workshop/images/Vital-Sense/02_1873-1772310280-778646608~1.jpg` and `03_...`/`04_...`
show this same red glyph beside enemy `ZZ` and a dark glyph beside green labels.
The canonical, Emkej and Asur copies all selected the same crop and tinted it.

The red authored RGB was multiplied by the relationship colour, including
squad `#40FF40`. Among 497 nearly opaque crop pixels, this makes every RGB
channel less than 64 for 387 pixels in vanilla and 447 in Dark UI Advanced.
For Dark UI the opaque-pixel mean changes from RGB `(103.4,26.7,26.8)` to
`(26.0,26.7,6.7)`. Alpha remains unchanged. This deterministic pixel calculation
reproduces the dark colour mechanism; it is not a new in-game capture.

The recent preflight commit `594717a` only added resource-existence checks and
diagnostics. It changed neither crop nor tint. Thus this is inherited rendering
behaviour, not evidence of a new texture lookup or alpha regression.

Actual installed `MyGUIEngine_x64.dll` SHA256
`fea6f1f935703812485bd6606ff4d3ceea172ece131ed4c6dcbf7a7e25b083a8`
was inspected read-only. Export thunks resolve `setImageCoord` to RVA `EB530`
and `setImageTile` to `EB250`. The latter only initializes an all-zero image
rectangle; it does not discard the nonzero crop set by the former. No native
function was called for this check.

Asset hashes used by the test:

| Input | SHA256 |
| --- | --- |
| Native `kenshi_images.xml` | `8ff6c67d5a298e7111955b6ed5567fa21d9eaaea4cbb9f50dc10488fca6c6e80` |
| Vanilla `Kenshi_UI.png` | `6d62296cbdc958783b92a3325c4870a951e6a3ea7da85e0bb3c84454fb41c6ff` |
| Dark UI Advanced `Kenshi_UI.png` | `b9f868e7cf5c32928c7fda86c2e944cb88728be5976ebc64bf31bc79718c487f` |

## Narrow change

`ResolveAtlasMarkerIconColour` preserves the Knockout artwork with white vertex
colour for unconscious/recovery-coma atlas fallbacks. This applies both to empty
custom paths and failed custom loads. The neutral invalid/stealth fallback masks
retain relationship tint; successfully loaded custom images already keep their
own colours. Text/body colours, atlas coordinates, settings, resource lookup and
artwork are unchanged. No new images or settings were introduced.

The helper performs only enum comparisons and returns one colour value during
the existing visual-state refresh. It allocates nothing, retains no pointers,
creates no widgets/textures/caches, and adds no scans or per-frame I/O. Existing
visual-state caching, ownership and teardown remain in place. This review is not
an in-game CPU/FPS or memory measurement.

## Verification

Bundled Python commands from the forks repository:

```powershell
python Vital-Sense/tests/test_atlas_icon_colour.py --baseline  # expected failure
python Vital-Sense/tests/test_atlas_icon_colour.py
python Vital-Read/tests/test_texture_preflight.py
D:/DEV/Kenshi/build.ps1 -SrcDir D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/Vital-Sense -Name Vital-Sense -FromVcxproj
python Container-Highlight/tests/check_candidate_imports.py D:/DEV/Kenshi/build/Vital-Sense/Vital-Sense.dll
```

The new fixture compiles the exact production icon application branch and colour,
crop and custom-selection helpers. MyGUI widgets and native texture loading are
explicit seams; actual loader/preflight behavior is tested separately. It checks
all five states/four relations, empty/custom/missing-custom paths, reused widgets,
neutral masks, disabled icons, bounty-only state and absent widget. The native XML
and both real atlas crops are then checked for provenance, RGB and alpha.

- Pinned pre-fix `27895fa`: 123 checks, 16 expected failures, all KO/recovery
  natural-colour cases; actual XML/atlas checks pass.
- Revised branch: 123 checks, 0 failures; both real atlas/XML/alpha checks pass.
- Existing production texture loader/preflight: Vital Sense 352 checks and
  Vital Read 352 checks, all pass.
- Full VC10/SDK7.1 13-translation-unit project build: PASS. Initial invocation
  omitted `-FromVcxproj`, compiled only the entry file and failed linking; the
  correct full project command above then succeeded.
- Candidate imports: 243 named imports, 6 modules, 0 missing. Diff check: PASS.

Candidate `D:/DEV/Kenshi/build/Vital-Sense/Vital-Sense.dll`: 338432 bytes,
SHA256 `cbc895107ec0e4cc5b6a42b53f946ca450d1311cea15c80a2a09caf8ed506ad3`.
Production `src/vs_marker_render.cpp` SHA256:
`212e0debf1cf6c8956f73fd0e68ad4b119ba4b479de51c6b41d7a004af709949`.

No installation, process/UI control, publication, packaging or commit was
performed. The personal `mod-config.json` stays at SHA256
`8d8dbfd79a61ee049f8fb2c81f69237a5a3857c0205f700f3d0b89eb531d4419`.
The existing prepared version 7 archive still needs refresh after independent
review; this correction belongs to that same pending version, not a new bump.
Visual acceptance in the running game remains pending.
