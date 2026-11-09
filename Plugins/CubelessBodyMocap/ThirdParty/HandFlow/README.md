# Optional HandFlow / HaMeR runtime

This is a separate, local research runtime. It does not replace WiLoR, MediaPipe,
the body extractor, original/fast body options, or the shared Manny retarget pass.
No Blender add-on or RGB MoCap application is installed in the plugin.

## Select and compare

In **Hands → Hand Enhancement**, choose **None (original hands)** or
**HandFlow / HaMeR**. HandFlow is the default. None selects the saved original
animation without inference or asset creation. Legacy estimators are retained
only under **Hands → Diagnostics → Enable Legacy Hand Diagnostics**:

- **WiLoR + MediaPipe (diagnostic)**: WiLoR predicts physical finger rotations.
  Independent MediaPipe observations assist hand identity and crop selection;
  MediaPipe rotations are not mixed into WiLoR rotations. Existing spike/gap
  handling remains. If the optional helper is unavailable, the existing WiLoR
  fallback is explicitly reported.
- **HandFlow / HaMeR**: HaMeR extracts image features and 2D hand
  conditions; HandFlow predicts a sequence using 16-frame overlapping windows,
  overlap 8, three flow integration steps by default, FP32, fixed seed 0.
  WiLoR's YOLO detector is used only to locate hands, not to supply hand poses.
  Missing detections are flagged temporal predictions, not recovered detections.
- **MediaPipe only (diagnostic)** remains available with the diagnostic toggle.

Select the saved **Original Animation**, matching video/start time, target mesh,
and retargeter; click **Refine fingers**. New results are saved separately under
the selected output folder's `Hands` directory. Comparison buttons select **No
hand enhancement** or **HandFlow / HaMeR** for the same original asset without
changing playback time. WiLoR and MediaPipe comparison buttons appear only in
diagnostic mode. Existing saved animations remain loadable in either mode;
reapplying their legacy caches requires diagnostic mode.
Changing the original disables stale comparison slots. After reopening the
editor, **Apply cached fingers** can restore each run's `hands.json` without
running inference. It creates another saved result rather than overwriting one.

## Files and environment

For a new computer, follow the numbered installation procedure in the
[plugin README](../../README.md). It includes a standalone HandFlow environment,
Chumpy, the required CUDA check, and the missing Seaborn dependency that the old
base lock does not include. Do not copy another workstation's venv or `.pth`.

`manifest.json` pins the source revision, every downloaded source SHA256, and
seven model/config hashes. First place the user-licensed `MANO_RIGHT.pkl` in
`ThirdParty/HandFlow/models/`. Then run from the project root:

```powershell
python Plugins/CubelessBodyMocap/Tools/prepare_handflow.py --weights
```

MANO is not downloaded by this command. Missing MANO makes setup fail even if
other downloads finish. Required files are listed in the manifest and the
editor's readiness message. Missing HandFlow files do not disable body extraction
or the None hand option. The downloader refuses mismatched existing files and
does not install Python packages.

The existing development workstation uses `venv/Scripts/python.exe`, Python 3.12.10 and Torch
2.8.0+cu128. `venv/Lib/site-packages/cbm_shared_torch.pth` reads the existing
WiLoR environment's packages; no packages in that environment are modified.
HandFlow-specific overrides are installed in its own venv and take precedence.
The base WiLoR environment must remain installed **for that shared installation**.
This avoids duplicating Torch while keeping HandFlow package writes separate.
It is not a requirement for new standalone installs. The main README installs
the common package lock, matching Torch wheels, overlay and Chumpy directly into
HandFlow's venv; it does not require WiLoR pose weights or the WiLoR repository.
This independent fresh-PC procedure has been source-reviewed but has not yet
been executed end-to-end on a clean machine. The current adapter requires CUDA;
it does not provide a CPU fallback.

The worker sets job-local YOLO settings and Win32 OpenGL before importing model
code. Rendering is disabled during inference. Package versions, model hashes,
input hashes, code identity, timing, process-tree RAM and CUDA allocation peaks
are recorded per run. UI readiness checks file presence; actual runtime imports
and hashes are validated by the worker, so "files ready" is not an import test.

## Rotation contract and preservation

Both model adapters export flat-hand physical local quaternions in side order
left/right and MANO joint order index, middle, pinky, ring, thumb, three joints
each. HaMeR produces 21-point conditions in its OpenPose ordering; that ordering
is not used to reorder the 15 MANO rotations. HandFlow statistics are
denormalized by its own inference function, whose MANO evaluation uses
`flat_hand_mean=True`. Therefore no MANO/SMPL-X mean pose is added or subtracted
in this adapter and no PCA projection is used.

Canonical right-hand output is reflected once for the left hand by `M R M`,
`M=diag(-1,1,1)`. The shared converter then maps SMPL to UE with
`C(x,y,z)=(x,z,y)`, quaternion vector `-C*v`, scalar unchanged. These operations
do not include the MANO wrist/global rotation or translation. The existing
native pass alone performs palm-frame calibration, terminal-axis calibration,
and rebasing onto the actual Manny wrist/metacarpal parents. The reference app's
Blender/Mixamo finger retargeting is not copied or applied a second time.

