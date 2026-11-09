"""Isolated hand dispatch, content-addressed cache and reproducible result record.

No body inference, no automatic estimator selection. Native owner cancels this
process tree. Incomplete cache generations have no published receipt.
"""
import argparse
import contextlib
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import sys
import threading
import time
import traceback
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
from refine_wilor import atomic_json


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def signature(c):
    tools = Path(__file__).resolve().parent
    backend = c['backend']
    verified_model_hashes = {}
    model_files = []
    runtime_files = []
    if backend == 'HandFlow':
        root = Path(c['handflow_root'])
        manifest = json.loads((root/'manifest.json').read_text())
        for entry in manifest['models']:
            path = root/entry['path']
            if not path.is_file():
                raise FileNotFoundError('HandFlow model missing: '+str(path))
            digest = sha(path)
            verified_model_hashes[str(path.resolve())] = digest
            if digest != entry['sha256']:
                raise ValueError('HandFlow model SHA256 mismatch: '+str(path))
            model_files.append(path)
        runtime_files = [root/x['path'] for x in manifest['source_files']]
        model_files.append(root/'manifest.json')
    elif backend == 'WiLoR':
        repo = Path(c['repo'])
        model_files = [repo/x for x in ['pretrained_models/wilor_final.ckpt', 'pretrained_models/detector.pt',
                       'pretrained_models/model_config.yaml', 'mano_data/MANO_RIGHT.pkl', 'mano_data/mano_mean_params.npz']]
        runtime_files = list((repo/'wilor').rglob('*.py'))
        model_files += [tools.parent/'ThirdParty/MediaPipe/hand_landmarker.task']
    elif backend == 'MediaPipe':
        model_files = [Path(c['model'])]
    else:
        raise ValueError('Unknown hand backend: '+backend)
    versions = {}
    for package in ['torch', 'torchvision', 'numpy', 'scipy', 'timm', 'ultralytics', 'smplx', 'mediapipe', 'webdataset', 'torchdiffeq']:
        try:
            versions[package] = importlib.metadata.version(package)
        except importlib.metadata.PackageNotFoundError:
            versions[package] = None
    inputs = {f'{i:08d}': sha(Path(c['frame_dir'])/f'{i:08d}.png') for i in range(c['frames'])}
    inputs['keypoints'] = sha(Path(c['work'])/'keypoints.f32')
    if c.get('video'):
        inputs['video'] = sha(c['video'])
    # Source asset identity is rebound on cache load; physical predictions depend
    # on source rest geometry, video interval and camera, all included here.
    settings = {k: v for k, v in c.items() if k not in {'work', 'frame_dir', 'source_asset', 'repo', 'model', 'handflow_root', 'video', 'cache_root', 'reuse_cache'}}
    # WiLoR launches MediaPipe in its own interpreter; include that environment
    # as well, rather than fingerprinting only this parent's installed packages.
    helper_runtime = {}
    if backend == 'WiLoR':
        helper_site = tools.parent/'ThirdParty/MediaPipe/venv/Lib/site-packages'
        helper_runtime = {d.metadata['Name']: d.version for d in importlib.metadata.distributions(path=[str(helper_site)])}
    payload = dict(version=1, backend=backend, inputs=inputs, settings=settings, runtime=versions, helper_runtime=helper_runtime,
                   python=sys.version, models={str(p.resolve()): (verified_model_hashes[str(p.resolve())] if str(p.resolve()) in verified_model_hashes else sha(p)) if p.is_file() else None for p in model_files},
                   code={str(p.resolve()): sha(p) for p in sorted(set(list(tools.glob('*.py'))+runtime_files))})
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest(), payload


def run(c):
    work = Path(c['work'])
    os.environ['YOLO_CONFIG_DIR'] = str(work/'yolo_settings')
    os.environ['WANDB_MODE'] = 'disabled'
    started = time.perf_counter()
    peak = [0]
    stop = threading.Event()

    def sample():
        try:
            import psutil
        except ImportError:
            # Legacy MediaPipe does not require psutil. Do not change its
            # environment for optional telemetry or report a false zero peak.
            return
        process = psutil.Process()
        while not stop.wait(.1):
            try:
                peak[0] = max(peak[0], sum(p.memory_info().rss for p in [process]+process.children(recursive=True)))
            except psutil.Error:
                pass

    sampler = threading.Thread(target=sample, daemon=True)
    sampler.start()
    try:
        atomic_json(work/'worker_status.json', dict(stage='Validating hand inputs and model fingerprints', done=0, frames=c['frames']))
        key, identity = signature(c)
        cache = Path(c['cache_root'])/c['backend']/key
        receipt = cache/'complete.json'
        reuse_cache = c.get('reuse_cache', True)
        hit = False
        if reuse_cache and receipt.is_file():
            entry = json.loads(receipt.read_text())
            hit = entry['signature'] == key and all((cache/name).is_file() and sha(cache/name) == digest for name, digest in entry['files'].items())
            if hit:
                for name in entry['files']:
                    shutil.copy2(cache/name, work/name)
        if not hit:
            if c['backend'] == 'HandFlow':
                from refine_handflow import infer
            elif c['backend'] == 'WiLoR':
                from refine_wilor import infer
            else:
                from refine_mediapipe import infer
            infer(c)
            if signature(c)[0] != key:
                raise ValueError('Hand inputs, code or models changed during processing; result not applied')
            names = [p.name for p in work.iterdir() if p.name in {
                'hands.json', 'metrics.json', 'detection.json', 'hand_diagnostics.npz', 'model_evidence.npz', 'contact_evidence.npz', 'geometry_rejections.json'}]
            if reuse_cache:
                staging = cache.parent/(key+'.partial-'+uuid.uuid4().hex)
                staging.mkdir(parents=True)
                for name in names:
                    shutil.copy2(work/name, staging/name)
                atomic_json(staging/'complete.json', dict(signature=key, files={name: sha(staging/name) for name in names}))
                if not cache.exists():
                    staging.replace(cache)
            # A damaged existing cache is not silently overwritten. Its new
            # verified generation remains available in this run and staging.
        result = json.loads((work/'hands.json').read_text())
        result['source_asset'] = c['source_asset']
        result['diagnostics'] = c.get('diagnostics', False)
        result['provenance'] = dict(backend=c['backend'], signature=key, cache_hit=hit,
                                    identity_file=str(work/'hand_identity.json'), metrics_file=str(work/'metrics.json'))
        atomic_json(work/'hands.json', result)
        metrics = json.loads((work/'metrics.json').read_text())
        metrics.update(source_asset=c['source_asset'], worker_wall_seconds=time.perf_counter()-started,
                       worker_peak_process_tree_mib=peak[0]/1048576. if peak[0] else None,
                       worker_memory_measurement='sampled process-tree RSS' if peak[0] else 'unavailable: optional psutil not installed',
                       cache_hit=hit, cache_signature=key, cache_reuse_enabled=reuse_cache)
        atomic_json(work/'metrics.json', metrics)
        atomic_json(work/'hand_identity.json', identity)
        atomic_json(work/'worker_status.json', dict(stage='Completed', done=c['frames'], frames=c['frames']))
    finally:
        stop.set()
        sampler.join(timeout=1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    c = json.loads(args.config.read_text(encoding='utf-8-sig'))
    work = Path(c['work'])
    with (work/'worker.log').open('w', encoding='utf-8', buffering=1) as log, contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
        try:
            run(c)
        except Exception as e:
            traceback.print_exc()
            atomic_json(work/'worker_status.json', dict(stage='Failed', error=str(e)))
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
