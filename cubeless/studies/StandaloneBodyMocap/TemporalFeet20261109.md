# Whole-clip lower-body temporal review — 2026-11-09

The user explicitly permitted correction of identified airborne foot rotation jumps, while retaining HandFlow hands and prohibiting joint limits. The acceptance focus is average whole-clip quality, not fitting a previously supplied still. Existing body/hand inference and saved results were reused, with separate output assets. FootMR was not restored.

## Findings and changes

- Large foot steps included unevenly timed, monotonic turns, not only isolated reversals. `temporal_feet.py` detects geodesic midpoint residuals and applies a centred five-frame quadratic SO(3) correction only near detected outliers. Constant-speed rotation is preserved; the first/last two frames are unchanged. This is a heuristic temporal detector, not ground-truth classification of bad motion.
- Correction-only regularization could retain jitter already present in the original pose. The pelvis/legs solver now also penalizes second differences of actual output positions and strengthens output rotation curvature. The sparse banded solve remains intact; no joint-angle limits were added.
- Native application validates the cleanup mode and per-foot frame mask. Unflagged airborne world rotations, upper-body world motion, and 151 protected local tracks remain guarded. An intentionally corrupted unflagged airborne result was rejected without creating an asset or changing the selected result.
- The cache includes the native context signature and temporal cleanup code hash. Existing caches cannot silently cross context/mode changes.
- The synchronized preview previously posed the mannequin at quantized video timestamps. Its playback clock now fills the interval between timestamps, bounded to one video frame ahead. Paused seeking and live scrubbing retain exact requested positions. End-of-clip stopping uses actual media time.

## Whole-clip native comparison

Baseline is the previous `SupportReview20261108` support result, not raw body inference. Both full test clips use the same original body, HandFlow hands and Manny skeleton. These are geometric/temporal proxies, not human-rated quality percentages or video ground truth.

| Metric | test2 previous → new | test3 previous → new |
|---|---:|---:|
| Mean rotation unevenness across seven pelvis/leg bones | 1.2201 → 0.8883° (−27.2%) | 1.1276 → 0.8262° (−26.7%) |
| Largest adjacent-frame foot rotation, L / R | 30.86 / 54.57 → 29.12 / 33.77° | 31.92 / 34.98 → 24.04 / 25.59° |
| Foot position second difference p95, L / R | 7.31 / 7.04 → 3.46 / 2.57 cm | 6.56 / 6.80 → 3.30 / 2.84 cm |
| Support sole positive-height mean, L / R | .213 / .040 → .211 / .035 cm | .037 / .090 → .031 / .089 cm |
| Support sole speed p95, L / R | 9.59 / 8.93 → 9.68 / 8.91 cm/s | 7.31 / 14.96 → 7.49 / 14.66 cm/s |

Rotation unevenness is the geodesic midpoint residual of three successive samples. Lower is smoother, not necessarily more accurate. Contact metrics use a fixed independent baseline support mask and target shoe geometry. They do not prove perfect video contact. Grounding improved only slightly: test3 right hover p95 increased .301→.318 cm, and its worst penetration increased .366→.392 cm. No measured penetration exceeded .5 cm. Left support slip p95 worsened slightly in both clips. Depth-order regressions for original separations over 5 cm remained zero. Projection p95 changed 18.66→19.23 px in test2 and stayed about 23.22 px in test3.

For four-frame windows whose previous net foot rotation exceeded 30°, median retained rotation was 99.3%/99.4% in test2 and 99.5%/99.9% in test3 (L/R). The low fifth percentile was approximately 89.8%/80.7% and 91.4%/91.4%; some fast segments are attenuated. Centred processing introduces no designed time shift, but ground-truth perceptual latency was not independently measured.

Native saved-asset checks confirmed exactly unchanged 151 protected tracks, including HandFlow fingers, and compensated upper-body world pose. Maximum unflagged airborne orientation error was 0.000121°. Cleanup affected L/R 63/56 of 244 frames in test2 and 119/163 of 604 frames in test3; airborne subsets were 25/17 and 47/50.

## Execution and validation

- Native cold jobs completed in 8.5 / 22.5 s. Python worker times were 7.21 / 20.32 s and worker peak RSS 273.5 / 575.0 MiB. UI job peak RAM reported approximately 5075 / 5165 MiB and is not the worker-only metric. Offline numeric runs were faster under different load; these are not controlled speed comparisons against the previous version.
- Editor Development builds succeeded: 35.26 s, 8.36 s and final incremental 7.54 s. No parallel/headless editor was used.
- `Cubeless.BodyMocap.Preview.ContinuousPlayback` and `DelayedSeekDuringScrub` passed. Synthetic constant 180°/s motion, isolated error correction, opposite-foot preservation and endpoint preservation passed.
- Full playback after the clock change collected 446 / 1154 samples and five evenly spaced captures per clip. The mannequin advanced while the video timestamp was unchanged on 204 / 549 sampled transitions. Pose time never moved backward; maximum lead was one 30 fps source frame within floating-point tolerance. Clips reached their terminal video timestamps (8.0667 / 20.1 s). Screenshot capture deliberately evaluates exact media time, so screenshots alone do not measure interpolation.
- Native captures were inspected at distributed times. Overall motion phase is recognizable, but pose/proportion and contact discrepancies remain. Screenshot capture perturbs frame timing; this is not a controlled input-latency benchmark or independent human full-motion review.
- A first read-only Python probe used an unavailable API and failed without changing assets; the supported component position API was then used. Final native result application succeeded and left no dirty packages.

## Reproduce and view

Evidence root: `Saved/CubelessBodyMocap/TemporalFeet20261109/`.
`final_assets.json` records original, prior and new animation paths. `native_summary.json`, per-clip `*_feet_metrics.json`, `whole_clip_metrics.json`, `airborne_guard.json`, `temporal_filter_test.json`, `clock_tests.log`, and `playback_interpolated_summary.json` contain measurements. Final handoff uses a separately applied cached test3 result recorded in `handoff.json`; no solver or neural inference was rerun for this UI restoration.

In Cubeless Body Mocap → Lower Body, enable **Refine Pelvis and Legs from Video**, **Refine Supported Ankle and Forefoot**, then **Stabilize Foot Rotation Outliers**. The new option defaults off, preserving existing settings behavior. It is enabled in the current review session. **Before legs** selects the original body with the same HandFlow result; **Pelvis / legs** selects the new support/temporal result. For the prior support result, use its `previous` asset path from `final_assets.json` as Preview Animation and load comparison. Match the test2/test3 source video when switching assets.

Final editor PID 17904 is the same project, normal visible editor. No CUA, Git publication, packaging test or other-video generalization was performed. Final visual acceptance remains with the user.
