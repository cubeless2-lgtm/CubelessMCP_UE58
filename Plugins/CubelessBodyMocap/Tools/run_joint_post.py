"""CPU-only, no-inference postprocess of a saved Manny animation."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import traceback
import numpy as np
from scipy.spatial.transform import Rotation as R
sys.path.insert(0, str(Path(__file__).resolve().parent))
from joint_constraints import constrain, VERSION


def write(path, data):
    path = Path(path);tmp = path.with_suffix(path.suffix+'.tmp')
    tmp.write_text(json.dumps(data, allow_nan=False), encoding='utf-8')
    # UE's Windows status-file reader may briefly hold a handle without delete
    # sharing. Retry the same atomic publication after that reader closes it.
    for attempt in range(21):
        try:
            tmp.replace(path)
            return
        except PermissionError:
            if attempt==20: raise
            time.sleep(.01)


def matrix(q):
    q = np.asarray(q,float)
    return R.from_quat(q.reshape(-1,4)).as_matrix().reshape(*q.shape[:-1],3,3)


def run(c):
    started=time.perf_counter();work=Path(c['work']);n=c['frames'];ctx=c['calibration']
    def status(stage,done):write(work/'worker_status.json',dict(stage=stage,done=done,frames=n))
    status('Validating saved-animation calibration',0)
    target=matrix(ctx['input_world_q']);wrists=matrix(ctx['wrists_q'])
    left=matrix(ctx['left_q']);right=matrix(ctx['right_q']);local=matrix(ctx['source_local_q']);fixed=matrix(ctx['source_parent_q'])
    sp=ctx['source_parents'];tp=ctx['parents'];sides=np.arange(30)//15
    source=np.swapaxes(left[sides],-1,-2) @ np.swapaxes(wrists[:,sides],-1,-2) @ target[:,:30] @ np.swapaxes(right,-1,-2)
    delta=[]
    for j in range(30):delta.append(np.swapaxes(local[j],-1,-2) @ np.swapaxes(source[:,sp[j]] if sp[j]>=0 else fixed[j],-1,-2) @ source[:,j])
    delta=np.stack(delta,1)
    q=R.from_matrix(delta.reshape(-1,3,3)).as_quat().reshape(n,2,15,4)
    original=np.asarray(ctx['input_local_q'],float).reshape(n,30,4)
    def reconstruct(values):
        d=matrix(values).reshape(n,30,3,3);sg=[];tg=[];out=[]
        for j in range(30):
            sg.append((sg[sp[j]] if sp[j]>=0 else fixed[j]) @ local[j] @ d[:,j])
            tg.append(wrists[:,j//15] @ left[j//15] @ sg[j] @ right[j])
            parent=tg[tp[j]] if tp[j]<30 else target[:,tp[j]] if tp[j]<38 else wrists[:,tp[j]-38]
            out.append(np.swapaxes(parent,-1,-2) @ tg[j])
        return R.from_matrix(np.stack(out,1).reshape(-1,3,3)).as_quat().reshape(n,30,4)
    roundtrip=reconstruct(q)
    error=np.rad2deg((R.from_quat(original.reshape(-1,4)).inv()*R.from_quat(roundtrip.reshape(-1,4))).magnitude())
    if error.max()>.01:raise ValueError(f'Inverse/forward retarget calibration error {error.max():.6f} degrees')
    confidence=None
    evidence=c.get('confidence_file','')
    if evidence:
        data=np.load(evidence,allow_pickle=False)
        scores=np.asarray(data['guidance_scores'],float);valid=np.asarray(data['valid'],bool)
        if scores.shape!=(n,2) or valid.shape!=(n,2):raise ValueError('Hand evidence does not match animation frame count')
        confidence=np.where(valid,np.clip(scores,0,1),0.)
    status('Constraining anatomical axes and isolated temporal outliers',n*.25)
    corrected,metrics,diagnostics=constrain(q,ctx,c['strength'],c['fps'],confidence,
        lambda i,total:status('Separating same-hand fingers',n*(.35+.5*i/total)))
    out=reconstruct(corrected)
    untouched=np.linalg.norm(corrected-q,axis=-1).reshape(n,30)<1e-12
    out[untouched]=original[untouched]
    out*=np.where(np.sum(out*original,axis=-1,keepdims=True)<0,-1,1)
    metrics.update(roundtrip_max_degrees=float(error.max()),seconds=time.perf_counter()-started,source_asset=c['source_asset'],confidence_file=evidence,
                   code_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),Path(__file__).with_name('joint_constraints.py'),Path(__file__).with_name('joint_contact.py')]})
    np.savez_compressed(work/'joint_diagnostics.npz',input_quaternions_ue=q,output_quaternions_ue=corrected,**diagnostics)
    write(work/'metrics.json',metrics)
    write(work/'corrected.json',dict(version=1,backend='JointCorrection',source_asset=c['source_asset'],frames=n,fps=c['fps'],names=ctx['names'][:30],target_local_quaternions=out.reshape(-1).tolist(),profile=VERSION))
    status('Completed',n)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--config',type=Path,required=True);args=parser.parse_args()
    c=json.loads(args.config.read_text(encoding='utf-8-sig'))
    try:run(c)
    except Exception as error:
        Path(c['work'],'worker.log').write_text(traceback.format_exc(),encoding='utf-8')
        write(Path(c['work'])/'worker_status.json',dict(stage='Failed',error=str(error)))
        raise
