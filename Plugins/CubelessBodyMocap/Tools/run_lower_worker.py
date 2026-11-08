"""Independent video pelvis/legs job with optional model-free sole support."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import threading
import time
import traceback

START = time.perf_counter()

def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for part in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()

def write(path, value):
    tmp = Path(str(path) + '.tmp')
    tmp.write_text(json.dumps(value), encoding='utf8')
    # Windows readers can briefly hold the destination without delete sharing.
    # Retry only this idempotent owned-file replacement, never an Editor mutation.
    for attempt in range(11):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            if attempt == 10:
                raise
            time.sleep(.01 * (attempt + 1))

def main(c):
    import numpy as np
    import scipy
    import psutil
    from lower_body import fit_lower_body, finish_legs_only
    work = Path(c['work']); work.mkdir(parents=True, exist_ok=True)
    peak = [0]; stop = threading.Event()
    def monitor():
        process = psutil.Process()
        while not stop.wait(.025):
            peak[0] = max(peak[0], process.memory_info().rss)
    thread = threading.Thread(target=monitor, daemon=True); thread.start()
    last_status = [None, 0.0]
    def status(stage, done=0):
        now = time.perf_counter()
        if stage == last_status[0] and now - last_status[1] < .1:
            return
        write(work/'worker_status.json', dict(stage=stage, done=done))
        last_status[:] = [stage, now]
    try:
        status('Validate saved video observations')
        body = Path(c['body_data']); n = int(c['frames'])
        # Legacy filenames describe saved body observations, not a model dependency.
        meta_path = body/'FootMRInput.json'
        meta = json.loads(meta_path.read_text(encoding='utf-8-sig'))
        if not 2 <= n <= 4096 or c['fps'] != 30 or n != meta['frames'] or c['fps'] != meta['fps'] or abs(meta['start_seconds']-c['start_seconds']) > .0001:
            raise ValueError('Body/video frame range mismatch; supported range is 2..4096 frames at 30 FPS.')
        video = Path(c['video']).resolve()
        if str(video).casefold() != str(Path(meta['video']).resolve()).casefold() or video.stat().st_size != meta['video_size'] or int(video.stat().st_mtime) != int(meta['video_mtime']):
            raise ValueError('Video changed since body extraction.')
        context = c['contact_context']
        if context.get('version') != 1 or context.get('frames') != n or 'lower_body' not in context or not c.get('contact_context_signature'):
            raise ValueError('Missing native target skeleton/geometry snapshot.')
        paths = [video, meta_path, body/'FootMR_Keypoints25.f32', body/'FootMR_Camera.f32', Path(__file__), Path(__file__).with_name('lower_body.py'), Path(__file__).with_name('temporal_solver.py')]
        if context.get('support_rotation',False):paths.append(Path(__file__).with_name('support_contact.py'))
        if context.get('temporal_cleanup',False):paths.append(Path(__file__).with_name('temporal_feet.py'))
        hashes = {str(p.resolve()): digest(p) for p in paths}
        versions = dict(numpy=np.__version__, scipy=scipy.__version__, python=sys.version)
        identity = dict(version=1, hashes=hashes, versions=versions, context=context, context_signature=c['contact_context_signature'], source_asset=c['source_asset'], source_guid=c['source_guid'], start_seconds=c['start_seconds'])
        key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        cache = Path(c['cache_root'])/key; cached = cache/'result.json'; hit = cached.exists()
        if hit:
            result = json.loads(cached.read_text())
            if result.get('cache_key') != key or result.get('backend') != 'VideoLegs':
                raise ValueError('Invalid lower-body cache identity.')
        else:
            kp = np.fromfile(body/'FootMR_Keypoints25.f32', dtype='<f4').reshape(n,25,3)
            camera = np.fromfile(body/'FootMR_Camera.f32', dtype='<f4')
            if len(camera) < 5 or not np.isfinite(camera).all():
                raise ValueError('Invalid body camera input.')
            solve_context=context;cleanup=None
            if context.get('temporal_cleanup',False):
                if not context.get('support_rotation',False):raise ValueError('Temporal foot cleanup requires support rotation mode')
                from temporal_feet import stabilize
                solve_context,cleanup=stabilize(context)
            _, state, fit = fit_lower_body(solve_context, kp, camera, status)
            result = dict(version=1, backend='VideoLegs', frames=n, fps=30, source_asset=c['source_asset'], source_guid=c['source_guid'], contact_context_signature=c['contact_context_signature'], cache_key=key, code_and_input_sha256=hashes, versions=versions, model='none; saved body video observations', joint_limits=False)
            if context.get('support_rotation',False):
                from support_contact import fit_support_rotation
                result['support_contact_fit']=fit_support_rotation(state,context,status)
            result = finish_legs_only(result, state, fit)
            result['temporal_cleanup']=cleanup is not None
            if cleanup is not None:result['temporal_foot_fit']=cleanup
            result['low_confidence_leg_frames'] = [int((kp[:,ids,2].min(1)<.7).sum()) for ids in [[12,13,14],[9,10,11]]]
            cache.mkdir(parents=True, exist_ok=True); write(cached, result)
        result['cache_hit'] = hit
        result['worker_seconds'] = time.perf_counter()-START
        result['worker_peak_ram_mib'] = max(peak[0], psutil.Process().memory_info().rss)/1048576
        write(work/'feet.json', result); status('Complete', n)
        print(json.dumps({k:result[k] for k in ['backend','cache_hit','worker_seconds','worker_peak_ram_mib']}))
    finally:
        stop.set(); thread.join()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--config', required=True)
    args = parser.parse_args(); config = json.loads(Path(args.config).read_text(encoding='utf-8-sig'))
    try:
        # -I omits the script directory; only the installed plugin's tools are added.
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        main(config)
    except Exception as exc:
        work = Path(config['work']); work.mkdir(parents=True, exist_ok=True)
        (work/'worker.log').write_text(traceback.format_exc(), encoding='utf8')
        try:
            write(work/'worker_status.json', dict(stage='Failed', error=str(exc)))
        except OSError:
            pass  # Preserve the original traceback even if status reporting fails.
        raise
