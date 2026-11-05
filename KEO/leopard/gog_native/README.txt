KEO native GOG compatibility candidate - internal verification, 2026-11-05
Real native profile for supplied RE_Kenshi-compatible GOG1.0.65; not the old startup-only stub.

python leopard/gog_native/run_tests.py --gog-exe <path-to-pinned-GOG-EXE>
Requires VC10+SDK7.1. The supplied EXE is read-only and NEVER executed or redistributed.
Production fixture:393 map/reverse pairs,344 real-byte windows,28 priority contracts,
TownClaim/Stitch/PhysQuery/Dialog native builders and corruption/refusal tests.
Windows host test checks Steam behavior and persistent failed-GOG refusal.

Accepted GOG EXE SHA256:
2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681
Accepted KenshiLib0.5.0 DLL SHA256:
5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b
Other files refuse native initialization.107 SDK registry entries are checked.
Ogre/PhysX/D3D keep separate module-relative gates, never EXE address translation.
GOG PhysX caller-role diagnostics report unknown EXE callers rather than Steam roles.

Host/byte tests do not prove GOG startup/gameplay, FPS, leak freedom or spawn correctness.
Real GOG testers and Steam gameplay regression after installation are still required.
No publication has occurred; this candidate does not create a personal changelog entry.

The first e6082d6 tester candidate omitted au from its profile. That error is
reproduced by --check-old-profile in test_resolve_registry.py. The current test
executes the actual Resolve/Add/global loop and startup comparison, covering all
107 entries rather than only enumerating existing profile entries. The runtime
comparison remains strict and unchanged. Real GOG retest is still required.
