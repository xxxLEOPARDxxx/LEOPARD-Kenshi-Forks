# Rejected loading accelerator experiment

Private offline research only, 2026-11-17. These files are deliberately OUTSIDE
`src`, absent from `coresrc.txt`, not installed, not an MCM setting and not a
release feature. Production remains commit226af2d. Do not transplant this code
into the game based on the synthetic dense-map speedup.

Run from KEO: `leopard/test_pose_cpu.cmd [Od|bench|assets|baseline]`.
`assets` imports five installed mesh files in an isolated Ogre process; no
game/save/scene/NPC simulation is launched. Missing skeleton warnings are
expected for this metadata-only import. The fixture uses native CPU buffers,
not the real D3D driver. It does not measure gameplay loading or FPS.

The initial baseline fails ordinary CHECK. The native CPU oracle has193 checks
after isolation, including scalar SSE operand order, rounding/FTZ/DAZ controls,
matching sticky FP flags, disjoint worker ranges, native buffer final bytes,
native late C++ upload failure, synchronous failure join, cleared borrowed jobs
and explicit shutdown. This is a spike, NOT a complete production validation.
Distinct simultaneous map/target NaN payloads, full SEH lock/unlock cleanup,
pool-creation fault injection, competing stop/entry schedules and actual hook
activation would need further verification before any future production use.

Synthetic tests include both full-buffer copies and dispatch/join overhead.
Dense4096/16384/65536-entry runs initially gave0.80/0.75/0.64 native-time ratios.
Sparse and clustered layouts gave1.09..3.35 (slower). Those figures are local
fixture timings, not a game gain. The proposed density/size guards deliberately
reject sparse or small work.

Static native imports of actual ordinary models establish ZERO proposed-path
coverage for those five imported snapshots: male15504verts/23poses/max2040positionentries; female16614/29/max1511;
ordinaryhive9414/9/max1735; queen25650/0; robot29727/0. Normal maps are empty in
all five imports. Therefore the experimental dense path cannot accelerate
those imported snapshots. Later live postprocessing and modded assets are not
exhaustively covered. Keeping its new threads/settings/hooks would be unjustified.

Across-pose deferral was separately rejected: native GameData lookup can allocate,
and a reentrant callback may observe intermediate geometry. Main-thread scene/GPU ownership,
RNG, save format and spawn order were never changed in an installed DLL.

Full retained-profile findings: `../heavy-loading-review-20261117.md`.
