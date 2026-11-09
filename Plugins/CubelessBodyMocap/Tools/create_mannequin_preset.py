"""Run inside Unreal Editor once. Creates only the named preset assets; refuses overwrite."""
import unreal, json
ROOT='/Game/CubelessMocap/Setup'
paths=[ROOT+'/IK_CBM_SMPL',ROOT+'/IK_CBM_Manny',ROOT+'/RTG_CBM_SMPL_to_Manny']
assert all(not unreal.EditorAssetLibrary.does_asset_exist(p) for p in paths), 'Preset exists: inspect it instead of overwriting.'
source=unreal.EditorAssetLibrary.duplicate_asset('/CubelessBodyMocap/IK_SMPL',paths[0])
target=unreal.EditorAssetLibrary.duplicate_asset('/Game/Characters/UE5_Mannequins/Rigs/IK_UE5_Mannequin_Retarget',paths[1])
assert source and target
sc=unreal.IKRigController.get_controller(source)
tc=unreal.IKRigController.get_controller(target)
assert sc.set_skeletal_mesh(unreal.load_asset('/CubelessBodyMocap/SKM_SMPL'))
assert tc.set_skeletal_mesh(unreal.load_asset('/Game/Characters/UE5_Mannequins/Meshes/SKM_Manny'))
sc.add_retarget_chain('Root','root','root','')
sc.add_retarget_chain('LeftToe','ball_l','ball_l','')
sc.add_retarget_chain('RightToe','ball_r','ball_r','')
asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('RTG_CBM_SMPL_to_Manny',ROOT,unreal.IKRetargeter,unreal.IKRetargetFactory())
assert asset
c=unreal.IKRetargeterController.get_controller(asset)
c.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,source)
c.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,target)
c.add_default_ops()
c.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE,source)
c.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET,target)
source_names={str(x.chain_name).lower():str(x.chain_name) for x in sc.get_retarget_chains()}
mappings={}
for chain in tc.get_retarget_chains():
 name=str(chain.chain_name)
 # The default op setup fuzzy-maps Manny metacarpals to the same SMPL
 # phalanx chains. Clear those mappings explicitly: otherwise MCP motion
 # is applied twice even when hand enhancement is disabled.
 if name.lower().endswith('metacarpal'):
  assert c.set_source_chain('',name), 'Cannot clear '+name
  mappings[name]=''
  continue
 # Terminal toes require terminal source chains; do not reuse foot -> ball.
 source_name=source_names.get(name.lower(),'')
 if source_name:
  assert c.set_source_chain(source_name,name), 'Cannot map '+name
  mappings[name]=source_name
c.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
# UE default setup can infer pelvis as TargetRoot. Root Motion runs after pelvis/FK/IK;
# writing it into pelvis would overwrite hip rotation and corrupt child offsets.
for index in range(c.get_num_retarget_ops()):
 op=c.get_op_controller(index)
 if isinstance(op,unreal.IKRetargetRootMotionController):
  op.set_source_root_bone('root')
  op.set_target_root_bone('root')
  op.set_target_pelvis_bone('pelvis')
  assert str(op.get_target_root_bone())=='root'

for obj in (source,target,asset):
 assert unreal.EditorAssetLibrary.save_loaded_asset(obj,only_if_is_dirty=False)
print('CBM_PRESET '+json.dumps({'paths':paths,'chain_mapping':mappings,'ops':c.get_num_retarget_ops()}))
