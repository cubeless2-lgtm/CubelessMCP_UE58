import unittest
import numpy as np
from hand_image_support import image_support, select_supported_candidate
class ImageSupportTests(unittest.TestCase):
 def setUp(self):
  self.uv=np.arange(42,dtype=float).reshape(21,2);self.k=np.c_[self.uv,np.ones(21)*.9]
 def test_independent_full_hand_rejects_forearm_patch(self):
  self.assertEqual(image_support(self.uv,self.k)['cost'],0.)
  self.assertGreater(image_support(self.uv+[60,0],self.k)['cost'],.8)
 def test_weak_or_invalid_reference_does_not_veto(self):
  self.k[4:,2]=.1
  self.assertIsNone(image_support(self.uv,self.k))
  self.assertIsNone(image_support(np.full((21,2),np.nan),self.k))
 def test_only_material_agreement_changes_preference(self):
  def candidate(rank,cost):return (1,rank,.1,None,None,{'image_support':None if cost is None else {'cost':cost}})
  first=candidate(0,.24);better=candidate(2,.12)
  self.assertIs(select_supported_candidate([first,better]),better)
  self.assertIs(select_supported_candidate([first,candidate(2,.2)]),first)
  unknown=candidate(0,None)
  self.assertIs(select_supported_candidate([unknown,better]),unknown)
 def test_scale_and_translation_preserve_agreement(self):
  cost=image_support(self.uv+[8,0],self.k)['cost'];k=self.k.copy();k[:,:2]=k[:,:2]*2+100
  self.assertAlmostEqual(cost,image_support((self.uv+[8,0])*2+100,k)['cost'])
if __name__=='__main__':unittest.main()
