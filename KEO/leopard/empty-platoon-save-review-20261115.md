# Empty platoon save files - private review, 2026-11-15

**Decision: preserve the files. A 100-byte file with `char count = 0` is not a
safe deletion criterion and does not establish a dead or unused squad.** No save,
production source, settings or runtime state was modified. This review read
existing save bytes, SDK declarations and the on-disk Steam executable only;
there was no game/process access, capture, profiler or runtime collector.

## Actual saved counterexample

`C:/Users/User/AppData/Local/kenshi/save/LEO 3/platoon/Банда костей_10.platoon`
is exactly 100 bytes, SHA256
`e458f65a02b60024c175f913fbac0bd8c27108e71643470a464600e0dde586eb`.
It contains one type-30 container record, one integer `char count = 0`, and no
actor-instance records. Its bytes contain no `dead` flag.

The same save's `quick.save` explicitly references it as
`platoon/Банда костей_10.platoon` in `content file` at byte offset **213063**.
The owning type-34 squad record begins at offset **211626**, instance ID 1212792.
Its typed Boolean/integer fields are:

| Field | Saved value |
| --- | --- |
| `dead` | false |
| `persistent` | true |
| `never been activated` | false |
| `intact` | true |
| `canref` | true |
| `char count` | 0 |

The template is `blank squad`. `quick.save` SHA256:
`6192260c99207ffb7cf21f33f6dc30bbd6673923e00a5da9a82368d957d7bd02`.
This single concrete counterexample is enough to reject the proposed
empty-file-equals-dead cleanup rule. It is not a census of all empty squads;
`canref` alone does not prove that a refresh is currently scheduled.

## Why native saving produces these files

All addresses below are actual Steam RVAs resolved through installed KenshiLib
exports and the Steam 7630-entry RVA table. EXE SHA256:
`504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`.
The existing offline inspector asserts this identity. SDK address comments were
not used as Steam addresses.

- `ActivePlatoon::serialiseCharacterData` **4FE980..4FEB07** clears its container,
  creates a type-30 record at **4FEA19**, serialises its current objects at
  **4FEA79**, then writes the actual `things.count` as `char count` at **4FEAB8**.
  Zero objects therefore naturally produce a valid minimal container.
- `serialiseEverythingToDisk` **4FEB10** invokes that routine and `saveToDisk`.
  The latter **36BB50..36BD09** writes `platoon/<ID>.platoon` through
  SaveFileSystem and GameDataContainer. Nonplayer zero-object saves can be skipped
  here; an already existing empty file is still not proof of an orphan.
- `Platoon::serialiseEverything` **372DB0..373DB8** saves `dead`, `persistent`,
  `never been activated`, `char count` and the separate `content file` reference.
  `loadStateData` **7EC550..7ED589** restores these separate fields. Its `dead`
  read writes `Platoon+1F0` at **7ECEE2**; the metadata character count writes
  `Platoon+A0` at **7ED35D**. The two states are not interchangeable.
- `emptySquadCheck` **799F70** calls `declareDead` when the active object count
  is zero. However, `declareDead` **7964F0..79668A** can schedule **CM_REFRESH**
  at **79655E** for a regenerating home squad instead of setting `isDead`.
  Its actual dead branch sets `+1F0=1` and clears persistence at **796661**.
  `undeclareDead` **790B90..790B9E** clears dead and sets persistent. Native
  death/refresh/persistence rules carry more meaning than file size.

## Loading, absence and polling

`ActivePlatoon::loadCharacters` **794F90..795301** resolves the platoon filename
through `SaveFileSystem::readFile` at **79505E**, then invokes `loadToReality` at
**79514D**. `readFile` **470C20..470CF4** consults its in-memory file index;
an absent key returns an empty path. The fallback string at RVA 1F78098 was
checked to start with zero bytes. This is not loading the valid empty container.

`ActivePlatoon::loadFromDisk` **36B520..36B52B** delegates to
`DataObjectContainer::loadFromDisk` **36B490..36B515**. The latter bypasses the
read when `isStored` is already true and force is false, and propagates a failed
`loadGameDataReturn`. `RootObjectContainer::loadToReality` **36D6A0..36DC62**
returns on that failure at **36D71B**, before iterating saved actor instances.
No equivalent empty-container fallback or guaranteed safe regeneration was
established on this path. Later caller handling may differ; neither automatic
respawn nor harmless deletion is claimed.

There is no evidence that every saved `.platoon` file is polled every frame.
Native faction updates traverse **in-memory active/unloaded platoon objects**.
`updateUnloadedPlatoons` **6B9620..6B96DC** handles at most one indexed unloaded
entry per call. A saved-file count is not an active-object or update count.
Deleting the file alone also leaves the demonstrated squad metadata/reference;
it is not a proved way to remove that squad's simulation work.

The empty content may serve as a persistent record of zero actors, but its role
as a specific death tombstone cannot be inferred from the content alone. Safe
cleanup would require a supported whole-save transformation that proves every
affected reference, lifecycle/refresh condition and absence behavior. That proof
does not exist here. No automatic deletion or save rewrite is authorized or
recommended by this review.

Evidence: bounded `appearance-native-<RVA>-20261115.txt` excerpts for the named
functions above, SDK `Platoon.h`, `RootObject.h`, `SaveFileSystem.h`, and the
existing save bytes. The complete chained `updateActivePlatoons` range
6BA810..6BA995 remains retained; short `.pdata` fragments were not treated as
whole functions. No gameplay FPS, IO-frequency or cleanup benefit was measured.
