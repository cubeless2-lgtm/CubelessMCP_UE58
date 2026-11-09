# Cubeless finger refinement dependencies

The editor launches an isolated Python worker after body extraction. Results change the 30 finger rotation tracks and any of the eight Manny metacarpal tracks present in a duplicate animation. Metacarpals must be corrected because fuzzy mapping can apply finger MCP motion to both the palm and the finger. The original asset, wrist, arm, root, pelvis, IK, other tracks, curves and timing are preserved. No Python or model dependency is serialized into the output AnimSequence.

Normal use: click **Start Mocap** once. **Solve > Enhance Hands (HandFlow)** is checked by default: body extraction saves an original animation, then HandFlow runs automatically and saves a separate finger-enhanced animation. Uncheck for body only. Missing model files are reported before starting; unchecking keeps body extraction available. Decoded frames are shared, but full-frame hand assignment keypoints remain distinct from body crop keypoints. Their content-checked cache is separate from body checkpoints. Existing HandFlow quality settings and retargeting are unchanged.

**Original Animation**, `refine_hands`, and `apply_hand_result` remain available for diagnostics and cached reapplication. Diagnostic FPS/key count come from the selected saved original. WiLoR/MediaPipe controls and diagnostic action buttons appear only with **Hands > Diagnostics > Enable Legacy Hand Diagnostics** enabled. This does not override the HandFlow checkbox for normal Start. Comparison buttons preserve video time/camera. Starting a new body extraction clears the comparison slots, not saved assets. A hand failure/cancellation preserves the body animation already saved.

To change blend strength or restore the two-way comparison after an editor restart, select the original animation, set **Hand Result** to the run's `hands.json`, adjust **Hand Strength**, and use **Apply cached fingers**. This creates a new output without rerunning video/model inference. WiLoR/MediaPipe caches require the diagnostic toggle; existing saved animation assets can always be opened normally. Optional pelvis/legs post-refinement and its LowerBody environment have been removed; existing animation assets remain readable.

## MediaPipe (diagnostic only)

Installed locally under `MediaPipe/venv/`, Python 3.12, mediapipe 0.10.21, numpy 1.26.4, scipy 1.15.3. The SDK dependency lock is `MediaPipe/requirements-hands.txt`.

