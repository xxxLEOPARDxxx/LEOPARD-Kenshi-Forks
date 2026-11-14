# Forward resource preparation and native GPU adoption (private)

Goal: prepare terrain files for unvisited nearby zones without activating NPCs; precreate the exact native BC1/BC3 terrain texture arrays on a separate COM-owned D3D11 device worker, and adopt them in the engine instead of uploading twice.

Native evidence: ResourceBackgroundQueue load is worker prepare + indivisible main-thread GPU load, unsuitable for a promised frame-time bound. The specialized array creation is main executable A10080, static hidden return pointer/biome/type; native six-layer order base/slope/vertical/grass/dirt/road. Current DDS12mips -> native7actual levels. Device flags0; runtime concurrent creation support still mandatory. Native UpdateSubresource remains fallback on any pixel/layout mismatch.

Constraints: no immediate-context worker calls, only ID3D11Device COM ownership; no game pointers on worker; bounded paths/jobs/cache; live independent toggles, VRAM ceiling1-8GiB and system DXGI budget reserve; RAMexisting1-32GiB; stop/throttle warmup during frame spikes; native fallback for unverified builds/formats/source changes/device failures; copies/manifests built on main thread with bounded work. No cold GPU warmup claim for models, actors or arbitrary formats. Actual performance requires gameplay test.

- [x] TDD fixed forward-neighborhood and cooldown policy, native metadata guarded collector.
- [x] RAM explicit-prefetch API + paced main metadata scheduler before first-time-zone rejection.
- [x] Strict DDS parser and worker GPU cache with WARP readback equivalence tests.
- [x] Main-thread COM device discovery, verified native array TLS, exact descriptor adoption and byte-verified upload suppression.
- [x] Three locale/settings updates, native builds, independent performance/lifetime review.
- [ ] Fresh process check, local test install, gameplay frame-time/queue/counter verification. No publication.
