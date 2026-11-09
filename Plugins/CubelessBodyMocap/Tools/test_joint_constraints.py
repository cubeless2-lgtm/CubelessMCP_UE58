"""CPU regression checks; no model weights or Unreal instance required."""
import unittest
import tempfile
import json
from pathlib import Path
from unittest.mock import patch
import numpy as np
from scipy.spatial.transform import Rotation as R
from joint_constraints import anatomical_frames, constrain, matrices_from_angles, coupled_limits, soft_project
from joint_contact import closest, FingerCapsules, refine


def calibration():
    # Flat palms face -Z; thumb is oblique to the four long fingers.
    hand = []
    for y in (2., .5, -2., -1.):
        hand.extend([[x, y, 0.] for x in (5., 8., 10.)])
    hand.extend([[3., 3., -.5], [5., 5., -.5], [7., 6.5, -.5]])
    hand = np.array(hand)
    return dict(source_rest_positions=np.stack([hand, hand*[-1, 1, 1]]).tolist(),
                source_rest_rotations=np.tile([0., 0., 0., 1.], (2, 15, 1)).tolist(),
                source_rest_wrists=[[0., 0., 0.], [0., 0., 0.]])


class FingerPostTests(unittest.TestCase):
    def setUp(self):
        self.c = calibration(); self.basis = anatomical_frames(self.c)

    def encode(self, degrees):
        a = np.deg2rad(degrees)
        matrix = self.basis @ matrices_from_angles(a) @ self.basis.swapaxes(-1, -2)
        return R.from_matrix(matrix.reshape(-1, 3, 3)).as_quat().reshape(*a.shape[:-1], 4)

    def test_fast_valid_motion_and_quaternion_sign_are_preserved(self):
        a = np.zeros((9, 2, 15, 3))
        a[..., 0] = np.array([0, 10, 80, 10, 0, 10, 80, 10, 0])[:, None, None]
        q = self.encode(a); q[::2] *= -1
        result, _, _ = constrain(q, self.c)
        np.testing.assert_array_equal(result, q)

    def test_isolated_invalid_pose_and_zero_strength(self):
        a = np.zeros((9, 2, 15, 3)); a[4, 0, 1] = [160, 70, 90]
        q = self.encode(a); result, metrics, _ = constrain(q, self.c)
        self.assertEqual(metrics['range_violations_after'], 0)
        self.assertGreater(metrics['temporal_flags'], 0)
        self.assertLess(metrics['step_max_after'], metrics['step_max_before'])
        zero, _, _ = constrain(q, self.c, strength=0)
        np.testing.assert_array_equal(zero, q)
        opposite, _, _ = constrain(-q, self.c)
        np.testing.assert_allclose(opposite, -result, atol=1e-12)

    def test_bilateral_bend_and_distinct_thumb_plane(self):
        for side in (0, 1):
            for joint in (0, 12):
                basis = self.basis[side, joint]
                before = basis[:, 2]
                after = basis @ R.from_euler('X', 20, degrees=True).as_matrix() @ basis.T @ before
                self.assertLess((after-before)[2], 0.)
                if joint == 12: self.assertLess((after-before)[1], 0.)

    def test_invalid_calibration_fails(self):
        c = calibration(); c['source_rest_positions'] = np.zeros((2, 15, 3)).tolist()
        with self.assertRaises(ValueError): anatomical_frames(c)

    def test_bent_long_finger_restricts_lateral_motion_not_thumb_opposition(self):
        a=np.zeros((2,2,15,3));a[1,...,0]=np.deg2rad(80)
        _,high=coupled_limits(a)
        self.assertTrue(np.all(high[1,:,:12,1:]<high[0,:,:12,1:]))
        np.testing.assert_array_equal(high[1,:,12:],high[0,:,12:])

    def test_bad_spread_does_not_interpolate_valid_flexion(self):
        a=np.zeros((9,2,15,3));a[:,0,0,0]=90;a[4,0,0]=[60,45,0]
        _,_,diagnostics=constrain(self.encode(a),self.c)
        self.assertAlmostEqual(np.rad2deg(diagnostics['angles_after'][4,0,0,0]),60,places=6)

    def test_closest_segment_crossing_and_parallel(self):
        a=np.array([[0.,0,0],[0,0,0]]);b=np.array([[2.,0,0],[2,0,0]])
        c=np.array([[1.,-1,0],[.5,1,0]]);d=np.array([[1.,1,0],[1.5,1,0]])
        p,q=closest(a,b,c,d)
        np.testing.assert_allclose(np.linalg.norm(p-q,axis=-1),[0,1])

    def test_narrow_hinge_bounds_keep_neutral_and_flexion(self):
        a=np.zeros((3,2,15,3));a[...,0]=np.deg2rad(85)
        lo,hi=coupled_limits(a);out=soft_project(a,lo,hi)
        np.testing.assert_array_equal(out,a)
        self.assertTrue(np.all(hi[...,1:12:3,1:]<np.deg2rad(5)))

    def test_weak_image_cues_are_not_treated_as_strong_pose_evidence(self):
        from handflow_image_guidance import image_reliability
        x=image_reliability(np.array([0.,.3,.34,.5,.7,.9,1.]))
        np.testing.assert_allclose(x,[0,0,.01,.25,1,1,1])

    def test_status_publication_retries_transient_reader_lock(self):
        from run_joint_post import write
        real=Path.replace;attempts=[]
        def replace(source,target):
            attempts.append(1)
            if len(attempts)<3:raise PermissionError('simulated reader handle')
            return real(source,target)
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'status.json';p.write_text('{"stage":"old"}')
            with patch.object(Path,'replace',replace):write(p,{'stage':'new'})
            self.assertEqual(json.loads(p.read_text()),{'stage':'new'})
            self.assertEqual(len(attempts),3)

    def test_same_hand_collision_gradient_and_open_pose_preservation(self):
        c=calibration();n=5;points=np.array(c['source_rest_positions']).reshape(30,3)
        positions=points.copy();ends=points.copy();parents=[]
        for j in range(30):
            parents.append(j-1 if j%3 else 38+j//15)
            if j%3:positions[j]=points[j]-points[j-1]
            ends[j]=points[j+1]-points[j] if j%3<2 else points[j]-points[j-1]
        c.update(source_local_q=np.tile([0,0,0,1],(30,1)),source_parent_q=np.tile([0,0,0,1],(30,1)),
                 left_q=np.tile([0,0,0,1],(2,1)),right_q=np.tile([0,0,0,1],(30,1)),
                 input_world_q=np.tile([0,0,0,1],(n,38,1)),wrists_q=np.tile([0,0,0,1],(n,2,1)),
                 wrists_p=np.zeros((n,2,3)),positions=np.tile(np.vstack([positions,np.zeros((8,3))]),(n,1,1)),
                 parents=parents+[38]*4+[39]*4,ends=ends,radii=np.full(30,.25))
        geo=FingerCapsules(c,self.basis);a=np.zeros((n,2,15,3))
        pairs=set(map(tuple,geo.pairs))
        self.assertIn((0,2),pairs);self.assertIn((12,14),pairs)
        self.assertNotIn((0,1),pairs);self.assertNotIn((1,2),pairs)
        project=lambda v:np.clip(v,*coupled_limits(v))
        out,m,_=refine(a,c,self.basis,30,project)
        self.assertFalse(m['applied']);np.testing.assert_array_equal(out,a)
        a[:,0,0,1]=np.deg2rad(-25)
        cost,_,g=geo.evaluate(a,True);self.assertGreater(cost.sum(),0)
        plus=a.copy();minus=a.copy();plus[2,0,0,1]+=1e-6;minus[2,0,0,1]-=1e-6
        fd=(geo.evaluate(plus)[0][2,0]-geo.evaluate(minus)[0][2,0])/2e-6
        self.assertAlmostEqual(fd,g[2,0,0,1],places=5)
        out,m,_=refine(a,c,self.basis,30,project)
        self.assertLess(m['proxy_cost_after'],m['proxy_cost_before'])
        np.testing.assert_array_equal(out[:,1],a[:,1])
        pulse=np.zeros_like(a);pulse[2]=a[2]
        out,m,_=refine(pulse,c,self.basis,30,project)
        self.assertLess(m['proxy_cost_after'],m['proxy_cost_before'])
        self.assertLessEqual(m['max_contact_correction_step_degrees'],3.001)
        np.testing.assert_array_equal(out[:,1],pulse[:,1])


if __name__ == '__main__': unittest.main()
