# KEO: normal DDS loader retention coverage fix

This is a concrete retention bug fix, not another timing/profiling build.
No additional gameplay reproduction, ETW session, native hook or worker was added.

## Confirmed cause and resulting behavior

The verified native TextureLoadData.getTexture path creates/retrieves a texture,
loads its image and marks it loaded directly. It bypasses both existing narrow
retention observation hooks. The active texture cache therefore missed resources
loaded through that ordinary engine path. The existing Texture2D wrapper now
offers its successful native result to retention, even when detailed logging is
off. No additional resource load or disk/archive lookup is performed.

The observer borrows existing caller-owned TexturePtr output. New candidates are
weak identities, admitted only by the existing bounded owner-thread heartbeat.
Already-held textures whose loaded size or eligibility changed lose their stale
pin/accounting immediately; unchanged eligible resources remain reference hits.

## Safety and performance review

Capture OFF and non-owner gates precede mutable State/output access. Native
arguments/result/exception behavior and exactly-once forwarding are preserved.
Observation does not create a transient controlling SharedPtr. No generic
Resource::load or per-render hook is reintroduced. Pending and held entry ceilings,
per-heartbeat work, shared geometry/texture budget and shutdown/world/device
clearing remain unchanged. Detailed-log-only reporting adds loaderCompletions;
that counter is not a promise of avoided GPU uploads or elapsed-time savings.

Independent review by completion_attribution_review found no Critical/Important
issues. The minor negative-test count message was corrected to derive its count.

## Offline verification

- Actual installed Ogre: 120 production controls each under VC10 /Od and /O2.
- Actual installed image/export/vtable/load-path contracts: 53 checks.
- 24 independently compiled production cache mutants rejected as expected.
- Native wrapper forwarding: 114 controls each under /Od and /O2, plus DEV and
  PROD production translation-unit compilation.
- Six compiled forwarding mutants rejected, including removed completion capture
  and capture before successful native return.

These checks establish code behavior and native contract coverage. They do not
establish a measured FPS gain or elimination of all NPC/zone stutters. Detailed
logging remains optional; personal configuration is preserved when installing.
No release number, archive, Nexus/Steam publication or personal changelog entry.
