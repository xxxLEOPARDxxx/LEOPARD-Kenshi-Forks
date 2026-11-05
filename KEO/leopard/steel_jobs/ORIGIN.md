# Steel job integration provenance

Original: **KenshiQOL by Steel**. Thanks to Steel for releasing these features and their source.
Original mod: https://steamcommunity.com/sharedfiles/filedetails/?id=3772744830
Original source: `src/KenshiQOL/KenshiQOL.cpp` and `src/Common/TaskTypeFactoryPatch.h` bundled with that mod. A separate source repository has not been verified.
Source-release comment: https://steamcommunity.com/sharedfiles/filedetails/comments/3772744830
Audited main source SHA-256: `955b340eb25b0f66c1e08479ec2e8d4636a39f622962582d9bff8e6bed27363e`.
Both distributions use GPL-3.0; see the accompanying LICENSE.

Adapted concepts: Gather Output291, Move to Position292 with native saved subject payload, and Sell Items293. The last is named Stock Shop Counter because it deposits items and makes no monetary trade.

KEO integration changes: exact Steam/GOG factory contracts with native constructors/common initialization, independently live/default-OFF settings, actor/route/pending limits, stable selected handles captured at click, no native242 migration, no retained AI/widget pointers, epoch invalidation, native coordinate restoration, own-counter-only stocking, bounded whole-stack transfers with exact original-section restoration and conservation checks. No custom RWX mid-function stub. A transfer fault stops stocking until a world change; no automatic replay or provenance laundering.

The physical click guard cannot distinguish another plugin issuing a programmatic ground move during the exact same native right-click event. It prevents ordinary keyboard-only programmatic orders and duplicate interception by KEO's two input paths; no absolute caller provenance claim is made.

Host controls and static review do not establish gameplay performance, complete third-party compatibility or leak freedom. In-game machine collection, physical counter stocking, route save/load and GOG startup remain acceptance checks for this private candidate.