Only the existing 30 finger and eight available metacarpal rotation tracks are
edited in a duplicate animation. Every other track, including wrists, root,
pelvis and IK tracks, is compared exactly before save. No joint limits, frame
shifts or automatic WiLoR/HandFlow switching are introduced. A wholly unseen
hand retains the source hand; other missing HandFlow observations remain
explicitly flagged temporal predictions. Default Hand Strength is unchanged.

## Cache and diagnostics

Hand caches are under `Saved/CubelessBodyMocap/HandCache/<backend>/<signature>`,
separate from body caches. SHA256 identities include video and decoded frames,
keypoints, interval/FPS, source reference geometry, camera/settings, models,
worker/model source and Python/package versions. Complete cache files are
checksummed. Only a completed generation is published. A cache hit rebinds the
source asset identity; it never overwrites a saved animation. Decode and hand
assignment currently still run before the prediction-cache lookup.

`request.json`, `hand_identity.json`, `metrics.json`, `hands.json`,
`hand_diagnostics.npz` and `model_evidence.npz` distinguish raw model evidence
from native `retarget_stages.json` and `preservation.json`. Finger motion is
offline: future context can affect earlier frames, but output timestamps are
unchanged. This is not zero-latency streaming. The network uses 16-frame windows
with eight-frame overlap. Global shape and bidirectional residual refinement
can use the whole clip; there is no strict 15-frame latency bound. Visible lag
must be evaluated from the clip. Final visual quality is the user's evaluation.

### Image-supported gestures (2026-11-08 follow-up)

The adapter now uses eight-frame overlap instead of two to reduce abrupt
changes of temporal context at window boundaries. ODE steps remain three by
default: increasing them alone did not fix test2's boundary spike.

`Tools/handflow_image_guidance.py` is a Cubeless implementation of bounded-memory
MANO reprojection fitting. It refines only the 45 finger axis-angle components
around HandFlow's prediction using HaMeR crop-space 2D observations, a residual
pose prior, continuity of that residual and soft final rotation acceleration
(100 Adam steps). Constant angular velocity is not penalized by the acceleration
term; there is no angle or speed ceiling. Palm alignment and HaMeR camera
recovery are nuisance parameters for projection and are never exported as
body/wrist motion. No HaMeR or WiLoR finger rotations are substituted/blended;
no angle limits or frame-specific rules are used. Missing observations have
zero reprojection weight, with correction inferred from temporal context.

The follow-up overlap fix uses existing ViTPose wrists to detect ambiguous
ownership of HaMeR's projected root when both hands are close. It softly reduces
only the image-guidance weight; it does not swap sides, add detections, replace
the temporal model output or read WiLoR/MediaPipe poses. Detector scores and
missing flags remain separate from `guidance_scores` and
`ownership_reliability` in the evidence archive. The SO(3) acceleration prior
also addresses sudden final rotations that residual-only continuity missed.
Occluded fingers still have no ground-truth observation and may be incorrect.

`model_evidence.npz` distinguishes pre-guidance `temporal_poses`, final `poses`,
HaMeR observations and compressed tokens. `metrics.json.image_guidance` records
the objective, iterations and timing. Reprojection error is against a model
observation, not ground truth. Test2's 7.7s gesture improved, but some frame
changes increased; this is not a general accuracy guarantee. Prior assets stay
unchanged, and changed Python source invalidates the prediction cache.

## Provenance and terms

Reference: `mayalhc/Chamiseul_RGBMocap` revision
`4e09cfc2b322acd918e7833d50a0a00ed04ba697`.

- `mocap/handflow_engine.py`: selectively adapted into our worker. Changed input
  frames/keypoints/camera sources, job paths/progress, resource batches, diagnostic
  export and shared quaternion output. MIT notice retained here.
- `mocap/handflow.py`: reviewed for cache and physical pose semantics; app result
  merging and SMPL-X residual-mean handling were not copied into Unreal.
- `mocap/hand_pose.py`, `mocap/retarget/fingers.py`: reviewed only. Their
  Blender/Mixamo-specific corrections are not applied by our Manny path.
- Downloaded HandFlow/HaMeR runtime files remain byte-identical to the pinned
  fork. `license_inventory.json` records every file and SHA256.

**Do not describe the combined runtime as uniformly MIT.** HandFlow's
`dualstream_transformer.py` and flow-matching helper files carry **CC BY-NC 4.0**
notices; the transformer also acknowledges Apache-2.0 Flux origins. HaMeR's ViT
retains OpenMMLab attribution. Repository MIT notices do not replace these
file-level conditions. The original headers and separate license texts are kept.

The HandFlow weight provider model card declares MIT. HaMeR's model Space does
not provide a separate weight license field in the inspected card; its source
MIT license alone is not asserted to settle all weight/data rights. MANO retains
its separately accepted model terms. The detector/WiLoR and Ultralytics retain
their own CC BY-NC-ND / AGPL-or-commercial conditions. `models/`, runtime sources
and virtual environments are local, Git-excluded dependencies; they are not
redistributed as part of plugin source. This integration is not a declaration of
commercial redistribution clearance.
