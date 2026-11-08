"""Small regression cases for the observed closed-fist and single-joint flips."""
import unittest
import numpy as np
from scipy.spatial.transform import Rotation as R
from refine_mediapipe import solve_fingers,stabilize_fingers
from refine_wilor import clean_rotations


class FingerStability(unittest.TestCase):
    def test_fast_transition_is_not_mislabeled_as_a_missing_detection(self):
        q=np.tile([0.,0.,0.,1.],(3,2,15,1))
        q[:,0]=R.from_euler('x',[0,55,110],degrees=True).as_quat()[:,None,:]
        _,weights,state=clean_rotations(q,np.ones((3,2),bool),30)
        np.testing.assert_array_equal(state[:,0],1)
        np.testing.assert_array_equal(weights,1)

    def test_return_to_neighbor_spike_still_becomes_interpolation(self):
        q=np.tile([0.,0.,0.,1.],(3,2,15,1))
        q[1,0]=R.from_euler('x',110,degrees=True).as_quat()
        fixed,_,state=clean_rotations(q,np.ones((3,2),bool),30)
        self.assertEqual(state[1,0],2)
        np.testing.assert_allclose(abs(fixed[...,3]),1,atol=1e-6)

    def test_landmark_directions_are_not_replaced_by_curl_constraints(self):
        rest=np.array([[x,y,0.] for x,y0 in [(.3,.7),(0,.8),(-.6,.6),(-.3,.7),(.6,.35)] for y in [y0,y0+.3,y0+.5]])
        p=np.zeros((21,3))
        for chain,start in enumerate([5,9,17,13,1]):
            p[start:start+3]=rest[chain*3:chain*3+3]
            p[start+3]=2*p[start+2]-p[start+1]
        p[6]=p[5]+[.04,.2,.08]
        p[7]=p[6]+[-.05,.12,-.17]
        p[8]=p[7]+[.04,-.13,-.06]
        for mirror in [1.,-1.]:
            ref=rest*np.array([mirror,1,1]);observed=p*np.array([mirror,1,1])
            q=solve_fingers(observed[:,[0,2,1]],ref);parent=R.identity()
            for seg in range(3):
                delta=ref[seg+1]-ref[seg] if seg<2 else ref[seg]-ref[seg-1]
                parent=parent*R.from_quat(q[seg])
                result=parent.apply(delta);target=observed[6+seg]-observed[5+seg]
                np.testing.assert_allclose(result/np.linalg.norm(result),target/np.linalg.norm(target),atol=1e-6)

    def test_closed_fist_has_no_distal_roll_flip(self):
        rest=np.array([[x,y,0.] for x,y0 in [(.3,.7),(0,.8),(-.6,.6),(-.3,.7),(.6,.35)] for y in [y0,y0+.3,y0+.5]])
        poses=[]
        for mcp in [85.,90.,95.]:
            p=np.zeros((21,3))
            for chain,start in enumerate([5,9,17,13,1]):
                p[start:start+3]=rest[chain*3:chain*3+3]
                p[start+3]=2*p[start+2]-p[start+1]
            p[6]=p[5]+R.from_euler('x',mcp,degrees=True).apply([0,.3,0])
            p[7]=p[6]+R.from_euler('x',mcp+95,degrees=True).apply([0,.2,0])
            p[8]=p[7]+R.from_euler('x',mcp+145,degrees=True).apply([0,.15,0])
            q=solve_fingers(p[:,[0,2,1]],rest)
            distal=R.from_quat(q[1:3]).as_rotvec()*180/np.pi
            np.testing.assert_allclose(distal,[[95,0,0],[50,0,0]],atol=1e-6)
            poses.append(q)
        step=R.from_quat(poses[0]).inv()*R.from_quat(poses[1])
        self.assertLess(np.degrees(step.magnitude()).max(),6)

    def test_single_joint_spike_is_not_hidden_by_other_fingers(self):
        q=np.tile([0.,0.,0.,1.],(9,2,15,1));valid=np.ones((9,2),bool)
        q[4,0,7]=R.from_euler('x',150,degrees=True).as_quat()
        fixed,w,_,count=stabilize_fingers(q,valid,30)
        self.assertGreaterEqual(count,1)
        np.testing.assert_allclose(abs(fixed[...,3]),1,atol=1e-6)
        np.testing.assert_array_equal(w,1)

    def test_no_detection_keeps_original_fallback(self):
        q=np.tile([0.,0.,0.,1.],(40,2,15,1))
        fixed,w,_,_=stabilize_fingers(q,np.zeros((40,2),bool),30)
        np.testing.assert_array_equal(w,0)
        self.assertTrue(np.isfinite(fixed).all())


if __name__=='__main__':
    unittest.main()
