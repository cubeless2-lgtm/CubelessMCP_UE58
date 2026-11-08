"""Editor-only preset repair: map terminal toes, never the entire ankle chain.

SMPL's Left/RightFoot chains span foot -> ball. Manny's Left/RightToe
chains contain ball only. Mapping these directly changes the source parent
frame and can transfer ankle motion into the toe. Preserve the existing leg
and foot chains; add explicit ball-only source chains for Manny's toes.
"""
import unreal


def configure_toe_chains(source_rig, retargeter):
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    source = unreal.IKRigController.get_controller(source_rig)
    assert controller.get_ik_rig(unreal.RetargetSourceOrTarget.SOURCE) == source_rig
    target = unreal.IKRigController.get_controller(
        controller.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET))
    source_names = {str(c.chain_name) for c in source.get_retarget_chains()}
    target_names = {str(c.chain_name) for c in target.get_retarget_chains()}
    # Validate both sides before mutating either one; do not reinterpret a
    # custom user's differently named/defined toe chain.
    for chain, bone in [('LeftToe', 'ball_l'), ('RightToe', 'ball_r')]:
        assert chain in target_names, 'Missing target toe chain: '+chain
        assert str(target.get_retarget_chain_start_bone(chain)) == bone
        assert str(target.get_retarget_chain_end_bone(chain)) == bone
        if chain in source_names:
            assert str(source.get_retarget_chain_start_bone(chain)) == bone
            assert str(source.get_retarget_chain_end_bone(chain)) == bone
    changes = []
    for chain, bone in [('LeftToe', 'ball_l'), ('RightToe', 'ball_r')]:
        if chain not in source_names:
            assert str(source.add_retarget_chain(chain, bone, bone, '')) == chain
            changes.append('Added '+chain+' '+bone+' -> '+bone)
        previous = str(controller.get_source_chain(chain))
        if previous != chain:
            assert controller.set_source_chain(chain, chain)
            changes.append(chain+': '+previous+' -> '+chain)
    return changes