Model: [Google Hand Landmarker float16 v1](https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task), SHA256 `fbc2a30080c3c557093b5ddfc334698132eb341044ccee322ccf8bcf3607cde1`. Place in `MediaPipe/hand_landmarker.task`. Model/license reference: [official guide](https://developers.google.com/edge/mediapipe/solutions/vision/hand_landmarker), [model card](https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Hand%20Tracking%20%28Lite_Full%29%20with%20Fairness%20Oct%202021.pdf).

No MANO account/model is used by this backend. It estimates 21 hand landmarks in original-resolution hand crops. The v3 solver converts palm-relative segment directions into local rotations using parent-frame minimum swings. Axial twist is not measured. The previous v2 curl-sign assumptions and joint/speed clamps were removed after they failed the test2 landmark-direction comparison. Isolated per-joint spike rejection, temporal smoothing, gap interpolation and original-pose fallback remain; timing is not resampled. No new anatomical joint limits have been added. Occlusion and small/blurred hands remain limitations; no claim of ground-truth accuracy or parity with WiLoR.

The native hand pass overrides a job-local retarget profile: metacarpal source mappings are cleared, three-joint finger chains use one-to-one rotation transfer, and hand-relative global rotations are rebased onto the actual output parents. The shared Retargeter asset is not modified by this pass. On Manny, all 123 tracks outside the 38 edited hand tracks are checked against the original at every frame before saving.

The hand pass also calibrates source/target palm frames from wrist and index/middle/pinky MCP positions. World-space FK alignment alone leaves the palms rolled differently. Terminal phalanges have no child for FK auto-alignment, so their incoming bind-translation axis (approximately signed local X on Manny) is aligned to the source terminal-direction convention. This is reference-frame calibration, not an anatomical joint constraint. The preservation report identifies it as `palm_frame_terminal_axis_v1`. MediaPipe's v3 solver and existing temporal filtering are unchanged by this calibration.

Use **Left hand** / **Right hand** to inspect enlarged fingers during playback; **Full body** restores framing. Camera yaw and zoom remain adjustable. The native original/enhanced blend follows a temporally continuous quaternion branch to avoid flips when the two poses differ by about 180 degrees.

## WiLoR (diagnostic only)

- Unmodified source: https://github.com/rolpotamias/WiLoR at `fcb911312a38fa8badd30d9656a167485d61b8f9`, installed in `WiLoR/repo/`.
- Python 3.12 in `WiLoR/venv/`, torch 2.8.0+cu128, torchvision 0.23.0+cu128. `WiLoR/requirements-hands.txt` locks the other installed packages. CUDA 12.8 wheels support this PC's RTX 5060.
- Chumpy source `580566eafc9ac68b2614b64d6f7aaa84eebb70da` from https://github.com/mattloper/chumpy, pure Python package copied into this venv only. Its MIT license is retained in the local source checkout. The worker provides legacy numpy/inspect aliases only inside its process.
- Official model URLs: `https://huggingface.co/spaces/rolpotamias/WiLoR/resolve/main/pretrained_models/{detector.pt,wilor_final.ckpt}`.
- detector.pt: 53,582,271 bytes, SHA256 `5ef3df44e42d2db52d4ffe91f83a22ce9925e2acc9abebf453f2c5d22e380033`.
- wilor_final.ckpt: 2,564,989,533 bytes, SHA256 `3e97aafc7dd08d883a4cc5a027df61fdb6fda6136dbd1319405413862ada6bb2`.
- The user must obtain `MANO_RIGHT.pkl` from https://mano.is.tue.mpg.de/ after accepting its terms and place it in `WiLoR/repo/mano_data/`. Left hands use the model's mirrored right-hand inference; `MANO_LEFT.pkl` is not required by this worker.
- Local setup on 2026-11-08: the user supplied `C:/Git/mano_v1_2`; its right-hand model and license were copied into `WiLoR/repo/mano_data/`. Model SHA256: `45d60aa3b27ef9107a7afd4e00808f307fd91111e1cfa35afd5c4a62de264767`. Loading this pickle also required `dill==0.3.8`, now included in the local dependency lock. The successful test2 run used 244 frames; its inference loop took 43.80 seconds with peak PyTorch allocated CUDA memory of 2693.27 MiB. This excludes video decoding, ViTPose assignment and model loading and is not an end-to-end benchmark.
- WiLoR code/weights: CC BY-NC-ND 4.0; MANO has its separate model license; Ultralytics has AGPL/commercial terms. These dependencies are local research installations, excluded from Git and not a redistributable plugin bundle.

Reference design: internal GVHMR3 kit v13, `gvhmr3_hands_detect.py` and `gvhmr3_hands_smplx.py`, from [the provided R&D page](https://app.notion.com/p/40bdadb56b2f81049463efb200f5dfde). The Cubeless adapter uses body-wrist assignment, left-hand reflection, short-gap interpolation, spike rejection and quaternion smoothing. It deliberately retains existing wrists and blends hidden hands back to the original animation, unlike the kit's wrist replacement/relaxed-pose fallback.

## Diagnostics

Per run: `Saved/CubelessBodyMocap/Hands_<id>/`. `worker.log`, `metrics.json`, `detection.json`, `hands.json`, and `preservation.json` record worker results and the native untouched-track preservation check. The legacy `preserved_nonfinger_tracks` report key now excludes edited metacarpals as specified by `preservation_scope` and `edited_hand_tracks`. MediaPipe also saves pre-filter landmark/rotation data in `raw_hands.npz`; diagnostic hand JSON can request native `retarget_stages.json`. Detection coverage and transfer fidelity are not ground-truth accuracy metrics. Failed inference does not create an enhanced animation; an edited input during inference is rejected. Cancel stops the owned external worker. The existing body extraction does not run again.

The current assignment pass uses ViTPose on the full frame and is intended for a single clearly visible performer. Multi-person selection is not implemented in this hand-only pass.
