# CPU loading integration, 2026-11-16

Release remains 2, local candidate only. This pass adds final save-state-list ownership transfer and three constant slot-property queries in equipment assembly. Existing saveStringTransfer governs the list path. New equipmentFieldLookup is default ON/live; detailed log stays last/default OFF. EN/RU/ZH catalogs are maintained. Personal settings, mod list, saves and seven foreign Steel changes are protected by hashes.

Equipment patch authenticates exact executable identity, Boost/native map ABI, hash routine, callers and full five-byte CALL instructions. A normal C++ bridge with unwind metadata is reached through a leaf RX jump thunk. Unsupported/missing/disabled/nonowner paths call native once. The batch installer checks complete instructions again after synchronized thread exclusion, rolls back all partial publication under SEH and restores same-page protections in reverse order. Cross-page edits are refused. Published target data/code is retained conservatively. No runtime scan, worker or map-pointer cache is introduced.

Validation: production lookup 218 checks per optimization mode and four killed mutants; emitted real Win64 equipment callsites 16 checks per mode, including missing insertion and exception unwinding; patch batch 120 fault/boundary checks; thread fixture and fatal-policy child; save-state native/compiled-control suites; complete DEV/PROD build, configuration host suites, catalogs, imports and final PE unwind-table checks. Exact final outputs/hashes are recorded in the private stage receipts.

Reviews: material_copy_design_review and morph_upload_design_review. Changes requested for full-instruction revalidation, capacity retention, synchronized publication, retained resume ownership and fatal-policy observability were incorporated. No gameplay run, new profile or log collection was requested or performed. This does not finish all CPU save decoding/NPC equipment work and does not quantify an in-game speedup.

## Final integration result

Both VC10 /O2 /GL /LTCG DLL variants build all 376 production sources. All 127 host suites pass; EN/RU/ZH each compile 145 keys, with Russian unsupported dash glyphs excluded. Installed-library exports satisfy 183 KenshiLib, 37 Ogre and 65 MyGUI imports. Final PROD and DEV PE exception tables contain correct nonempty Win64 unwind records for both C++ bridge functions. PROD SHA256 `1dca49f11d49effae37e1cbcc8552b999fc3a11f1e405e52674e0f4eeb0e0eca`, 2094080 bytes. DEV SHA256 `7768eff3eaf5127fa6e738164a6b4095d1babab3e72c2ebdcec7db8f6980a8f5`, 2202112 bytes.

PROD, README and three MO catalogs installed locally after a fresh name-only no-game check. Personal INI/mod-list and seven foreign Steel file hashes are unchanged. The previous PROD and personal settings/catalogs are backed up in private stage `cpu-loading-installed-backup`. No publication, version bump or personal published changelog entry.

A final parallel-build benchmark repeat was noisier than the earlier isolated sample, including one guarded path slower than the SDK baseline. Per-query timings are not a guarantee of benefit under contention; no whole-NPC/FPS improvement is claimed. The production behavioral tests and four mutation controls still pass.
