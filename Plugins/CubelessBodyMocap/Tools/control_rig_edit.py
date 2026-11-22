"""Editor Python helpers for the prepared Sequencer Control Rig, without C++ edits.
Call through Unreal Python (MCP execute_python transaction_policy='script' for asset-editor changes).
Caller owns the editing sequence; duplicate it before testing a new correction.
"""
import math
import unreal


def _context(sequence):
    if isinstance(sequence, str):
        sequence = unreal.load_asset(sequence)
    if not isinstance(sequence, unreal.LevelSequence):
        raise ValueError('Expected an existing LevelSequence')
    if unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence() != sequence:
        raise ValueError('Open the intended sequence first, then call on a later tick')
    rigs = unreal.ControlRigSequencerLibrary.get_control_rigs(sequence)
    if len(rigs) != 1 or len(rigs[0].track.get_sections()) != 1:
        raise ValueError('Expected exactly one editable Control Rig and section')
    return sequence, rigs[0]


def offset_rotation(sequence, control_name, start_seconds, end_seconds,
                    pitch=0., yaw=0., roll=0., ease_seconds=.5):
    """Key a local rotation offset within [start,end], with ramps on both ends.
    Changes only the named control and saves only the supplied editing sequence.
    """
    sequence, proxy = _context(sequence)
    values = [start_seconds,end_seconds,pitch,yaw,roll,ease_seconds]
    if not all(math.isfinite(x) for x in values) or end_seconds <= start_seconds or ease_seconds < 0:
        raise ValueError('Invalid correction interval or angles')
    if control_name not in [str(k.name) for k in proxy.control_rig.get_hierarchy().get_controls()]:
        raise ValueError('Control does not exist')
    rate = sequence.get_display_rate();fps = rate.numerator/rate.denominator
    start=sequence.get_playback_start();end=sequence.get_playback_end()
    if end-start > 3600 or start_seconds*fps < start or end_seconds*fps > end:
        raise ValueError('Correction outside sequence range or over3600frames')
    frames=[unreal.FrameNumber(i) for i in range(start,end)]
    library=unreal.ControlRigSequencerLibrary
    rows=library.batch_get_control_transforms(sequence,proxy.control_rig,[control_name],frames,unreal.ControlRigTransformSpace.LOCAL)
    if len(rows)!=1 or len(rows[0].transforms)!=len(frames):
        raise ValueError('Expected one transform control with all requested samples')
    transforms=list(rows[0].transforms)
    for frame,transform in zip(range(start,end),transforms):
        t=frame/fps
        weight=0.
        if start_seconds < t < end_seconds:
            weight=min(1.,(t-start_seconds)/ease_seconds,(end_seconds-t)/ease_seconds) if ease_seconds else 1.
        if weight:
            transform.rotation=transform.rotation*unreal.Rotator(pitch=pitch*weight,yaw=yaw*weight,roll=roll*weight).quaternion()
    # Rebuild the row explicitly: assigning to an array's temporary struct copy
    # does not update the payload. Pin writes to the intended owned section.
    row=unreal.ArrayOfRigControlTransforms(control_name=control_name,transforms=transforms)
    if not library.batch_set_control_transforms(sequence,proxy.control_rig,[row],frames,unreal.ControlRigTransformSpace.LOCAL,proxy.track.get_sections()[0]):
        raise RuntimeError('Control keying failed; inspect the sequence before retrying')
    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence,only_if_is_dirty=True):
        raise RuntimeError('Sequence save failed; unsaved edit retained')
    return dict(sequence=sequence.get_path_name(),control=control_name,start=start_seconds,end=end_seconds)


def export_animation(sequence, template_animation, output_package):
    """Save a NEW AnimSequence; preserve the template and existing outputs."""
    sequence,proxy=_context(sequence)
    if not output_package.startswith('/Game/') or unreal.EditorAssetLibrary.does_asset_exist(output_package):
        raise ValueError('Output must be a new /Game asset')
    if isinstance(template_animation,str):template_animation=unreal.load_asset(template_animation)
    if not isinstance(template_animation,unreal.AnimSequence):raise ValueError('Expected AnimSequence template')
    asset=unreal.EditorAssetLibrary.duplicate_asset(template_animation.get_path_name(),output_package)
    if not asset:raise RuntimeError('Cannot create output animation')
    rate=sequence.get_display_rate();options=unreal.AnimSeqExportOption()
    for name,value in dict(export_transforms=True,use_custom_frame_rate=True,custom_frame_rate=rate,
        use_custom_time_range=True,custom_start_frame=unreal.FrameNumber(sequence.get_playback_start()),
        custom_end_frame=unreal.FrameNumber(sequence.get_playback_end()-1),custom_display_rate=rate,
        record_in_world_space=False,set_retarget_source_asset=True,export_morph_targets=False,
        export_attribute_curves=False,export_material_curves=False,transact_recording=False).items():
        options.set_editor_property(name,value)
    if not unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(asset,options,proxy.proxy,False):
        raise RuntimeError('Export failed; new partial output retained, do not replay blindly')
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=True):
        raise RuntimeError('Animation save failed; unsaved output retained')
    return asset
