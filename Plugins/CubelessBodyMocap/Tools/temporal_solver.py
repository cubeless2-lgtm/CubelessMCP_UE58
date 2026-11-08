"""Damped least squares for frame-local residuals spanning <=3 adjacent frames.

Keeps every frame and the complete objective. Numeric Jacobian columns are
coloured by frame modulo3; damped normal equations use banded Cholesky rather
than thousands of iterative LSMR passes. No parameter or joint-angle bounds.
"""
import time
from types import SimpleNamespace
import numpy as np
from scipy.sparse import csr_matrix
from scipy.linalg import solveh_banded

def temporal_least_squares(fun, x0, jac_sparsity, block_size, max_nfev=45,
                           ftol=1e-4, progress=None):
    start=time.perf_counter();x=np.asarray(x0,dtype=float).copy();size=len(x)
    pattern=jac_sparsity.tocsr();pattern.eliminate_zeros();coo=pattern.tocoo()
    rows,cols=coo.row,coo.col
    colours=(cols//block_size%3)*block_size+cols%block_size
    groups=[]
    for colour in range(3*block_size):
        ids=np.flatnonzero(colours==colour)
        if not len(ids):continue
        if len(np.unique(rows[ids]))!=len(ids):raise ValueError('Residual exceeds temporal solver three-frame support')
        groups.append((ids,np.unique(cols[ids])))
    r=np.asarray(fun(x));cost=float(r@r/2);nfev=1;njev=0;fd_calls=0;damping=.001
    if not np.isfinite(cost):raise ValueError('Nonfinite initial temporal objective')
    history=[cost];times=dict(jacobian=0.,normal=0.,linear=0.,objective=0.);status=0
    for iteration in range(max_nfev):
        if progress:progress(iteration,max_nfev,cost)
        t=time.perf_counter();steps=1e-6*np.maximum(1.,np.abs(x));values=np.empty(len(rows))
        for ids,variables in groups:
            perturbed=x.copy();perturbed[variables]+=steps[variables]
            diff=(np.asarray(fun(perturbed))-r);fd_calls+=1
            values[ids]=diff[rows[ids]]/steps[cols[ids]]
        J=csr_matrix((values,(rows,cols)),shape=pattern.shape);J.eliminate_zeros();njev+=1;times['jacobian']+=time.perf_counter()-t
        t=time.perf_counter();gradient=np.asarray(J.T@r).reshape(-1);H=(J.T@J).tocoo()
        lower=H.row>=H.col;offsets=H.row[lower]-H.col[lower];bandwidth=int(offsets.max(initial=0))
        if bandwidth>=3*block_size:raise ValueError('Normal matrix exceeds expected temporal band')
        band=np.zeros((bandwidth+1,size));band[offsets,H.col[lower]]=H.data[lower]
        diagonal=np.maximum(band[0],1.);times['normal']+=time.perf_counter()-t
        if np.max(np.abs(gradient))<1e-6:status=1;break
        accepted=False;previous=cost
        for attempt in range(10):
            t=time.perf_counter();system=band.copy();system[0]+=damping*diagonal
            try:delta=solveh_banded(system,-gradient,lower=True,overwrite_ab=True,check_finite=False)
            except np.linalg.LinAlgError:damping*=10;continue
            times['linear']+=time.perf_counter()-t
            t=time.perf_counter();candidate=x+delta;nr=np.asarray(fun(candidate));nfev+=1;nc=float(nr@nr/2);times['objective']+=time.perf_counter()-t
            if np.isfinite(nc) and nc<cost:
                x=candidate;r=nr;cost=nc;history.append(cost);damping=max(damping*.3,1e-9);accepted=True;break
            damping*=10
        if not accepted:status=3;break
        if previous-cost<ftol*max(previous,1.):status=2;break
    return SimpleNamespace(x=x,cost=cost,nfev=nfev,njev=njev,status=status,
        solver_report=dict(method='temporal_banded_lm',seconds=time.perf_counter()-start,
        iterations=njev,objective_evaluations=nfev,finite_difference_evaluations=fd_calls,
        timings=times,cost_history=history,bandwidth=bandwidth,all_frames=True,joint_limits=False))
