"""Optional independent 2D observations for WiLoR input-crop selection."""
import json
from pathlib import Path
import time


def prepare_reference(config, progress):
    import hashlib
    import shutil
    import subprocess
    work=Path(config['work']).resolve()
    explicit=config.get('landmark_reference')
    started=time.perf_counter()
    if config.get('adaptive_crop',True) is False:return None,0.
    if explicit:
        path=Path(explicit).resolve()
        request=json.loads((path.parent/'request.json').read_text(encoding='utf-8-sig'))
        for key in ['frames','fps','source_asset','frame_dir']:
            if request[key]!=config[key]:raise ValueError('Landmark reference input mismatch: '+key)
        def digest(p):
            with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
        if digest(path.parent/'keypoints.f32')!=digest(work/'keypoints.f32'):raise ValueError('Landmark reference keypoints mismatch')
    else:
        plugin=Path(__file__).resolve().parent.parent
        python=plugin/'ThirdParty/MediaPipe/venv/Scripts/python.exe'
        model=plugin/'ThirdParty/MediaPipe/hand_landmarker.task'
        if not python.is_file() or not model.is_file():return None,0.
        target=work/'LandmarkReference';target.mkdir(exist_ok=False)
        request=dict(config,work=str(target),model=str(model));request.pop('landmark_reference',None)
        shutil.copyfile(work/'keypoints.f32',target/'keypoints.f32')
        request_path=target/'request.json';request_path.write_text(json.dumps(request))
        progress('Independent hand observations')
        with (target/'process.log').open('w') as log:
            result=subprocess.run([str(python),'-I',str(plugin/'Tools/refine_mediapipe.py'),'--config',str(request_path)],stdout=log,stderr=subprocess.STDOUT,timeout=600)
        if result.returncode:
            # A missing observation is not a failed WiLoR model. Use the
            # original crop, explicitly reporting the unavailable reference.
            return None,time.perf_counter()-started
        path=target/'detection.json'
    return json.loads(path.read_text()),time.perf_counter()-started


def select_crop(projected, reference):
    import numpy as np
    if reference is None:return 0,None
    uv=np.asarray(reference['keypoints2d'])
    scale=max(24.,float(np.ptp(uv,axis=0).max()))
    scores=np.linalg.norm(np.asarray(projected)-uv,axis=-1).mean(-1)/scale
    # Leave the official crop alone unless the alternative has a material
    # advantage against an independent, anatomically assigned observation.
    best=int(np.argmin(scores))
    if best and (scores[0]-scores[best]<.03 or scores[best]>.85*scores[0]):best=0
    return best,scores.tolist()
