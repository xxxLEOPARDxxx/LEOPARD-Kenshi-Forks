# RE_Kenshi entry and MCM lifecycle contract

The installed loader is `D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi.dll`. The matching local loader implementation at `D:/DEV/Kenshi/forks/RE_Kenshi-src/Plugins.cpp`, `LoadPlugin`, lines 32-52 performs `LoadLibraryW`, then **`GetProcAddress(plugin, "?startPlugin@@YAXXZ")`**, then calls that address. It does not fall back to the C name `startPlugin`.

Therefore a DLL can load successfully, expose `MCM_Describe`, and show its translated MCM page while its C-only startup export prevents the gameplay startup from running. That was the original candidate's defect. Its settings callback then entered a critical section that only the skipped startup initialized.

Production now retains the upstream C++ entry definitions: `__declspec(dllexport) void startPlugin()` and `stopPlugin()`. MCM alone retains `extern "C"`. The required PE names are `?startPlugin@@YAXXZ`, `?stopPlugin@@YAXXZ`, and `MCM_Describe`.

`BonusTransaction` initializes its critical section in its global constructor. Windows completes DLL CRT initialization before `LoadLibrary` returns and a caller can invoke its exports. Later `Initialise` binds the bonus address under that live lock and does not initialize the critical section again. The lock remains available when startup is absent or exits early.

Regression evidence:

- With the old production DLL, the corrected manual PE audit fails specifically on missing `?startPlugin@@YAXXZ`.
- With the old source, the lifecycle fixture fails both the early initialized-lock check and exact decorated `GetProcAddress` lookup.
- With the fix, the fixture invokes extracted real production startup through that exact export; then extracted real Describe and its **actual LoadConfig callback**, including real INI readers, execute successfully.
- Separate early-Describe/apply, duplicate-start, target-address-failure, patch-failure and hook-failure scenarios pass.
- Native hook/game calls are test boundaries; this is not a claim that the running game's newly built DLL has been installed or tested.

`release/binary_audit.json` records the installed loader hash, exact plugin exports, runtime-image signature/import evidence, and `lifecycle_proof` matching the current production source and lock-header SHA256. `release/lifecycle_proof.json` carries the same fixture evidence separately.
