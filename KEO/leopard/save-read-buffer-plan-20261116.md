# Native save-read batching plan

Goal: reduce refills while the existing native parser reads saved-world/squad
fields. No parser rewrite, file cache, worker, readiness or saved-data change.
Read only existing pinned binaries/reports; no game runs or data collection.

The Steam parser6C0800 opens its private wide-path ifstream via6C8330, returning
to6C092C before any read. On that exact call, on the main thread, for binary
input `.save`, `.platoon`, `.zone` files, request64KiB through native filebuf
pubsetbuf(NULL,n), only for files >=128KiB (existing-handle size query). CRT owns/frees the buffer at normal close. Validate native
image/function/vtable/layout; changed/foreign paths forward once unchanged.
Setting saveReadBuffer is live/defaultON with RU/EN/ZH tooltip; no restart.

- [x] Prove baseline4096-byte buffering and tested64KiB request offline.
- [x] Implement isolated exact-call preparation plus fail-closed installation.
- [x] Exercise forwarding, identity/extension/thread/settings gates, errors,
      small fields, seek/EOF/gcount, nested calls and buffer lifetime.
- [x] Validate pinned native contracts and compile negative controls.
- [x] Review, build DEV/PROD, verify imports/catalogs/host suites; install only
      if game absent, preserving personal settings and foreign7Steel files.
- [x] Scoped commit/push, local status only. Working release2; no publication.

This is read buffering, not proof of faster CPU deserialization or game load.
Equipment remains separate; existing material reuse must not be duplicated.
