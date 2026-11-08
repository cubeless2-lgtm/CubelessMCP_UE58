import unittest
import numpy as np
import cv2
from hand_observations import undo_augmentation,overlap_requires_anchor
from hand_crop_consensus import select_crop

class HandObservationTests(unittest.TestCase):
    def test_mirror_and_rotation_round_trip(self):
        uv=np.array([[8.,12.],[20.,25.],[44.,32.]])
        world=np.array([[.01,.02,.03],[-.03,.01,.05],[.02,-.02,.01]])
        for mirrored in [False,True]:
            for angle in [0,90,180]:
                p=uv.copy();q=world.copy()
                if mirrored:p[:,0]=63-p[:,0];q[:,0]*=-1
                m=cv2.getRotationMatrix2D((32,32),angle,1.)
                p=p@m[:,:2].T+m[:,2];q[:,:2]=q[:,:2]@m[:,:2].T
                a,b=undo_augmentation(p,q,cv2.invertAffineTransform(m),64,mirrored,np.array([10,20]))
                np.testing.assert_allclose(a,uv+[10,20],atol=1e-10)
                np.testing.assert_allclose(b,world,atol=1e-10)

    def test_occluded_identity_cannot_spawn_from_classification_flip(self):
        self.assertTrue(overlap_requires_anchor(1,165,[164,150],True,6))
        self.assertFalse(overlap_requires_anchor(0,165,[164,150],True,6))
        self.assertFalse(overlap_requires_anchor(1,165,[164,150],False,6))

    def test_crop_selection_requires_material_independent_agreement(self):
        points=np.arange(42,dtype=float).reshape(21,2)
        wrong=points.copy();wrong[17:21,1]+=20
        best,scores=select_crop([wrong,points],{'keypoints2d':points.tolist()})
        self.assertEqual(best,1)
        self.assertEqual(select_crop([wrong,points],None)[0],0)
        self.assertEqual(select_crop([points+.2,points],{'keypoints2d':points.tolist()})[0],0)

if __name__=='__main__':unittest.main()
