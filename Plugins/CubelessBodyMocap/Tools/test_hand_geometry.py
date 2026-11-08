import unittest
import numpy as np
from scipy.spatial.transform import Rotation
from hand_geometry import assess_geometry, GeometryHistory, reject_palm_flips


def hand():
    p=np.zeros((21,3))
    for start,x,y in [(5,.03,.07),(9,0,.08),(17,-.04,.06),(13,-.02,.07),(1,.055,.025)]:
        p[start]=[x,y,0]
        for i,length in enumerate([.035,.025,.018]):
            p[start+i+1]=p[start+i]+[0,length,0]
    return p


class HandGeometryTests(unittest.TestCase):
    def test_rigid_motion_scale_and_chirality_do_not_change_acceptance(self):
        p=hand();reference=assess_geometry(p)[2]
        for scale in [.01,1.,100.]:
            for mirror in [-1,1]:
                q=Rotation.from_euler('xyz',[73,121,-48],degrees=True).apply(p*[mirror,1,1])*scale+[3,4,5]
                self.assertTrue(assess_geometry(q,reference)[0])

    def test_bending_is_not_an_angle_limit(self):
        p=hand();reference=assess_geometry(p)[2]
        # Deliberately include anatomically impossible curls: this validator
        # checks measurement quality, not which joint rotations are allowed.
        for degrees in [0,90,180,270]:
            q=p.copy()
            for start in [5,9,17,13,1]:
                for i in range(3):
                    q[start+i+1]=q[start+i]+Rotation.from_euler('x',degrees*(i+1),degrees=True).apply(p[start+i+1]-p[start+i])
            self.assertTrue(assess_geometry(q,reference)[0])

    def test_collapsed_palm_and_segment_are_rejected_before_rotation(self):
        p=hand();p[17]=p[5]+[1e-6,0,0]
        self.assertEqual(assess_geometry(p)[1]['reason'],'ill_conditioned_palm')
        p=hand();p[18]=p[17]+[0,.0001,0]
        self.assertEqual(assess_geometry(p)[1]['reason'],'collapsed_segment')

    def test_history_detects_length_change_but_expires_after_a_gap(self):
        p=hand();history=GeometryHistory(30)
        for frame in range(5):history.accept(p,0,frame)
        moderate=p.copy();moderate[6:9]-=[0,.022,0]
        self.assertTrue(history.assess(moderate,0,5)[0])
        q=p.copy();q[6:9]-=[0,.026,0]
        self.assertTrue(assess_geometry(q)[0])
        self.assertEqual(history.assess(q,0,5)[1]['reason'],'inconsistent_bone_lengths')
        self.assertTrue(history.assess(q,0,40)[0])
        self.assertTrue(history.assess(q,1,5)[0])

    def test_depth_normal_outlier_across_short_gap_is_not_an_anchor(self):
        p=hand();world=np.tile(p,(6,2,1,1));uv=world[...,:2].copy();valid=np.zeros((6,2),bool);valid[[0,1,5],0]=True
        world[1,0]=Rotation.from_euler('x',180,degrees=True).apply(p)
        accepted,rejected=reject_palm_flips(world,uv,valid,30)
        self.assertTrue(rejected[1,0]);self.assertFalse(accepted[1,0]);self.assertEqual(int(rejected.sum()),1)
        # A real visible movement contradicts the stationary image evidence.
        uv[1,0]*=-1
        self.assertFalse(reject_palm_flips(world,uv,valid,30)[1].any())
        # A persistent turn is not an isolated front/back ambiguity.
        uv[1,0]*=-1;world[5,0]=world[1,0]
        self.assertFalse(reject_palm_flips(world,uv,valid,30)[1].any())


if __name__=='__main__':unittest.main()
