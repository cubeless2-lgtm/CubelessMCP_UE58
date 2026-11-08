"""Independent HaMeR/HandFlow worker; outputs the shared finger-only contract."""
import os
from pathlib import Path
import time


def infer(config):
    import numpy as np
    import torch
    from scipy.spatial.transform import Rotation
    from refine_wilor import atomic_json
    from hand_contract import smpl_quaternions_to_ue
    from handflow_engine import infer as predict
    c = config
    work = Path(c['work'])
    n = c['frames']
    if not torch.cuda.is_available():
        raise RuntimeError('HandFlow requires CUDA in its separate Python environment')
    torch.cuda.reset_peak_memory_stats()
    started = time.perf_counter()

    def emit(event, **values):
        atomic_json(work/'worker_status.json', {
            'stage': values.get('message', 'HandFlow'),
            'done': n*values.get('current', 0)/max(1, values.get('total', n)),
            'frames': n})

    rotations, valid, details = predict(c, emit, n)
    if not valid.any():
        raise RuntimeError('HandFlow found no hands; original animation retained')
    if np.max(np.abs(rotations.swapaxes(-1, -2)@rotations-np.eye(3))) > 2e-4 or np.min(np.linalg.det(rotations)) < .9998:
        raise ValueError('Invalid HandFlow SO(3) output')
    q = Rotation.from_matrix(rotations.reshape(-1, 3, 3)).as_quat().reshape(n, 2, 15, 4)
    # Temporal predictions include gaps. Do not run WiLoR/MediaPipe cleaning or
    # blend between estimators. An entirely unseen side retains the input hand.
    weights = np.broadcast_to(valid.any(0), (n, 2)).astype(float).copy()
    state = np.where(valid, 1, np.where(weights > 0, 4, 0))
    ue = smpl_quaternions_to_ue(q)
    from handflow_contact import refine as refine_contact
    ue,contact=refine_contact(ue,weights,c,lambda done,total:emit('progress',current=done,total=total,message='HandFlow: fitting finger contact on target geometry'))
    np.savez_compressed(work/'hand_diagnostics.npz', quaternions_smpl=q, weights=weights, state=state)
    metrics = dict(details, target_contact=contact, backend='HandFlow', frames=n, fps=c['fps'],
                   seconds=time.perf_counter()-started,
                   detected_frames_LR=valid.sum(0).tolist(),
                   missing_frames_LR=(~valid).sum(0).tolist(),
                   temporally_predicted_missing_frames_LR=(state == 4).sum(0).tolist(),
                   original_fallback_frames_LR=(state == 0).sum(0).tolist(),
                   cuda_peak_allocated_mib=torch.cuda.max_memory_allocated()/1048576.,
                   frame_offset=0, offline_future_context_frames=None,
                   inference_window_frames=16, offline_bidirectional_refinement=True,
                   offline_context='Whole clip: global shape and image-guidance residual continuity; no strict 15-frame latency bound',
                   model_output='physical flat-hand local rotations; MANO order',
                   mean_hand_correction='none; already physical, not PCA residuals',
                   note='Missing observations are flagged temporal predictions, not detections. No other hand estimator is substituted.')
    atomic_json(work/'metrics.json', metrics)
    atomic_json(work/'hands.json', dict(version=1, backend='HandFlow', frames=n,
                fps=c['fps'], source_asset=c['source_asset'], diagnostics=c.get('diagnostics', False),
                quaternions_ue=ue.reshape(-1).tolist(), weights=weights.reshape(-1).tolist()))
    atomic_json(work/'worker_status.json', dict(stage='Completed', done=n, frames=n))
