# HandFlow selection, independent lower-body environment and gentle slip refinement

2026-11-09. User requested normal hand choices limited to None/HandFlow, retained diagnostic WiLoR/MediaPipe and existing assets, an independent lower-body Python environment, and a small slip improvement while preserving the currently satisfactory motion. No joint limits, model mixing or new body/hand inference was introduced. Whole test2/test3 clips are the comparison unit.

## Delivered behavior

**Hands → Hand Enhancement** exposes **None (original hands)** and **HandFlow / HaMeR**; HandFlow remains the default. None plus Refine fingers selects the explicitly assigned Original Animation, creates no asset and runs no model. The preview's No hand enhancement button also selects that original directly. This does not attempt to remove fingers from an arbitrary already-enhanced input.

**Hands → Diagnostics → Enable Legacy Hand Diagnostics** reveals the WiLoR/MediaPipe diagnostic selector, paths and comparison controls. It overrides normal hand selection only while enabled. Legacy inference and cache reapplication are gated; existing saved animations remain freely loadable in either mode. Enum values MediaPipe=0, WiLoR=1, HandFlow=2 are retained; None=3 was appended. Existing cached pose conversion and shared Manny retargeting were not changed. HandFlow still includes HaMeR image features and its packaged hand detector; it does not consume WiLoR pose estimates.

The lower-body default is now `Plugins/CubelessBodyMocap/ThirdParty/LowerBody/venv/Scripts/python.exe`. `Tools/prepare_lower_body.py`, run with standalone Windows Python 3.12, creates it from pinned NumPy1.26.4/SciPy1.15.3/psutil7.2.2. It does not install into or modify hand environments. The installed environment occupies approximately216MiB; it and the machine-local receipt are ignored by Git. Package origins and isolated `sys.path` were checked: no hand environment is imported, Torch/MediaPipe are absent, and pip check passes. No model weights are required. The historical `FootMR_*` observation filenames remain compatibility data, not a model dependency.

## Slip change and tradeoff

The existing solver balances supported sole stability against image pose, original pose and temporal continuity. Its soft slip term still permits drift even with consecutive strong contact. Increase the weight gently from `20 * support` to `(20 + 4 * support) * support`; retain the existing contact estimator, support ankle/forefoot solver and temporal foot cleanup. There is no fixed foot anchor, new angle clamp or stronger global smoothing.

A stronger `+8 * support` candidate reduced drift further but changed test3 foot orientation by up to11° relative to the user's preferred baseline. It was not selected. The chosen `+4` candidate keeps pelvis position p95 differences to0.164/0.107cm in test2/test3; foot position p95 differences are0.45/0.68cm and0.55/0.73cm (L/R). Some individual foot orientation differences remain: maximum6.80° test2 left and4.76° test3 right. Median four-frame fast-turn retention is essentially100%; the test3 right fifth percentile is95.2%.

Measured native Manny support sole speed p95, same fixed baseline support mask and geometry:

| Clip | Left previous → new | Right previous → new |
|---|---:|---:|
| test2 | 9.68 → 9.59 cm/s | 8.91 → 8.14 cm/s |
| test3 | 7.49 → 6.59 cm/s | 14.66 → 13.24 cm/s |

The previous result is `TemporalFeet20261109`, not raw body or the earlier SupportReview result. New native outputs reuse the same original body/HandFlow input. These metrics are proxies for sliding, not ground-truth foot contact. test2 left improvement is small. Residual slip is not eliminated.

Other checks and limitations:

- HandFlow and all151 protected local tracks remain exactly equal; upper-body world pose remains compensated. Unflagged airborne world orientation maximum error is0.000146°.
- Seven-bone mean rotation unevenness changes0.8883→0.8901° and0.8262→0.8276° (about0.2% worse). Largest test3 right adjacent-frame foot step changes25.59→26.01°. The previous temporal improvement is substantially retained, not numerically identical.
- test2 hover p95 L/R improves0.875/0.133→0.807/0.108cm. test3 p95 changes0.123/0.318→0.127/0.323cm. Worst test3 penetration L/R changes0.246/0.392→0.258/0.380cm; none exceeds0.5cm.
- Offline projection p95 changes19.23→19.50px and23.22→23.84px. Original depth-order regressions at separations over5cm remain zero. A small loss of image fit is exchanged for reduced support drift.

## Validation actually run

Before changing the slip term, both complete clips were recomputed in the new isolated environment with an empty cache. Every `lower_body_local` value matched the previous environment exactly (`environment_regression.json`). This isolates environment migration from the intentional algorithm change.

Editor Development build succeeded in22.93s; the same project was closed normally after checking clean state and reopened as regular editor PID9636. Native checks covered None without an output/job, normal-mode rejection of a WiLoR cache, diagnostic WiLoR readiness and successful cached application, MediaPipe diagnostic readiness, HandFlow readiness and successful cached application, and loading existing WiLoR/HandFlow assets. Neural hand inference was not repeated; MediaPipe inference/cache application was not rerun. The original/fast body extraction implementation was untouched and was not rerun.

Native lower-body jobs with the new environment completed in5.2/14.2s, with Python worker4.16/12.30s and peak worker RSS267.8/580.0MiB. UI whole-job memory is a different metric (5026/5193MiB). These runs were not a controlled speed comparison. The asynchronous editor job path was retained.

All new animation frames were evaluated natively. Unchanged baseline samples were reused from the preceding verified run. Full playback reached8.1/20.1s with474/1194 samples and five distributed native comparison captures per clip. Source media time did not reverse. Distributed captures were inspected; this is not an independent human assessment of every moving frame. No CUA, parallel editor, packaging test or Git publication was used.

Evidence: `Saved/CubelessBodyMocap/HandFlowOnly20261109/`, including `hand_ui_tests.json`, `environment_regression.json`, `balanced_metrics.json`, `pose_delta_balanced.json`, `whole_clip_balanced_metrics.json`, `native_summary.json`, per-clip `*_feet_metrics.json`, `final_assets.json`, playback captures and `handoff.json`. The runner initially recorded the older SupportReview path in its `previous` field; `baseline_pointer_correction.json` records the correction to TemporalFeet. Actual input and measured comparison arrays already used the intended baseline, so no engine job was repeated for this metadata correction. A documentation-edit attempt failed on Windows default text encoding before writing; it was rerun explicitly as UTF-8.

## Handoff

New test2/test3 asset suffixes are **18EE1E22** and **5B967672**. Normal hand mode is HandFlow and legacy diagnostic mode is off. Before legs compares against the original body with the same HandFlow hands; the preceding temporal result remains saved and can be chosen as Preview Animation for a direct previous/new comparison. Existing results were not overwritten. Final visual acceptance remains with the user.

After successful playback and asset opening, the final handoff check found no matching transient media player. A subsequent verifier incorrectly called `MediaPlayer.GetUrl` on all MediaPlayer objects, including an uninitialized/default object, triggering an engine SharedPointer assertion and terminating PID9636. This was caused by the inspection script, not by a recorded lower-body solve or playback failure. The preceding identity check reported no dirty packages; all outputs were already saved. The crash log is retained as `verifier_media_crash.log`. The unsafe generic media query was removed, and recovery uses the plugin's initialized comparison and screenshot APIs. The same project was reopened as PID31368; final restoration state is recorded separately in `handoff.json`. No inference is needed to restore cached results.
