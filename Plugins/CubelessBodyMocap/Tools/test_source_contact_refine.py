"""Regression checks: physical rotation continuity and jitter versus real motion."""
import unittest
import numpy as np
from scipy.spatial.transform import Rotation
from source_contact_refine import smooth_quaternions,rotation_metrics,repair_contact_observations


class SourceContactTests(unittest.TestCase):
    def test_quaternion_sign_flips_are_not_motion(self):
        q=np.tile([0.,0.,0.,1.],(80,1,1));q[::2]*=-1
        out=smooth_quaternions(q)
        self.assertLess(rotation_metrics(out)['max_step_degrees'],1e-9)
        np.testing.assert_allclose(np.linalg.norm(out,axis=-1),1,atol=1e-12)

    def test_jitter_is_reduced_without_erasing_a_slow_turn(self):
        t=np.arange(100);truth=t*.6;angles=truth+3*(-1.)**t
        q=Rotation.from_euler('z',angles,degrees=True).as_quat()[:,None,:]
        out=smooth_quaternions(q,sigma=1.5,pin_first=True)
        actual=Rotation.from_quat(out[:,0]).as_euler('xyz',degrees=True)[:,2]
        self.assertLess(np.std(actual[10:-10]-truth[10:-10]),.15)
        self.assertGreater(actual[-10]-actual[10],40)
        np.testing.assert_array_equal(out[0],q[0])

    def test_genuine_fast_monotonic_motion_is_retained(self):
        q=Rotation.from_euler('x',np.arange(30)*5.,degrees=True).as_quat()[:,None,:]
        out=smooth_quaternions(q,sigma=1.,pin_first=True)
        r=Rotation.from_quat(out[:,0]);v=(r[:-1].inv()*r[1:]).magnitude()
        np.testing.assert_allclose(np.rad2deg(v[5:-5]),5,atol=.001)

    def test_invalid_rotation_is_rejected(self):
        with self.assertRaises(ValueError):smooth_quaternions(np.zeros((10,1,4)))

    def test_short_hidden_hand_hallucination_is_repaired_but_release_is_retained(self):
        t=np.arange(60)/30;k=np.zeros((60,133,3));k[:,:,2]=.95
        k[:,9,0]=700;k[:,10,0]=750;k[:,100,0]=740;k[:,121,0]=790
        k[20:28,9,0]=350;k[20:28,100,0]=360;k[20:28,100,2]=.35
        hints={'l':{'weight_keys':[[0,1],[1.4,1],[1.5,0],[2,0]]}}
        # A later genuine release must not be rewritten as contact.
        k[50:,9,0]=300;k[50:,100,0]=320;k[50:,100,2]=.4
        result,receipt=repair_contact_observations(k,t,1920,hints)
        np.testing.assert_allclose(result[20:28,9,0],700)
        np.testing.assert_array_equal(result[50:,9],k[50:,9])
        self.assertTrue(receipt['l']['frames']);np.testing.assert_array_equal(k[20:28,9,0],350)


if __name__=='__main__':unittest.main()
